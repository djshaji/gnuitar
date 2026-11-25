/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include <lv2/core/lv2.h>

#include "dsp/amp_dsp.h"
#include "dsp/tremolo_dsp.h"

#define GNUITAR_URI "https://github.com/djshaji/gnuitar#"
#define SINE_TABLE_SIZE 65536
#define TWO_PI 6.28318530717958647692

typedef struct {
    const float *inputs[2];
    float *outputs[2];
    const float *controls[2];
    double sample_rate;
    float phase;
    float *sine_table;
    uint32_t channels;
    uint32_t num_controls;
} Pilot;

static LV2_Handle
create_instance(double rate, uint32_t channels, bool tremolo)
{
    Pilot *self;
    uint32_t i;

    if (!isfinite(rate) || rate < 1.0)
        return NULL;

    self = calloc(1, sizeof(*self));
    if (!self)
        return NULL;

    self->sample_rate = rate;
    self->channels = channels;
    self->num_controls = tremolo ? 2 : 1;

    if (tremolo) {
        self->sine_table = malloc((SINE_TABLE_SIZE + 1) * sizeof(*self->sine_table));
        if (!self->sine_table) {
            free(self);
            return NULL;
        }

        for (i = 0; i <= SINE_TABLE_SIZE; ++i)
            self->sine_table[i] = (float) sin(TWO_PI * i / SINE_TABLE_SIZE);
    }

    return self;
}

static void
connect_port(LV2_Handle instance, uint32_t port, void *data)
{
    Pilot *self = instance;
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
    Pilot *self = instance;
    self->phase = 0.0f;
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
apply_gain(const Pilot *self, uint32_t frame, double gain)
{
    float input[2];
    uint32_t channel;

    /* Stage both inputs before writing, including when the host swaps buffers. */
    for (channel = 0; channel < self->channels; ++channel)
        input[channel] = self->inputs[channel][frame];
    for (channel = 0; channel < self->channels; ++channel)
        self->outputs[channel][frame] = (float) (input[channel] * gain);
}

static void
run_amp(LV2_Handle instance, uint32_t frames)
{
    const Pilot *self = instance;
    float db = control_value(self->controls[0], 0.0f, -30.0f, 30.0f);
    float gain = gnuitar_amp_gain(db);
    uint32_t frame;

    for (frame = 0; frame < frames; ++frame)
        apply_gain(self, frame, gain);
}

static void
run_tremolo(LV2_Handle instance, uint32_t frames)
{
    Pilot *self = instance;
    float period_ms = control_value(self->controls[0], 200.0f, 20.0f, 2000.0f);
    float depth = control_value(self->controls[1], 25.0f, 0.0f, 100.0f);
    double period = gnuitar_tremolo_period(period_ms, self->sample_rate);
    uint32_t frame;

    for (frame = 0; frame < frames; ++frame) {
        double gain = gnuitar_tremolo_gain(&self->phase, period, depth,
                                          self->sine_table, SINE_TABLE_SIZE);
        apply_gain(self, frame, gain);
    }
}

static void
cleanup(LV2_Handle instance)
{
    Pilot *self = instance;
    free(self->sine_table);
    free(self);
}

static LV2_Handle
instantiate_mono(const LV2_Descriptor *descriptor, double rate, const char *bundle_path,
                 const LV2_Feature *const *features)
{
    (void) bundle_path;
    (void) features;
    return create_instance(rate, 1, descriptor->run == run_tremolo);
}

static LV2_Handle
instantiate_stereo(const LV2_Descriptor *descriptor, double rate, const char *bundle_path,
                   const LV2_Feature *const *features)
{
    (void) bundle_path;
    (void) features;
    return create_instance(rate, 2, descriptor->run == run_tremolo);
}

static const LV2_Descriptor descriptors[] = {
    {GNUITAR_URI "digital-amp-mono", instantiate_mono, connect_port, activate, run_amp, NULL, cleanup, NULL},
    {GNUITAR_URI "digital-amp-stereo", instantiate_stereo, connect_port, activate, run_amp, NULL, cleanup, NULL},
    {GNUITAR_URI "tremolo-mono", instantiate_mono, connect_port, activate, run_tremolo, NULL, cleanup, NULL},
    {GNUITAR_URI "tremolo-stereo", instantiate_stereo, connect_port, activate, run_tremolo, NULL, cleanup, NULL},
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor *
lv2_descriptor(uint32_t index)
{
    return index < sizeof(descriptors) / sizeof(descriptors[0]) ? &descriptors[index] : NULL;
}
