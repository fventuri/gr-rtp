// Miscellaneous low-level routines for ka9q-radio
// Copyright 2018, Phil Karn, KA9Q

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <stdint.h>

#include "misc.h"

int64_t gps_time_ns(void){
  struct timespec ts = {0};
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec -= UNIX_EPOCH;
  ts.tv_sec += GPS_UTC_OFFSET;
  return ts.tv_sec * BILLION + ts.tv_nsec;
}

// FNV-1 hash (https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function)
uint32_t fnv1hash(const uint8_t *s,unsigned long length){
  uint32_t hash = 0x811c9dc5;
  while(length-- > 0){
    hash *= 0x01000193;
    hash ^= *s++;
  }
  return hash;
}
