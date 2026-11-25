/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2000,2001,2003 Max Rudensky <fonin@ziet.zhitomir.ua>
 */

#ifndef GNUITAR_TREMOLO_DSP_H
#define GNUITAR_TREMOLO_DSP_H

static inline double
gnuitar_tremolo_period(float period_ms, double sample_rate)
{
    return period_ms / 1000.0 * sample_rate;
}

/* Advance once per frame so all channels receive the same gain. */
static inline double
gnuitar_tremolo_gain(float *phase, double period, float depth_pct,
                    const float *sine_table, unsigned int table_size)
{
    float position;
    unsigned int index;

    if (*phase >= period)
        *phase = 0.0f;

    position = (float) (*phase / period);
    index = (unsigned int) (position * (float) table_size);
    *phase += 1.0f;
    return 1.0 - depth_pct / 100.0 * (1.0 + sine_table[index]) / 2.0;
}

#endif
