#pragma once
#include <stdint.h>

// ESP32 Dev Module (ESP32-WROOM); not ESP32-C3 (controller count differs).
// Keep credentials here locally; do not commit real credentials.
static const char WIFI_SSID[] = "YOUR_WIFI";
static const char WIFI_PASSWORD[] = "YOUR_PASSWORD";
static const char MQTT_HOST[] = "192.168.1.100"; // IP Pi 4
static const uint16_t MQTT_PORT = 1883;
static const char MQTT_USER[] = "esp32";
static const char MQTT_PASSWORD[] = "CHANGE_ME";
static const char NODE_ID[] = "NODE_01";

#define SENSOR_COUNT 2 // 2 or 3. Third requires 0x45 on bus 0.
#define ENABLE_EDGE_IMPULSE 0 // 1 only after installing the model library
#define SAMPLE_HZ 1 // 1 for new model; 7 to test the supplied old model
static_assert(SENSOR_COUNT == 2 || SENSOR_COUNT == 3, "Use 2 or 3 sensors");
static_assert(SAMPLE_HZ == 1 || SAMPLE_HZ == 7, "Validated schedules: 1 or 7 Hz");

struct SensorConfig { const char *id; const char *zone; uint8_t bus; uint8_t address; };
static const SensorConfig SENSORS[] = {
  {"SHT_01", "ZONE_01", 0, 0x44}, // SDA21 SCL22
  {"SHT_02", "ZONE_01", 1, 0x44}, // SDA18 SCL19
#if SENSOR_COUNT == 3
  {"SHT_03", "ZONE_02", 0, 0x45}, // ADDR high; shares SDA21 SCL22
#endif
};
static const int LED_PIN = 25; // provisional: verify LED wiring before upload
static const bool LED_ACTIVE_HIGH = true;
static const uint32_t SAMPLE_PERIOD_US = 1000000UL / SAMPLE_HZ;
static const uint32_t PUBLISH_PERIOD_MS = 5000;
static const uint32_t STALE_MS = 3000;
static const float TEMP_ON = 50.0f, TEMP_OFF = 49.0f;
static const float HUM_ON = 70.0f, HUM_OFF = 68.0f;
// Merge is opt-in after verifying both sensors measure the SAME zone.
static const bool MERGE_ENABLED = false;
static const float MERGE_TEMP_C = 0.5f, MERGE_HUM_RH = 3.0f;
static const float AI_ALARM_PROBABILITY = 0.80f; // tune on held-out data
static const char AI_ALARM_LABEL[] = "qua_nhiet";
static const char AI_NORMAL_LABEL[] = "binhthuong";
static const uint32_t AI_STRIDE_MS = 1000;
static const uint16_t HISTORY_SIZE = 15; // Pi baseline uses last 15 real samples
static const uint32_t OFFLOAD_PERIOD_MS = 15000, OFFLOAD_TIMEOUT_MS = 5000;
// 0=off, 1=uncertain local probability [0.4, 0.8), 2=always (experiment)
static const int OFFLOAD_POLICY = 0;
