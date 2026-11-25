/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GNUITAR_LEGACY_SAMPLE_H
#define GNUITAR_LEGACY_SAMPLE_H

#include <limits.h>
#include <math.h>

static inline int
gnuitar_truncate_sample(double sample)
{
    if (isnan(sample))
        return 0;
    if (sample >= INT_MAX)
        return INT_MAX;
    if (sample <= INT_MIN)
        return INT_MIN;
    return (int) sample;
}

#endif
