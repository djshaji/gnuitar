GNUitar’s first LV2 release includes 18 effects in 39 plugin variants,
with mono, stereo, and mono-to-stereo layouts where supported.
The Linux x86-64 bundle was built on Ubuntu 24.04.

## Install

Download the `gnuitar-lv2-linux-x86_64-<commit>.tar.gz` release asset.
From the directory containing that archive, extract it to your user
LV2 directory.

```sh
mkdir -p "$HOME/.lv2"
tar -xzf gnuitar-lv2-linux-x86_64-*.tar.gz -C "$HOME/.lv2"
```

Restart your LV2 host or rescan its plugins. The archive contains one
`gnuitar.lv2` bundle, including the binary, metadata, and GPL license.

[Plugin controls and build instructions](https://github.com/djshaji/gnuitar/tree/master/lv2)
