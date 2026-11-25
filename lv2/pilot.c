/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <lv2/core/lv2.h>

#include "dsp/amp_dsp.h"
#include "dsp/tremolo_dsp.h"
#include "dsp/vibrato_dsp.h"
#include "dsp/noise_gate_dsp.h"

#define GNUITAR_URI "https://github.com/djshaji/gnuitar#"
#define SINE_TABLE_SIZE 65536
#define TWO_PI 6.28318530717958647692
#define LEGACY_FULL_SCALE 8388608.0f

typedef struct {
    const float *inputs[2];
    float *outputs[2];
    const float *controls[5];
    double sample_rate;
    float phase;
    float *sine_table;
    VibratoDSP *vibrato;
    NoiseGateDSP gate;
    uint32_t channels;
    uint32_t num_controls;
} Plugin;

static LV2_Handle
create_instance(double rate, uint32_t channels, uint32_t controls,
                bool needs_sine, bool needs_vibrato)
{
    Plugin *self;
    uint32_t i;

    if (!isfinite(rate) || rate < 1.0 || rate > UINT32_MAX / 15.0)
        return NULL;

    self = calloc(1, sizeof(*self));
    if (!self)
        return NULL;

    self->sample_rate = rate;
    self->channels = channels;
    self->num_controls = controls;

    if (needs_sine) {
        self->sine_table = malloc((SINE_TABLE_SIZE + 1) * sizeof(*self->sine_table));
        if (!self->sine_table) {
            free(self);
            return NULL;
        }

        for (i = 0; i <= SINE_TABLE_SIZE; ++i)
            self->sine_table[i] = (float) sin(TWO_PI * i / SINE_TABLE_SIZE);
    }

    if (needs_vibrato) {
        void *memory;

        /* The existing Hilbert biquads may contain 16-byte SSE vectors. */
        if (posix_memalign(&memory, 16, sizeof(*self->vibrato))) {
            free(self->sine_table);
            free(self);
            return NULL;
        }
        self->vibrato = memory;
    }

    return self;
}

static void
connect_port(LV2_Handle instance, uint32_t port, void *data)
{
    Plugin *self = instance;
    uint32_t channels = self->channels;

    if (port < channels)
        self->inputs[port] = data;
    else if (port < 2 * channels)
        self->outputs[port - channels] = data;
    else if (port < 2 * channels + self->num_controls)
        self->controls[port - 2 * channels] = data;
}

static void
activate(LV2_Handle instance)
{
    Plugin *self = instance;
    self->phase = 0.0f;
    self->gate = (NoiseGateDSP) {0};
    if (self->vibrato) {
        memset(self->vibrato, 0, sizeof(*self->vibrato));
        hilbert_init(&self->vibrato->hilbert);
    }
}

static float
control_value(const float *port, float fallback, float minimum, float maximum)
{
    float value = port ? *port : fallback;
    if (!isfinite(value))
        return fallback;
    return fminf(maximum, fmaxf(minimum, value));
}

static void
read_frame(const Plugin *self, uint32_t frame, float input[2])
{
    /* Stage both inputs before writing, including when the host swaps buffers. */
    input[0] = self->inputs[0][frame];
    input[1] = self->channels == 2 ? self->inputs[1][frame] : 0.0f;
}

static void
write_gain(const Plugin *self, uint32_t frame, const float input[2], double gain)
{
    uint32_t channel;

    for (channel = 0; channel < self->channels; ++channel)
        self->outputs[channel][frame] = (float) (input[channel] * gain);
}

static void
run_amp(LV2_Handle instance, uint32_t frames)
{
    const Plugin *self = instance;
    float db = control_value(self->controls[0], 0.0f, -30.0f, 30.0f);
    float gain = gnuitar_amp_gain(db);
    uint32_t frame;

    for (frame = 0; frame < frames; ++frame) {
        float input[2];
        read_frame(self, frame, input);
        write_gain(self, frame, input, gain);
    }
}

static void
run_tremolo(LV2_Handle instance, uint32_t frames)
{
    Plugin *self = instance;
    float period_ms = control_value(self->controls[0], 200.0f, 20.0f, 2000.0f);
    float depth = control_value(self->controls[1], 25.0f, 0.0f, 100.0f);
    double period = gnuitar_tremolo_period(period_ms, self->sample_rate);
    uint32_t frame;

    for (frame = 0; frame < frames; ++frame) {
        float input[2];
        double gain = gnuitar_tremolo_gain(&self->phase, period, depth,
                                          self->sine_table, SINE_TABLE_SIZE);
        read_frame(self, frame, input);
        write_gain(self, frame, input, gain);
    }
}

