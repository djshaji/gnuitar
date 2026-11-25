/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdbool.h>
#include <stdlib.h>

#include <lv2/core/lv2.h>

#include "dsp/delay_echo_dsp.h"
#include "dsp/reverb_chorus_dsp.h"
#include "dsp/spatial_dsp.h"
#include "dsp/autowah_dsp.h"

#define GNUITAR_URI "https://github.com/djshaji/gnuitar#"
#define SINE_SIZE 65536
#define LEGACY_FULL_SCALE 8388608.0f

typedef enum { DELAY, ECHO, REVERB, CHORUS, PHASER, ROTARY, AUTOWAH } TimeEffectKind;

typedef struct {
    const float *inputs[2];
    float *outputs[2];
    const float *controls[8];
    Backbuf_t *history[2][GNUITAR_REVERB_LINES];
    float *sine;
    void *dsp;
    TimeEffectKind kind;
    double phase;
    double rate;
    unsigned int inputs_count, outputs_count, lines, num_controls;
} TimeEffect;

static void
cleanup(LV2_Handle handle)
{
    TimeEffect *self = handle;
    unsigned int channel, line;

    for (channel = 0; channel < self->inputs_count; ++channel) {
        for (line = 0; line < self->lines; ++line)
            del_Backbuf(self->history[channel][line]);
    }
    free(self->dsp);
    free(self->sine);
    free(self);
}

