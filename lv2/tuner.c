/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>

#include <lv2/core/lv2.h>

#include "dsp/tuner_dsp.h"

#define GNUITAR_URI "https://github.com/djshaji/gnuitar#"

enum { FREQUENCY, MIDI_NOTE, CENTS, VALID, METER_COUNT };

typedef struct {
    TunerDSP dsp;
    const float *inputs[2];
    float *outputs[2];
    float *meters[METER_COUNT];
    unsigned int channels;
} Tuner;

static LV2_Handle
instantiate(const LV2_Descriptor *descriptor, double rate, const char *bundle_path,
            const LV2_Feature *const *features)
{
    Tuner *self;

    (void) bundle_path;
    (void) features;
    if (!isfinite(rate) || rate < GNUITAR_TUNER_MIN_RATE || rate > GNUITAR_TUNER_MAX_RATE)
        return NULL;
    self = calloc(1, sizeof(*self));
    if (self) {
        self->channels = strcmp(descriptor->URI, GNUITAR_URI "tuner-mono") == 0 ? 1 : 2;
        gnuitar_tuner_reset(&self->dsp, rate);
    }
    return self;
}

static void
connect_port(LV2_Handle instance, uint32_t port, void *data)
{
    Tuner *self = instance;

    if (port < self->channels)
        self->inputs[port] = data;
    else if (port < 2 * self->channels)
        self->outputs[port - self->channels] = data;
    else if (port < 2 * self->channels + METER_COUNT)
        self->meters[port - 2 * self->channels] = data;
}

static void
activate(LV2_Handle instance)
{
    Tuner *self = instance;

    gnuitar_tuner_reset(&self->dsp, self->dsp.host_rate);
}

static void
run(LV2_Handle instance, uint32_t frames)
{
    Tuner *self = instance;
    const float *left = self->inputs[0], *right = self->inputs[1];
    float *out_left = self->outputs[0], *out_right = self->outputs[1];
    float values[METER_COUNT] = {0.0f, -1.0f, 0.0f, 0.0f};
    uint32_t frame, port;

    for (frame = 0; frame < frames; ++frame) {
        float l = left ? left[frame] : 0.0f;
        float r = right ? right[frame] : 0.0f;
        float sample = self->channels == 2 ? 0.5f * l + 0.5f * r : l;

        gnuitar_tuner_push(&self->dsp, sample);
        if (out_left)
            out_left[frame] = l;
        if (out_right)
            out_right[frame] = r;
    }
    if (self->dsp.frequency > 0.0f) {
        double note = 69.0 + 12.0 * log2(self->dsp.frequency / 440.0);
        long nearest = lround(note);

        values[FREQUENCY] = self->dsp.frequency;
        values[MIDI_NOTE] = (float) nearest;
        values[CENTS] = (float) (100.0 * (note - nearest));
        values[VALID] = 1.0f;
    }
    for (port = 0; port < METER_COUNT; ++port)
        if (self->meters[port])
            *self->meters[port] = values[port];
}

const LV2_Descriptor *
gnuitar_tuner_descriptor(uint32_t index)
{
    static const LV2_Descriptor descriptors[] = {
        {GNUITAR_URI "tuner-mono", instantiate, connect_port, activate, run, NULL, free, NULL},
        {GNUITAR_URI "tuner-stereo", instantiate, connect_port, activate, run, NULL, free, NULL},
    };

    return index < sizeof(descriptors) / sizeof(descriptors[0]) ? &descriptors[index] : NULL;
}
