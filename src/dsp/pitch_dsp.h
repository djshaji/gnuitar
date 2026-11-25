/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2005 Antti S. Lankila <alankila@bel.fi>
 */

#ifndef GNUITAR_PITCH_DSP_H
#define GNUITAR_PITCH_DSP_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../audio-driver.h"

#define GNUITAR_PITCH_MIN_RATE 8000.0
#define GNUITAR_PITCH_MAX_RATE 384000.0
#define GNUITAR_PITCH_TAU 6.28318530717958647692

typedef struct {
    float real, imag;
} PitchComplex;

typedef struct {
    float *input, *output;
    float *previous_phase;
    PitchComplex *fft, *shifted;
} PitchChannel;

typedef struct {
    PitchChannel channel[MAX_CHANNELS];
    PitchComplex *twiddle;
    float *window, *synthesis_phase;
    unsigned char *reference;
    unsigned int size, hop, channels, cursor, filled, pending;
} PitchDSP;

static inline unsigned int
gnuitar_pitch_size(double rate)
{
    unsigned int size = 256;

    /* Use a roughly 46 ms analysis window with a radix-2 transform. */
    while (size * 1.5 < rate * 0.046)
        size <<= 1;
    return size;
}

static inline void
gnuitar_pitch_destroy(PitchDSP *dsp)
{
    unsigned int channel;

    if (!dsp)
        return;
    for (channel = 0; channel < dsp->channels; ++channel) {
        free(dsp->channel[channel].input);
        free(dsp->channel[channel].output);
        free(dsp->channel[channel].previous_phase);
        free(dsp->channel[channel].fft);
        free(dsp->channel[channel].shifted);
    }
    free(dsp->synthesis_phase);
    free(dsp->reference);
    free(dsp->twiddle);
    free(dsp->window);
    free(dsp);
}

static inline PitchDSP *
gnuitar_pitch_create(double max_rate, unsigned int channels)
{
    unsigned int channel;
    unsigned int size = gnuitar_pitch_size(max_rate);
    PitchDSP *dsp = calloc(1, sizeof(*dsp));

    if (!dsp)
        return NULL;
    dsp->channels = channels;
    dsp->synthesis_phase = malloc(size / 2 * sizeof(float));
    dsp->reference = malloc(size / 2 * sizeof(*dsp->reference));
    dsp->twiddle = malloc(size / 2 * sizeof(*dsp->twiddle));
    dsp->window = malloc(size * sizeof(float));
    if (!dsp->synthesis_phase || !dsp->reference || !dsp->twiddle || !dsp->window) {
        gnuitar_pitch_destroy(dsp);
        return NULL;
    }

    for (channel = 0; channel < channels; ++channel) {
        PitchChannel *line = &dsp->channel[channel];
        line->input = malloc(2 * size * sizeof(float));
        line->output = malloc(2 * size * sizeof(float));
        line->previous_phase = malloc(size / 2 * sizeof(float));
        line->fft = malloc(size * sizeof(*line->fft));
        line->shifted = malloc(size * sizeof(*line->shifted));
        if (!line->input || !line->output || !line->previous_phase || !line->fft || !line->shifted) {
            gnuitar_pitch_destroy(dsp);
            return NULL;
        }
    }
    return dsp;
}

static inline void
gnuitar_pitch_reset(PitchDSP *dsp, double rate)
{
    unsigned int size = gnuitar_pitch_size(rate);
    unsigned int i, channel;

    if (dsp->size != size) {
        for (i = 0; i < size / 2; ++i) {
            double angle = GNUITAR_PITCH_TAU * i / size;
            dsp->twiddle[i].real = (float) cos(angle);
            dsp->twiddle[i].imag = (float) -sin(angle);
            dsp->window[i] = (1.0f - dsp->twiddle[i].real) * 0.5f;
            dsp->window[i + size / 2] = (1.0f + dsp->twiddle[i].real) * 0.5f;
        }
        dsp->size = size;
        dsp->hop = size / 4;
    }
    dsp->cursor = 0;
    dsp->filled = 0;
    dsp->pending = 0;
    memset(dsp->reference, 0, size / 2 * sizeof(*dsp->reference));
    memset(dsp->synthesis_phase, 0, size / 2 * sizeof(float));
    for (channel = 0; channel < dsp->channels; ++channel) {
        PitchChannel *line = &dsp->channel[channel];
        memset(line->input, 0, 2 * size * sizeof(float));
        memset(line->output, 0, 2 * size * sizeof(float));
        memset(line->previous_phase, 0, size / 2 * sizeof(float));
    }
}

static inline void
gnuitar_pitch_fft(PitchComplex *data, const PitchComplex *twiddle, unsigned int size,
                  int inverse)
{
    unsigned int i, j = 0, length;

    for (i = 1; i < size; ++i) {
        unsigned int bit = size / 2;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) {
            PitchComplex temp = data[i];
            data[i] = data[j];
            data[j] = temp;
        }
    }
    for (length = 2; length <= size; length <<= 1) {
        unsigned int half = length / 2;
        unsigned int stride = size / length;
        for (i = 0; i < size; i += length) {
            for (j = 0; j < half; ++j) {
                PitchComplex w = twiddle[j * stride];
                PitchComplex a = data[i + j], b = data[i + j + half];
                float real, imag;

                if (inverse)
                    w.imag = -w.imag;
                real = b.real * w.real - b.imag * w.imag;
                imag = b.real * w.imag + b.imag * w.real;
                data[i + j].real = a.real + real;
                data[i + j].imag = a.imag + imag;
                data[i + j + half].real = a.real - real;
                data[i + j + half].imag = a.imag - imag;
            }
        }
    }
}

