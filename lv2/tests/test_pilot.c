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

static void
test_sustain(Plugin plugin)
{
    float left[FRAMES] = {0}, right[FRAMES] = {0};
    float out_l[FRAMES], out_r[FRAMES];
    float settings[] = {100.0f, 15.625f, 100.0f};
    uint32_t frame, i;

    for (i = 0; i < 3; ++i)
        connect_control(plugin, i, &settings[i]);
    plugin.descriptor->activate(plugin.handle);
    connect_audio(plugin, left, right, out_l, out_r);
    plugin.descriptor->run(plugin.handle, 0);
    plugin.descriptor->run(plugin.handle, FRAMES);

    for (frame = 0; frame < FRAMES; ++frame) {
        CHECK(out_l[frame] == 0.0f);
        if (plugin.channels == 2)
            CHECK(out_r[frame] == 0.0f);
    }

    left[0] = 0.0005f;
    right[0] = 0.2f;
    plugin.descriptor->run(plugin.handle, 1);
    /* Reference the original gain and gate equations with the legacy internal units. */
    for (i = 0; i < plugin.channels; ++i) {
        float sample = i == 0 ? left[0] : right[0];
        double envelope = sample * 8388608.0 * (40.0 / 256.0);
        double gain = 2.56 * (30000.0 / envelope) - 1.56;
        check_audio(i == 0 ? out_l[0] : out_r[0],
                    sample * gain * fmin(1.0, envelope / 25600.0));
    }

    /* Run a long tail through multiple reconnects with the gate disabled. */
    settings[2] = 0.0f;
    left[0] = 0.0f;
    right[0] = 0.0f;
    for (i = 0; i < 3; ++i) {
        plugin.descriptor->run(plugin.handle, FRAMES);
        for (frame = 0; frame < FRAMES; ++frame) {
            CHECK(out_l[frame] == 0.0f);
            if (plugin.channels == 2)
                CHECK(out_r[frame] == 0.0f);
        }
    }

    for (frame = 0; frame < FRAMES; ++frame) {
        left[frame] = (float) sin(frame * 0.07) * 0.03f;
        right[frame] = (float) cos(frame * 0.03) * 0.2f;
    }
    plugin.descriptor->activate(plugin.handle);
    plugin.descriptor->run(plugin.handle, FRAMES);
    plugin.descriptor->activate(plugin.handle);
    for (frame = 0; frame < FRAMES; ++frame) {
        connect_audio(plugin, left + frame, right + frame,
                      (plugin.channels == 2 ? right : left) + frame, left + frame);
        plugin.descriptor->run(plugin.handle, 1);
    }
    CHECK(memcmp(plugin.channels == 2 ? right : left, out_l, sizeof(left)) == 0);
    if (plugin.channels == 2)
        CHECK(memcmp(left, out_r, sizeof(right)) == 0);

    plugin.descriptor->activate(plugin.handle);
    settings[1] = 0.0f;
    left[0] = 0.01f;
    right[0] = -0.03f;
    connect_audio(plugin, left, right, out_l, out_r);
    plugin.descriptor->run(plugin.handle, 1);
    CHECK(out_l[0] == left[0]);
    if (plugin.channels == 2)
        CHECK(out_r[0] == right[0]);
}

static void
legacy_distort(float samples[FRAMES], double rate, int drive, int sat, int level,
               double lowpass)
{
    const double rc_cap = 1.0 / (TWO_PI * 350.0 * 1000.0);
    const double capacitance = 1.0 / (4.0 * pow(TWO_PI * lowpass / 2.0, 2.0) * 0.05);
    double current = 0.0, velocity = 0.0, previous = 0.0;
    int last_distorted = 0;
    uint32_t stage, frame;

    for (frame = 0; frame < FRAMES; ++frame)
        samples[frame] *= 8388608.0f;

    for (stage = 0; stage < 10; ++stage) {
        double prev_in = 0.0, i = 0.0;
        for (frame = 0; frame < FRAMES; ++frame) {
            double sample = samples[frame];
            i += (sample - prev_in - i / (rate * rc_cap)) * 0.001;
            prev_in = sample;
            samples[frame] = (float) (i * 1000.0 * 1.5);
        }
    }

    for (frame = 0; frame < FRAMES; ++frame) {
        double sample = (double) samples[frame] * drive / 16.0;
        double delta = (double) sat * 256.0;
        sample = fmax(last_distorted - delta, fmin(last_distorted + delta, sample));
        last_distorted = (int) sample;
        samples[frame] = (float) (sample * level / 256.0);
    }

    for (frame = 0; frame < FRAMES; ++frame) {
        double sample = samples[frame];
        velocity += (sample - previous - current / (rate * capacitance) - 300.0 * velocity)
                  / (0.05 * rate);
        current += velocity;
        previous = sample;
        samples[frame] = (int) (current * 500.0) / 8388608.0f;
    }
}

