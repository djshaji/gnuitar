/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2006 Vasily Khoruzhick <anarsoul@gmail.com>
 */

#ifndef GNUITAR_AMP_DSP_H
#define GNUITAR_AMP_DSP_H

#include <math.h>

static inline float
gnuitar_amp_gain(float gain_db)
{
    return (float) pow(10.0, gain_db / 20.0);
}

#endif
