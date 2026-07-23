// Teensy Audio utility/dspinst.h — portable versions of ARM DSP intrinsics.
#pragma once
#include <stdint.h>

static inline int32_t signed_saturate_rshift(int32_t val, int bits, int rshift) {
  int64_t v = (int64_t)val >> rshift;
  int64_t lim = ((int64_t)1 << (bits - 1));
  if (v >= lim) return (int32_t)(lim - 1);
  if (v < -lim) return (int32_t)(-lim);
  return (int32_t)v;
}

static inline int32_t signed_multiply_32x16b(int32_t a, uint32_t b) {
  return (int32_t)(((int64_t)a * (int16_t)(b & 0xFFFF)) >> 16);
}

static inline int32_t signed_multiply_32x16t(int32_t a, uint32_t b) {
  return (int32_t)(((int64_t)a * (int16_t)(b >> 16)) >> 16);
}

static inline int32_t multiply_32x32_rshift32(int32_t a, int32_t b) {
  return (int32_t)(((int64_t)a * (int64_t)b) >> 32);
}

static inline int32_t multiply_32x32_rshift32_rounded(int32_t a, int32_t b) {
  return (int32_t)((((int64_t)a * (int64_t)b) + 0x80000000LL) >> 32);
}

static inline int32_t multiply_accumulate_32x32_rshift32_rounded(int32_t sum, int32_t a, int32_t b) {
  return sum + multiply_32x32_rshift32_rounded(a, b);
}

static inline int32_t multiply_subtract_32x32_rshift32_rounded(int32_t sum, int32_t a, int32_t b) {
  return sum - multiply_32x32_rshift32_rounded(a, b);
}

static inline uint32_t pack_16b_16b(int32_t a, int32_t b) {
  return (((uint32_t)a & 0xFFFF) << 16) | ((uint32_t)b & 0xFFFF);
}

static inline uint32_t pack_16t_16t(int32_t a, int32_t b) {
  return ((uint32_t)a & 0xFFFF0000) | (((uint32_t)b >> 16) & 0xFFFF);
}

static inline uint32_t pack_16t_16b(int32_t a, int32_t b) {
  return ((uint32_t)a & 0xFFFF0000) | ((uint32_t)b & 0xFFFF);
}

static inline uint32_t signed_add_16_and_16(uint32_t a, uint32_t b) {
  int16_t lo = (int16_t)(a & 0xFFFF) + (int16_t)(b & 0xFFFF);
  int16_t hi = (int16_t)(a >> 16) + (int16_t)(b >> 16);
  return (((uint32_t)(uint16_t)hi) << 16) | (uint16_t)lo;
}

static inline uint32_t signed_saturate_16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return (uint32_t)(uint16_t)-32768;
  return (uint32_t)(uint16_t)(int16_t)v;
}

static inline int16_t saturate16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
}