static void
test_distort(Plugin plugin, double rate)
{
    float left[FRAMES], right[FRAMES], out_l[FRAMES], out_r[FRAMES];
    float ref_l[FRAMES], ref_r[FRAMES];
    float settings[] = {55.5f, 100.0f / 3.0f, 7.8125f, 1800.0f};
    uint32_t frame, control, offset = 0;

    for (frame = 0; frame < FRAMES; ++frame) {
        left[frame] = (float) sin(frame * 0.11) * 0.8f;
        right[frame] = (float) cos(frame * 0.31) * 0.5f;
    }

    for (control = 0; control < 4; ++control)
        connect_control(plugin, control, &settings[control]);

    plugin.descriptor->activate(plugin.handle);
    connect_audio(plugin, left, right, out_l, out_r);
    plugin.descriptor->run(plugin.handle, 0);
    plugin.descriptor->run(plugin.handle, FRAMES);

    memcpy(ref_l, left, sizeof(left));
    legacy_distort(ref_l, rate, 555, 10000, 20, 1800.0);
    if (plugin.channels == 2) {
        memcpy(ref_r, right, sizeof(right));
        legacy_distort(ref_r, rate, 555, 10000, 20, 1800.0);
    }
    for (frame = 0; frame < FRAMES; ++frame) {
        check_audio(out_l[frame], ref_l[frame]);
        if (plugin.channels == 2)
            check_audio(out_r[frame], ref_r[frame]);
    }

    plugin.descriptor->activate(plugin.handle);
    while (offset < FRAMES) {
        uint32_t frames = offset % 263 + 1;
        if (frames > FRAMES - offset)
            frames = FRAMES - offset;
        connect_audio(plugin, left + offset, right + offset,
                      (plugin.channels == 2 ? right : left) + offset, left + offset);
        plugin.descriptor->run(plugin.handle, frames);
        offset += frames;
    }
    CHECK(memcmp(plugin.channels == 2 ? right : left, out_l, sizeof(left)) == 0);
    if (plugin.channels == 2)
        CHECK(memcmp(left, out_r, sizeof(right)) == 0);
}

static void
test_distort_low_rate(const LV2_Descriptor *descriptor, uint32_t channels,
                      const char *bundle_path)
{
    Plugin plugin = open_plugin(descriptor, channels, 8000.0, bundle_path);
    float input[FRAMES] = {0.9f}, out_l[FRAMES], out_r[FRAMES];
    float lowpass = 3000.0f;
    uint32_t frame;

    connect_audio(plugin, input, input, out_l, out_r);
    connect_control(plugin, 3, &lowpass);
    descriptor->activate(plugin.handle);
    descriptor->run(plugin.handle, FRAMES);
    for (frame = 0; frame < FRAMES; ++frame) {
        CHECK(isfinite(out_l[frame]) && fabsf(out_l[frame]) < 10.0f);
        if (channels == 2)
            CHECK(isfinite(out_r[frame]) && fabsf(out_r[frame]) < 10.0f);
    }
    descriptor->cleanup(plugin.handle);
}

