# Calsynth XLOC — VCV Rack plugin

Runs the **actual Phazerville Suite firmware** (Teensy 4.1 / CalSynth XLOC2
target, v2.0.1) as a VCV Rack 2 module. The firmware is vendored unmodified as
a git submodule; a small host-portability patch set (`patches/`) plus a shim
layer (`shim/`, `emu/`) let it compile and run on desktop.

The module identifies as an XLOC2 the same way the hardware does — a 0.30 V ID
voltage on pin A17 — so you get the CalSynthXL panel mapping: 8 CV ins, 8 CV
outs (±10 V), 4 trigger ins, the SH1106 OLED, two encoders and the A/X/B/Y/Z
buttons. The core ISR runs at the hardware's 16.666 kHz on a virtual clock
driven by Rack's audio thread, so CV timing matches real hardware.

**Audio applets are present but silent** — the Teensy Audio graph compiles
against stubs and no DSP scheduler runs (same as phase 1/2 of the desktop
emulator). CV-land is fully functional.

## Using the module

- **Drag** an encoder to turn it, **click** for a short press, **right-click**
  for a long press, **shift+drag** for push+turn.
- Buttons A/X/Z/Y/B are momentary; hold with the mouse for long presses.
- On the very first boot the firmware asks "Reset application settings?" on
  the OLED — click the right encoder (EDIT) to confirm, like on hardware.
- Settings, calibration and presets persist in `<Rack user dir>/Calsynth-XLOC2/`
  (emulated EEPROM, LittleFS and SD card as plain files).
- Only one XLOC2 instance can run per Rack process (the firmware is a global
  singleton, exactly like the hardware). A second instance stays inert.

## Building

```sh
git clone --recurse-submodules <this repo>
cd xloc-vcv
make apply-patches           # applies patches/ to the firmware submodule
RACK_DIR=/path/to/Rack-SDK make -j$(nproc) dist
```

CI (`.github/workflows/build.yml`) builds Linux x64, macOS arm64/x64 and
Windows x64 packages from the official Rack SDK on every push.

### Headless test harness

```sh
mkdir build && cd build && cmake .. && make -j$(nproc)
./headless_test --factory storage   # first boot, auto-confirms the reset dialog
./headless_test storage             # boots, dumps OLED framebuffers as .pbm
```

## Layout

- `firmware/` — djphazer/O_C-Phazerville @ v2.0.1 (submodule, GPL-3.0)
- `patches/` — host-portability patch (portable fallbacks for ARM asm,
  clang const-correctness, one null-deref guard)
- `shim/include/` — Arduino/Teensy API shims (Arduino.h, Audio.h, SD.h, …)
- `shim/src/` — replaced hardware TUs (ADC scan, SH1106, digital inputs) and
  host implementations (EEPROM file, virtual pins, fake IMXRT registers —
  the DAC8568 is emulated by decoding writes to the fake LPSPI4_TDR register)
- `emu/` — virtual clock/ISR scheduler and panel state shared by frontends
- `src/` — the VCV Rack module
- `test/` — headless boot/CV harness

## License

GPL-3.0-or-later (the firmware is GPL-3.0; everything here follows).
