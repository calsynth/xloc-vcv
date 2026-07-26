# Teensy Audio Library (subset)

Selected sources from https://github.com/PaulStoffregen/Audio
(commit 3039be2773e86daf1f381a1e8bdc1e6a55ed11f1), lightly patched for the
XLOC2 emulator host build:

- `defined(__ARM_ARCH_7EM__)` guards extended with `|| defined(XEMU_HOST_DSP)`
  so the real (intrinsics-based) DSP paths compile on desktop hosts.
- `utility/dspinst.h` routes to portable C implementations of the ARM DSP
  intrinsics (shim/include/xemu_dspinst.h) on non-ARM builds.

Original copyright Paul Stoffregen / PJRC and contributors; see the license
text in each file header (MIT-style with attribution requirement).