static void
test_overdrive(Plugin plugin)
{
    enum { count = 128 };
    float left[count] = {0}, right[count] = {0};
    float out_l[count], out_r[count], silence_l[count], silence_r[count];
    float settings[] = {75.0f, 68.0f, -2.0f};
    float one_in = 0.5f, one_out = 42.0f, one_right = 0.0f;
    const uint32_t sizes[] = {1, 2, 31, 32, 33, 29};
    uint32_t i, offset = 0;
    bool heard_transient = false;

    for (i = 0; i < 3; ++i)
        connect_control(plugin, i, &settings[i]);

    plugin.descriptor->activate(plugin.handle);
    connect_audio(plugin, &one_in, &one_right, &one_out, &one_right);
    plugin.descriptor->run(plugin.handle, 0);
    CHECK(one_out == 42.0f);
    plugin.descriptor->run(plugin.handle, 1);
    CHECK(isfinite(one_out));

    plugin.descriptor->activate(plugin.handle);
    connect_audio(plugin, left, right, silence_l, silence_r);
    plugin.descriptor->run(plugin.handle, count);

    /* Sample 4 is outside all legacy silence probes in either layout. */
    left[4] = 0.9f;
    plugin.descriptor->activate(plugin.handle);
    connect_audio(plugin, left, right, out_l, out_r);
    plugin.descriptor->run(plugin.handle, count);

    for (i = 0; i < count; ++i) {
        CHECK(isfinite(out_l[i]));
        if (fabsf(out_l[i] - silence_l[i]) > 0.001f)
            heard_transient = true;
        if (plugin.channels == 2)
            CHECK(out_r[i] == silence_r[i]);
    }
    CHECK(heard_transient);

    plugin.descriptor->activate(plugin.handle);
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        connect_audio(plugin, left + offset, right + offset,
                      (plugin.channels == 2 ? right : left) + offset, left + offset);
        plugin.descriptor->run(plugin.handle, sizes[i]);
        offset += sizes[i];
    }
    CHECK(offset == count);
    CHECK(memcmp(plugin.channels == 2 ? right : left, out_l, sizeof(left)) == 0);
    if (plugin.channels == 2)
        CHECK(memcmp(left, out_r, sizeof(right)) == 0);
}

enum { eq_bands = 14 };
static const unsigned int eq_centers[eq_bands] = {40, 100, 200, 320, 640, 1000, 1600,
    2200, 3000, 4000, 6000, 8000, 12000, 16000};
static const unsigned int eq_widths[eq_bands] = {30, 120, 160, 320, 640, 800, 1200,
    1200, 1400, 1800, 2600, 4000, 6000, 8000};

static float
reference_eq_frame(Biquad_t filters[eq_bands], float interpolate[8],
                   float decimate[8], float input, float gain, int channel)
{
    float first, second;
    unsigned int band;

    fir_interpolate_2x(interpolate, input * 8388608.0f, &first, &second);
    for (band = 0; band < eq_bands; ++band) {
        first = do_biquad(first, &filters[band], channel);
        second = do_biquad(second, &filters[band], channel);
    }
    return fir_decimate_2x(decimate, first, second) * gain / 8388608.0f;
}

