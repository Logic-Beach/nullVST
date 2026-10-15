#pragma once

// eSpeak NG, GPL-3.0. Jonathan Duddington and the eSpeak NG authors.
// https://github.com/espeak-ng/espeak-ng

// Renders on the message thread. dataPath is the folder that contains espeak-ng-data.
int renderEspeakSpeech (float* dst, int maxLen, double sampleRate,
                        const char* text, int rate, int pitch, int range, int gap, int variant,
                        int* cut, const char* dataPath);
