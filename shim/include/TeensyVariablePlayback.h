// teensy-variable-playback stub (WAV players silent in CV-only phase).
// API mirrors ResamplingReader / AudioPlaySdResmp from the real library.
#pragma once
#include <Audio.h>
#include <SD.h>

typedef enum loop_type { looptype_none = 0, looptype_repeat, looptype_pingpong } loop_type;

typedef enum play_start {
  play_start_sample = 0,
  play_start_loop,
  play_start_arbitrary,
} play_start;

class AudioPlaySdResmp : public AudioStream {
public:
  AudioPlaySdResmp() : AudioStream(0, nullptr) {}
  void update() override {}
  void begin() {}
  bool play(const char *filename, bool isWave = true, uint16_t numChannelsIfRaw = 0,
            bool startPaused = false) {
    (void)filename; (void)isWave; (void)numChannelsIfRaw; (void)startPaused;
    return false;
  }
  bool play() { return false; }
  bool playWav(const char *filename) { (void)filename; return false; }
  bool playRaw(const char *filename, uint16_t channels = 1) {
    (void)filename; (void)channels; return false;
  }
  void stop() {}
  bool isPlaying() { return false; }
  void togglePlayPause() {}
  void retrigger() {}
  void syncTrig() {}
  void enableInterpolation(bool enable) { (void)enable; }
  void setBufferInPSRAM(bool flag) { (void)flag; }
  void setPlaybackRate(double rate) { (void)rate; }
  void setLoopType(loop_type t) { (void)t; }
  void setLoopStart(uint32_t s) { (void)s; }
  void setLoopFinish(uint32_t f) { (void)f; }
  uint32_t getLoopStart() { return 0; }
  void setPlayStart(play_start start, uint32_t playback_start = 0) {
    (void)start; (void)playback_start;
  }
  void setBeatStart(uint16_t beatnum) { (void)beatnum; }
  float getBPM() { return 0.f; }
  void matchTempo(float target) { (void)target; }
  uint32_t getPosition() { return 0; }
  uint32_t positionMillis() { return 0; }
  uint32_t lengthMillis() { return 0; }
  int available() { return 0; }
};
