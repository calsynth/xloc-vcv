// Portable (host) implementations of the ARM DSP intrinsics used by the
// Teensy Audio library and Phazerville firmware. Exact-semantics C versions.
#pragma once
#include <stdint.h>

static inline int32_t signed_saturate_rshift(int32_t val, int bits, int rshift) {
  int64_t v = (int64_t)val >> rshift;
  int64_t lim = (int64_t)1 << (bits - 1);
  if (v >= lim) return (int32_t)(lim - 1);
  if (v < -lim) return (int32_t)(-lim);
  return (int32_t)v;
}

static inline int16_t saturate16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
}

// (a[31:0] * b[15:0]) >> 16
static inline int32_t signed_multiply_32x16b(int32_t a, uint32_t b) {
  return (int32_t)(((int64_t)a * (int16_t)(b & 0xFFFF)) >> 16);
}
// (a[31:0] * b[31:16]) >> 16
static inline int32_t signed_multiply_32x16t(int32_t a, uint32_t b) {
  return (int32_t)(((int64_t)a * (int16_t)(b >> 16)) >> 16);
}
static inline int32_t signed_multiply_accumulate_32x16b(int32_t sum, int32_t a, uint32_t b) {
  return sum + signed_multiply_32x16b(a, b);
}
static inline int32_t signed_multiply_accumulate_32x16t(int32_t sum, int32_t a, uint32_t b) {
  return sum + signed_multiply_32x16t(a, b);
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

// a[15:0] * b[31:16] (signed 16x16 -> 32)
static inline int32_t multiply_16bx16t(uint32_t a, uint32_t b) {
  return (int32_t)(int16_t)(a & 0xFFFF) * (int16_t)(b >> 16);
}
static inline int32_t multiply_16tx16t(uint32_t a, uint32_t b) {
  return (int32_t)(int16_t)(a >> 16) * (int16_t)(b >> 16);
}
static inline int32_t multiply_16bx16b(uint32_t a, uint32_t b) {
  return (int32_t)(int16_t)(a & 0xFFFF) * (int16_t)(b & 0xFFFF);
}
// SMLAD-ish: a.top*b.top + a.bot*b.bot (no accumulator saturation on overflow
// matters here; matches ARM smuad wrap behaviour for our value ranges)
static inline int32_t multiply_accumulate_16tx16t_add_16bx16b(int32_t sum, uint32_t a, uint32_t b) {
  return sum + multiply_16tx16t(a, b) + multiply_16bx16b(a, b);
}

static inline uint32_t pack_16b_16b(int32_t a, int32_t b) {
  return (((uint32_t)a & 0xFFFF) << 16) | ((uint32_t)b & 0xFFFF);
}
static inline uint32_t pack_16t_16t(int32_t a, int32_t b) {
  return ((uint32_t)a & 0xFFFF0000u) | (((uint32_t)b >> 16) & 0xFFFF);
}
static inline uint32_t pack_16t_16b(int32_t a, int32_t b) {
  return ((uint32_t)a & 0xFFFF0000u) | ((uint32_t)b & 0xFFFF);
}

// per-halfword signed add (no saturation, like ARM sadd16 wrap semantics)
static inline uint32_t signed_add_16_and_16(uint32_t a, uint32_t b) {
  int16_t lo = (int16_t)((int16_t)(a & 0xFFFF) + (int16_t)(b & 0xFFFF));
  int16_t hi = (int16_t)((int16_t)(a >> 16) + (int16_t)(b >> 16));
  return (((uint32_t)(uint16_t)hi) << 16) | (uint16_t)lo;
}
// per-halfword saturating add (qadd16)
static inline uint32_t signed_saturate_add_16_and_16(uint32_t a, uint32_t b) {
  int32_t lo = (int16_t)(a & 0xFFFF) + (int16_t)(b & 0xFFFF);
  int32_t hi = (int16_t)(a >> 16) + (int16_t)(b >> 16);
  return (((uint32_t)(uint16_t)saturate16(hi)) << 16) | (uint16_t)saturate16(lo);
}

static inline int32_t signed_saturate_16(int32_t v) { return saturate16(v); }

static inline uint32_t get_q_psr(void) { return 0; }
static inline void clr_q_psr(void) {}

// qsub: saturating 32-bit subtract
static inline int32_t substract_32_saturate(uint32_t a, uint32_t b) {
  int64_t d = (int64_t)(int32_t)a - (int64_t)(int32_t)b;
  if (d > 2147483647LL) return 2147483647;
  if (d < -2147483648LL) return (int32_t)-2147483648LL;
  return (int32_t)d;
}

// --- additional intrinsics used by Phazerville in-tree DSP ---
static inline int32_t multiply_16tx16b(uint32_t a, uint32_t b) {
  return (int32_t)(int16_t)(a >> 16) * (int16_t)(b & 0xFFFF);
}
static inline int32_t multiply_16tx16b_add_16bx16t(uint32_t a, uint32_t b) {
  return multiply_16tx16b(a, b) + multiply_16bx16t(a, b);  // smuadx
}
static inline int32_t multiply_16tx16t_add_16bx16b(uint32_t a, uint32_t b) {
  return multiply_16tx16t(a, b) + multiply_16bx16b(a, b);  // smuad
}
static inline int32_t multiply_accumulate_16tx16b_add_16bx16t(int32_t sum, uint32_t a, uint32_t b) {
  return sum + multiply_16tx16b_add_16bx16t(a, b);
}
static inline uint32_t multiply_u32xu32_rshift32(uint32_t a, uint32_t b) {
  return (uint32_t)(((uint64_t)a * (uint64_t)b) >> 32);
}
static inline uint32_t pack_16x16(int32_t a, int32_t b) {
  return (((uint32_t)a & 0xFFFF) << 16) | ((uint32_t)b & 0xFFFF);
}
// shadd16 / shsub16: per-halfword signed halving add/sub
static inline uint32_t signed_halving_add_16_and_16(uint32_t a, uint32_t b) {
  int16_t lo = (int16_t)((((int32_t)(int16_t)(a & 0xFFFF)) + (int16_t)(b & 0xFFFF)) >> 1);
  int16_t hi = (int16_t)((((int32_t)(int16_t)(a >> 16)) + (int16_t)(b >> 16)) >> 1);
  return (((uint32_t)(uint16_t)hi) << 16) | (uint16_t)lo;
}
static inline uint32_t signed_halving_subtract_16_and_16(uint32_t a, uint32_t b) {
  int16_t lo = (int16_t)((((int32_t)(int16_t)(a & 0xFFFF)) - (int16_t)(b & 0xFFFF)) >> 1);
  int16_t hi = (int16_t)((((int32_t)(int16_t)(a >> 16)) - (int16_t)(b >> 16)) >> 1);
  return (((uint32_t)(uint16_t)hi) << 16) | (uint16_t)lo;
}
// ssub16 (wrap semantics)
static inline uint32_t signed_subtract_16_and_16(uint32_t a, uint32_t b) {
  int16_t lo = (int16_t)((int16_t)(a & 0xFFFF) - (int16_t)(b & 0xFFFF));
  int16_t hi = (int16_t)((int16_t)(a >> 16) - (int16_t)(b >> 16));
  return (((uint32_t)(uint16_t)hi) << 16) | (uint16_t)lo;
}
