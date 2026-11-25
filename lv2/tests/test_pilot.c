/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <dlfcn.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lv2/core/lv2.h>

#include "biquad.h"

#define URI_PREFIX "https://github.com/djshaji/gnuitar#"
#define FRAMES 4001
#define TWO_PI 6.28318530717958647692

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

typedef struct {
    const LV2_Descriptor *descriptor;
    LV2_Handle handle;
    uint32_t channels;
} Plugin;

static Plugin
open_plugin(const LV2_Descriptor *descriptor, uint32_t channels, double rate,
            const char *bundle_path)
{
    const LV2_Feature *const features[] = {NULL};
    Plugin plugin = {descriptor, NULL, channels};

    plugin.handle = descriptor->instantiate(descriptor, rate, bundle_path, features);
    CHECK(plugin.handle);
    return plugin;
}

static void
connect_audio(Plugin plugin, float *left_in, float *right_in,
              float *left_out, float *right_out)
{
    plugin.descriptor->connect_port(plugin.handle, 0, left_in);
    plugin.descriptor->connect_port(plugin.handle, plugin.channels, left_out);
    if (plugin.channels == 2) {
        plugin.descriptor->connect_port(plugin.handle, 1, right_in);
        plugin.descriptor->connect_port(plugin.handle, 3, right_out);
    }
}

static void
connect_control(Plugin plugin, uint32_t index, float *value)
{
    plugin.descriptor->connect_port(plugin.handle, 2 * plugin.channels + index, value);
}

static void
check_audio(float actual, float expected)
{
    if (!isfinite(actual) || fabsf(actual - expected) > 1.0e-6f + fabsf(expected) * 1.0e-5f) {
        fprintf(stderr, "Sample mismatch: %.9g vs %.9g\n", actual, expected);
        exit(EXIT_FAILURE);
    }
}

static void
test_amp(Plugin plugin)
{
    float left[] = {0.0f, -0.375f, 0.125f, 1.25f};
    float right[] = {0.9f, 0.25f, -0.75f, -1.25f};
    float output_left[4], output_right[4];
    const float settings[] = {0.0f, -6.0f, 6.0f, -100.0f, 100.0f, NAN};
    const float gains[] = {1.0f, 0.501187234f, 1.995262315f, 0.031622777f, 31.62277660f, 1.0f};
    float db;
    unsigned int setting, frame;

    connect_audio(plugin, left, right, output_left, output_right);
    connect_control(plugin, 0, &db);
    plugin.descriptor->activate(plugin.handle);

    db = 0.0f;
    output_left[0] = 42.0f;
    plugin.descriptor->run(plugin.handle, 0);
    CHECK(output_left[0] == 42.0f);

    for (setting = 0; setting < sizeof(settings) / sizeof(settings[0]); ++setting) {
        db = settings[setting];
        plugin.descriptor->run(plugin.handle, 4);
        for (frame = 0; frame < 4; ++frame) {
            check_audio(output_left[frame], left[frame] * gains[setting]);
            if (plugin.channels == 2)
                check_audio(output_right[frame], right[frame] * gains[setting]);
        }
    }

    db = -6.0f;
    if (plugin.channels == 2) {
        float expected_left = left[1] * gains[1];
        float expected_right = right[1] * gains[1];
        connect_audio(plugin, left + 1, right + 1, right + 1, left + 1);
        plugin.descriptor->run(plugin.handle, 1);
        check_audio(right[1], expected_left);
        check_audio(left[1], expected_right);
    } else {
        float expected = left[1] * gains[1];
        connect_audio(plugin, left + 1, NULL, left + 1, NULL);
        plugin.descriptor->run(plugin.handle, 1);
        check_audio(left[1], expected);
    }
}

static void
check_legacy_tremolo(const float *left, const float *right, const float *output_left,
                     const float *output_right, uint32_t channels, float period_ms,
                     float depth, double sample_rate)
{
    double period = period_ms / 1000.0 * sample_rate;
    float phase = 0.0f;
    unsigned int frame;

    for (frame = 0; frame < FRAMES; ++frame) {
        unsigned int index;
        float sine;
        double volume;

        if (phase >= period)
            phase = 0.0f;
        index = (unsigned int) ((float) (phase / period) * 65536.0f);
        sine = (float) sin(TWO_PI * index / 65536.0);
        volume = 1.0 - depth / 100.0 * (1.0 + sine) / 2.0;
        check_audio(output_left[frame], (float) (ldexpf(left[frame], 23) * volume / 8388608.0));
        if (channels == 2)
            check_audio(output_right[frame], (float) (ldexpf(right[frame], 23) * volume / 8388608.0));
        ++phase;
    }
}

