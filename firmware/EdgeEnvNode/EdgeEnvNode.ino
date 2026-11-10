/* Hierarchical edge: ESP32 (L1) -> Raspberry Pi 4 (L2).
   Dependencies: Arduino-ESP32, ArduinoJson 7, PubSubClient 2.8.
   No original .ino was supplied: this is a replacement reference firmware.
   See README before enabling EI or connecting a third sensor. */
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <time.h>
#include "Config.h"
#include "Logic.h"
#include "RuntimeTypes.h"
#include <nhietdo_doam_inferencing.h>
#if ENABLE_EDGE_IMPULSE
#include <nhietdo_doam_inferencing.h>
static_assert(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME == 2, "Expected temperature + humidity");
static const size_t EI_SAMPLES = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
static_assert(EI_SAMPLES > 0, "Empty EI window");
SampleWindow<EI_SAMPLES> aiWindow[SENSOR_COUNT];
float aiInput[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE]; // only SensorTask accesses
int getSignal(size_t offset, size_t length, float *out) {
  if(offset+length > EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) return -1;
  memcpy(out, aiInput+offset, length*sizeof(float)); return 0;
}
#endif

Snapshot state;
SemaphoreHandle_t stateMutex;
QueueHandle_t eventQueue;
TwoWire bus0(0), bus1(1);
WiFiClient tcp;
PubSubClient mqtt(tcp);
char telemetryTopic[96], eventTopic[96], statusTopic[96], requestTopic[96], responseTopic[96];
char sessionId[24], pendingId[80]="";
uint32_t pendingSince=0, sequence=0, offloadSequence=0;