static void
test_eqbank(Plugin plugin, double rate)
{
    float input[FRAMES], silent[FRAMES] = {0}, output[FRAMES], right[FRAMES];
    float gains[eq_bands + 1] = {0};
    Biquad_t filters[eq_bands] = {0};
    float interpolate[2][8] = {{0}}, decimate[2][8] = {{0}};
    uint32_t frame, offset = 0, band;
    float gain;

    gains[12] = -7.0f;
    gains[13] = 9.0f;
    gains[eq_bands] = 3.0f;
    gain = (float) pow(10.0, gains[eq_bands] / 20.0);
    for (band = 0; band < eq_bands; ++band)
        set_peq_biquad(2.0 * rate, eq_centers[band], eq_widths[band], gains[band],
                       &filters[band]);
    for (band = 0; band <= eq_bands; ++band)
        connect_control(plugin, band, &gains[band]);
    for (frame = 0; frame < FRAMES; ++frame)
        input[frame] = (float) (0.15 * sin(TWO_PI * frame * 12000 / rate)
                              + 0.2 * cos(TWO_PI * frame * 16000 / rate));

    plugin.descriptor->activate(plugin.handle);
    connect_audio(plugin, input, silent, output, right);
    plugin.descriptor->run(plugin.handle, 0);
    plugin.descriptor->run(plugin.handle, FRAMES);
    for (frame = 0; frame < FRAMES; ++frame) {
        float expected = reference_eq_frame(filters, interpolate[0], decimate[0], input[frame], gain, 0);
        check_audio(output[frame], expected);
        if (plugin.channels == 2)
            check_audio(right[frame], reference_eq_frame(filters, interpolate[1], decimate[1],
                                                        0.0f, gain, 1));
    }

    plugin.descriptor->activate(plugin.handle);
    while (offset < FRAMES) {
        uint32_t frames = offset % 183 + 1;
        if (frames > FRAMES - offset)
            frames = FRAMES - offset;
        connect_audio(plugin, input + offset, silent + offset,
                      (plugin.channels == 2 ? silent : input) + offset, input + offset);
        plugin.descriptor->run(plugin.handle, frames);
        offset += frames;
    }
    CHECK(memcmp(plugin.channels == 2 ? silent : input, output, sizeof(output)) == 0);
    if (plugin.channels == 2)
        CHECK(memcmp(input, right, sizeof(right)) == 0);
}

static void
test_eqbank_live_control(Plugin plugin, double rate)
{
    enum { count = 128 };
    Biquad_t filters[eq_bands] = {0};
    float input[count], output[count], other[count];
    float interpolate[8] = {0}, decimate[8] = {0};
    float gains[eq_bands + 1] = {0};
    unsigned int band, i;

    for (band = 0; band < eq_bands; ++band)
        set_peq_biquad(2.0 * rate, eq_centers[band], eq_widths[band], 0.0, &filters[band]);
    for (i = 0; i < count; ++i)
        input[i] = (float) sin(TWO_PI * i * 16000.0 / rate);
    for (band = 0; band <= eq_bands; ++band)
        connect_control(plugin, band, &gains[band]);
    plugin.descriptor->activate(plugin.handle);

    for (i = 0; i < count; ++i) {
        float expected;

        if (i == 19) {
            gains[13] = 9.0f;
            set_peq_biquad(2.0 * rate, 16000, 8000, gains[13], &filters[13]);
        }
        connect_audio(plugin, input + i, input + i, output + i, other + i);
        plugin.descriptor->run(plugin.handle, 1);
        expected = reference_eq_frame(filters, interpolate, decimate, input[i], 1.0f, 0);
        check_audio(output[i], expected);
        if (plugin.channels == 2)
            check_audio(other[i], expected);
    }
}

static void
test_eqbank_low_rate(const LV2_Descriptor *descriptor, uint32_t channels,
                     const char *bundle_path)
{
    Plugin plugin = open_plugin(descriptor, channels, 16000.0, bundle_path);
    float input[FRAMES] = {0.5f}, output[FRAMES], right[FRAMES];
    float flat[FRAMES], flat_right[FRAMES];
    float high_band = 0.0f;
    uint32_t i;

    connect_control(plugin, 13, &high_band);
    descriptor->activate(plugin.handle);
    connect_audio(plugin, input, input, flat, flat_right);
    descriptor->run(plugin.handle, FRAMES);

    high_band = 9.9f;
    descriptor->activate(plugin.handle);
    connect_audio(plugin, input, input, output, right);
    descriptor->run(plugin.handle, FRAMES);
    for (i = 0; i < FRAMES; ++i) {
        check_audio(output[i], flat[i]);
        if (channels == 2)
            check_audio(right[i], flat_right[i]);
    }
    descriptor->cleanup(plugin.handle);
}

