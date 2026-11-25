# GNUitar LV2 plugins

The Linux bundle provides Digital Amp, Tremolo, Tremolo Bar, Noise Gate,
Sustain, Distort, Overdrive, Eq Bank, Delay, Echo, Reverb, Chorus / Flanger, Phaser, and Rotary Speaker.
All effects except Rotary Speaker have mono and stereo variants. Delay, Echo,
Chorus / Flanger, Phaser, and Rotary Speaker have mono-to-stereo variants. Controls take effect at host block boundaries.

- Digital Amp and Tremolo use the same calculations as the standalone app
  directly on normalized LV2 audio.
- Tremolo Bar uses GNUitar's Hilbert frequency shifter. Its period is in
  milliseconds, and its depth and base shift are in Hz. Both channels use
  the same oscillator phases and have separate filter histories.
- Noise Gate uses a linked peak detector in stereo. Opening and closing
  thresholds use the legacy 0–100 scale. Each percentage point is 10
  counts of a signed 16-bit signal, so the default opening threshold of 50%
  corresponds to about −36.3 dBFS. The closing threshold is limited to the
  opening threshold. Attack, hold and release are in milliseconds, with
  linear gain ramps and a gate that starts closed.
- Sustain retains GNUitar's sustain, noise, and gate scales. Each channel
  has its own envelope, which persists across blocks. Silence stays at
  zero even when the envelope has decayed or the detector is frozen.
- Distort pairs the legacy RC high-pass and LC output filter with the
  original drive, saturation, and level calculations. It supports sample
  rates of 8 kHz and up. On low-rate hosts, its low-pass setting is limited
  to one-sixth of the sample rate to keep the LC integrator stable.
- Overdrive keeps the original four-times oversampling, diode solver, and
  output filters. Controls expose drive, output level, and the legacy
  treble scale. The solver and filters keep separate histories per channel.
  Silent input continues to advance those histories so tails can decay
  across block boundaries.
- Eq Bank has 14 fixed bands from 40 Hz to 16 kHz and a separate
  output gain. All gains range from −9.9 to +9.9 dB. Filters use twice
  the host sample rate and maintain separate histories for each channel.
  At low sample rates, bands whose upper edge reaches the oversampled
  Nyquist frequency are skipped.
- Delay supports up to 10 repeats spaced up to 10 seconds apart. Its
  mono-to-stereo version puts the dry input on the left and alternates
  repeats between right and left.
- Echo uses four possible prime-ratio feedback voices. Its stereo version
  keeps channels independent, and its mono-to-stereo version sends dry
  audio to both outputs and distributes the feedback voices between them.
  Delay and Echo allocate their full history when the host creates an
  instance. Parameter changes during playback reuse the existing history.
  Delay and Echo support host rates up to 384 kHz.
- Reverb uses GNUitar's original allpass and shared comb topology. Its
  taps scale from the 44.1 kHz reference to the host rate. The LV2 feedback
  history is bounded to eight times nominal full scale at extreme settings.
- Chorus / Flanger keeps a continuous modulation phase and uses an
  interpolated delay. In the mono-to-stereo version, the outputs use
  different modulation phases and delay offsets.
- Phaser uses the original 24-stage allpass sweep for mono and stereo.
  The mono-to-stereo variant uses Hilbert sidebands and exposes sweep period
  and wet mix. Both algorithms update on a continuous eight-frame schedule.
- Rotary Speaker uses the original Hilbert split and filtered crossfeed.
  Its period follows the standalone control, with two rotations per
  indicated period.

## Build and test

Install a C compiler, CMake 3.19 or later, and the LV2 development headers.
Install Lilv's `lv2ls` and `lv2info` to enable the discovery test. On Debian,
the relevant packages are `build-essential`, `cmake`, `lv2-dev`, and
`lilv-utils`.

From the repository root:

```sh
cmake -S lv2 -B build-lv2 -DCMAKE_BUILD_TYPE=Release
cmake --build build-lv2
ctest --test-dir build-lv2 --output-on-failure
LV2_PATH="$PWD/build-lv2/bundles" lv2ls
```

The build produces `build-lv2/bundles/gnuitar.lv2`. To install for the current
user:

```sh
cmake -S lv2 -B build-lv2 -DGNUITAR_LV2_INSTALL_DIR="$HOME/.lv2"
cmake --install build-lv2
```

The plugins support in-place processing and reset their DSP state on
activation. The library has no GTK, sound-device, or Lilv dependency.
