/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2000,2001,2003 Max Rudensky <fonin@ziet.zhitomir.ua>
 * Copyright (C) 2006 Antti S. Lankila <alankila@bel.fi>
 */

#ifndef GNUITAR_SPATIAL_DSP_H
#define GNUITAR_SPATIAL_DSP_H

#include "../biquad.h"

#include <math.h>
#include <string.h>

#define GNUITAR_PHASER_STAGES 24
#define GNUITAR_SPATIAL_UPDATE_FRAMES 8

typedef struct {
    double phase;
    unsigned int frames_left;
    float sine, cosine;
} SpatialLFO;

typedef struct {
    SpatialLFO lfo;
    Biquad_t allpass[GNUITAR_PHASER_STAGES];
    Hilbert_t hilbert;
    int coefficients_ready;
} PhaserDSP;

typedef struct {
    SpatialLFO lfo;
    Biquad_t opposite_left, opposite_right;
    Hilbert_t hilbert;
    double rate;
} RotaryDSP;

static inline int
gnuitar_spatial_tick(SpatialLFO *lfo, double step)
{
    int update = !lfo->frames_left;

    if (update) {
        double radians = lfo->phase * 6.28318530717958647692;
        lfo->sine = (float) sin(radians);
        lfo->cosine = (float) cos(radians);
        lfo->frames_left = GNUITAR_SPATIAL_UPDATE_FRAMES;
    }

    lfo->phase += step;
    if (lfo->phase >= 1.0)
        lfo->phase -= floor(lfo->phase);
    --lfo->frames_left;
    return update;
}

static inline void
gnuitar_phaser_reset(PhaserDSP *state)
{
    memset(state, 0, sizeof(*state));
    hilbert_init(&state->hilbert);
}

static inline void
gnuitar_phaser_tick(PhaserDSP *state, double step, double depth)
{
    if (gnuitar_spatial_tick(&state->lfo, step) || !state->coefficients_ready) {
        double sweep = 1.0 - depth * (state->lfo.sine + 1.0) * 0.5;
        double delay = (exp(0.7 * sweep) - 1.0) / (exp(0.7) - 1.0);
        unsigned int stage;

        /* Keep the allpass poles inside the unit circle at the endpoint. */
        if (delay > 0.999)
            delay = 0.999;
        for (stage = 0; stage < GNUITAR_PHASER_STAGES; ++stage)
            set_phaser_biquad(delay, &state->allpass[stage]);
        state->coefficients_ready = 1;
    }
}

static inline DSP_SAMPLE
gnuitar_phaser_channel(PhaserDSP *state, DSP_SAMPLE input, int channel, float wet)
{
    DSP_SAMPLE filtered = input;
    unsigned int stage;

    for (stage = 0; stage < GNUITAR_PHASER_STAGES; ++stage)
        filtered = do_biquad(filtered, &state->allpass[stage], channel);
    return input * (1.0f - wet) + filtered * wet;
}

static inline void
gnuitar_spatial_sidebands(Hilbert_t *hilbert, const SpatialLFO *lfo, DSP_SAMPLE input,
                          DSP_SAMPLE *left, DSP_SAMPLE *right)
{
    DSP_SAMPLE x0, x1;
    hilbert_transform(input, &x0, &x1, hilbert, 0);
    *left = lfo->cosine * x0 + lfo->sine * x1;
    *right = lfo->cosine * x0 - lfo->sine * x1;
}

static inline void
gnuitar_phaser_spread(PhaserDSP *state, DSP_SAMPLE input, double step, float wet,
                      DSP_SAMPLE *output)
{
    DSP_SAMPLE left, right;

    gnuitar_spatial_tick(&state->lfo, step);
    gnuitar_spatial_sidebands(&state->hilbert, &state->lfo, input, &left, &right);
    output[0] = input * (1.0f - wet) + wet * left;
    output[1] = input * (1.0f - wet) + wet * right;
}

static inline void
gnuitar_rotary_set_rate(RotaryDSP *state, double rate)
{
    if (state->rate != rate) {
        set_rc_lowpass_biquad(rate, 2000.0, &state->opposite_left);
        set_rc_lowpass_biquad(rate, 2000.0, &state->opposite_right);
        state->rate = rate;
    }
}

static inline void
gnuitar_rotary_reset(RotaryDSP *state, double rate)
{
    memset(state, 0, sizeof(*state));
    hilbert_init(&state->hilbert);
    gnuitar_rotary_set_rate(state, rate);
}

static inline void
gnuitar_rotary_spread(RotaryDSP *state, DSP_SAMPLE input, double step, DSP_SAMPLE *output)
{
    DSP_SAMPLE left, right;

    gnuitar_spatial_tick(&state->lfo, step);
    gnuitar_spatial_sidebands(&state->hilbert, &state->lfo, input, &left, &right);
    output[0] = 0.60 * left + 0.40 * do_biquad(right, &state->opposite_left, 0);
    output[1] = 0.60 * right + 0.40 * do_biquad(left, &state->opposite_right, 0);
}

#endif
