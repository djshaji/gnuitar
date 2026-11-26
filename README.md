# GNUitar

GNUitar is a real-time guitar effects processor. Use the original GTK
standalone application to process a live input, or load the same
collection of effects as LV2 plugins in a Linux audio host such as
Ardour or Carla.

## Install the LV2 bundle

Download the Linux x86-64 tarball from
[GitHub Releases](https://github.com/djshaji/gnuitar/releases) or download
the `gnuitar-lv2-linux-x86_64` artifact from a successful
[LV2 validation run](https://github.com/djshaji/gnuitar/actions/workflows/lv2.yml).
If you use a workflow artifact, unzip it first. The CI build targets
Ubuntu 24.04.

From the directory containing the tarball, extract it into your user
LV2 directory.

```sh
mkdir -p "$HOME/.lv2"
tar -xzf gnuitar-lv2-linux-x86_64-*.tar.gz -C "$HOME/.lv2"
```

Restart your host or rescan its plugins, then search for **GNUitar**.
Choose the channel layout that matches your track or routing.

## LV2 effects

The bundle contains **18 effects in 39 plugin variants**.

| Effects | Channel layouts |
| --- | --- |
| Digital Amp, Tremolo, Tremolo Bar, Noise Gate, Sustain, Distort, Overdrive, Eq Bank, Reverb, Autowah, Pitch Shift, Tube Amplifier, Tuner | Mono and stereo |
| Delay, Echo, Chorus / Flanger, Phaser | Mono, stereo, and mono-to-stereo |
| Rotary Speaker | Mono-to-stereo |

See the [LV2 guide](lv2/README.md) for routing, control behavior, and
release packaging. The [Tube Amplifier controls](lv2/README.md#tube-amplifier-controls)
section includes an Ardour check for both cabinets.

## Build the LV2 bundle

Install a C compiler, CMake 3.19 or later, and the LV2 development
headers. On Debian or Ubuntu, install `build-essential`, `cmake`,
and `lv2-dev`. Install `lilv-utils` to enable the host discovery test.

Run these commands from the repository root.

```sh
cmake -S lv2 -B build-lv2 -DCMAKE_BUILD_TYPE=Release \
  -DGNUITAR_LV2_INSTALL_DIR="$HOME/.lv2"
cmake --build build-lv2
ctest --test-dir build-lv2 --output-on-failure
cmake --install build-lv2
```

The bundle is also available at `build-lv2/bundles/gnuitar.lv2`. The
LV2 build uses the host for audio I/O and has no GTK dependency.

## Standalone application

The standalone GNUitar application has a GTK interface and opens the
audio device directly. Its source is in [src/](src/). The
[CI workflow](.github/workflows/lv2.yml) includes a Linux GTK2/OSS
build command. The [original README](README), [INSTALL](INSTALL),
and [docs/](docs/) cover the historical application and platform
builds, including older Windows toolchains.

## License and credits

GNUitar is free software under the GNU General Public License. See
[COPYING](COPYING) for the license and [AUTHORS](AUTHORS) for credits.