static LV2_Handle
create(double rate, unsigned int inputs, unsigned int outputs, TimeEffectKind kind)
{
    TimeEffect *self;
    unsigned int channel, line;

    if (!isfinite(rate) || rate < 1.0 || rate > 384000.0)
        return NULL;

    self = calloc(1, sizeof(*self));
    if (!self)
        return NULL;
    self->rate = rate;
    self->kind = kind;
    self->inputs_count = inputs;
    self->outputs_count = outputs;
    self->lines = kind == ECHO ? GNUITAR_ECHO_MAX_VOICES
                : kind == REVERB ? GNUITAR_REVERB_LINES
                : kind == PHASER || kind == ROTARY ? 0 : 1;
    self->num_controls = kind == CHORUS ? 6 : 3;
    if (kind == PHASER || kind == ROTARY) {
        size_t size = kind == PHASER ? sizeof(PhaserDSP) : sizeof(RotaryDSP);

        if (posix_memalign(&self->dsp, 16, size)) {
            cleanup(self);
            return NULL;
        }
        if (kind == PHASER && inputs != outputs)
            self->num_controls = 2;
        if (kind == ROTARY)
            self->num_controls = 1;
    }
    if (kind == AUTOWAH) {
        self->num_controls = 8;
        if (posix_memalign(&self->dsp, 16, sizeof(AutowahDSP))) {
            cleanup(self);
            return NULL;
        }
    }
    if (kind == CHORUS) {
        self->sine = malloc((SINE_SIZE + 1) * sizeof(*self->sine));
        if (!self->sine) {
            cleanup(self);
            return NULL;
        }
        for (line = 0; line <= SINE_SIZE; ++line)
            self->sine[line] = (float) sin(6.28318530717958647692 * line / SINE_SIZE);
    }
    for (channel = 0; channel < inputs; ++channel) {
        for (line = 0; line < self->lines; ++line) {
            unsigned int capacity;

            switch (kind) {
            case ECHO:
                capacity = (unsigned int) ceil(rate * GNUITAR_ECHO_MAX_MS / 1000.0 *
                                               gnuitar_echo_ratio(line));
                break;
            case REVERB:
                capacity = gnuitar_reverb_capacity(line, rate);
                break;
            case AUTOWAH:
                capacity = (unsigned int) ceil(rate * GNUITAR_WAH_HISTORY_MS / 1000.0) + 1;
                break;
            case CHORUS:
                capacity = (unsigned int) ceil(rate * (GNUITAR_CHORUS_MAX_BASE +
                                                       GNUITAR_CHORUS_MAX_DEPTH) / 1000.0) + 1;
                break;
            default:
                capacity = (unsigned int) ceil(rate * GNUITAR_DELAY_MAX_SECONDS *
                                               GNUITAR_DELAY_MAX_REPEATS);
                break;
            }
            self->history[channel][line] = new_Backbuf(capacity);
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
    TimeEffect *self = handle;
    unsigned int controls_start = self->inputs_count + self->outputs_count;

    if (port < self->inputs_count)
        self->inputs[port] = data;
    else if (port < controls_start)
        self->outputs[port - self->inputs_count] = data;
    else if (port < controls_start + self->num_controls)
        self->controls[port - controls_start] = data;
}

static void
activate(LV2_Handle handle)
{
    TimeEffect *self = handle;
    unsigned int channel, line;

    self->phase = 0.0;
    if (self->kind == PHASER)
        gnuitar_phaser_reset(self->dsp);
    if (self->kind == ROTARY)
        gnuitar_rotary_reset(self->dsp, self->rate);
    if (self->kind == AUTOWAH)
        memset(self->dsp, 0, sizeof(AutowahDSP));
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
    TimeEffect *self = handle;
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
    TimeEffect *self = handle;
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

static void
run_reverb(LV2_Handle handle, uint32_t frames)
{
    TimeEffect *self = handle;
    ReverbTaps taps = gnuitar_reverb_taps(self->rate,
        control(self->controls[0], 130, 1, GNUITAR_REVERB_MAX_MS),
        control(self->controls[1], 25, 0, 100),
        control(self->controls[2], 25, 0, 100));
    uint32_t frame;

    for (frame = 0; frame < frames; ++frame) {
        float left = self->inputs[0][frame];
        float right = self->inputs_count == 2 ? self->inputs[1][frame] : 0.0f;

        /* Bound the recirculating signal at high regeneration settings. */
        self->outputs[0][frame] = gnuitar_reverb_sample(self->history[0], left, &taps, 8.0f);
        if (self->outputs_count == 2)
            self->outputs[1][frame] = gnuitar_reverb_sample(self->history[1], right, &taps, 8.0f);
    }
}

static void
run_chorus(LV2_Handle handle, uint32_t frames)
{
    TimeEffect *self = handle;
    ChorusControls settings = gnuitar_chorus_controls(self->rate,
        control(self->controls[0], 3.5, 0, GNUITAR_CHORUS_MAX_BASE),
        control(self->controls[1], 2.5, 0, GNUITAR_CHORUS_MAX_DEPTH),
        control(self->controls[2], 1000, 200, 5000),
        control(self->controls[3], 50, 0, 100),
        control(self->controls[4], 0, 0, 50),
        (unsigned int) lround(control(self->controls[5], 3, 1, GNUITAR_CHORUS_MAX_VOICES)),
        self->sine, SINE_SIZE);
    uint32_t frame;

    for (frame = 0; frame < frames; ++frame) {
        float left = self->inputs[0][frame];
        float right = self->inputs_count == 2 ? self->inputs[1][frame] : 0.0f;

        if (self->inputs_count == 1 && self->outputs_count == 2) {
            DSP_SAMPLE output[2];
            gnuitar_chorus_spread(self->history[0][0], left, &settings, self->phase, output, 2);
            self->outputs[0][frame] = output[0];
            self->outputs[1][frame] = output[1];
        } else {
            self->outputs[0][frame] = gnuitar_chorus_channel(self->history[0][0], left,
                                                            &settings, self->phase);
            if (self->outputs_count == 2)
                self->outputs[1][frame] = gnuitar_chorus_channel(self->history[1][0], right,
                                                                &settings, self->phase);
        }
        self->phase = gnuitar_chorus_wrap(self->phase + settings.step);
    }
}

static float
spatial_input(float value)
{
    value *= LEGACY_FULL_SCALE;
    return isfinite(value) ? value : 0.0f;
}

static void
run_phaser(LV2_Handle handle, uint32_t frames)
{
    TimeEffect *self = handle;
    PhaserDSP *state = self->dsp;
    bool spread = self->inputs_count != self->outputs_count;
    double step = 1000.0 / control(self->controls[0], 200, 150, 2000) / self->rate;
    float wet = control(self->controls[1], 50, 0, 100) / 100.0;
    double depth = spread ? 0.0 : control(self->controls[2], 100, 0, 100) / 100.0;
    uint32_t frame;

    for (frame = 0; frame < frames; ++frame) {
        float left = spatial_input(self->inputs[0][frame]);
        float right = self->inputs_count == 2 ? spatial_input(self->inputs[1][frame]) : 0.0f;

        if (spread) {
            DSP_SAMPLE output[2];
            gnuitar_phaser_spread(state, left, step, wet, output);
            self->outputs[0][frame] = output[0] / LEGACY_FULL_SCALE;
            self->outputs[1][frame] = output[1] / LEGACY_FULL_SCALE;
        } else {
            gnuitar_phaser_tick(state, step, depth);
            self->outputs[0][frame] = gnuitar_phaser_channel(state, left, 0, wet) / LEGACY_FULL_SCALE;
            if (self->outputs_count == 2)
                self->outputs[1][frame] = gnuitar_phaser_channel(state, right, 1, wet) / LEGACY_FULL_SCALE;
        }
    }
}

static void
run_rotary(LV2_Handle handle, uint32_t frames)
{
    TimeEffect *self = handle;
    double step = 2000.0 / control(self->controls[0], 1000, 200, 5000) / self->rate;
    uint32_t frame;

    for (frame = 0; frame < frames; ++frame) {
        DSP_SAMPLE output[2];
        gnuitar_rotary_spread(self->dsp, spatial_input(self->inputs[0][frame]), step, output);
        self->outputs[0][frame] = output[0] / LEGACY_FULL_SCALE;
        self->outputs[1][frame] = output[1] / LEGACY_FULL_SCALE;
    }
}

static void
run_autowah(LV2_Handle handle, uint32_t frames)
{
    TimeEffect *self = handle;
    AutowahDSP *state = self->dsp;
    Backbuf_t *history[] = {self->history[0][0], self->history[1][0]};
    AutowahControls settings = {
        .rate = self->rate,
        .sweep_step = 2000.0 / control(self->controls[0], 500, 100, 10000) / self->rate,
        .low = control(self->controls[1], 280, 80, 5000),
        .high = control(self->controls[2], 900, 80, 5000),
        .resonance = control(self->controls[3], 85, 30, 100) / 100.0,
        .wet = control(self->controls[4], 100, 0, 100) / 100.0,
        .mode = (int) lround(control(self->controls[5], WAH_ENVELOPE, WAH_ENVELOPE, WAH_PEDAL)),
        .model = (int) lround(control(self->controls[6], WAH_LOWPASS, WAH_LOWPASS, WAH_TB303)),
        .pedal = control(self->controls[7], 0, 0, 1),
    };
    uint32_t frame, channel;

    for (frame = 0; frame < frames; ++frame) {
        DSP_SAMPLE input[2];
        for (channel = 0; channel < self->inputs_count; ++channel)
            input[channel] = spatial_input(self->inputs[channel][frame]);

        gnuitar_wah_measure(state, history, input, self->inputs_count, self->rate);
        gnuitar_wah_tick(state, &settings);
        for (channel = 0; channel < self->outputs_count; ++channel) {
            self->outputs[channel][frame] =
                gnuitar_wah_sample(state, input[channel], channel, &settings) / LEGACY_FULL_SCALE;
        }
    }
}

static TimeEffectKind
effect_kind(const LV2_Descriptor *descriptor)
{
    if (descriptor->run == run_echo)
        return ECHO;
    if (descriptor->run == run_reverb)
        return REVERB;
    if (descriptor->run == run_chorus)
        return CHORUS;
    if (descriptor->run == run_phaser)
        return PHASER;
    if (descriptor->run == run_rotary)
        return ROTARY;
    if (descriptor->run == run_autowah)
        return AUTOWAH;
    return DELAY;
}

static LV2_Handle
instantiate_mono(const LV2_Descriptor *descriptor, double rate, const char *bundle_path,
                 const LV2_Feature *const *features)
{
    (void) bundle_path;
    (void) features;
    return create(rate, 1, 1, effect_kind(descriptor));
}

static LV2_Handle
instantiate_stereo(const LV2_Descriptor *descriptor, double rate, const char *bundle_path,
                   const LV2_Feature *const *features)
{
    (void) bundle_path;
    (void) features;
    return create(rate, 2, 2, effect_kind(descriptor));
}

static LV2_Handle
instantiate_mono_to_stereo(const LV2_Descriptor *descriptor, double rate, const char *bundle_path,
                          const LV2_Feature *const *features)
{
    (void) bundle_path;
    (void) features;
    return create(rate, 1, 2, effect_kind(descriptor));
}

const LV2_Descriptor *
gnuitar_time_effect_descriptor(uint32_t index)
{
    static const LV2_Descriptor descriptors[] = {
        {GNUITAR_URI "delay-mono", instantiate_mono, connect_port, activate, run_delay, NULL, cleanup, NULL},
        {GNUITAR_URI "delay-stereo", instantiate_stereo, connect_port, activate, run_delay, NULL, cleanup, NULL},
        {GNUITAR_URI "delay-mono-to-stereo", instantiate_mono_to_stereo, connect_port, activate, run_delay, NULL, cleanup, NULL},
        {GNUITAR_URI "echo-mono", instantiate_mono, connect_port, activate, run_echo, NULL, cleanup, NULL},
        {GNUITAR_URI "echo-stereo", instantiate_stereo, connect_port, activate, run_echo, NULL, cleanup, NULL},
        {GNUITAR_URI "echo-mono-to-stereo", instantiate_mono_to_stereo, connect_port, activate, run_echo, NULL, cleanup, NULL},
        {GNUITAR_URI "reverb-mono", instantiate_mono, connect_port, activate, run_reverb, NULL, cleanup, NULL},
        {GNUITAR_URI "reverb-stereo", instantiate_stereo, connect_port, activate, run_reverb, NULL, cleanup, NULL},
        {GNUITAR_URI "chorus-mono", instantiate_mono, connect_port, activate, run_chorus, NULL, cleanup, NULL},
        {GNUITAR_URI "chorus-stereo", instantiate_stereo, connect_port, activate, run_chorus, NULL, cleanup, NULL},
        {GNUITAR_URI "chorus-mono-to-stereo", instantiate_mono_to_stereo, connect_port, activate, run_chorus, NULL, cleanup, NULL},
        {GNUITAR_URI "phaser-mono", instantiate_mono, connect_port, activate, run_phaser, NULL, cleanup, NULL},
        {GNUITAR_URI "phaser-stereo", instantiate_stereo, connect_port, activate, run_phaser, NULL, cleanup, NULL},
        {GNUITAR_URI "phaser-mono-to-stereo", instantiate_mono_to_stereo, connect_port, activate, run_phaser, NULL, cleanup, NULL},
        {GNUITAR_URI "rotary-speaker-mono-to-stereo", instantiate_mono_to_stereo, connect_port, activate, run_rotary, NULL, cleanup, NULL},
        {GNUITAR_URI "autowah-mono", instantiate_mono, connect_port, activate, run_autowah, NULL, cleanup, NULL},
        {GNUITAR_URI "autowah-stereo", instantiate_stereo, connect_port, activate, run_autowah, NULL, cleanup, NULL},
    };
    return index < sizeof(descriptors) / sizeof(descriptors[0]) ? &descriptors[index] : NULL;
}