static void
test_delay_echo(const LV2_Descriptor *descriptor, const char *path, double rate,
                uint32_t inputs, uint32_t outputs, bool echo)
{
    enum { count = 1024 };
    const LV2_Feature *const features[] = {NULL};
    LV2_Handle handle = descriptor->instantiate(descriptor, rate, path, features);
    bool spread = inputs != outputs;
    uint32_t control = inputs + outputs;
    float settings[] = {1.0f, 50.0f, echo ? 2.0f : 3.0f};
    float left[count] = {0.5f}, right[count] = {0};
    float out_l[count], out_r[count];
    uint32_t first, second, offset, block;

    CHECK(handle);
    descriptor->connect_port(handle, control, &settings[0]);
    descriptor->connect_port(handle, control + 1, &settings[1]);
    descriptor->connect_port(handle, control + 2, &settings[2]);
    descriptor->connect_port(handle, 0, left);
    if (inputs == 2)
        descriptor->connect_port(handle, 1, right);
    descriptor->connect_port(handle, inputs, out_l);
    if (outputs == 2)
        descriptor->connect_port(handle, inputs + 1, out_r);

    descriptor->activate(handle);
    out_l[0] = 42.0f;
    descriptor->run(handle, 0);
    CHECK(out_l[0] == 42.0f);
    descriptor->run(handle, count);
    check_audio(out_l[0], 0.5f);
    if (outputs == 2)
        check_audio(out_r[0], echo && spread ? 0.5f : 0.0f);

    first = (uint32_t) (rate / 1000.0);
    if (echo) {
        float second_gain = (float) pow(0.5, 37.0 / 23.0);

        /* Echo reads before writing, so the stored input is one frame older. */
        ++first;
        second = (uint32_t) (rate / 1000.0 * (37.0 / 23.0)) + 1;
        check_audio(out_l[first], 0.25f);
        check_audio(spread ? out_r[second] : out_l[second], 0.5f * second_gain);
        check_audio(out_l[2 * first], 0.125f);
        check_audio(out_l[first + second], 0.5f * second_gain / 10.0f * 0.5f);
    } else if (spread) {
        check_audio(out_r[first], 0.25f);
        check_audio(out_l[2 * first], 0.125f);
        check_audio(out_r[3 * first], 0.0625f);
        CHECK(out_l[first] == 0.0f);
    } else {
        check_audio(out_l[first + 1], 0.25f);
        check_audio(out_l[2 * first + 1], 0.125f);
        check_audio(out_l[3 * first + 1], 0.0625f);
        CHECK(out_l[first] == 0.0f);
    }
    if (inputs == 2) {
        for (offset = 0; offset < count; ++offset)
            CHECK(out_r[offset] == 0.0f);
    }

    descriptor->activate(handle);
    offset = 0;
    block = 0;
    while (offset < count) {
        const uint32_t sizes[] = {0, 1, 37, 2, 63, 128};
        uint32_t frames = sizes[block++ % (sizeof(sizes) / sizeof(sizes[0]))];
        if (frames > count - offset)
            frames = count - offset;
        descriptor->connect_port(handle, 0, left + offset);
        if (inputs == 2)
            descriptor->connect_port(handle, 1, right + offset);
        descriptor->connect_port(handle, inputs, (outputs == 2 ? right : left) + offset);
        if (outputs == 2)
            descriptor->connect_port(handle, inputs + 1, left + offset);
        descriptor->run(handle, frames);
        offset += frames;
    }
    CHECK(memcmp(outputs == 2 ? right : left, out_l, sizeof(out_l)) == 0);
    if (outputs == 2)
        CHECK(memcmp(left, out_r, sizeof(out_r)) == 0);

    /* The longest legal tap fits the allocated ring at this sample rate. */
    settings[0] = echo ? 500.0f : 10000.0f;
    settings[2] = echo ? 4.0f : 10.0f;
    descriptor->connect_port(handle, 0, left);
    if (inputs == 2)
        descriptor->connect_port(handle, 1, right);
    descriptor->connect_port(handle, inputs, out_l);
    if (outputs == 2)
        descriptor->connect_port(handle, inputs + 1, out_r);
    descriptor->run(handle, 1);
    CHECK(isfinite(out_l[0]));
    if (outputs == 2)
        CHECK(isfinite(out_r[0]));
    descriptor->cleanup(handle);
}

