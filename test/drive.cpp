// Interactive/scripted gesture driver for the headless emulator.
// Reads simple commands from stdin, one per line:
//   run <ms>            advance virtual time
//   turn <L|R> <n>      turn encoder n detents (negative = ccw)
//   click <L|R>         short encoder press (~80ms)
//   long <L|R>          long encoder press (~1600ms)
//   btn <A|B|X|Y|Z> <ms> press button for ms
//   hold <A|B|X|Y|Z>    press and keep held
//   rel <A|B|X|Y|Z>     release held button
//   cv <ch> <volts>     set CV input
//   dump                print framebuffer as ascii
//   quit
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <string>

#include "../emu/xloc_emu.h"

#include <AudioStream.h>

#include <cmath>

// Audio test-tone state: while enabled, run_ms() feeds a sine into the
// emulated codec input and gathers stats from the codec output.
static bool g_tone_on = false;
static double g_tone_freq = 220.0, g_tone_phase = 0.0, g_frame_accum = 0.0;
static double g_out_sumsq = 0.0;
static float g_out_peak = 0.f;
static long g_out_frames = 0;

static void audio_service(double ms) {
  g_frame_accum += ms * 44.1;
  int frames = (int)g_frame_accum;
  if (frames <= 0) return;
  g_frame_accum -= frames;
  if (frames > 4096) frames = 4096;
  static float buf[4096 * 2];
  if (g_tone_on) {
    for (int i = 0; i < frames; ++i) {
      float s = 0.5f * (float)sin(g_tone_phase);
      g_tone_phase += 2.0 * M_PI * g_tone_freq / 44100.0;
      buf[i * 2] = buf[i * 2 + 1] = s;
    }
    xemu::audio_in_write(buf, frames);
  }
  int avail = xemu::audio_out_available();
  int take = avail < frames ? avail : frames;
  if (take > 0) {
    xemu::audio_out_read(buf, take);
    for (int i = 0; i < take * 2; ++i) {
      g_out_sumsq += (double)buf[i] * buf[i];
      float a = fabsf(buf[i]);
      if (a > g_out_peak) g_out_peak = a;
    }
    g_out_frames += take;
  }
}

// Optional real-time pacing per virtual ms (XEMU_DRIVE_SLOW_US env var).
// Needed for sanitizer builds: the firmware loop() thread must get real CPU
// time to consume UI events between scripted gestures.
static int g_slow_us = 0;

static void run_ms(int ms) {
  for (int i = 0; i < ms; ++i) {
    xemu::clock().step(1000000ull);
    audio_service(1.0);
    if (g_slow_us) usleep(g_slow_us);
  }
}

static int enc(const std::string &s) { return (s == "L" || s == "l") ? 0 : 1; }

static int btn_pin(const std::string &s) {
  switch (toupper(s[0])) {
    case 'A': return xemu::BTN_A;
    case 'B': return xemu::BTN_B;
    case 'X': return xemu::BTN_X;
    case 'Y': return xemu::BTN_Y;
    case 'Z': return xemu::BTN_Z;
  }
  return -1;
}

static void dump(bool invert = false) {
  uint8_t fb[xemu::kFBSize];
  xemu::get_framebuffer(fb);
  // 2 rows per line using half blocks
  for (int y = 0; y < 64; y += 2) {
    std::string line;
    for (int x = 0; x < 128; ++x) {
      auto px = [&](int yy) {
        int v = (fb[(yy >> 3) * 128 + x] >> (yy & 7)) & 1;
        return invert ? !v : v;
      };
      int a = px(y), b = px(y + 1);
      line += a && b ? "\xE2\x96\x88" : a ? "\xE2\x96\x80" : b ? "\xE2\x96\x84" : " ";
    }
    printf("|%s|\n", line.c_str());
  }
  fflush(stdout);
}

