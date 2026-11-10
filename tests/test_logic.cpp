#include <cassert>
#include <cstdio>
#include "../firmware/EdgeEnvNode/Logic.h"
int main() {
  assert(!alarmLatch(false,50,50,49));
  assert(alarmLatch(false,50.01,50,49));
  assert(alarmLatch(true,49.5,50,49));
  assert(!alarmLatch(true,49,50,49));
  assert(!alarmLatch(false,70,70,68));
  assert(alarmLatch(false,70.1,70,68));
  assert(fresh(10,0xfffffff0U,30)); // millis wrap
  assert(!fresh(5000,1000,3000));
  assert(mergePair(30,60,30.4,62,0.5,3));
  assert(!mergePair(30,60,30.4,65,0.5,3));
  assert(!mergePair(NAN,60,30,60,0.5,3));
  const uint8_t bytes[]={0xbe,0xef}; assert(shtCrc(bytes,2)==0x92);
  SampleWindow<3> a,b;
  for(int i=0;i<5;++i) a.add(i,40+i,i*1000);
  b.add(99,80,1);
  assert(a.count==3 && a.t[a.index(0)]==2 && a.t[a.index(2)]==4);
  assert(b.count==1 && b.t[b.index(0)]==99); // independent sensor history
  a.clear(); assert(a.count==0 && b.count==1);
  puts("PASS core: thresholds, hysteresis, freshness/wrap, merge, CRC, independent windows");
}
