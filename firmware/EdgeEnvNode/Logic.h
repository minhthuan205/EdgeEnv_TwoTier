#pragma once
#include <math.h>
#include <stddef.h>
#include <stdint.h>

inline bool alarmLatch(bool previous, float value, float on, float off) {
  return previous ? value > off : value > on;
}
inline bool fresh(uint32_t now, uint32_t measured, uint32_t limit) {
  return uint32_t(now - measured) <= limit;
}
inline bool mergePair(float t1, float h1, float t2, float h2, float dt, float dh) {
  return isfinite(t1) && isfinite(h1) && isfinite(t2) && isfinite(h2)
      && fabsf(t1-t2) <= dt && fabsf(h1-h2) <= dh;
}
inline uint8_t shtCrc(const uint8_t *p, size_t count) {
  uint8_t crc = 0xff;
  while (count--) {
    crc ^= *p++;
    for (int b=0;b<8;++b) crc = (crc & 0x80) ? uint8_t((crc<<1)^0x31) : uint8_t(crc<<1);
  }
  return crc;
}
template<size_t N> struct SampleWindow {
  float t[N]{}, h[N]{};
  uint32_t ms[N]{};
  size_t head=0, count=0;
  void clear() { head=0; count=0; }
  void add(float temperature, float humidity, uint32_t stamp) {
    t[head]=temperature; h[head]=humidity; ms[head]=stamp;
    head=(head+1)%N; if(count<N) ++count;
  }
  size_t index(size_t chronological) const { return (head+N-count+chronological)%N; }
};
