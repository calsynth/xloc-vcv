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
#include <iostream>
#include <sstream>
#include <string>

#include "../emu/xloc_emu.h"

static void run_ms(int ms) {
  for (int i = 0; i < ms; ++i) xemu::clock().step(1000000ull);
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

static void dump() {
  uint8_t fb[xemu::kFBSize];
  xemu::get_framebuffer(fb);
  // 2 rows per line using half blocks
  for (int y = 0; y < 64; y += 2) {
    std::string line;
    for (int x = 0; x < 128; ++x) {
      auto px = [&](int yy) {
        return (fb[(yy >> 3) * 128 + x] >> (yy & 7)) & 1;
      };
      int a = px(y), b = px(y + 1);
      line += a && b ? "\xE2\x96\x88" : a ? "\xE2\x96\x80" : b ? "\xE2\x96\x84" : " ";
    }
    printf("|%s|\n", line.c_str());
  }
  fflush(stdout);
}

int main(int argc, char **argv) {
  xemu::set_storage_dir(argc > 1 ? argv[1] : "./drive-storage");
  fprintf(stderr, "booting...\n");
  xemu::boot();
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
    } else if (cmd == "dump") {
      dump();
    } else if (cmd == "quit") {
      break;
    }
    fprintf(stderr, "ok: %s\n", line.c_str());
  }
  printf("done\n");
  return 0;
}
