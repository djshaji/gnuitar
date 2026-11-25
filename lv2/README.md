# GNUitar LV2 pilot

This Linux bundle provides GNUitar Digital Amp and Tremolo, each in mono and
stereo. Host-generated controls expose gain in dB, and Tremolo period in
milliseconds and depth in percent. Controls are applied at host block
boundaries. The standalone app and LV2 share the original gain and modulation
calculations. Both effects multiply samples by gain, so the legacy fixed-scale
conversion cancels out for normalized LV2 audio.

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

The Amp stages channel inputs before writing outputs, and Tremolo advances the
oscillator once per frame. Each Tremolo instance owns its phase and sine
table. It resets phase on activation. The plugin library has no GTK,
sound-device, or Lilv dependency.
