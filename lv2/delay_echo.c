/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdbool.h>
#include <stdlib.h>

#include <lv2/core/lv2.h>

#include "dsp/delay_echo_dsp.h"

#define GNUITAR_URI "https://github.com/djshaji/gnuitar#"

typedef struct {
    const float *inputs[2];
    float *outputs[2];
    const float *controls[3];
    Backbuf_t *history[2][GNUITAR_ECHO_MAX_VOICES];
    double rate;
    unsigned int inputs_count, outputs_count, lines;
} DelayEcho;

static void
cleanup(LV2_Handle handle)
{
    DelayEcho *self = handle;
    unsigned int channel, line;

    for (channel = 0; channel < self->inputs_count; ++channel) {
        for (line = 0; line < self->lines; ++line)
            del_Backbuf(self->history[channel][line]);
    }
    free(self);
}

static LV2_Handle
create(double rate, unsigned int inputs, unsigned int outputs, bool echo)
{
    DelayEcho *self;
    unsigned int channel, line;

    if (!isfinite(rate) || rate < 1.0 || rate > 384000.0)
        return NULL;

    self = calloc(1, sizeof(*self));
    if (!self)
        return NULL;
    self->rate = rate;
    self->inputs_count = inputs;
    self->outputs_count = outputs;
    self->lines = echo ? GNUITAR_ECHO_MAX_VOICES : 1;
    for (channel = 0; channel < inputs; ++channel) {
        for (line = 0; line < self->lines; ++line) {
            double seconds = echo ? GNUITAR_ECHO_MAX_MS / 1000.0 * gnuitar_echo_ratio(line)
                                  : GNUITAR_DELAY_MAX_SECONDS * GNUITAR_DELAY_MAX_REPEATS;
            self->history[channel][line] = new_Backbuf((unsigned int) ceil(rate * seconds));
            if (!self->history[channel][line]) {
                cleanup(self);
                return NULL;
            }
        }
    }
    return self;
}

static void
connect_port(LV2_Handle handle, uint32_t port, void *data)
{
    DelayEcho *self = handle;
    unsigned int controls_start = self->inputs_count + self->outputs_count;

    if (port < self->inputs_count)
        self->inputs[port] = data;
    else if (port < controls_start)
        self->outputs[port - self->inputs_count] = data;
    else if (port < controls_start + 3)
        self->controls[port - controls_start] = data;
}

static void
activate(LV2_Handle handle)
{
    DelayEcho *self = handle;
    unsigned int channel, line;

    for (channel = 0; channel < self->inputs_count; ++channel) {
        for (line = 0; line < self->lines; ++line) {
            Backbuf_t *history = self->history[channel][line];
            history->clear(history);
        }
    }
}

static double
control(const float *port, double fallback, double minimum, double maximum)
{
    double value = port ? *port : fallback;
    return isfinite(value) ? fmin(maximum, fmax(minimum, value)) : fallback;
}

static void
run_delay(LV2_Handle handle, uint32_t frames)
{
    DelayEcho *self = handle;
    DelayTaps taps = gnuitar_delay_taps(self->rate,
        control(self->controls[0], 1000, 1, GNUITAR_DELAY_MAX_SECONDS * 1000),
        control(self->controls[1], 55, 10, 100),
        (unsigned int) lround(control(self->controls[2], 8, 1, GNUITAR_DELAY_MAX_REPEATS)));
    uint32_t frame;

    for (frame = 0; frame < frames; ++frame) {
        float left = self->inputs[0][frame];
        float right = self->inputs_count == 2 ? self->inputs[1][frame] : 0.0f;

        if (self->inputs_count == 1 && self->outputs_count == 2) {
            DSP_SAMPLE output[2];
            gnuitar_delay_spread(self->history[0][0], left, &taps, output, 2);
            self->outputs[0][frame] = output[0];
            self->outputs[1][frame] = output[1];
        } else {
            self->outputs[0][frame] = gnuitar_delay_channel(self->history[0][0], left, &taps);
            if (self->outputs_count == 2)
                self->outputs[1][frame] = gnuitar_delay_channel(self->history[1][0], right, &taps);
        }
    }
}

static void
run_echo(LV2_Handle handle, uint32_t frames)
{
    DelayEcho *self = handle;
    EchoTaps taps = gnuitar_echo_taps(self->rate,
        control(self->controls[0], 200, 1, GNUITAR_ECHO_MAX_MS),
        control(self->controls[1], 30, 1, 80),
        (unsigned int) lround(control(self->controls[2], 4, 1, GNUITAR_ECHO_MAX_VOICES)));
    uint32_t frame;

    for (frame = 0; frame < frames; ++frame) {
        float left = self->inputs[0][frame];
        float right = self->inputs_count == 2 ? self->inputs[1][frame] : 0.0f;

        if (self->inputs_count == 1 && self->outputs_count == 2) {
            self->outputs[0][frame] = gnuitar_echo_channel(self->history[0], self->history[0],
                                                          left, &taps, 0, 2);
            self->outputs[1][frame] = gnuitar_echo_channel(self->history[0], self->history[0],
                                                          left, &taps, 1, 2);
        } else {
            self->outputs[0][frame] = gnuitar_echo_channel(self->history[0], self->history[0],
                                                          left, &taps, 0, 1);
            if (self->outputs_count == 2)
                self->outputs[1][frame] = gnuitar_echo_channel(self->history[1], self->history[1],
                                                              right, &taps, 0, 1);
        }
    }
}

static LV2_Handle
instantiate_mono(const LV2_Descriptor *descriptor, double rate, const char *bundle_path,
                 const LV2_Feature *const *features)
{
    (void) bundle_path;
    (void) features;
    return create(rate, 1, 1, descriptor->run == run_echo);
}

static LV2_Handle
instantiate_stereo(const LV2_Descriptor *descriptor, double rate, const char *bundle_path,
                   const LV2_Feature *const *features)
{
    (void) bundle_path;
    (void) features;
    return create(rate, 2, 2, descriptor->run == run_echo);
}

static LV2_Handle
instantiate_mono_to_stereo(const LV2_Descriptor *descriptor, double rate, const char *bundle_path,
                          const LV2_Feature *const *features)
{
    (void) bundle_path;
    (void) features;
    return create(rate, 1, 2, descriptor->run == run_echo);
}

const LV2_Descriptor *
gnuitar_delay_echo_descriptor(uint32_t index)
{
    static const LV2_Descriptor descriptors[] = {
        {GNUITAR_URI "delay-mono", instantiate_mono, connect_port, activate, run_delay, NULL, cleanup, NULL},
        {GNUITAR_URI "delay-stereo", instantiate_stereo, connect_port, activate, run_delay, NULL, cleanup, NULL},
        {GNUITAR_URI "delay-mono-to-stereo", instantiate_mono_to_stereo, connect_port, activate, run_delay, NULL, cleanup, NULL},
        {GNUITAR_URI "echo-mono", instantiate_mono, connect_port, activate, run_echo, NULL, cleanup, NULL},
        {GNUITAR_URI "echo-stereo", instantiate_stereo, connect_port, activate, run_echo, NULL, cleanup, NULL},
        {GNUITAR_URI "echo-mono-to-stereo", instantiate_mono_to_stereo, connect_port, activate, run_echo, NULL, cleanup, NULL},
    };
    return index < sizeof(descriptors) / sizeof(descriptors[0]) ? &descriptors[index] : NULL;
}
