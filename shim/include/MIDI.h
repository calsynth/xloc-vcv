// Arduino MIDI Library stub (DIN MIDI on Serial8 — inert for now).
#pragma once
#include <Arduino.h>

#define MIDI_CHANNEL_OMNI 0
#define MIDI_CHANNEL_OFF 17

namespace midi {

enum MidiType : uint8_t {
  InvalidType = 0x00,
  NoteOff = 0x80,
  NoteOn = 0x90,
  AfterTouchPoly = 0xA0,
  ControlChange = 0xB0,
  ProgramChange = 0xC0,
  AfterTouchChannel = 0xD0,
  PitchBend = 0xE0,
  SystemExclusive = 0xF0,
  TimeCodeQuarterFrame = 0xF1,
  SongPosition = 0xF2,
  SongSelect = 0xF3,
  TuneRequest = 0xF6,
  Clock = 0xF8,
  Tick = 0xF9,
  Start = 0xFA,
  Continue = 0xFB,
  Stop = 0xFC,
  ActiveSensing = 0xFE,
  SystemReset = 0xFF,
};

typedef uint8_t Channel;
typedef uint8_t DataByte;

template <class SerialPort>
class SerialMIDI {
public:
  explicit SerialMIDI(SerialPort &port) : port_(port) {}
private:
  SerialPort &port_;
};

template <class Transport>
class MidiInterface {
public:
  explicit MidiInterface(Transport &t) : t_(t) {}
  void begin(int = 1) {}
  bool read() { return false; }
  MidiType getType() { return InvalidType; }
  Channel getChannel() { return 1; }
  DataByte getData1() { return 0; }
  DataByte getData2() { return 0; }
  uint8_t *getSysExArray() { return sysex_dummy_; }
  unsigned getSysExArrayLength() { return 0; }
  void send(MidiType, DataByte, DataByte, Channel) {}
  void sendNoteOn(DataByte, DataByte, Channel) {}
  void sendNoteOff(DataByte, DataByte, Channel) {}
  void sendControlChange(DataByte, DataByte, Channel) {}
  void sendProgramChange(DataByte, Channel) {}
  void sendAfterTouch(DataByte, Channel) {}
  void sendPitchBend(int, Channel) {}
  void sendRealTime(MidiType) {}
  void sendSysEx(unsigned, const uint8_t *, bool = false) {}
  void turnThruOn() {}
  void turnThruOff() {}

private:
  Transport &t_;
  uint8_t sysex_dummy_[8] = {0};
};

}  // namespace midi

#define MIDI_CREATE_INSTANCE(Type, port, name)                 \
  static midi::SerialMIDI<Type> name##_transport((port));      \
  midi::MidiInterface<midi::SerialMIDI<Type> > name(name##_transport);