int main(int argc, char **argv) {
  if (const char *s = getenv("XEMU_DRIVE_SLOW_US")) g_slow_us = atoi(s);
  xemu::set_storage_dir(argc > 1 ? argv[1] : "./drive-storage");
  fprintf(stderr, "booting...\n");
  // On a fresh storage dir the firmware first-run flow blocks in
  // Ui::ConfirmReset() waiting for a button. Boot non-blocking, and if setup()
  // hasn't finished after the splash would have elapsed, answer the prompt
  // with a right-encoder press ([OK]) like a user would on real hardware.
  xemu::boot_async();
  // Step virtual time with a little real time per ms: setup() work (EEPROM
  // erase, config save) runs on the firmware thread in real time.
  auto step_wait = [](int ms) {
    for (int i = 0; i < ms && !xemu::booted(); ++i) {
      xemu::clock().step(1000000ull);
      usleep(200);
    }
  };
  step_wait(4000);
  for (int attempt = 0; !xemu::booted() && attempt < 4; ++attempt) {
    fprintf(stderr, "first-run prompt: confirming reset (attempt %d)\n", attempt + 1);
    xemu::press_encoder(1, true);
    for (int i = 0; i < 100; ++i) { xemu::clock().step(1000000ull); usleep(200); }
    xemu::press_encoder(1, false);
    step_wait(8000);
  }
  if (!xemu::booted()) {
    fprintf(stderr, "boot failed; screen:\n");
    dump();
    return 1;
  }
  run_ms(500);
  fprintf(stderr, "ready\n");

  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream ss(line);
    std::string cmd, a1, a2;
    ss >> cmd >> a1 >> a2;
    if (cmd == "run") {
      run_ms(atoi(a1.c_str()));
    } else if (cmd == "turn") {
      xemu::turn_encoder(enc(a1), atoi(a2.c_str()));
      run_ms(50);
    } else if (cmd == "click") {
      xemu::press_encoder(enc(a1), true);
      run_ms(80);
      xemu::press_encoder(enc(a1), false);
      run_ms(50);
    } else if (cmd == "long") {
      xemu::press_encoder(enc(a1), true);
      run_ms(1700);
      xemu::press_encoder(enc(a1), false);
      run_ms(50);
    } else if (cmd == "btn") {
      xemu::set_button(btn_pin(a1), true);
      run_ms(a2.empty() ? 80 : atoi(a2.c_str()));
      xemu::set_button(btn_pin(a1), false);
      run_ms(50);
    } else if (cmd == "hold") {
      xemu::set_button(btn_pin(a1), true);
      run_ms(30);
    } else if (cmd == "rel") {
      xemu::set_button(btn_pin(a1), false);
      run_ms(30);
    } else if (cmd == "cv") {
      xemu::set_cv_volts(atoi(a1.c_str()), atof(a2.c_str()));
      run_ms(20);
    } else if (cmd == "ehold") {
      xemu::press_encoder(enc(a1), true);
      run_ms(30);
    } else if (cmd == "erel") {
      xemu::press_encoder(enc(a1), false);
      run_ms(30);
    } else if (cmd == "tone") {
      g_tone_on = true;
      if (!a1.empty()) g_tone_freq = atof(a1.c_str());
    } else if (cmd == "notone") {
      g_tone_on = false;
    } else if (cmd == "astat") {
      double rms = g_out_frames ? sqrt(g_out_sumsq / (g_out_frames * 2)) : 0.0;
      printf("ASTAT frames=%ld rms=%.5f peak=%.5f ring=%d\n", g_out_frames, rms,
             g_out_peak, xemu::audio_out_available());
      fflush(stdout);
      g_out_sumsq = 0.0; g_out_peak = 0.f; g_out_frames = 0;
    } else if (cmd == "amem") {
      printf("AMEM used=%u max=%u\n", AudioMemoryUsage(), AudioMemoryUsageMax());
      fflush(stdout);
    } else if (cmd == "dump") {
      dump();
    } else if (cmd == "dumpi") {
      dump(true);
    } else if (cmd == "quit") {
      break;
    }
    fprintf(stderr, "ok: %s\n", line.c_str());
  }
  printf("done\n");
  return 0;
}
