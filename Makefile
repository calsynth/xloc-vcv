# VCV Rack plugin build for the XLOC2 emulator.
# The firmware submodule must have patches/ applied first: make apply-patches
RACK_DIR ?= ../Rack

FW := firmware/software/src

FLAGS += -Ishim/include -I$(FW) -I$(FW)/src/extern \
         -Ilib/teensy-audio \
         -Ifirmware/software/teensy-variable-playback/src -I.
FLAGS += -DARDUINO_TEENSY41 -D__IMXRT1062__ -DARDUINO=10819 -DTEENSYDUINO=159 \
         -DUSB_MIDI -DXEMU_HOST -DXEMU_HOST_DSP -DDRUMMAP_GRIDS2 \
         -DENABLE_APP_CALIBR8OR -DENABLE_APP_SCENES -DENABLE_APP_PONG \
         -DOC_VERSION_EXTRA="\"_VCV\""

# Firmware needs GNU extensions and C++17; Rack's compile.mk sets -std=c++11,
# ours must come later. (No -fpermissive: Apple clang doesn't know it, and the
# host-portability patches fixed the const-correctness issues properly.)
EXTRA_CXXFLAGS += -std=gnu++17 -Wno-unused-parameter \
                  -Wno-unused-variable -Wno-sign-compare

# macOS: we require 10.15+ (set below, after plugin.mk). Rack's default
# target is 10.9, which forced us to disable libc++ availability checks and
# aligned-allocation guards — with the honest 10.15 floor those hacks are
# gone and machines older than Catalina get a clean load failure from Rack
# instead of undefined behavior at runtime (reported as hangs on old Intel
# Macs). Apple Silicon is unaffected (arm64 implies macOS 11+).

# Plugin sources
SOURCES += src/plugin.cpp src/XLOC2.cpp
SOURCES += emu/xloc_emu.cpp

# Teensy Audio library (real DSP)
TA := lib/teensy-audio
SOURCES += $(TA)/synth_waveform.cpp $(TA)/synth_dc.cpp $(TA)/synth_whitenoise.cpp \
           $(TA)/synth_pinknoise.cpp $(TA)/synth_karplusstrong.cpp \
           $(TA)/filter_variable.cpp $(TA)/filter_ladder.cpp $(TA)/filter_biquad.cpp \
           $(TA)/mixer.cpp $(TA)/effect_freeverb.cpp $(TA)/effect_wavefolder.cpp \
           $(TA)/effect_delay.cpp $(TA)/analyze_peak.cpp $(TA)/analyze_rms.cpp \
           $(TA)/analyze_notefreq.cpp $(TA)/record_queue.cpp $(TA)/play_queue.cpp \
           $(TA)/data_waveforms.c $(TA)/data_bandlimit_step.c
SOURCES += shim/src/shim_arduino.cpp shim/src/shim_eeprom.cpp \
           shim/src/shim_fs.cpp shim/src/shim_globals.cpp \
           shim/src/shim_audiostream.cpp shim/src/shim_audio_io.cpp \
           shim/src/emu_OC_ADC.cpp shim/src/emu_OC_digital_inputs.cpp \
           shim/src/emu_OC_FreqMeasure.cpp shim/src/emu_SH1106.cpp

# Firmware sources (hardware-only TUs excluded; see shim/src replacements)
FW_EXCLUDE := $(FW)/OC_ADC.cpp $(FW)/OC_digital_inputs.cpp
SOURCES += $(filter-out $(FW_EXCLUDE),$(wildcard $(FW)/*.cpp))
SOURCES += $(wildcard $(FW)/src/Audio/*.cpp)
SOURCES += $(FW)/src/drivers/display.cpp $(FW)/src/drivers/weegfx.cpp
SOURCES += $(wildcard $(FW)/src/extern/*.cpp)
SOURCES += $(FW)/src/util/util_misc.cpp $(FW)/src/util/util_settings.cpp

DISTRIBUTABLES += res

include $(RACK_DIR)/plugin.mk

# macOS floor: 10.15 (Catalina). Appended after plugin.mk so it overrides
# Rack's -mmacosx-version-min=10.9 (the last version-min flag wins), for
# both compile and link.
ifdef ARCH_MAC
FLAGS += -mmacosx-version-min=10.15
CXXFLAGS += -mmacosx-version-min=10.15
LDFLAGS += -mmacosx-version-min=10.15
endif

.PHONY: apply-patches
apply-patches:
	cd firmware && git apply --3way ../patches/*.patch || true