static inline void
gnuitar_pitch_bin(PitchDSP *dsp, unsigned int bin, unsigned int channels, double ratio)
{
    double expected = GNUITAR_PITCH_TAU * dsp->hop * bin / dsp->size;
    double phase[MAX_CHANNELS], magnitude[MAX_CHANNELS], advance = expected;
    double peak = 0.0, reference_phase = 0.0;
    double synthesis = dsp->synthesis_phase[bin];
    unsigned int ref = 0, previous = dsp->reference[bin];
    unsigned int target = (unsigned int) (bin * ratio + 0.5), channel;

    for (channel = 0; channel < channels; ++channel) {
        PitchComplex sample = dsp->channel[channel].fft[bin];
        phase[channel] = atan2(sample.imag, sample.real);
        magnitude[channel] = hypotf(sample.real, sample.imag);
        if (magnitude[channel] > peak) {
            peak = magnitude[channel];
            reference_phase = phase[channel];
            ref = channel;
        }
    }
    if (peak < 1.0e-12) {
        dsp->reference[bin] = 0;
        return;
    }

    if (previous) {
        double prev_phase = dsp->channel[ref].previous_phase[bin];
        /* Move the running phase to the new reference without losing the
         * original phase difference between channels. */
        synthesis += prev_phase - dsp->channel[previous - 1].previous_phase[bin];
        advance += remainder(reference_phase - prev_phase - expected, GNUITAR_PITCH_TAU);
        synthesis = remainder(synthesis + advance * ratio, GNUITAR_PITCH_TAU);
    } else {
        synthesis = reference_phase;
    }

    dsp->reference[bin] = (unsigned char) (ref + 1);
    dsp->synthesis_phase[bin] = (float) synthesis;
    for (channel = 0; channel < channels; ++channel)
        dsp->channel[channel].previous_phase[bin] = (float) phase[channel];

    /* Frequencies shifted past Nyquist are removed before synthesis. */
    if (target && target < dsp->size / 2 &&
        advance * ratio < GNUITAR_PITCH_TAU * dsp->hop / 2.0) {
        for (channel = 0; channel < channels; ++channel) {
            PitchComplex *output = &dsp->channel[channel].shifted[target];
            double angle = synthesis + phase[channel] - reference_phase;

            output->real += (float) (magnitude[channel] * cos(angle));
            output->imag += (float) (magnitude[channel] * sin(angle));
        }
    }
}

static inline void
gnuitar_pitch_grain(PitchDSP *dsp, unsigned int channels, double ratio)
{
    unsigned int size = dsp->size, mask = 2 * size - 1, i, channel;
    unsigned int start = dsp->cursor + 1 - size;
    float scale = (2.0f / 3.0f) / size; /* Four Hann-squared overlaps. */

    for (channel = 0; channel < channels; ++channel) {
        PitchChannel *line = &dsp->channel[channel];
        for (i = 0; i < size; ++i) {
            line->fft[i].real = dsp->window[i] * line->input[(start + i) & mask];
            line->fft[i].imag = 0.0f;
        }
        gnuitar_pitch_fft(line->fft, dsp->twiddle, size, 0);
        memset(line->shifted, 0, size * sizeof(*line->shifted));
        line->shifted[0].real = line->fft[0].real;
    }

    for (i = 1; i < size / 2; ++i)
        gnuitar_pitch_bin(dsp, i, channels, ratio);

    /* Schedule the window one transform length after its input start. */
    start = dsp->cursor + 1;
    for (channel = 0; channel < channels; ++channel) {
        PitchChannel *line = &dsp->channel[channel];
        for (i = 1; i < size / 2; ++i) {
            line->shifted[size - i].real = line->shifted[i].real;
            line->shifted[size - i].imag = -line->shifted[i].imag;
        }
        gnuitar_pitch_fft(line->shifted, dsp->twiddle, size, 1);
        for (i = 0; i < size; ++i)
            line->output[(start + i) & mask] += line->shifted[i].real * dsp->window[i] * scale;
    }
}

static inline double
gnuitar_pitch_ratio(double semitones, double fine)
{
    double ratio = pow(2.0, (semitones + fine) / 12.0);
    return fmin(2.0, fmax(0.25, ratio));
}

/* The window boundaries are driven by input frames and shared by all channels. */
static inline void
gnuitar_pitch_frame(PitchDSP *dsp, const float *input, float *output,
                    unsigned int channels, double target_ratio, float wet)
{
    unsigned int mask = 2 * dsp->size - 1, channel;
    int analyze = ++dsp->pending == dsp->hop;

    if (analyze)
        dsp->pending = 0;

    for (channel = 0; channel < channels; ++channel) {
        PitchChannel *line = &dsp->channel[channel];
        float dry = line->input[(dsp->cursor - dsp->size) & mask];
        float shifted = target_ratio == 1.0 ? dry : line->output[dsp->cursor];

        line->output[dsp->cursor] = 0.0f;
        line->input[dsp->cursor] = input[channel];
        output[channel] = dsp->filled == dsp->size
                        ? wet == 1.0f ? shifted : dry + wet * (shifted - dry) : 0.0f;
    }
    if (analyze)
        gnuitar_pitch_grain(dsp, channels, target_ratio);
    if (dsp->filled < dsp->size)
        ++dsp->filled;
    dsp->cursor = (dsp->cursor + 1) & mask;
}

#endif
