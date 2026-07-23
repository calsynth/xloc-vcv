// SD card shim: maps to <storage_dir>/sd on the host.
#pragma once
#include <FS.h>

class SDClass : public FS {
public:
  bool begin(uint8_t = 0);
  bool mediaPresent() { return true; }
};

extern SDClass SD;
