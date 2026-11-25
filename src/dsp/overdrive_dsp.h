/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2000,2001,2003 Max Rudensky <fonin@ziet.zhitomir.ua>
 */

#ifndef GNUITAR_OVERDRIVE_DSP_H
#define GNUITAR_OVERDRIVE_DSP_H

#ifdef _WIN32
#include "../utils.h"
#endif
#include "../biquad.h"

#include <math.h>
#include <string.h>

typedef struct {
    float last[MAX_CHANNELS];
    DSP_SAMPLE interpolate_firmem[MAX_CHANNELS][8];
    Biquad_t feedback_minus_loop, output_bass_cut, rolloff, treble_lowpass;
} OverdriveDSP;

static inline void
gnuitar_overdrive_reset(OverdriveDSP *state, double rate)
{
    const double tau = 6.28318530717958647692;

    memset(state, 0, sizeof(*state));
    set_rc_lowpass_biquad(rate, 3200, &state->treble_lowpass);
    set_rc_lowpass_biquad(rate * 4.0, 1.0 / (tau * 4.7e3 * 47e-9),
                          &state->feedback_minus_loop);
    set_rc_lowpass_biquad(rate * 4.0, 720, &state->rolloff);
    set_rc_highpass_biquad(rate, 160, &state->output_bass_cut);
}

static inline float
gnuitar_overdrive_resistance(float drive_pct)
{
    return 50e3f + drive_pct / 100.0f * 500e3f;
}

static inline float
gnuitar_overdrive_diode_current(void)
{
    const float thermal_voltage = (20.0f + 100.0f - 70.0f) * 1e-3f;
#if defined(_MSC_VER)
    return 10e-12f * (float) exp(0.39f / 30e-3f - 0.39f / thermal_voltage);
#else
    return 10e-12f * expf(0.39f / 30e-3f - 0.39f / thermal_voltage);
#endif
}

static inline DSP_SAMPLE
gnuitar_overdrive_sample(OverdriveDSP *state, DSP_SAMPLE input, int channel,
                         float resistance, float diode_current, float level, float treble)
{
    const float thermal_voltage = (20.0f + 100.0f - 70.0f) * 1e-3f;
    DSP_SAMPLE *history = state->interpolate_firmem[channel];
    DSP_SAMPLE upsample[4];
    float filtered = 0.0f;
    int i;

    /* Interpolate at 2x first, then estimate the two intermediate samples. */
    fir_interpolate_2x(history, input, &upsample[2], &upsample[0]);
    upsample[1] = (upsample[0] + upsample[2]) / (DSP_SAMPLE) 2;
    upsample[3] = ((DSP_SAMPLE) 3 * upsample[2] + history[2]) / (DSP_SAMPLE) 4;

    for (i = 0; i < 4; ++i) {
        float x = upsample[i] * (float) (1.0 / MAX_SAMPLE);
        float feedback = do_biquad(x, &state->feedback_minus_loop, channel);
        float input_current = (x - feedback) / 4.7e3f;
        float y = state->last[channel];
        float delta;
        int remaining = 20;

        /* Start the bounded Newton iteration at the previous solution. */
        do {
#if defined(_MSC_VER)
            float e1 = (float) exp((x - y) / thermal_voltage);
#else
            float e1 = expf((x - y) / thermal_voltage);
#endif
            float e2 = 1.0f / e1;
            float f = input_current + (x - y) / resistance + diode_current * (e1 - e2);
            float slope = -1.0f / resistance - diode_current / thermal_voltage * (e1 + e2);

            delta = f / slope;
            y -= delta;
        } while (fabs(delta) > 1.0f / 32768.0f && --remaining);

        if (!(y >= -2.0f && y <= 2.0f))
            y = 0.0f;
        state->last[channel] = y;
        filtered = do_biquad(y, &state->rolloff, channel);
    }

    filtered += (filtered - do_biquad(filtered, &state->treble_lowpass, channel)) *
                treble / 3.0f;
    filtered = do_biquad(filtered, &state->output_bass_cut, channel);
    return (DSP_SAMPLE) (filtered * (float) MAX_SAMPLE * level);
}

#endif