static void
test_tremolo(const LV2_Descriptor *descriptor, uint32_t channels, const char *bundle_path)
{
    const double rates[] = {44100.0, 48000.0};
    const uint32_t blocks[] = {1, 0, 2, 31, 33, 257, 1024, 53};
    Plugin plugins[2];
    float left[FRAMES], right[FRAMES];
    float whole_left[2][FRAMES], whole_right[2][FRAMES];
    float split_left[2][FRAMES], split_right[2][FRAMES];
    float period_ms = 20.7f, depth = 93.0f;
    uint32_t frame, rate, offset, block;

    for (frame = 0; frame < FRAMES; ++frame) {
        left[frame] = (float) ((int) (frame % 13) - 6) / 12.0f;
        right[frame] = (float) ((int) (frame % 11) - 5) / 7.0f;
    }

    for (rate = 0; rate < 2; ++rate) {
        plugins[rate] = open_plugin(descriptor, channels, rates[rate], bundle_path);
        connect_audio(plugins[rate], left, right, whole_left[rate], whole_right[rate]);
        connect_control(plugins[rate], 0, &period_ms);
        connect_control(plugins[rate], 1, &depth);
        descriptor->activate(plugins[rate].handle);
        descriptor->run(plugins[rate].handle, FRAMES);
        check_legacy_tremolo(left, right, whole_left[rate], whole_right[rate],
                             channels, period_ms, depth, rates[rate]);
        descriptor->activate(plugins[rate].handle);
    }

    /* Interleave calls at different rates to catch shared oscillator state. */
    offset = 0;
    block = 0;
    while (offset < FRAMES) {
        uint32_t frames = blocks[block++ % (sizeof(blocks) / sizeof(blocks[0]))];
        if (frames > FRAMES - offset)
            frames = FRAMES - offset;
        for (rate = 0; rate < 2; ++rate) {
            connect_audio(plugins[rate], left + offset, right + offset,
                          split_left[rate] + offset, split_right[rate] + offset);
            descriptor->run(plugins[rate].handle, frames);
        }
        offset += frames;
    }

    for (rate = 0; rate < 2; ++rate) {
        CHECK(memcmp(whole_left[rate], split_left[rate], sizeof(left)) == 0);
        if (channels == 2)
            CHECK(memcmp(whole_right[rate], split_right[rate], sizeof(right)) == 0);

        /* Update both controls between blocks while buffers are in-place. */
        connect_audio(plugins[rate], left, right, left, right);
        depth = 0.0f;
        period_ms = INFINITY;
        descriptor->run(plugins[rate].handle, 1);
        CHECK(left[0] == -0.5f && right[0] == -5.0f / 7.0f);
        descriptor->cleanup(plugins[rate].handle);
    }
}

