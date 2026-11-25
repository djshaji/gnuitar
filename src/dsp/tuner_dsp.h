/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Time-domain tuner derived from GNUitar's tuner by Antti Lankila. */

#ifndef GNUITAR_TUNER_DSP_H
#define GNUITAR_TUNER_DSP_H

#include <math.h>
#include <string.h>

#define GNUITAR_TUNER_MIN_HZ 27.5
#define GNUITAR_TUNER_MAX_HZ 400.0
#define GNUITAR_TUNER_MIN_RATE 8000.0
#define GNUITAR_TUNER_MAX_RATE 384000.0
/* At the maximum analysis rate of 12 kHz, two A0 periods fit in this history. */
#define GNUITAR_TUNER_RING 1024
#define GNUITAR_TUNER_MEDIAN 5

typedef struct {
    double host_rate, analysis_rate;
    unsigned int decimation, pending, hop, elapsed;
    unsigned int cursor, filled, compare, min_lag, max_lag;
    float sum, power, dc_decay, previous, dc, fir[2];
    float history[2 * GNUITAR_TUNER_RING];
    float measurements[GNUITAR_TUNER_MEDIAN];
    unsigned int measurement_index;
    float frequency;
} TunerDSP;

static inline void
gnuitar_tuner_reset(TunerDSP *state, double rate)
{
    memset(state, 0, sizeof(*state));
    state->host_rate = rate;
    /* Average groups of samples for an analysis rate between 6 and 12 kHz. */
    state->decimation = (unsigned int) (rate / 6000.0);
    state->analysis_rate = rate / state->decimation;
    state->min_lag = (unsigned int) (state->analysis_rate / GNUITAR_TUNER_MAX_HZ);
    state->max_lag = (unsigned int) ceil(state->analysis_rate / GNUITAR_TUNER_MIN_HZ);
    state->compare = state->max_lag;
    state->hop = (unsigned int) lround(state->analysis_rate * 0.020);
    state->dc_decay = (float) exp(-2.0 * 3.14159265358979323846 * 10.0 / state->analysis_rate);
}

static inline float
gnuitar_tuner_difference(const float *end, unsigned int lag, unsigned int count)
{
    const float *recent = end - count;
    const float *previous = recent - lag;
    float difference = 0.0f, energy = 0.0f;
    unsigned int i;

    for (i = 0; i < count; ++i) {
        float a = recent[i], b = previous[i];
        float delta = a - b;
        difference += delta * delta;
        energy += a * a + b * b;
    }
    return energy > 0.0f ? difference / energy : 1.0f;
}

static inline float
gnuitar_tuner_detect(const TunerDSP *state)
{
    const float *end = state->history + GNUITAR_TUNER_RING + state->cursor;
    unsigned int first = state->min_lag / 2;
    unsigned int lag;
    float left = gnuitar_tuner_difference(end, first - 1, state->compare);
    float center = gnuitar_tuner_difference(end, first, state->compare);

    /* Use the first close repeat. Searching above 400 Hz as well keeps a
     * nearby out-of-range tone from being reported an octave too low. */
    for (lag = first; lag <= state->max_lag; ++lag) {
        float right = gnuitar_tuner_difference(end, lag + 1, state->compare);
        if (center < 0.015f && center < left && center <= right) {
            double offset = 0.5 * (left - right) / (left + right - 2.0 * center);
            double frequency = state->analysis_rate / (lag + offset);
            /* Permit interpolation error at the endpoints only. */
            if (frequency < GNUITAR_TUNER_MIN_HZ - 0.01 ||
                frequency > GNUITAR_TUNER_MAX_HZ + 0.2)
                return 0.0f;
            return (float) fmin(GNUITAR_TUNER_MAX_HZ, fmax(GNUITAR_TUNER_MIN_HZ, frequency));
        }
        left = center;
        center = right;
    }
    return 0.0f;
}

static inline void
gnuitar_tuner_update(TunerDSP *state, float measurement)
{
    float sorted[GNUITAR_TUNER_MEDIAN];
    unsigned int i;

    if (measurement == 0.0f) {
        state->frequency = 0.0f;
        return;
    }
    if (state->frequency == 0.0f)
        for (i = 0; i < GNUITAR_TUNER_MEDIAN; ++i)
            state->measurements[i] = measurement;

    state->measurements[state->measurement_index] = measurement;
    state->measurement_index = (state->measurement_index + 1) % GNUITAR_TUNER_MEDIAN;
    for (i = 0; i < GNUITAR_TUNER_MEDIAN; ++i) {
        unsigned int j = i;
        while (j > 0 && sorted[j - 1] > state->measurements[i]) {
            sorted[j] = sorted[j - 1];
            --j;
        }
        sorted[j] = state->measurements[i];
    }
    state->frequency = state->frequency == 0.0f ? measurement :
                       (sorted[GNUITAR_TUNER_MEDIAN / 2] + 2.0f * state->frequency) / 3.0f;
}

static inline void
gnuitar_tuner_push(TunerDSP *state, float sample)
{
    float averaged, smoothed, filtered;

    state->sum += isfinite(sample) ? fminf(8.0f, fmaxf(-8.0f, sample)) : 0.0f;
    if (++state->pending != state->decimation)
        return;
    averaged = state->sum / state->decimation;
    state->pending = 0;
    state->sum = 0.0f;

    smoothed = (averaged + 2.0f * state->fir[0] + state->fir[1]) * 0.25f;
    state->fir[1] = state->fir[0];
    state->fir[0] = averaged;
    filtered = smoothed - state->previous + state->dc_decay * state->dc;
    state->previous = smoothed;
    state->dc = fabsf(filtered) < 1.0e-20f ? 0.0f : filtered;

    state->history[state->cursor] = state->history[state->cursor + GNUITAR_TUNER_RING] = filtered;
    state->cursor = (state->cursor + 1) % GNUITAR_TUNER_RING;
    if (state->filled < GNUITAR_TUNER_RING)
        ++state->filled;
    state->power += filtered * filtered;
    if (++state->elapsed == state->hop) {
        /* Roughly the legacy quiet threshold on the normalized LV2 scale. */
        float mean_square = state->power / state->hop;
        float measurement = 0.0f;
        if (mean_square >= 1.7e-6f && state->filled >= state->compare + state->max_lag + 1)
            measurement = gnuitar_tuner_detect(state);
        gnuitar_tuner_update(state, measurement);
        state->power = 0.0f;
        state->elapsed = 0;
    }
}

#endif