static void
run_vibrato(LV2_Handle instance, uint32_t frames)
{
    Plugin *self = instance;
    float period_ms = control_value(self->controls[0], 200.0f, 50.0f, 3000.0f);
    float depth = control_value(self->controls[1], 10.0f, 0.0f, 50.0f);
    float base = control_value(self->controls[2], 0.0f, -50.0f, 50.0f);
    double sweep_step = 1000.0 / period_ms / self->sample_rate;
    uint32_t frame, channel;

    for (frame = 0; frame < frames; ++frame) {
        float input[2];
        float sine = gnuitar_vibrato_sine(self->sine_table, SINE_TABLE_SIZE,
                                          self->vibrato->shift_phase);
        float cosine = gnuitar_vibrato_cosine(self->sine_table, SINE_TABLE_SIZE,
                                              self->vibrato->shift_phase);
        read_frame(self, frame, input);

        for (channel = 0; channel < self->channels; ++channel) {
            float sample = gnuitar_vibrato_sample(self->vibrato,
                                                  input[channel] * LEGACY_FULL_SCALE,
                                                  channel, sine, cosine, base);
            self->outputs[channel][frame] = sample / LEGACY_FULL_SCALE;
        }
        gnuitar_vibrato_advance(self->vibrato, self->sine_table, SINE_TABLE_SIZE,
                               sweep_step, self->sample_rate, depth, base);
    }
}

static uint32_t
milliseconds_to_frames(float milliseconds, double rate)
{
    return (uint32_t) floor(milliseconds * rate / 1000.0 + 0.5);
}

static void
run_noise_gate(LV2_Handle instance, uint32_t frames)
{
    Plugin *self = instance;
    /* Each point on the GNUitar scale represents 10 legacy 16-bit counts. */
    const float threshold_scale = 10.0f / 32768.0f;
    float threshold = control_value(self->controls[0], 50.0f, 0.0f, 100.0f);
    float hysteresis = control_value(self->controls[1], 41.0f, 0.0f, 100.0f);
    uint32_t attack = milliseconds_to_frames(
        control_value(self->controls[2], 0.0f, 0.0f, 4000.0f), self->sample_rate);
    uint32_t hold = milliseconds_to_frames(
        control_value(self->controls[3], 2.0f, 0.0f, 200.0f), self->sample_rate);
    uint32_t release = milliseconds_to_frames(
        control_value(self->controls[4], 500.0f, 0.0f, 15000.0f), self->sample_rate);
    double attack_step = attack ? 1.0 / attack : 1.0;
    double release_step = release ? 1.0 / release : 1.0;
    uint32_t frame;

    hysteresis = fminf(hysteresis, threshold) * threshold_scale;
    threshold *= threshold_scale;
    for (frame = 0; frame < frames; ++frame) {
        float input[2];
        float peak;
        double gain;

        read_frame(self, frame, input);
        peak = fabsf(input[0]);
        if (self->channels == 2)
            peak = fmaxf(peak, fabsf(input[1]));
        gain = gnuitar_gate_step(&self->gate, peak, threshold, hysteresis, hold,
                                  attack_step, release_step);
        write_gain(self, frame, input, gain);
    }
}

static Plugin *
create_for_descriptor(const LV2_Descriptor *descriptor, double rate, uint32_t channels)
{
    if (descriptor->run == run_tremolo)
        return create_instance(rate, channels, 2, true, false);
    if (descriptor->run == run_vibrato)
        return create_instance(rate, channels, 3, true, true);
    if (descriptor->run == run_noise_gate)
        return create_instance(rate, channels, 5, false, false);
    return create_instance(rate, channels, 1, false, false);
}

static void
cleanup(LV2_Handle instance)
{
    Plugin *self = instance;
    free(self->vibrato);
    free(self->sine_table);
    free(self);
}

static LV2_Handle
instantiate_mono(const LV2_Descriptor *descriptor, double rate, const char *bundle_path,
                 const LV2_Feature *const *features)
{
    (void) bundle_path;
    (void) features;
    return create_for_descriptor(descriptor, rate, 1);
}

static LV2_Handle
instantiate_stereo(const LV2_Descriptor *descriptor, double rate, const char *bundle_path,
                   const LV2_Feature *const *features)
{
    (void) bundle_path;
    (void) features;
    return create_for_descriptor(descriptor, rate, 2);
}

static const LV2_Descriptor descriptors[] = {
    {GNUITAR_URI "digital-amp-mono", instantiate_mono, connect_port, activate, run_amp, NULL, cleanup, NULL},
    {GNUITAR_URI "digital-amp-stereo", instantiate_stereo, connect_port, activate, run_amp, NULL, cleanup, NULL},
    {GNUITAR_URI "tremolo-mono", instantiate_mono, connect_port, activate, run_tremolo, NULL, cleanup, NULL},
    {GNUITAR_URI "tremolo-stereo", instantiate_stereo, connect_port, activate, run_tremolo, NULL, cleanup, NULL},
    {GNUITAR_URI "tremolo-bar-mono", instantiate_mono, connect_port, activate, run_vibrato, NULL, cleanup, NULL},
    {GNUITAR_URI "tremolo-bar-stereo", instantiate_stereo, connect_port, activate, run_vibrato, NULL, cleanup, NULL},
    {GNUITAR_URI "noise-gate-mono", instantiate_mono, connect_port, activate, run_noise_gate, NULL, cleanup, NULL},
    {GNUITAR_URI "noise-gate-stereo", instantiate_stereo, connect_port, activate, run_noise_gate, NULL, cleanup, NULL},
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor *
lv2_descriptor(uint32_t index)
{
    return index < sizeof(descriptors) / sizeof(descriptors[0]) ? &descriptors[index] : NULL;
}
