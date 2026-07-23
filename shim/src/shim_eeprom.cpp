#include <EEPROM.h>

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include "../../emu/xloc_emu.h"

namespace {
struct Backing {
  uint8_t data[E2END + 1];
  bool loaded = false;
  std::mutex m;

  std::string path() {
    const std::string &dir = xemu::storage_dir();
    return dir.empty() ? std::string() : dir + "/eeprom.bin";
  }

  void ensure_loaded() {
    if (loaded) return;
    std::memset(data, 0xFF, sizeof data);  // erased flash reads 0xFF
    std::string p = path();
    if (!p.empty()) {
      if (FILE *f = std::fopen(p.c_str(), "rb")) {
        size_t n = std::fread(data, 1, sizeof data, f);
        (void)n;
        std::fclose(f);
      }
    }
    loaded = true;
  }

  void persist() {
    std::string p = path();
    if (p.empty()) return;
    if (FILE *f = std::fopen(p.c_str(), "wb")) {
      std::fwrite(data, 1, sizeof data, f);
      std::fclose(f);
    }
  }
};

Backing &backing() {
  static Backing b;
  return b;
}
}  // namespace

uint8_t EEPROMClass::read(int addr) {
  Backing &b = backing();
  std::lock_guard<std::mutex> lk(b.m);
  b.ensure_loaded();
  if (addr < 0 || addr > E2END) return 0xFF;
  return b.data[addr];
}

void EEPROMClass::write(int addr, uint8_t value) {
  Backing &b = backing();
  std::lock_guard<std::mutex> lk(b.m);
  b.ensure_loaded();
  if (addr < 0 || addr > E2END) return;
  b.data[addr] = value;
  b.persist();
}

void EEPROMClass::update(int addr, uint8_t value) {
  Backing &b = backing();
  std::lock_guard<std::mutex> lk(b.m);
  b.ensure_loaded();
  if (addr < 0 || addr > E2END) return;
  if (b.data[addr] != value) {
    b.data[addr] = value;
    b.persist();
  }
}

uint8_t xemu_eeprom_read(int addr) { return EEPROM.read(addr); }
void xemu_eeprom_write(int addr, uint8_t value) { EEPROM.write(addr, value); }
void xemu_eeprom_update(int addr, uint8_t value) { EEPROM.update(addr, value); }

EEPROMClass EEPROM;
