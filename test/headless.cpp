// Headless proof harness: boot the firmware as an XLOC2, run virtual time,
// dump the OLED framebuffer as PBM/PNG, and check CV in -> out.
#include <cstdio>
#include <cstring>
#include <string>

#include "../emu/xloc_emu.h"

static void dump_framebuffer(const char *path) {
  uint8_t fb[xemu::kFBSize];
  xemu::get_framebuffer(fb);
  // SH1106 page layout: page p, column x, bit b => pixel (x, p*8+b)
  FILE *f = fopen(path, "w");
  fprintf(f, "P1\n%d %d\n", xemu::kFBWidth, xemu::kFBHeight);
  for (int y = 0; y < xemu::kFBHeight; ++y) {
    for (int x = 0; x < xemu::kFBWidth; ++x) {
      int page = y / 8, bit = y & 7;
      int on = (fb[page * 128 + x] >> bit) & 1;
      fputc(on ? '1' : '0', f);
      fputc(x == 127 ? '\n' : ' ', f);
    }
  }
  fclose(f);
  printf("wrote %s\n", path);
}

static void run_ms(int ms) {
  // Step virtual time in 1 ms slices (fires core ISR @16.67k, UI @1k).
  for (int i = 0; i < ms; ++i) xemu::clock().step(1000000ull);
}

#include <atomic>
#include <thread>

int main(int argc, char **argv) {
  std::string storage = "./xloc-storage";
  bool factory = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--factory") factory = true;
    else storage = argv[i];
  }
  xemu::set_storage_dir(storage);

  // First-ever boot shows a "Reset application settings?" confirm dialog.
  // In factory mode, click the right encoder repeatedly until boot finishes;
  // the resulting storage dir is the reusable factory image.
  std::atomic<bool> done{false};
  std::thread presser;
  if (factory) {
    presser = std::thread([&] {
      while (!done.load()) {
        // Note: boot() advances virtual time ~20x faster than real time, so
        // these real-time sleeps become ~100 ms virtual press / ~1 s gap.
        xemu::press_encoder(1, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        xemu::press_encoder(1, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    });
  }

  printf("== booting ==\n");
  xemu::boot();
  done = true;
  if (presser.joinable()) presser.join();
  printf("== booted ==\n");

  run_ms(100);
  dump_framebuffer("boot_screen.pbm");

  // Give the splash time to pass, then capture the main menu.
  run_ms(3000);
  dump_framebuffer("main_screen.pbm");

  // CV loopback sanity: put voltages on inputs, read raw ADC-visible values.
  for (int ch = 0; ch < 8; ++ch) xemu::set_cv_volts(ch, (float)ch - 3.f);
  run_ms(50);

  printf("CV outputs:");
  for (int ch = 0; ch < 8; ++ch) printf(" %+.3f", xemu::get_cv_out_volts(ch));
  printf("\n");

  // Poke some buttons/encoders to prove the UI reacts.
  xemu::turn_encoder(1, 4);
  run_ms(200);
  dump_framebuffer("after_encoder.pbm");

  printf("ok\n");
  return 0;
}
