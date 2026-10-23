# NullVST

Inlcudes a source wave (and voices!), a second oscillator, and a math function.  
Crush, filters, delay, tape, reverb, and a compressor/limiter effects chain which can be rearranged to taste

## How it works

The wave is sine, saw up, saw down, square, triangle, or noise. Width changes the pulse. There is also SAM and eSpeak, which are the two talking voices.

The sub is the other oscillator. It has its own shape, tune, fine tune, width, envelope, and a little LP filter. The mode is what the two do to each other: add, subtract, multiply, divide, AM, FM, min, max, fold, xor, diff. Order picks math function application order. Mix is how hard the other one gets applied. The level faders are how loud each oscillator is inside the math.

Tape Speed affects entire output as if recorded to tape with a PWM knob for slowing the motors. Slow it down and the pitch drops and the sound gets rougher, the way a deck does. Fix keeps the speech pitch and only changes the time. Tempo on, Speed is a beat division from a 32nd note out to 8 bars, and a speech line fills that division and starts again on the beat. Trig makes that division start on the key instead of the host clock.

Drift is either warble or repeat. Warble sways everything going through the tape, sub and echoes included. Amount is how far. Repeat walks the source through a run of pitches and leaves the sub where it is. Steps is how many pitches, seed is which pattern. The same key and seed repeat. Slide glides the source when you change notes. The sub ignores that.

Crush is bits, hold, and glitch. Bits is how many volume steps, 16 is clean and 2 is a few fat levels. Hold freezes each step. Glitch punches holes and sticks the sample. Wet is how much of that replaces the clean note.

Two filters run in a row at the filter's place in the chain. Lowpass, highpass, bandpass, remove, formant, resonator. Each one has an envelope. Delay has feedback and echo, and it can lock to the tempo. Trig on the delay clears the repeats when you play. Reverb is size, damp, and mix. The compressor is threshold, ratio, and makeup. Limit is the ceiling after the output level. Dist bends the note into that ceiling instead of only flattening the peaks.

Four LFOs stay running. Shape, rate, depth, and where it goes. Depth at zero leaves the knob alone. Trig restarts that LFO on a new note. The control it is moving wears the LFO's colour.

Mono keeps the newest key. Poly lets notes overlap, up to eight. A chord gets pulled down a bit so it stays near the level of one note. The limiter still catches a stacked peak.

## Talking

Type a line in the say box and hit return, or hit lock. Digits get read as numbers. If the line will not fit, it gets refused and the last good line stays. A held note follows the line. The robot mouth moves with either speech voice.

SAM is Software Automatic Mouth, the 1982 speech program. The key is the note. Mouth, throat, and speed still shape the voice. The pitch knob is locked out, note controls this one.

eSpeak is the other voice. It has its own rate, pitch, range, gap, and voice. eSpeak NG 1.52.0 is in the plugin and is GPL-3.0. The source for a release is that version's tag.

## Installation

Grab the zip for your machine from the releases page. -->

- macOS 11 or newer. One plugin, Apple Silicon and Intel.
- Windows 10 or newer, 64-bit.
- Linux, x86_64.

Unzip and put `NullAtomo.vst3` in the host's VST3 folder.

- macOS: `~/Library/Audio/Plug-Ins/VST3/`
- Reason on macOS also scans `~/Library/Audio/Plug-Ins/VST/`
- Windows: `C:\Program Files\Common Files\VST3\`
- Linux: `~/.vst3/`

Rescan plugins in the host DAW  

The macOS build is not notarized. If the Mac blocks it after you download it, unzip it and run:

```
xattr -dr com.apple.quarantine "/path/to/NullAtomo.vst3"
```



## Credits

SAM in here is Sebastian Macke's C port of Software Automatic Mouth: [https://github.com/s-macke/SAM](https://github.com/s-macke/SAM)

eSpeak NG is by Jonathan Duddington and the eSpeak NG authors, GPL-3.0: [https://github.com/espeak-ng/espeak-ng](https://github.com/espeak-ng/espeak-ng)

The panel type is Chicago 12 by Damien Guard, CC BY-SA 3.0.