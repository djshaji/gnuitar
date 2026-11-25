/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2000,2001,2003 Max Rudensky <fonin@ziet.zhitomir.ua>
 */

#ifndef GNUITAR_VIBRATO_DSP_H
#define GNUITAR_VIBRATO_DSP_H

#include "biquad.h"

typedef struct {
    Hilbert_t hilbert;
    float sweep_phase;
    float shift_phase;
} VibratoDSP;

static inline float
gnuitar_vibrato_sine(const float *table, unsigned int size, float phase)
{
    return table[(unsigned int) (phase * (float) size)];
}

static inline float
gnuitar_vibrato_wrap(float phase)
{
    if (phase < 1.0f)
        return phase;
    return phase < 2.0f ? phase - 1.0f : fmodf(phase, 1.0f);
}

static inline DSP_SAMPLE
gnuitar_vibrato_sample(VibratoDSP *state, DSP_SAMPLE sample, int channel,
                       float sine, float cosine, float base_hz)
{
    DSP_SAMPLE x0, x1;

    hilbert_transform(sample, &x0, &x1, &state->hilbert, channel);
    if (base_hz > 0.0f)
        return cosine * x0 + sine * x1;
    return cosine * x0 - sine * x1;
}

static inline float
gnuitar_vibrato_cosine(const float *table, unsigned int size, float phase)
{
    float shifted = phase >= 0.75f ? phase - 0.75f : phase + 0.25f;
    return gnuitar_vibrato_sine(table, size, shifted);
}

static inline void
gnuitar_vibrato_advance(VibratoDSP *state, const float *table, unsigned int size,
                       double sweep_step, double sample_rate, float depth_hz,
                       float base_hz)
{
    float sweep;
    double shift_hz;

    state->sweep_phase = gnuitar_vibrato_wrap((float) (state->sweep_phase + sweep_step));
    sweep = gnuitar_vibrato_sine(table, size, state->sweep_phase);
    shift_hz = fabs(base_hz) + (depth_hz + sweep * depth_hz) / 2.0f;
    state->shift_phase = gnuitar_vibrato_wrap((float) (state->shift_phase + shift_hz / sample_rate));
}

#endif
