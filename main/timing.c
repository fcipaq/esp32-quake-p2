#include "timing.h"
struct timeval _startTime;
  
uint32_t micros() {
  struct timeval currentTime;
  gettimeofday(&currentTime, NULL);
  return (uint32_t) (((currentTime.tv_sec - _startTime.tv_sec) * 1000000) + currentTime.tv_usec - _startTime.tv_usec);  // - _timeSuspended;
}

uint32_t millis() {
  struct timeval currentTime;
  gettimeofday(&currentTime, NULL);
  return (uint32_t)(((currentTime.tv_sec - _startTime.tv_sec) * 1000) + ((currentTime.tv_usec - _startTime.tv_usec) / 1000));  // - _timeSuspended;
}

void timing_init() {
  gettimeofday(&_startTime, NULL);
}