static void
test_reverb_chorus(const LV2_Descriptor *descriptor, const char *path, double rate,
                   uint32_t inputs, uint32_t outputs, bool chorus)
{
    enum { count = 4096 };
    const LV2_Feature *const features[] = {NULL};
    LV2_Handle handle = descriptor->instantiate(descriptor, rate, path, features);
    float left[count] = {0.5f}, right[count] = {0};
    float out_l[count], out_r[count];
    float reverb[] = {130.0f, 100.0f, 0.0f};
    float chorus_settings[] = {4.0f, 0.0f, 200.0f, 100.0f, 0.0f, 2.0f};
    bool spread = inputs != outputs;
    uint32_t frame, offset, block, port;

    CHECK(handle);
    for (port = 0; port < (chorus ? 6u : 3u); ++port)
        descriptor->connect_port(handle, inputs + outputs + port,
                                 chorus ? &chorus_settings[port] : &reverb[port]);

    descriptor->connect_port(handle, 0, left);
    if (inputs == 2)
        descriptor->connect_port(handle, 1, right);
    descriptor->connect_port(handle, inputs, out_l);
    if (outputs == 2)
        descriptor->connect_port(handle, inputs + 1, out_r);

    descriptor->activate(handle);
    descriptor->run(handle, count);
    if (chorus) {
        float amplitude = 0.5f / sqrtf(2.0f);
        double delay = rate * 0.002;
        uint32_t tap = (uint32_t) delay + 1;
        float *target = spread ? out_r : out_l;

        check_audio(out_l[0], 0.0f);
        check_audio(out_l[1], amplitude);
        check_audio(target[tap - 1], 0.0f);
        check_audio(target[tap], amplitude * (1.0 - (delay - floor(delay))));
        check_audio(target[tap + 1], amplitude * (delay - floor(delay)));

        /* Exercise modulation and feedback with deterministic input across many calls. */
        chorus_settings[1] = 2.5f;
        chorus_settings[3] = 65.0f;
        chorus_settings[4] = 30.0f;
        for (frame = 0; frame < count / 2; ++frame)
            left[frame] = (float) (0.25 * sin(0.12 * frame));
        descriptor->activate(handle);
        descriptor->run(handle, count);
        if (spread)
            CHECK(memcmp(out_l, out_r, sizeof(out_l)) != 0);
    } else {
        uint32_t first_ap = (uint32_t) (113 * (rate / 44100.0));

        check_audio(out_l[0], 0.5 * 6 * 0.7 * 0.7 * 0.7);
        check_audio(out_l[first_ap - 1], 0.0f);
        check_audio(out_l[first_ap], 0.5 * 6 * 0.7 * 0.7 * (1.0 - 0.7 * 0.7));
    }

    for (frame = 0; frame < count; ++frame) {
        CHECK(isfinite(out_l[frame]));
        if (outputs == 2)
            CHECK(isfinite(out_r[frame]));
        if (inputs == 2)
            CHECK(out_r[frame] == 0.0f);
    }

    descriptor->activate(handle);
    offset = 0;
    block = 0;
    while (offset < count) {
        const uint32_t sizes[] = {0, 1, 31, 2, 263, 127};
        uint32_t frames = sizes[block++ % (sizeof(sizes) / sizeof(sizes[0]))];
        if (frames > count - offset)
            frames = count - offset;
        descriptor->connect_port(handle, 0, left + offset);
        if (inputs == 2)
            descriptor->connect_port(handle, 1, right + offset);
        descriptor->connect_port(handle, inputs, (outputs == 2 ? right : left) + offset);
        if (outputs == 2)
            descriptor->connect_port(handle, inputs + 1, left + offset);
        descriptor->run(handle, frames);
        offset += frames;
    }
    CHECK(memcmp(outputs == 2 ? right : left, out_l, sizeof(out_l)) == 0);
    if (outputs == 2)
        CHECK(memcmp(left, out_r, sizeof(out_r)) == 0);

    /* Check both extremes against the rate-specific allocation. */
    reverb[0] = 3000.0f;
    reverb[2] = 100.0f;
    chorus_settings[0] = 50.0f;
    chorus_settings[1] = 20.0f;
    chorus_settings[5] = 4.0f;
    descriptor->run(handle, 1);
    if (!chorus) {
        reverb[0] = 1.0f;
        for (frame = 0; frame < count; ++frame) {
            left[frame] = 0.5f;
            right[frame] = 0.0f;
        }
        descriptor->activate(handle);
        descriptor->connect_port(handle, 0, left);
        if (inputs == 2)
            descriptor->connect_port(handle, 1, right);
        descriptor->connect_port(handle, inputs, out_l);
        if (outputs == 2)
            descriptor->connect_port(handle, inputs + 1, out_r);
        for (block = 0; block < 32; ++block) {
            descriptor->run(handle, count);
            for (frame = 0; frame < count; ++frame)
                CHECK(isfinite(out_l[frame]) && fabsf(out_l[frame]) < 128.0f);
        }
    }
    descriptor->cleanup(handle);
}

