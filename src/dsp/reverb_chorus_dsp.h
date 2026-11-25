/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2000,2001,2003 Max Rudensky <fonin@ziet.zhitomir.ua>
 */

#ifndef GNUITAR_REVERB_CHORUS_DSP_H
#define GNUITAR_REVERB_CHORUS_DSP_H

#include "../backbuf.h"

#include <math.h>

#define GNUITAR_REVERB_MAX_MS 3000
#define GNUITAR_REVERB_LINES 5
#define GNUITAR_CHORUS_MAX_DEPTH 20
#define GNUITAR_CHORUS_MAX_BASE 50
#define GNUITAR_CHORUS_MAX_VOICES 4

static const unsigned int gnuitar_reverb_ap_taps[] = {1051, 337, 113};
static const unsigned int gnuitar_reverb_comb_taps[] = {4799, 4999, 5399, 5801, 3533, 6521};

typedef struct {
    unsigned int feedback, ap[3], comb[6];
    float wet, regen;
} ReverbTaps;

static inline unsigned int
gnuitar_reverb_scale(unsigned int delay, double rate)
{
    double samples = delay * (rate / 44100.0);
    return samples >= 1.0 ? (unsigned int) samples : 1;
}

static inline unsigned int
gnuitar_reverb_capacity(unsigned int line, double rate)
{
    if (!line)
        return (unsigned int) ceil(GNUITAR_REVERB_MAX_MS / 1000.0 * rate);
    return gnuitar_reverb_scale(line < 4 ? gnuitar_reverb_ap_taps[line - 1] : 6521, rate);
}

static inline ReverbTaps
gnuitar_reverb_taps(double rate, double delay_ms, double wet_pct, double regen_pct)
{
    ReverbTaps taps;
    unsigned int i;

    taps.feedback = (unsigned int) (delay_ms / 1000.0 * rate);
    taps.wet = wet_pct / 100.0;
    taps.regen = regen_pct / 100.0;
    for (i = 0; i < 3; ++i)
        taps.ap[i] = gnuitar_reverb_scale(gnuitar_reverb_ap_taps[i], rate);
    for (i = 0; i < 6; ++i)
        taps.comb[i] = gnuitar_reverb_scale(gnuitar_reverb_comb_taps[i], rate);
    return taps;
}

static inline DSP_SAMPLE
gnuitar_reverb_sample(Backbuf_t *const *history, DSP_SAMPLE input,
                      const ReverbTaps *taps, float feedback_limit)
{
    static const double factors[] = {0.742, 0.733, 0.715, 0.697, 0.75, 0.65};
    float a = input + history[0]->get(history[0], taps->feedback) * taps->regen;
    float wet = 0.0f;
    unsigned int i;

    for (i = 0; i < 3; ++i) {
        Backbuf_t *line = history[i + 1];
        DSP_SAMPLE previous = line->get(line, taps->ap[i] - 1);
        double next = a - 0.7 * previous;
        line->add(line, next);
        a = previous + 0.7 * next;
    }

    /* GNUitar advances a shared comb history once for each tap. */
    for (i = 0; i < 6; ++i) {
        Backbuf_t *line = history[4];
        line->add(line, a);
        wet += a + factors[i] * line->get(line, taps->comb[i]);
    }
    {
        float feedback = wet / 6.0;
        if (feedback > feedback_limit)
            feedback = feedback_limit;
        else if (feedback < -feedback_limit)
            feedback = -feedback_limit;
        history[0]->add(history[0], feedback);
    }

    return input * (1.0f - taps->wet) + wet * taps->wet;
}

typedef struct {
    double base, depth, step, dry, wet, feedback, voice_gain, voice_step;
    unsigned int voices;
    const float *sine;
    unsigned int sine_size;
} ChorusControls;

static inline ChorusControls
gnuitar_chorus_controls(double rate, double delay_ms, double depth_ms, double period_ms,
                         double wet_pct, double feedback_pct, unsigned int voices,
                         const float *sine, unsigned int sine_size)
{
    ChorusControls controls;
    controls.base = delay_ms / 1000.0 * rate;
    controls.depth = depth_ms / 1000.0 * rate;
    controls.step = 1000.0 / period_ms / rate;
    controls.wet = wet_pct / 100.0;
    controls.dry = 1.0 - controls.wet;
    controls.feedback = feedback_pct / 100.0;
    controls.voice_gain = sqrt(voices);
    controls.voice_step = 1.0 / voices;
    controls.voices = voices;
    controls.sine = sine;
    controls.sine_size = sine_size;
    return controls;
}

static inline double
gnuitar_chorus_wrap(double phase)
{
    return phase < 1.0 ? phase : phase - floor(phase);
}

static inline double
gnuitar_chorus_depth(const ChorusControls *controls, double phase)
{
    float position = (float) phase;
    unsigned int index = (unsigned int) (position * (float) controls->sine_size);
    return controls->depth * (1.0f + controls->sine[index]) / 2.0;
}

static inline DSP_SAMPLE
gnuitar_chorus_clip(DSP_SAMPLE sample)
{
#ifdef CLIP_EVERYWHERE
    if (sample > MAX_SAMPLE)
        return MAX_SAMPLE;
    if (sample < -MAX_SAMPLE)
        return -MAX_SAMPLE;
#endif
    return sample;
}

static inline void
gnuitar_chorus_feedback(Backbuf_t *line, DSP_SAMPLE input, const ChorusControls *controls,
                         double phase, int clip)
{
    double delay = controls->base + gnuitar_chorus_depth(controls, phase);
    DSP_SAMPLE sum = input + line->get_interpolated(line, delay) * controls->feedback;
    line->add(line, clip ? gnuitar_chorus_clip(sum) : sum);
}

static inline DSP_SAMPLE
gnuitar_chorus_channel(Backbuf_t *line, DSP_SAMPLE input, const ChorusControls *controls,
                        double phase)
{
    DSP_SAMPLE wet = 0;
    double voice_phase = phase;
    unsigned int voice;

    for (voice = 0; voice < controls->voices; ++voice) {
        double delay = controls->base * voice / controls->voices +
                       gnuitar_chorus_depth(controls, voice_phase);
        wet += line->get_interpolated(line, delay) / controls->voice_gain;
        voice_phase = gnuitar_chorus_wrap(voice_phase + controls->voice_step);
    }
    gnuitar_chorus_feedback(line, input, controls, phase, 1);
    return input * controls->dry / controls->voice_gain +
           gnuitar_chorus_clip(wet) * controls->wet;
}

static inline void
gnuitar_chorus_spread(Backbuf_t *line, DSP_SAMPLE input, const ChorusControls *controls,
                       double phase, DSP_SAMPLE *output, unsigned int channels)
{
    double voice_phase = phase;
    unsigned int channel;

    for (channel = 0; channel < channels; ++channel) {
        double delay = controls->base * channel / channels +
                       gnuitar_chorus_depth(controls, voice_phase);
        DSP_SAMPLE wet = line->get_interpolated(line, delay);
        output[channel] = (input * controls->dry + wet * controls->wet) / sqrt(2.0);
        voice_phase = gnuitar_chorus_wrap(voice_phase + controls->voice_step);
    }
    gnuitar_chorus_feedback(line, input, controls, phase, 0);
}

#endif
