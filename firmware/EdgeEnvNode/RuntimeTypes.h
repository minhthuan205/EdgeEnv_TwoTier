#pragma once
#include "Config.h"
#include "Logic.h"

// In a header so Arduino's generated prototypes see these types.
struct Reading {
  float t=0, h=0, aiP=-1;
  uint32_t at=0, errors=0;
  bool valid=false, overT=false, overH=false, aiAlarm=false;
  char aiStatus[32]="disabled";
};
struct Snapshot {
  Reading r[SENSOR_COUNT];
  SampleWindow<HISTORY_SIZE> history[SENSOR_COUNT];
  uint32_t eventDrops=0, missedSlots=0;
};
struct Event { uint8_t sensor; Reading reading; bool fault; };