static void
test_noise_gate(Plugin plugin, double rate)
{
    const uint32_t attack = (uint32_t) (rate * 3.0 / 1000.0 + 0.5);
    const uint32_t hold = (uint32_t) (rate * 2.0 / 1000.0 + 0.5);
    const uint32_t release = (uint32_t) (rate * 4.0 / 1000.0 + 0.5);
    const uint32_t quiet_start = attack + 10;
    const uint32_t release_start = quiet_start + hold;
    const uint32_t restart = release_start + release + 10;
    const uint32_t total = restart + 10;
    float left[FRAMES], right[FRAMES], out_l[FRAMES], out_r[FRAMES];
    float settings[] = {50.0f, 41.0f, 3.0f, 2.0f, 4.0f};
    uint32_t frame, block, offset, control;

    CHECK(total <= FRAMES);
    for (frame = 0; frame < total; ++frame) {
        float key = frame < attack || frame >= restart ? 0.5f
                  : frame < quiet_start ? 0.014f : 0.005f;
        left[frame] = plugin.channels == 2 ? 0.005f : key;
        right[frame] = -key;
    }

    for (control = 0; control < sizeof(settings) / sizeof(settings[0]); ++control)
        connect_control(plugin, control, &settings[control]);

    plugin.descriptor->activate(plugin.handle);
    offset = 0;
    for (block = 0; offset < total; ++block) {
        const uint32_t sizes[] = {1, 31, 0, 64, 3, 257};
        uint32_t frames = sizes[block % (sizeof(sizes) / sizeof(sizes[0]))];
        if (frames > total - offset)
            frames = total - offset;
        connect_audio(plugin, left + offset, right + offset, out_l + offset, out_r + offset);
        plugin.descriptor->run(plugin.handle, frames);
        offset += frames;
    }

    for (frame = 0; frame < total; ++frame) {
        double gain = 1.0;
        if (frame < attack)
            gain = (frame + 1.0) / attack;
        else if (frame >= restart)
            gain = (frame - restart + 1.0) / attack;
        else if (frame >= release_start)
            gain = fmax(0.0, 1.0 - (frame - release_start + 1.0) / release);
        check_audio(out_l[frame], (float) (left[frame] * gain));
        if (plugin.channels == 2)
            check_audio(out_r[frame], (float) (right[frame] * gain));
    }

    /* A crossed stereo buffer still uses the original right channel to open. */
    plugin.descriptor->activate(plugin.handle);
    settings[2] = 0.0f;
    settings[3] = 0.0f;
    settings[4] = 0.0f;
    if (plugin.channels == 2) {
        float saved_left = left[0], saved_right = right[0];
        connect_audio(plugin, left, right, right, left);
        plugin.descriptor->run(plugin.handle, 1);
        CHECK(right[0] == saved_left && left[0] == saved_right);
    } else {
        float saved_left = left[0];
        connect_audio(plugin, left, NULL, left, NULL);
        plugin.descriptor->run(plugin.handle, 1);
        CHECK(left[0] == saved_left);
    }
    connect_audio(plugin, left + quiet_start, right + quiet_start, out_l, out_r);
    plugin.descriptor->run(plugin.handle, 1);
    CHECK(out_l[0] == 0.0f);
}

static float
legacy_sine(float phase)
{
    unsigned int index = (unsigned int) (phase * 65536.0f);
    return (float) sin(TWO_PI * index / 65536.0);
}

static void
test_vibrato(Plugin plugin, double rate, float base)
{
    float left[FRAMES], right[FRAMES], out_l[FRAMES], out_r[FRAMES];
    float period = 50.0f, depth = 47.0f;
    float sweep_phase = 0.0f, shift_phase = 0.0f;
    Hilbert_t history = {0};
    uint32_t frame, channel, offset, frames;

    hilbert_init(&history);
    for (frame = 0; frame < FRAMES; ++frame) {
        left[frame] = (float) sin(0.09 * frame) * 0.3f;
        right[frame] = (float) cos(0.14 * frame) * 0.6f;
    }
    connect_control(plugin, 0, &period);
    connect_control(plugin, 1, &depth);
    connect_control(plugin, 2, &base);
    plugin.descriptor->activate(plugin.handle);
    for (offset = 0; offset < FRAMES; offset += frames) {
        frames = offset % 257 + 1;
        if (frames > FRAMES - offset)
            frames = FRAMES - offset;
        connect_audio(plugin, left + offset, right + offset, out_l + offset, out_r + offset);
        plugin.descriptor->run(plugin.handle, frames);
    }

    for (frame = 0; frame < FRAMES; ++frame) {
        float sine = legacy_sine(shift_phase);
        float cosine = legacy_sine(shift_phase >= 0.75f ? shift_phase - 0.75f : shift_phase + 0.25f);
        for (channel = 0; channel < plugin.channels; ++channel) {
            float input = channel ? right[frame] : left[frame];
            float x0, x1, expected;
            hilbert_transform(input * 8388608.0f, &x0, &x1, &history, channel);
            expected = base > 0.0f ? cosine * x0 + sine * x1 : cosine * x0 - sine * x1;
            check_audio(channel ? out_r[frame] : out_l[frame], expected / 8388608.0f);
        }

        sweep_phase += 1000.0 / period / rate;
        if (sweep_phase >= 1.0f)
            sweep_phase -= 1.0f;
        shift_phase += (fabs(base) + (depth + legacy_sine(sweep_phase) * depth) / 2) / rate;
        if (shift_phase >= 1.0f)
            shift_phase -= 1.0f;
    }

    plugin.descriptor->activate(plugin.handle);
    connect_audio(plugin, left, right,
                 plugin.channels == 2 ? right : left, left);
    plugin.descriptor->run(plugin.handle, 0);
    plugin.descriptor->run(plugin.handle, FRAMES);
    CHECK(memcmp(plugin.channels == 2 ? right : left, out_l, sizeof(left)) == 0);
    if (plugin.channels == 2)
        CHECK(memcmp(left, out_r, sizeof(right)) == 0);
}

