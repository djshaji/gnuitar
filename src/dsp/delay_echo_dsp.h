/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2000,2001,2003 Max Rudensky <fonin@ziet.zhitomir.ua>
 */

#ifndef GNUITAR_DELAY_ECHO_DSP_H
#define GNUITAR_DELAY_ECHO_DSP_H

#include "../backbuf.h"

#include <math.h>

#define GNUITAR_DELAY_MAX_REPEATS 10
#define GNUITAR_DELAY_MAX_SECONDS 10
#define GNUITAR_ECHO_MAX_VOICES 4
#define GNUITAR_ECHO_MAX_MS 500

typedef struct {
    unsigned int step;
    unsigned int repeats;
    double decay;
} DelayTaps;

typedef struct {
    unsigned int delay[GNUITAR_ECHO_MAX_VOICES];
    float gain[GNUITAR_ECHO_MAX_VOICES];
    unsigned int voices;
} EchoTaps;

static inline double
gnuitar_echo_ratio(unsigned int voice)
{
    /* The legacy prime progression starts at 23 and grows by 1.6. */
    static const int primes[GNUITAR_ECHO_MAX_VOICES] = {23, 37, 59, 97};
    return primes[voice] / 23.0;
}

static inline DelayTaps
gnuitar_delay_taps(double rate, double time_ms, double decay_pct, unsigned int repeats)
{
    DelayTaps taps;
    taps.step = (unsigned int) (time_ms / 1000.0 * rate);
    taps.decay = decay_pct / 100.0;
    taps.repeats = repeats;
    return taps;
}

static inline DSP_SAMPLE
gnuitar_delay_channel(Backbuf_t *history, DSP_SAMPLE input, const DelayTaps *taps)
{
    DSP_SAMPLE wet = 0;
    double gain = 1.0;
    unsigned int i;

    for (i = 1; i <= taps->repeats; ++i) {
        gain *= taps->decay;
        wet += history->get(history, i * taps->step) * gain;
    }
    history->add(history, input);
    return input + wet;
}

static inline void
gnuitar_delay_spread(Backbuf_t *history, DSP_SAMPLE input, const DelayTaps *taps,
                     DSP_SAMPLE *outputs, unsigned int channels)
{
    static const unsigned int order[] = {0, 1, 3, 2};
    double gain = 1.0;
    unsigned int i;

    for (i = 0; i < channels; ++i)
        outputs[i] = 0;
    history->add(history, input);
    for (i = 0; i <= taps->repeats; ++i) {
        unsigned int channel = order[i % channels];
        if (channel < channels)
            outputs[channel] += history->get(history, i * taps->step) * gain;
        gain *= taps->decay;
    }
}

static inline EchoTaps
gnuitar_echo_taps(double rate, double time_ms, double decay_pct, unsigned int voices)
{
    EchoTaps taps = {0};
    double samples = time_ms / 1000.0 * rate;
    unsigned int i;

    taps.voices = voices;
    for (i = 0; i < voices; ++i) {
        double ratio = gnuitar_echo_ratio(i);
        taps.delay[i] = (unsigned int) (samples * ratio);
        taps.gain[i] = (float) pow(decay_pct / 100.0, ratio);
    }
    return taps;
}

static inline DSP_SAMPLE
gnuitar_echo_channel(Backbuf_t *const *history, Backbuf_t *const *cross_history,
                     DSP_SAMPLE input, const EchoTaps *taps, unsigned int first,
                     unsigned int stride)
{
    double output = input;
    unsigned int i;

    for (i = first; i < taps->voices; i += stride) {
        DSP_SAMPLE wet = history[i]->get(history[i], taps->delay[i]) * taps->gain[i];
        output += wet;
        if (taps->voices > 1) {
            /* Keep the legacy cross-feedback source and traversal order. */
            unsigned int source = i > 1 ? i - 1 : taps->voices - 1;
            Backbuf_t *line = cross_history[source];
            wet += line->get(line, taps->delay[source]) * taps->gain[source] / 10.0;
        }
        history[i]->add(history[i], (double) input + wet);
    }
    return (DSP_SAMPLE) output;
}

#endif
