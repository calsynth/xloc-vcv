#pragma once
#include <math.h>
#include <stdint.h>

static inline uint32_t sqrt_uint32(uint32_t in) { return (uint32_t)sqrt((double)in); }
static inline uint32_t sqrt_uint32_approx(uint32_t in) { return sqrt_uint32(in); }
