/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2000,2001,2003 Max Rudensky <fonin@ziet.zhitomir.ua>
 * Ciulei Bogdan /Dexterus <dexterus@hackernetwork.com>
 */

#ifndef GNUITAR_EQBANK_DSP_H
#define GNUITAR_EQBANK_DSP_H

#include "../biquad.h"

#include <string.h>

#define GNUITAR_EQ_BANDS 14
#define GNUITAR_EQ_GAIN_MAX 9.9

static const int gnuitar_eq_centers[GNUITAR_EQ_BANDS] =
    {40, 100, 200, 320, 640, 1000, 1600, 2200, 3000, 4000, 6000, 8000, 12000, 16000};
static const int gnuitar_eq_bandwidths[GNUITAR_EQ_BANDS] =
    {30, 120, 160, 320, 640, 800, 1200, 1200, 1400, 1800, 2600, 4000, 6000, 8000};

typedef struct {
    Biquad_t filters[GNUITAR_EQ_BANDS];
    DSP_SAMPLE history_in[MAX_CHANNELS][8], history_out[MAX_CHANNELS][8];
    double gain_db[GNUITAR_EQ_BANDS];
    double rate;
    int active_bands;
} EqBankDSP;

static inline void
gnuitar_eqbank_set_band(EqBankDSP *state, int band, double db)
{
    if (band >= state->active_bands || db == state->gain_db[band])
        return;
    state->gain_db[band] = db;
    set_peq_biquad(2.0 * state->rate, gnuitar_eq_centers[band],
                   gnuitar_eq_bandwidths[band], db, &state->filters[band]);
}

static inline void
gnuitar_eqbank_reset(EqBankDSP *state, double rate)
{
    int band;

    memset(state, 0, sizeof(*state));
    state->rate = rate;
    /* Oversampled Nyquist is the host rate. Leave room for the upper
     * half of the band and avoid coefficient calculation at Nyquist. */
    for (band = 0; band < GNUITAR_EQ_BANDS; ++band) {
        if (gnuitar_eq_centers[band] + 0.5 * gnuitar_eq_bandwidths[band] >= rate)
            break;
        set_peq_biquad(2.0 * rate, gnuitar_eq_centers[band],
                       gnuitar_eq_bandwidths[band], 0.0, &state->filters[band]);
    }
    state->active_bands = band;
}

static inline DSP_SAMPLE
gnuitar_eqbank_sample(EqBankDSP *state, DSP_SAMPLE input, int channel, float gain)
{
    DSP_SAMPLE first, second;
    int band;

    fir_interpolate_2x(state->history_in[channel], input, &first, &second);
    for (band = 0; band < state->active_bands; ++band) {
        first = do_biquad(first, &state->filters[band], channel);
        second = do_biquad(second, &state->filters[band], channel);
    }
    return fir_decimate_2x(state->history_out[channel], first, second) * gain;
}

#endif
