/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <dlfcn.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lv2/core/lv2.h>

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

int
main(int argc, char **argv)
{
    const char *uris[] = {
        URI_PREFIX "digital-amp-mono", URI_PREFIX "digital-amp-stereo",
        URI_PREFIX "tremolo-mono", URI_PREFIX "tremolo-stereo",
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
        } else {
            test_tremolo(descriptor, channels, bundle_path);
        }
    }

    CHECK(!get_descriptor(i));
    free(bundle_path);
    CHECK(dlclose(library) == 0);
    puts("Verified gain, legacy Tremolo parity, channel routing, aliasing, and block independence.");
    return EXIT_SUCCESS;
}
