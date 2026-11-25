/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2000,2001,2003 Max Rudensky <fonin@ziet.zhitomir.ua>
 */

#ifndef GNUITAR_AUTOWAH_DSP_H
#define GNUITAR_AUTOWAH_DSP_H

#include "../backbuf.h"
#include "../biquad.h"

#include <math.h>

#define GNUITAR_WAH_HISTORY_MS 100
#define GNUITAR_WAH_UPDATE_FRAMES 64

enum { WAH_LOWPASS, WAH_BANDPASS, WAH_MOOG, WAH_TB303 };
enum { WAH_ENVELOPE, WAH_CONTINUOUS, WAH_PEDAL };

typedef struct {
    double power, delta, delayed_power, delayed_delta;
    double position, smoothed, vibrato;
    unsigned int envelope_frames, frames_left;
    int mode, direction;
    Biquad_t lowpass, bandpass;
    float ladder[MAX_CHANNELS][4];
    float gain, first_gain;
} AutowahDSP;

typedef struct {
    double rate, sweep_step, pedal, low, high;
    float resonance, wet;
    int mode, model;
} AutowahControls;

static inline void
gnuitar_wah_measure(AutowahDSP *state, Backbuf_t *const *history,
                     const DSP_SAMPLE *input, unsigned int channels, double rate)
{
    unsigned int channel;
    unsigned int delay = (unsigned int) (rate * GNUITAR_WAH_HISTORY_MS / 1000.0);

    if (delay < 1)
        delay = 1;
    for (channel = 0; channel < channels; ++channel) {
        Backbuf_t *line = history[channel];
        unsigned int age = delay < line->nstor ? delay : line->nstor - 1;
        double current = input[channel];
        double old = line->get(line, age);
        double change = current - line->get(line, 0);
        double old_change = old - line->get(line, age ? age - 1 : 0);

        state->power += current * current;
        state->delta += change * change;
        state->delayed_power += old * old;
        state->delayed_delta += old_change * old_change;
        line->add(line, input[channel]);
    }
    if (++state->envelope_frames > 8192) {
        state->power *= 0.5;
        state->delta *= 0.5;
        state->delayed_power *= 0.5;
        state->delayed_delta *= 0.5;
        state->envelope_frames /= 2;
    }
}

static inline void
gnuitar_wah_tick(AutowahDSP *state, const AutowahControls *controls)
{
    const double tau = 6.28318530717958647692;

    if (state->mode != controls->mode || !state->frames_left) {
        double frequency, offset = 0.0;

        if (controls->mode == WAH_PEDAL) {
            state->position = controls->pedal;
            state->direction = 0;
            state->vibrato = 0.0;
        } else if (controls->mode == WAH_ENVELOPE) {
            if (state->mode != WAH_ENVELOPE) {
                state->position = 0.0;
                state->direction = 0;
            }
            /* Compare power ratios for the legacy 0.60 and 0.65 dB thresholds. */
            if (state->delta > 1.1481536215 * state->delayed_delta ||
                state->power > 1.1614486138 * state->delayed_power) {
                state->position = 1.0;
                state->direction = -1;
            }
        } else if (!state->direction) {
            state->direction = 1;
        }

        if (controls->mode != WAH_PEDAL)
            offset = 0.2 * sin(tau * state->vibrato);

        state->smoothed = (state->position + state->smoothed) * 0.5;
        frequency = controls->low * pow(controls->high / controls->low, state->smoothed)
                    * pow(2.0, offset);
        /* Keep coefficient evaluation away from Nyquist. */
        if (frequency > 0.4 * controls->rate)
            frequency = 0.4 * controls->rate;

        /* Update both biquads so model changes always see usable coefficients. */
        set_lpf_biquad(controls->rate, frequency, 1.1 - controls->resonance, &state->lowpass);
        set_bpf_biquad(controls->rate, frequency, 1.1 - controls->resonance, &state->bandpass);
        state->gain = (float) (1.0 - exp(-tau * frequency / controls->rate));
        state->first_gain = (float) (1.0 - exp(-2.0 * tau * frequency / controls->rate));
        state->frames_left = GNUITAR_WAH_UPDATE_FRAMES;
        state->mode = controls->mode;
    }

    state->position += state->direction * controls->sweep_step;
    if (state->position >= 1.0 && state->direction > 0) {
        state->position = 1.0;
        state->direction = -1;
    }
    if (state->position <= 0.0 && state->direction < 0) {
        state->position = 0.0;
        state->direction = controls->mode == WAH_CONTINUOUS ? 1 : 0;
    }
    if (controls->mode != WAH_PEDAL) {
        state->vibrato += controls->sweep_step * 0.25;
        if (state->vibrato >= 1.0)
            state->vibrato -= floor(state->vibrato);
    }
    --state->frames_left;
}

static inline DSP_SAMPLE
gnuitar_wah_sample(AutowahDSP *state, DSP_SAMPLE input, unsigned int channel,
                   const AutowahControls *controls)
{
    float filtered;

    switch (controls->model) {
    case WAH_LOWPASS:
        filtered = do_biquad(input, &state->lowpass, channel) * 0.5f;
        break;
    case WAH_BANDPASS:
        filtered = do_biquad(input, &state->bandpass, channel);
        break;
    default: {
        float *stage = state->ladder[channel];
        float first_gain = controls->model == WAH_TB303 ? state->first_gain : state->gain;
        const float scale = MAX_SAMPLE;
        float drive = (input - 4.0f * controls->resonance * stage[3]) / scale;
        unsigned int i;

        stage[0] += scale * first_gain * ((float) tanh(drive) - (float) tanh(stage[0] / scale));
        for (i = 1; i < 4; ++i) {
            float previous = (float) tanh(stage[i - 1] / scale);
            stage[i] += scale * state->gain * (previous - (float) tanh(stage[i] / scale));
        }
        filtered = stage[3] * 2.0f;
        break;
    }
    }
    return input * (1.0f - controls->wet) + filtered * controls->wet;
}

#endif
