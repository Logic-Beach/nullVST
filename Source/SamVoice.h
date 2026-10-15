#pragma once

/* Software Automatic Mouth, 1982, Don't Ask Software (later SoftVoice).
   Sebastian Macke's C port: https://github.com/s-macke/SAM
   speed, pitch, mouth, and throat are that program's own controls.
   pitch is its pitch byte, not a MIDI note. A line that does not fit
   the phoneme tables or the sample buffer is refused. */

int renderSamSpeech (float* dst, int maxLen, double sampleRate,
                     const char* text, int speed, int pitch, int mouth, int throat,
                     int* cut);
