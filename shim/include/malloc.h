// malloc.h shim: portable mallinfo so OC_core.cpp's FreeRam works on
// both Linux and macOS (macOS has no malloc.h/mallinfo).
#pragma once
#include <stdlib.h>
#include <string.h>

#ifndef XEMU_MALLINFO_DEFINED
#define XEMU_MALLINFO_DEFINED
struct mallinfo {
  int arena, ordblks, smblks, hblks, hblkhd;
  int usmblks, fsmblks, uordblks, fordblks, keepcost;
};
static inline struct mallinfo mallinfo(void) {
  struct mallinfo mi;
  memset(&mi, 0, sizeof mi);
  mi.fordblks = 8 << 20;  // pretend 8 MB free — keeps Factory<> on the heap
  return mi;
}
#endif
