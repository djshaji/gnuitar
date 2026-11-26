# GNUitar LV2 plugins

The Linux bundle provides Digital Amp, Tremolo, Tremolo Bar, Noise Gate,
Sustain, Distort, Overdrive, Eq Bank, Delay, Echo, Reverb, Chorus / Flanger,
Phaser, Rotary Speaker, Autowah, Pitch Shift, Tube Amplifier, and Tuner. All effects except
Rotary Speaker have mono and stereo variants. Delay, Echo, Chorus / Flanger, Phaser,
and Rotary Speaker have mono-to-stereo variants. Controls are read at host
block boundaries.

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
- Autowah offers Lowpass, Bandpass, Moog Ladder, and TB-303 models.
  Envelope mode listens to both input channels in stereo. Continuous Sweep
  cycles between the frequency settings. Pedal mode uses the 0–1 Pedal
  Position control, which hosts can automate or map to an expression pedal.
  The sweep and filter coefficients update every 64 audio frames.
- Pitch Shift uses a streaming phase vocoder with linked stereo phase,
  −24 to +12 semitones, ±50 cents of fine tuning, and wet mix. It reports
  one analysis window of latency and delays the dry path by the same
  amount. That is 2,048 frames at 44.1 and 48 kHz and 4,096 at 96 kHz.
  At unity pitch, the wet and dry paths are identical. Transposition can
  spread transients around the nominal alignment. The combined coarse
  and fine settings are capped at the legacy range of −24 to +12 semitones.

- Tube Amplifier uses six-times oversampled nonlinear stages, independent
  channel bias histories, and the two embedded cabinet models at four
  convolution lengths. The coefficients are resampled from 44.1 kHz,
  the rate required by the original WAV-to-coefficients script. The source
  WAVs and a separate recording provenance record are absent from the
  repository.
- Tuner passes audio through at zero latency. Its time-domain detector
  analyzes a single note from 27.5 to 400 Hz and updates every 20 ms.
  Stereo analyzes the average of both inputs and passes them through
  independently. Use one input for sources that cancel when summed.
  The output meters report frequency, nearest MIDI note, deviation in
  cents (A4 = 440 Hz), and validity. An invalid reading is 0 Hz, MIDI
  note −1, and 0 cents. The detector is shared with standalone builds
  that use the time-domain backend. Hosts can display the control
  outputs in their generic plugin UI if supported.

## Tube Amplifier controls

The mono and stereo variants use the same controls. The tone controls sit
after the second tube stage, so any later stages can change how their EQ
sounds. Gain drives each stage. Once the amp is saturating, increasing Gain
can change the distortion while the output level moves only slightly.

| Control | Range and default | What it changes |
| --- | --- | --- |
| Stages | 2–4, default 4 | Number of tube stages in the signal path. Changing it can affect both distortion and output level. |
| Gain | 30–50 dB, default 35 dB | Drive into each tube stage. |
| Bias Factor | −25 to +25, default −7 | How much the signal shifts the tube's bias as you play. |
| Asymmetry | −5000 to +5000, default −3500 | Static bias offset that shapes how differently the amp distorts the positive and negative halves of the waveform. |
| Cabinet Model | Marshall G15R (default), Princeton II | Selects the cabinet impulse response. |
| Cabinet Quality | Lowest, Normal (default), High, Extreme | Uses 64, 128, 256, or 512 impulse-response taps at 44.1 kHz. Longer responses use more CPU. |
| Bass | −10 to +10 dB, default +3 dB | Low shelf at 500 Hz. |
| Middle | −20 to 0 dB, default −10 dB | Midrange cut centered at 650 Hz. |
| Treble | −10 to +10 dB, default 0 dB | High shelf at 800 Hz. |

To check the controls in Ardour, loop a dry guitar clip and switch
**Cabinet Model** between **Marshall G15R** and **Princeton II**. Listen
for a change in tone. If both settings sound identical, check that the
plugin is active and that an automation lane in **Play** mode is not
overriding the setting. To hear how **Gain** changes the onset of
distortion, lower the signal level before the plugin and sweep Gain again.

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

## Linux release candidate

On a successful [LV2 validation workflow run](https://github.com/djshaji/gnuitar/actions/workflows/lv2.yml),
download the `gnuitar-lv2-linux-x86_64` artifact and unpack the downloaded
ZIP. Inside it is a `gnuitar-lv2-linux-x86_64-<commit>.tar.gz` archive
built on Ubuntu 24.04. The workflow extracts that archive, verifies all
39 plugin URIs and their port symbols with Lilv, and runs the DSP tests
against the packaged shared library before uploading it.

From the directory containing the tarball, extract the bundle to your
user LV2 directory:

```sh
mkdir -p "$HOME/.lv2"
tar -xzf gnuitar-lv2-linux-x86_64-*.tar.gz -C "$HOME/.lv2"
```

Restart your LV2 host or rescan its plugins after installing the bundle.

## Prepare a GitHub release

Run the [LV2 validation workflow](https://github.com/djshaji/gnuitar/actions/workflows/lv2.yml)
on the default branch and enter a tag such as `lv2-v0.1.0` in
`release_tag`. A successful run creates a GitHub release draft with
the tested Linux archive attached and the selected commit as its
target. The tag is created when the draft is published.

Install the archive attached to the draft for the DAW smoke test.
Check plugin discovery, mono and stereo routing, control automation,
and session save and reopen in Ardour or Carla. Check the Tuner's
readout on a guitar or bass DI and audition both Tube Amplifier
cabinets. Publish the draft from GitHub Releases once these checks pass.
