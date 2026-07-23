#include <LittleFS.h>
#include <SD.h>
#include <sys/stat.h>

#include "../../emu/xloc_emu.h"

static void mkdirs(const std::string &p) {
  if (!p.empty()) ::mkdir(p.c_str(), 0755);
}

bool SDClass::begin(uint8_t) {
  const std::string &dir = xemu::storage_dir();
  if (dir.empty()) return false;  // no SD card without a storage dir
  mkdirs(dir);
  set_root(dir + "/sd");
  mkdirs(root_);
  return true;
}

bool LittleFS_Program::begin(uint32_t) {
  const std::string &dir = xemu::storage_dir();
  if (dir.empty()) {
    set_root(".xloc-lfs");  // last resort: cwd
  } else {
    mkdirs(dir);
    set_root(dir + "/lfs");
  }
  mkdirs(root_);
  return true;
}

SDClass SD;
