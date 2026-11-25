/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GNUITAR_NOISE_GATE_DSP_H
#define GNUITAR_NOISE_GATE_DSP_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t quiet_frames;
    double gain;
    bool open;
} NoiseGateDSP;

static inline double
gnuitar_gate_step(NoiseGateDSP *state, float peak, float threshold, float hysteresis,
                  uint32_t hold_frames, double attack_step, double release_step)
{
    if (peak >= (state->open ? hysteresis : threshold)) {
        state->open = true;
        state->quiet_frames = 0;
    } else if (state->open) {
        if (state->quiet_frames < hold_frames)
            ++state->quiet_frames;
        else
            state->open = false;
    }

    if (state->open)
        state->gain = fmin(1.0, state->gain + attack_step);
    else
        state->gain = fmax(0.0, state->gain - release_step);
    return state->gain;
}

#endif
