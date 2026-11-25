/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2000,2001,2003 Max Rudensky <fonin@ziet.zhitomir.ua>
 */

#ifndef GNUITAR_SUSTAIN_DSP_H
#define GNUITAR_SUSTAIN_DSP_H

#include <float.h>

static inline float
gnuitar_sustain_sample(float sample, double envelope, float weight, int threshold)
{
    double gate_level = threshold * 100.0;
    double dry = sample * (1.0 - weight);
    double output;

    if (sample == 0.0f)
        return sample;
    if (!(envelope > 0.0))
        return threshold <= 1 ? sample : 0.0f;

    if (threshold > 1 && envelope < gate_level) {
        /* Cancel the envelope in the compression term before division. */
        output = (dry * envelope + sample * (weight * 30000.0)) / gate_level;
    } else {
        output = dry;
        if (weight != 0.0f)
            output += (sample / envelope) * (weight * 30000.0);
    }

    if (output > FLT_MAX)
        return FLT_MAX;
    if (output < -FLT_MAX)
        return -FLT_MAX;
    return (float) output;
}

#endif
