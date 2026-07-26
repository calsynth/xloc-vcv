// Teensy Audio Library for the XLOC2 emulator — REAL DSP (phase 3).
//
// The classes the Phazerville audio applets use come straight from the
// vendored PJRC Audio library (lib/teensy-audio), compiled against the host
// AudioStream engine in shim/include/AudioStream.h. The I2S2 codec endpoints
// are host bridges into the emulator's audio rings (xemu_audio_io.h).
#pragma once

#include <Arduino.h>
#include <AudioStream.h>
#include <arm_math.h>

// Real DSP classes (vendored Teensy Audio library)
#include <synth_waveform.h>       // AudioSynthWaveform, AudioSynthWaveformModulated
#include <synth_dc.h>             // AudioSynthWaveformDc
#include <synth_whitenoise.h>     // AudioSynthNoiseWhite
#include <synth_pinknoise.h>      // AudioSynthNoisePink
#include <synth_karplusstrong.h>  // AudioSynthKarplusStrong
#include <filter_variable.h>      // AudioFilterStateVariable
#include <filter_ladder.h>        // AudioFilterLadder
#include <filter_biquad.h>        // AudioFilterBiquad
#include <mixer.h>                // AudioMixer4, AudioAmplifier
#include <analyze_peak.h>         // AudioAnalyzePeak
#include <analyze_rms.h>          // AudioAnalyzeRMS
#include <analyze_notefreq.h>     // AudioAnalyzeNoteFrequency
#include <effect_freeverb.h>      // AudioEffectFreeverb(+Stereo)
#include <effect_wavefolder.h>    // AudioEffectWaveFolder
#include <effect_delay.h>         // AudioEffectDelay
#include <record_queue.h>         // AudioRecordQueue
#include <play_queue.h>           // AudioPlayQueue

// Host codec bridge + inert USB endpoints
#include <xemu_audio_io.h>
