// USB Host stub — no devices ever enumerate.
#pragma once
#include <Arduino.h>

class USBHost {
public:
  void begin() {}
  void Task() {}
};

class USBHub {
public:
  explicit USBHub(USBHost &) {}
};

class MIDIDevice_BigBuffer {
public:
  explicit MIDIDevice_BigBuffer(USBHost &) {}
  operator bool() { return false; }
  bool read(uint8_t = 0) { return false; }
  uint8_t getType() { return 0; }
  uint8_t getChannel() { return 1; }
  uint8_t getData1() { return 0; }
  uint8_t getData2() { return 0; }
  uint8_t getCable() { return 0; }
  const uint8_t *getSysExArray() { return nullptr; }
  unsigned getSysExArrayLength() { return 0; }
  uint16_t idVendor() { return 0; }
  uint16_t idProduct() { return 0; }
  void send(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t = 0) {}
  void sendNoteOn(uint8_t, uint8_t, uint8_t, uint8_t = 0) {}
  void sendNoteOff(uint8_t, uint8_t, uint8_t, uint8_t = 0) {}
  void sendControlChange(uint8_t, uint8_t, uint8_t, uint8_t = 0) {}
  void sendProgramChange(uint8_t, uint8_t, uint8_t = 0) {}
  void sendAfterTouch(uint8_t, uint8_t, uint8_t = 0) {}
  void sendPitchBend(int, uint8_t, uint8_t = 0) {}
  void sendRealTime(uint8_t, uint8_t = 0) {}
  void sendSysEx(uint32_t, const uint8_t *, bool = false, uint8_t = 0) {}
};