static void
test_vibrato_multiple_wraps(const LV2_Descriptor *descriptor, uint32_t channels,
                            const char *bundle_path)
{
    Plugin plugin = open_plugin(descriptor, channels, 1.0, bundle_path);
    float input[32] = {0.5f}, out_l[32], out_r[32];
    float period = 51.0f, depth = 50.0f, base = 50.0f;
    uint32_t frame;

    connect_audio(plugin, input, input, out_l, out_r);
    connect_control(plugin, 0, &period);
    connect_control(plugin, 1, &depth);
    connect_control(plugin, 2, &base);
    descriptor->activate(plugin.handle);
    descriptor->run(plugin.handle, 32);
    for (frame = 0; frame < 32; ++frame) {
        CHECK(isfinite(out_l[frame]));
        if (channels == 2)
            CHECK(isfinite(out_r[frame]));
    }
    descriptor->cleanup(plugin.handle);
}

int
main(int argc, char **argv)
{
    const char *uris[] = {
        URI_PREFIX "digital-amp-mono", URI_PREFIX "digital-amp-stereo",
        URI_PREFIX "tremolo-mono", URI_PREFIX "tremolo-stereo",
        URI_PREFIX "tremolo-bar-mono", URI_PREFIX "tremolo-bar-stereo",
        URI_PREFIX "noise-gate-mono", URI_PREFIX "noise-gate-stereo",
    };
    const LV2_Descriptor *(*get_descriptor)(uint32_t);
    void *library;
    char *bundle_path;
    const char *filename;
    size_t path_length;
    uint32_t i;

    CHECK(argc == 2);
    library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        fprintf(stderr, "%s\n", dlerror());
        return EXIT_FAILURE;
    }

    get_descriptor = (const LV2_Descriptor *(*)(uint32_t)) dlsym(library, "lv2_descriptor");
    CHECK(get_descriptor);
    filename = strrchr(argv[1], '/');
    CHECK(filename);
    path_length = (size_t) (filename - argv[1]) + 1;
    bundle_path = calloc(path_length + 1, 1);
    CHECK(bundle_path);
    memcpy(bundle_path, argv[1], path_length);

    for (i = 0; i < sizeof(uris) / sizeof(uris[0]); ++i) {
        const LV2_Descriptor *descriptor = get_descriptor(i);
        uint32_t channels = i % 2 + 1;
        CHECK(descriptor && strcmp(descriptor->URI, uris[i]) == 0);
        if (i < 2) {
            Plugin plugin = open_plugin(descriptor, channels, 48000.0, bundle_path);
            test_amp(plugin);
            descriptor->cleanup(plugin.handle);
        } else if (i < 4) {
            test_tremolo(descriptor, channels, bundle_path);
        } else {
            const double rates[] = {44100.0, 48000.0, 96000.0};
            uint32_t r;
            for (r = 0; r < sizeof(rates) / sizeof(rates[0]); ++r) {
                Plugin plugin = open_plugin(descriptor, channels, rates[r], bundle_path);
                if (i < 6)
                    test_vibrato(plugin, rates[r], r == 0 ? 43.0f : -41.0f);
                else
                    test_noise_gate(plugin, rates[r]);
                descriptor->cleanup(plugin.handle);
            }
            if (i < 6)
                test_vibrato_multiple_wraps(descriptor, channels, bundle_path);
        }
    }

    CHECK(!get_descriptor(i));
    free(bundle_path);
    CHECK(dlclose(library) == 0);
    puts("Verified gain, legacy modulation parity, linked gate timing, aliasing, and variable blocks.");
    return EXIT_SUCCESS;
}