int
main(int argc, char **argv)
{
    const char *uris[] = {
        URI_PREFIX "digital-amp-mono", URI_PREFIX "digital-amp-stereo",
        URI_PREFIX "tremolo-mono", URI_PREFIX "tremolo-stereo",
        URI_PREFIX "tremolo-bar-mono", URI_PREFIX "tremolo-bar-stereo",
        URI_PREFIX "noise-gate-mono", URI_PREFIX "noise-gate-stereo",
        URI_PREFIX "sustain-mono", URI_PREFIX "sustain-stereo",
        URI_PREFIX "distort-mono", URI_PREFIX "distort-stereo",
        URI_PREFIX "overdrive-mono", URI_PREFIX "overdrive-stereo",
        URI_PREFIX "eq-bank-mono", URI_PREFIX "eq-bank-stereo",
        URI_PREFIX "delay-mono", URI_PREFIX "delay-stereo", URI_PREFIX "delay-mono-to-stereo",
        URI_PREFIX "echo-mono", URI_PREFIX "echo-stereo", URI_PREFIX "echo-mono-to-stereo",
        URI_PREFIX "reverb-mono", URI_PREFIX "reverb-stereo",
        URI_PREFIX "chorus-mono", URI_PREFIX "chorus-stereo", URI_PREFIX "chorus-mono-to-stereo",
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
        if (i >= 22) {
            uint32_t ins = i == 23 || i == 25 ? 2 : 1;
            uint32_t outs = i == 26 ? 2 : ins;
            test_reverb_chorus(descriptor, bundle_path, 44100.0, ins, outs, i >= 24);
            test_reverb_chorus(descriptor, bundle_path, 48000.0, ins, outs, i >= 24);
            test_reverb_chorus(descriptor, bundle_path, 96000.0, ins, outs, i >= 24);
            continue;
        }
        if (i >= 16) {
            uint32_t layout = (i - 16) % 3;
            test_delay_echo(descriptor, bundle_path, 44100.0,
                            layout == 1 ? 2 : 1, layout == 0 ? 1 : 2, i >= 19);
            test_delay_echo(descriptor, bundle_path, 96000.0,
                            layout == 1 ? 2 : 1, layout == 0 ? 1 : 2, i >= 19);
            continue;
        }
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
                else if (i < 8)
                    test_noise_gate(plugin, rates[r]);
                else if (i < 10)
                    test_sustain(plugin);
                else if (i < 12)
                    test_distort(plugin, rates[r]);
                else if (i < 14)
                    test_overdrive(plugin);
                else {
                    test_eqbank(plugin, rates[r]);
                    test_eqbank_live_control(plugin, rates[r]);
                }
                descriptor->cleanup(plugin.handle);
            }
            if (i < 6)
                test_vibrato_multiple_wraps(descriptor, channels, bundle_path);
            if (i == 10 || i == 11)
                test_distort_low_rate(descriptor, channels, bundle_path);
            if (i >= 14)
                test_eqbank_low_rate(descriptor, channels, bundle_path);
        }
    }

    CHECK(!get_descriptor(i));
    free(bundle_path);
    CHECK(dlclose(library) == 0);
    puts("Verified legacy DSP parity, sustain silence, linked gate timing, aliasing, and variable blocks.");
    return EXIT_SUCCESS;
}
