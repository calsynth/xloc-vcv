// EEPROM emulation backed by a file in the storage dir (or RAM if none).
#pragma once
#include <stdint.h>
#include <stddef.h>

#define E2END 4283  // Teensy 4.1 emulated EEPROM size - 1

uint8_t xemu_eeprom_read(int addr);
void xemu_eeprom_write(int addr, uint8_t value);
void xemu_eeprom_update(int addr, uint8_t value);

// Teensy/Arduino EERef + EEPtr iterator API
struct EERef {
  EERef(int index) : index(index) {}
  operator uint8_t() const { return xemu_eeprom_read(index); }
  EERef &operator=(uint8_t v) {
    xemu_eeprom_write(index, v);
    return *this;
  }
  EERef &operator=(const EERef &ref) { return *this = (uint8_t)ref; }
  EERef &update(uint8_t v) {
    xemu_eeprom_update(index, v);
    return *this;
  }
  int index;
};

struct EEPtr {
  EEPtr(int index) : index(index) {}
  operator int() const { return index; }
  EEPtr &operator=(int v) {
    index = v;
    return *this;
  }
  bool operator!=(const EEPtr &p) const { return index != p.index; }
  bool operator==(const EEPtr &p) const { return index == p.index; }
  EERef operator*() { return EERef(index); }
  EEPtr &operator++() { ++index; return *this; }
  EEPtr &operator--() { --index; return *this; }
  EEPtr operator++(int) { return EEPtr(index++); }
  EEPtr operator--(int) { return EEPtr(index--); }
  int index;
};

class EEPROMClass {
public:
  uint8_t read(int addr);
  void write(int addr, uint8_t value);
  void update(int addr, uint8_t value);
  EERef operator[](int idx) { return EERef(idx); }
  EEPtr begin() { return EEPtr(0); }
  EEPtr end() { return EEPtr(E2END + 1); }
  uint16_t length() { return E2END + 1; }

  template <typename T>
  T &get(int addr, T &t) {
    uint8_t *p = (uint8_t *)&t;
    for (size_t i = 0; i < sizeof(T); ++i) p[i] = read(addr + (int)i);
    return t;
  }
  template <typename T>
  const T &put(int addr, const T &t) {
    const uint8_t *p = (const uint8_t *)&t;
    for (size_t i = 0; i < sizeof(T); ++i) update(addr + (int)i, p[i]);
    return t;
  }
};

extern EEPROMClass EEPROM;
