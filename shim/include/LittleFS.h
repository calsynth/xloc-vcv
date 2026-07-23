// LittleFS shim: maps to <storage_dir>/lfs on the host.
#pragma once
#include <FS.h>

class LittleFS_Program : public FS {
public:
  bool begin(uint32_t size = 0);
  bool quickFormat() { return true; }
  bool lowLevelFormat(char = '.') { return true; }
};