static void copyState(Snapshot &out) {
  xSemaphoreTake(stateMutex, portMAX_DELAY); out=state; xSemaphoreGive(stateMutex);
}
static bool measurement(uint8_t sensor, float &t, float &h) {
  const SensorConfig &c=SENSORS[sensor];
  TwoWire &w=c.bus==0 ? bus0 : bus1;
  w.beginTransmission(c.address); w.write(0x24); w.write(0x00);
  if(w.endTransmission()!=0) return false;
  // Single-shot, high repeatability, no clock stretching; other sensor idle.
  vTaskDelay(pdMS_TO_TICKS(16));
  if(w.requestFrom(c.address, uint8_t(6))!=6) return false;
  uint8_t b[6]; for(int i=0;i<6;++i) b[i]=w.read();
  if(shtCrc(b,2)!=b[2] || shtCrc(b+3,2)!=b[5]) return false;
  t=-45.0f+175.0f*uint16_t((b[0]<<8)|b[1])/65535.0f;
  h=100.0f*uint16_t((b[3]<<8)|b[4])/65535.0f;
  return isfinite(t) && isfinite(h) && h>=0 && h<=100;
}
static void resetAi(uint8_t i) {
#if ENABLE_EDGE_IMPULSE
  aiWindow[i].clear();
#endif
}
static void infer(uint8_t i, Reading &r) {
  r.aiP=-1; r.aiAlarm=false;
#if ENABLE_EDGE_IMPULSE
  // Never reinterpret a 7 Hz deployment as 1 Hz or patch metadata to bypass.
  if(fabsf(float(EI_CLASSIFIER_FREQUENCY)-float(SAMPLE_HZ))>0.001f) {
    strlcpy(r.aiStatus,"model_rate_mismatch",sizeof(r.aiStatus)); return;
  }
  auto &win=aiWindow[i]; win.add(r.t,r.h,r.at);
  if(win.count<EI_SAMPLES) { strlcpy(r.aiStatus,"warming_up",sizeof(r.aiStatus)); return; }
  static uint32_t lastRun[SENSOR_COUNT]{};
  static float cachedP[SENSOR_COUNT]={};
  static bool cachedValid[SENSOR_COUNT]={};
  if(cachedValid[i] && uint32_t(r.at-lastRun[i])<AI_STRIDE_MS) {
    r.aiP=cachedP[i]; r.aiAlarm=r.aiP>=AI_ALARM_PROBABILITY;
    strlcpy(r.aiStatus,"ready",sizeof(r.aiStatus)); return;
  }
  for(size_t n=0;n<EI_SAMPLES;++n) {
    size_t j=win.index(n); aiInput[n*2]=win.t[j]; aiInput[n*2+1]=win.h[j];
  }
  signal_t signal; signal.total_length=EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; signal.get_data=getSignal;
  ei_impulse_result_t result={};
  if(run_classifier(&signal,&result,false)!=EI_IMPULSE_OK) {
    cachedValid[i]=false; strlcpy(r.aiStatus,"inference_error",sizeof(r.aiStatus)); return;
  }
  bool normalFound=false, alarmFound=false;
  for(size_t k=0;k<EI_CLASSIFIER_LABEL_COUNT;++k) {
    if(strcmp(result.classification[k].label,AI_NORMAL_LABEL)==0) normalFound=true;
    if(strcmp(result.classification[k].label,AI_ALARM_LABEL)==0) {
      r.aiP=result.classification[k].value; alarmFound=true;
    }
  }
  if(!normalFound || !alarmFound) {
    cachedValid[i]=false; r.aiP=-1; strlcpy(r.aiStatus,"model_label_mismatch",sizeof(r.aiStatus)); return;
  }
  lastRun[i]=r.at; cachedP[i]=r.aiP; cachedValid[i]=true;
  r.aiAlarm=r.aiP>=AI_ALARM_PROBABILITY;
  strlcpy(r.aiStatus,"ready",sizeof(r.aiStatus));
#else
  (void)i; strlcpy(r.aiStatus,"disabled",sizeof(r.aiStatus));
#endif
}
static void queueEvent(uint8_t i, const Reading &r) {
  Event e{i,r,!r.valid};
  if(xQueueSend(eventQueue,&e,0)!=pdTRUE) {
    // Drop newest on overflow; preserve queue head while network retries it.
    xSemaphoreTake(stateMutex,portMAX_DELAY); ++state.eventDrops; xSemaphoreGive(stateMutex);
  }
}
static void sensorTask(void *) {
  const uint32_t slot=SAMPLE_PERIOD_US/SENSOR_COUNT;
  uint32_t next=micros(); uint8_t i=0;
  for(;;) {
    if(int32_t(micros()-next)<0) { vTaskDelay(1); continue; }
    if(uint32_t(micros()-next)>slot) {
      xSemaphoreTake(stateMutex,portMAX_DELAY); ++state.missedSlots; xSemaphoreGive(stateMutex);
      next=micros(); // no catch-up burst of duplicate-time samples
    }
    next+=slot;
    Reading r,previous;
    xSemaphoreTake(stateMutex,portMAX_DELAY); previous=state.r[i]; xSemaphoreGive(stateMutex);
    r=previous; r.valid=measurement(i,r.t,r.h); r.at=millis();
    const uint32_t expected=SAMPLE_PERIOD_US/1000;
    bool gap=previous.valid && uint32_t(r.at-previous.at)>expected+expected/4+10;
    if(gap) resetAi(i);
    if(r.valid) {
      r.overT=alarmLatch(previous.overT,r.t,TEMP_ON,TEMP_OFF);
      r.overH=alarmLatch(previous.overH,r.h,HUM_ON,HUM_OFF);
      infer(i,r);
    } else {
      ++r.errors; r.aiP=-1; r.aiAlarm=false; resetAi(i);
      strlcpy(r.aiStatus,"sensor_error",sizeof(r.aiStatus));
      // Keep threshold latch until a good measurement clears it.
    }
    bool led=false;
    xSemaphoreTake(stateMutex,portMAX_DELAY);
    state.r[i]=r;
    if(!r.valid || gap) state.history[i].clear();
    if(r.valid) state.history[i].add(r.t,r.h,r.at);
    for(uint8_t k=0;k<SENSOR_COUNT;++k) {
      const auto &a=state.r[k]; led |= a.overT || a.overH || a.aiAlarm;
      led |= !a.valid || !fresh(millis(),a.at,STALE_MS); // fail-visible
    }
    xSemaphoreGive(stateMutex);
    digitalWrite(LED_PIN,led==LED_ACTIVE_HIGH ? HIGH : LOW);
    if(r.valid!=previous.valid || r.overT!=previous.overT || r.overH!=previous.overH || r.aiAlarm!=previous.aiAlarm)
      queueEvent(i,r);
    Serial.printf("%s T=%.2f H=%.2f valid=%d AI=%s p=%.3f\n",SENSORS[i].id,r.t,r.h,r.valid,r.aiStatus,r.aiP);
    i=(i+1)%SENSOR_COUNT;
  }
}
static bool publishJson(const char *topic, const JsonDocument &d, bool retained=false) {
  char out[4096]; size_t n=measureJson(d);
  if(n>=sizeof(out)) { Serial.println("MQTT JSON too large"); return false; }
  serializeJson(d,out,sizeof(out));
  bool ok=mqtt.publish(topic,out,retained); // PubSubClient publishes QoS 0
  if(!ok) Serial.println("MQTT publish failed");
  return ok;
}
static void envelope(JsonDocument &d, const char *kind) {
  d["schema"]=1; d["kind"]=kind; d["node_id"]=NODE_ID; d["session_id"]=sessionId;
  d["seq"]=++sequence; d["uptime_ms"]=millis();
  time_t now=time(nullptr); if(now>1700000000) d["unix_s"]=int64_t(now); else d["unix_s"]=nullptr;
}
static void addReading(JsonObject o, uint8_t i, const Reading &r, uint32_t now) {
  o["sensor_id"]=SENSORS[i].id; o["zone_id"]=SENSORS[i].zone;
  o["valid"]=r.valid; o["stale"]=!fresh(now,r.at,STALE_MS); o["sample_uptime_ms"]=r.at;
  if(r.valid) { o["temperature_c"]=r.t; o["humidity_rh"]=r.h; }
  o["over_temperature"]=r.overT; o["over_humidity"]=r.overH; o["ai_alarm"]=r.aiAlarm;
  o["ai_status"]=r.aiStatus;
  if(r.aiP>=0) o["ai_probability"]=r.aiP; else o["ai_probability"]=nullptr;
  o["errors"]=r.errors;
}
static void sendTelemetry(const Snapshot &s) {
  JsonDocument d; envelope(d,"telemetry"); uint32_t now=millis();
  d["sample_hz"]=SAMPLE_HZ; d["event_drops"]=s.eventDrops; d["missed_slots"]=s.missedSlots;
  JsonArray records=d["records"].to<JsonArray>(); bool used[SENSOR_COUNT]={};
  for(uint8_t i=0;i<SENSOR_COUNT;++i) {
    if(used[i]) continue;
    const auto &a=s.r[i];
    int partner=-1;
    if(MERGE_ENABLED && a.valid && fresh(now,a.at,STALE_MS) && !a.overT && !a.overH && !a.aiAlarm && strcmp(a.aiStatus,"ready")==0) {
      for(uint8_t j=i+1;j<SENSOR_COUNT;++j) {
        const auto &b=s.r[j];
        if(!used[j] && strcmp(SENSORS[i].zone,SENSORS[j].zone)==0 && b.valid && fresh(now,b.at,STALE_MS)
          && !b.overT && !b.overH && !b.aiAlarm && strcmp(b.aiStatus,"ready")==0
          && mergePair(a.t,a.h,b.t,b.h,MERGE_TEMP_C,MERGE_HUM_RH)) { partner=j; break; }
      }
    }
    JsonObject o=records.add<JsonObject>(); addReading(o,i,a,now); used[i]=true;
    JsonArray sources=o["source_ids"].to<JsonArray>(); sources.add(SENSORS[i].id);
    if(partner>=0) {
      const auto &b=s.r[partner]; used[partner]=true;
      sources.add(SENSORS[partner].id); o["sensor_id"]=nullptr;
      o["aggregation"]="mean"; o["temperature_c"]=(a.t+b.t)/2; o["humidity_rh"]=(a.h+b.h)/2;
      o["temp_spread_c"]=fabsf(a.t-b.t); o["humidity_spread_rh"]=fabsf(a.h-b.h);
      o["ai_probability"]=nullptr; o["ai_status"]="all_sources_normal";
      JsonArray stamps=o["source_sample_uptime_ms"].to<JsonArray>(); stamps.add(a.at); stamps.add(b.at);
    } else o["aggregation"]="none";
  }
  publishJson(telemetryTopic,d);
}
static void onMessage(char *topic, byte *payload, unsigned int length) {
  if(strcmp(topic,responseTopic)!=0 || !pendingId[0]) return;
  JsonDocument d; if(deserializeJson(d,payload,length)) return;
  const char *rid=d["request_id"] | "";
  if(strcmp(rid,pendingId)!=0 || uint32_t(millis()-pendingSince)>OFFLOAD_TIMEOUT_MS) return;
  // Advisory only; gateway cannot silence local threshold/AI alarms.
  Serial.printf("OFFLOAD result id=%s label=%s roundtrip_ms=%lu\n",rid,
    d["label"] | "unknown",(unsigned long)(millis()-pendingSince));
  pendingId[0]=0;
}
static void maybeOffload(const Snapshot &s) {
  if(OFFLOAD_POLICY==0 || pendingId[0]) return;
  static uint8_t rr=0;
  for(uint8_t n=0;n<SENSOR_COUNT;++n) {
    uint8_t i=rr; rr=(rr+1)%SENSOR_COUNT;
    const auto &r=s.r[i]; const auto &h=s.history[i];
    if(!r.valid || !fresh(millis(),r.at,STALE_MS) || h.count<HISTORY_SIZE) continue;
    if(OFFLOAD_POLICY==1 && !(r.aiP>=0.4f && r.aiP<AI_ALARM_PROBABILITY)) continue;
    JsonDocument d; envelope(d,"offload_request");
    char requestId[80]; snprintf(requestId,sizeof(requestId),"%s-%s-%lu",NODE_ID,sessionId,(unsigned long)++offloadSequence);
    d["request_id"]=requestId; d["sensor_id"]=SENSORS[i].id; d["sample_hz"]=SAMPLE_HZ;
    d["method"]="linear_trend_baseline";
    JsonArray samples=d["samples"].to<JsonArray>();
    for(size_t k=0;k<h.count;++k) {
      size_t j=h.index(k); JsonArray row=samples.add<JsonArray>();
      row.add(uint32_t(h.ms[j]-h.ms[h.index(0)])); row.add(h.t[j]); row.add(h.h[j]);
    }
    if(publishJson(requestTopic,d)) { strlcpy(pendingId,requestId,sizeof(pendingId)); pendingSince=millis(); }
    break;
  }
}
void setup() {
  Serial.begin(115200); pinMode(LED_PIN,OUTPUT); digitalWrite(LED_PIN,LED_ACTIVE_HIGH ? HIGH : LOW);
  snprintf(sessionId,sizeof(sessionId),"%08lx",(unsigned long)esp_random());
  stateMutex=xSemaphoreCreateMutex(); eventQueue=xQueueCreate(16,sizeof(Event));
  if(!stateMutex || !eventQueue) { Serial.println("Allocation failed"); while(true) delay(1000); }
  bus0.begin(21,22,100000); bus1.begin(18,19,100000); bus0.setTimeOut(25); bus1.setTimeOut(25);
  // Restore single-shot mode and turn heaters off, without cutting supply.
  for(uint8_t i=0;i<SENSOR_COUNT;++i) {
    TwoWire &w=SENSORS[i].bus==0 ? bus0 : bus1;
    w.beginTransmission(SENSORS[i].address); w.write(0x30); w.write(0x93); w.endTransmission(); delay(16);
    w.beginTransmission(SENSORS[i].address); w.write(0x30); w.write(0x66); w.endTransmission(); delay(2);
  }
  snprintf(telemetryTopic,sizeof(telemetryTopic),"edgeenv/v1/%s/telemetry",NODE_ID);
  snprintf(eventTopic,sizeof(eventTopic),"edgeenv/v1/%s/events",NODE_ID);
  snprintf(statusTopic,sizeof(statusTopic),"edgeenv/v1/%s/status",NODE_ID);
  snprintf(requestTopic,sizeof(requestTopic),"edgeenv/v1/%s/offload/request",NODE_ID);
  snprintf(responseTopic,sizeof(responseTopic),"edgeenv/v1/%s/offload/response",NODE_ID);
  mqtt.setServer(MQTT_HOST,MQTT_PORT); mqtt.setCallback(onMessage);
  mqtt.setBufferSize(4608); mqtt.setKeepAlive(30); mqtt.setSocketTimeout(1);
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  configTime(0,0,"pool.ntp.org");
  if(xTaskCreatePinnedToCore(sensorTask,"sensors",12288,nullptr,2,nullptr,1)!=pdPASS) {
    Serial.println("Sensor task creation failed"); while(true) delay(1000);
  }
}
void loop() {
  static uint32_t retry=0, lastPublish=0, lastOffload=0;
  uint32_t now=millis();
  if(pendingId[0] && uint32_t(now-pendingSince)>OFFLOAD_TIMEOUT_MS) {
    Serial.printf("OFFLOAD timeout %s; local continues\n",pendingId); pendingId[0]=0;
  }
  if(WiFi.status()!=WL_CONNECTED) { delay(10); return; }
  if(!mqtt.connected()) {
    if(uint32_t(now-retry)>=5000) {
      retry=now; char clientId[80]; snprintf(clientId,sizeof(clientId),"%s-%s",NODE_ID,sessionId);
      if(mqtt.connect(clientId,MQTT_USER,MQTT_PASSWORD,statusTopic,1,true,"offline")) {
        mqtt.subscribe(responseTopic,1); mqtt.publish(statusTopic,"online",true);
      }
    }
    delay(10); return;
  }
  mqtt.loop();
  Event e;
  if(xQueuePeek(eventQueue,&e,0)==pdTRUE) {
    JsonDocument d; envelope(d,"event");
    JsonObject o=d["reading"].to<JsonObject>(); addReading(o,e.sensor,e.reading,millis());
    if(publishJson(eventTopic,d)) xQueueReceive(eventQueue,&e,0);
  }
  if(uint32_t(now-lastPublish)>=PUBLISH_PERIOD_MS) {
    lastPublish=now; Snapshot s; copyState(s); sendTelemetry(s);
  }
  if(uint32_t(now-lastOffload)>=OFFLOAD_PERIOD_MS) {
    lastOffload=now; Snapshot s; copyState(s); maybeOffload(s);
  }
  delay(2);
}
