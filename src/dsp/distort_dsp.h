/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2000,2001,2003 Max Rudensky <fonin@ziet.zhitomir.ua>
 */

#ifndef GNUITAR_DISTORT_DSP_H
#define GNUITAR_DISTORT_DSP_H

#include "../rcfilter.h"
#include "legacy_sample.h"

typedef struct {
    int last[MAX_CHANNELS];
    struct filter_data highpass, lc;
} DistortDSP;

static inline void
gnuitar_distort_reset(DistortDSP *state, double rate)
{
    int channel;

    for (channel = 0; channel < MAX_CHANNELS; ++channel)
        state->last[channel] = 0;
    RC_setup(10, 1.5, &state->highpass);
    RC_set_freq(350.0, rate, &state->highpass);
    RC_setup(10, 1.0, &state->lc);
}

static inline void
gnuitar_distort_process(DistortDSP *state, data_block_t *block, double rate,
                        int drive, int saturation, int level, double lowpass)
{
    double max_delta = saturation * 256.0;
    int_fast32_t i;
    int channel = 0;

    RC_highpass(block, &state->highpass);
    for (i = 0; i < block->len; ++i) {
        double sample = block->data[i];
        double previous = state->last[channel];

        sample *= drive;
        sample /= 16;
        if (sample - previous > max_delta)
            sample = previous + max_delta;
        else if (previous - sample > max_delta)
            sample = previous - max_delta;

        state->last[channel] = gnuitar_truncate_sample(sample);
        channel = (channel + 1) % block->channels;

        sample *= level;
        sample /= 256;
#ifdef CLIP_EVERYWHERE
        if (sample > MAX_SAMPLE)
            sample = MAX_SAMPLE;
        if (sample < -MAX_SAMPLE)
            sample = -MAX_SAMPLE;
#endif
        block->data[i] = isnan(sample) ? 0 : (DSP_SAMPLE) sample;
    }
    LC_filter(block, 0, lowpass, rate, &state->lc);
}

#endif
