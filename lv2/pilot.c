/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <lv2/core/lv2.h>

#include "dsp/amp_dsp.h"
#include "dsp/tremolo_dsp.h"
#include "dsp/vibrato_dsp.h"
#include "dsp/noise_gate_dsp.h"
#include "dsp/sustain_dsp.h"
#include "dsp/distort_dsp.h"
#include "dsp/overdrive_dsp.h"
#include "dsp/eqbank_dsp.h"

#define GNUITAR_URI "https://github.com/djshaji/gnuitar#"
#define SINE_TABLE_SIZE 65536
#define TWO_PI 6.28318530717958647692
#define LEGACY_FULL_SCALE 8388608.0f
#define DISTORT_BLOCK_FRAMES 128

typedef struct {
    const float *inputs[2];
    float *outputs[2];
    const float *controls[GNUITAR_EQ_BANDS + 1];
    double sample_rate;
    float phase;
    float *sine_table;
    VibratoDSP *vibrato;
    NoiseGateDSP gate;
    double sustain_envelope[2];
    DistortDSP *distort;
    OverdriveDSP *overdrive;
    EqBankDSP *eqbank;
    uint32_t channels;
    uint32_t num_controls;
} Plugin;

static LV2_Handle
create_instance(double rate, uint32_t channels, uint32_t controls,
                bool needs_sine, bool needs_vibrato)
{
    Plugin *self;
    uint32_t i;

    if (!isfinite(rate) || rate < 1.0 || rate > UINT32_MAX / 15.0)
        return NULL;

    self = calloc(1, sizeof(*self));
    if (!self)
        return NULL;

    self->sample_rate = rate;
    self->channels = channels;
    self->num_controls = controls;

    if (needs_sine) {
        self->sine_table = malloc((SINE_TABLE_SIZE + 1) * sizeof(*self->sine_table));
        if (!self->sine_table) {
            free(self);
            return NULL;
        }

        for (i = 0; i <= SINE_TABLE_SIZE; ++i)
            self->sine_table[i] = (float) sin(TWO_PI * i / SINE_TABLE_SIZE);
    }

    if (needs_vibrato) {
        void *memory;

        /* The existing Hilbert biquads may contain 16-byte SSE vectors. */
        if (posix_memalign(&memory, 16, sizeof(*self->vibrato))) {
            free(self->sine_table);
            free(self);
            return NULL;
        }
        self->vibrato = memory;
    }

    return self;
}

static void
connect_port(LV2_Handle instance, uint32_t port, void *data)
{
    Plugin *self = instance;
    uint32_t channels = self->channels;

    if (port < channels)
        self->inputs[port] = data;
    else if (port < 2 * channels)
        self->outputs[port - channels] = data;
    else if (port < 2 * channels + self->num_controls)
        self->controls[port - 2 * channels] = data;
}

static void
activate(LV2_Handle instance)
{
    Plugin *self = instance;
    self->phase = 0.0f;
    self->gate = (NoiseGateDSP) {0};
    memset(self->sustain_envelope, 0, sizeof(self->sustain_envelope));
    if (self->distort)
        gnuitar_distort_reset(self->distort, self->sample_rate);
    if (self->overdrive)
        gnuitar_overdrive_reset(self->overdrive, self->sample_rate);
    if (self->eqbank)
        gnuitar_eqbank_reset(self->eqbank, self->sample_rate);
    if (self->vibrato) {
        memset(self->vibrato, 0, sizeof(*self->vibrato));
        hilbert_init(&self->vibrato->hilbert);
    }
}

static float
control_value(const float *port, float fallback, float minimum, float maximum)
{
    float value = port ? *port : fallback;
    if (!isfinite(value))
        return fallback;
    return fminf(maximum, fmaxf(minimum, value));
}

static void
read_frame(const Plugin *self, uint32_t frame, float input[2])
{
    /* Stage both inputs before writing, including when the host swaps buffers. */
    input[0] = self->inputs[0][frame];
    input[1] = self->channels == 2 ? self->inputs[1][frame] : 0.0f;
}

static void
write_gain(const Plugin *self, uint32_t frame, const float input[2], double gain)
{
    uint32_t channel;

    for (channel = 0; channel < self->channels; ++channel)
        self->outputs[channel][frame] = (float) (input[channel] * gain);
}

static void
run_amp(LV2_Handle instance, uint32_t frames)
{
    const Plugin *self = instance;
    float db = control_value(self->controls[0], 0.0f, -30.0f, 30.0f);
    float gain = gnuitar_amp_gain(db);
    uint32_t frame;

    for (frame = 0; frame < frames; ++frame) {
        float input[2];
        read_frame(self, frame, input);
        write_gain(self, frame, input, gain);
    }
}

static void
run_tremolo(LV2_Handle instance, uint32_t frames)
{
    Plugin *self = instance;
    float period_ms = control_value(self->controls[0], 200.0f, 20.0f, 2000.0f);
    float depth = control_value(self->controls[1], 25.0f, 0.0f, 100.0f);
    double period = gnuitar_tremolo_period(period_ms, self->sample_rate);
    uint32_t frame;

    for (frame = 0; frame < frames; ++frame) {
        float input[2];
        double gain = gnuitar_tremolo_gain(&self->phase, period, depth,
                                          self->sine_table, SINE_TABLE_SIZE);
        read_frame(self, frame, input);
        write_gain(self, frame, input, gain);
    }
}

static void
run_vibrato(LV2_Handle instance, uint32_t frames)
{
    Plugin *self = instance;
    float period_ms = control_value(self->controls[0], 200.0f, 50.0f, 3000.0f);
    float depth = control_value(self->controls[1], 10.0f, 0.0f, 50.0f);
    float base = control_value(self->controls[2], 0.0f, -50.0f, 50.0f);
    double sweep_step = 1000.0 / period_ms / self->sample_rate;
    uint32_t frame, channel;

    for (frame = 0; frame < frames; ++frame) {
        float input[2];
        float sine = gnuitar_vibrato_sine(self->sine_table, SINE_TABLE_SIZE,
                                          self->vibrato->shift_phase);
        float cosine = gnuitar_vibrato_cosine(self->sine_table, SINE_TABLE_SIZE,
                                              self->vibrato->shift_phase);
        read_frame(self, frame, input);

        for (channel = 0; channel < self->channels; ++channel) {
            float sample = gnuitar_vibrato_sample(self->vibrato,
                                                  input[channel] * LEGACY_FULL_SCALE,
                                                  channel, sine, cosine, base);
            self->outputs[channel][frame] = sample / LEGACY_FULL_SCALE;
        }
        gnuitar_vibrato_advance(self->vibrato, self->sine_table, SINE_TABLE_SIZE,
                               sweep_step, self->sample_rate, depth, base);
    }
}

static uint32_t
milliseconds_to_frames(float milliseconds, double rate)
{
    return (uint32_t) floor(milliseconds * rate / 1000.0 + 0.5);
}

static void
run_noise_gate(LV2_Handle instance, uint32_t frames)
{
    Plugin *self = instance;
    /* Each point on the GNUitar scale represents 10 legacy 16-bit counts. */
    const float threshold_scale = 10.0f / 32768.0f;
    float threshold = control_value(self->controls[0], 50.0f, 0.0f, 100.0f);
    float hysteresis = control_value(self->controls[1], 41.0f, 0.0f, 100.0f);
    uint32_t attack = milliseconds_to_frames(
        control_value(self->controls[2], 0.0f, 0.0f, 4000.0f), self->sample_rate);
    uint32_t hold = milliseconds_to_frames(
        control_value(self->controls[3], 2.0f, 0.0f, 200.0f), self->sample_rate);
    uint32_t release = milliseconds_to_frames(
        control_value(self->controls[4], 500.0f, 0.0f, 15000.0f), self->sample_rate);
    double attack_step = attack ? 1.0 / attack : 1.0;
    double release_step = release ? 1.0 / release : 1.0;
    uint32_t frame;

    hysteresis = fminf(hysteresis, threshold) * threshold_scale;
    threshold *= threshold_scale;
    for (frame = 0; frame < frames; ++frame) {
        float input[2];
        float peak;
        double gain;

        read_frame(self, frame, input);
        peak = fabsf(input[0]);
        if (self->channels == 2)
            peak = fmaxf(peak, fabsf(input[1]));
        gain = gnuitar_gate_step(&self->gate, peak, threshold, hysteresis, hold,
                                  attack_step, release_step);
        write_gain(self, frame, input, gain);
    }
}

static void
run_sustain(LV2_Handle instance, uint32_t frames)
{
    Plugin *self = instance;
    int strength = (int) (control_value(self->controls[0], 100.0f, 0.0f, 100.0f) * 2.56);
    int noise = (int) (control_value(self->controls[1], 15.625f, 0.0f, 100.0f) * 2.56);
    int gate = (int) (control_value(self->controls[2], 100.0f, 0.0f, 100.0f) * 2.56);
    double follow = noise / 256.0;
    float weight = strength / 100.0f;
    uint32_t frame, channel;

    for (frame = 0; frame < frames; ++frame) {
        float input[2];
        read_frame(self, frame, input);
        for (channel = 0; channel < self->channels; ++channel) {
            float sample = input[channel] * LEGACY_FULL_SCALE;
            double *envelope = &self->sustain_envelope[channel];
            if (!isfinite(sample))
                sample = 0.0f;
            *envelope = (1.0 - follow) * *envelope + follow * fabs(sample);
            self->outputs[channel][frame] =
                gnuitar_sustain_sample(sample, *envelope, weight, gate) / LEGACY_FULL_SCALE;
        }
    }
}

static void
run_distort(LV2_Handle instance, uint32_t frames)
{
    Plugin *self = instance;
    int drive = (int) lroundf(control_value(self->controls[0], 55.5f, 1.0f, 100.0f) * 10.0f);
    int saturation = (int) lroundf(control_value(self->controls[1], 100.0f / 3.0f,
                                               1.0f, 100.0f) * 300.0f);
    int level = (int) (control_value(self->controls[2], 7.8125f, 1.0f, 100.0f) * 2.56);
    double lowpass = control_value(self->controls[3], 350.0f, 1.0f, 3000.0f);
    DSP_SAMPLE scratch[2 * DISTORT_BLOCK_FRAMES];
    uint32_t offset = 0;

    /* Keep the legacy LC integrator stable on lower-rate hosts. */
    lowpass = fmin(lowpass, self->sample_rate / 6.0);
    while (offset < frames) {
        uint32_t count = frames - offset;
        uint32_t frame, channel;
        data_block_t block = {.data = scratch, .channels = self->channels};

        if (count > DISTORT_BLOCK_FRAMES)
            count = DISTORT_BLOCK_FRAMES;
        block.len = count * self->channels;

        for (frame = 0; frame < count; ++frame) {
            for (channel = 0; channel < self->channels; ++channel) {
                float sample = self->inputs[channel][offset + frame] * LEGACY_FULL_SCALE;
                scratch[frame * self->channels + channel] = isfinite(sample) ? sample : 0.0f;
            }
        }

        gnuitar_distort_process(self->distort, &block, self->sample_rate,
                                drive, saturation, level, lowpass);

        for (frame = 0; frame < count; ++frame) {
            for (channel = 0; channel < self->channels; ++channel) {
                self->outputs[channel][offset + frame] =
                    scratch[frame * self->channels + channel] / LEGACY_FULL_SCALE;
            }
        }
        offset += count;
    }
}

static Plugin *
create_distort(double rate, uint32_t channels)
{
    Plugin *self;

    if (rate < 8000.0)
        return NULL;
    self = create_instance(rate, channels, 4, false, false);
    if (!self)
        return NULL;
    self->distort = malloc(sizeof(*self->distort));
    if (!self->distort) {
        free(self);
        return NULL;
    }
    return self;
}

static void
run_overdrive(LV2_Handle instance, uint32_t frames)
{
    Plugin *self = instance;
    float drive = control_value(self->controls[0], 0.0f, 0.0f, 100.0f);
    float level = control_value(self->controls[1], 100.0f, 0.0f, 100.0f) / 100.0f;
    float treble = control_value(self->controls[2], 6.0f, -6.0f, 6.0f);
    float resistance = gnuitar_overdrive_resistance(drive);
    float diode_current = gnuitar_overdrive_diode_current();
    uint32_t frame, channel;

    for (frame = 0; frame < frames; ++frame) {
        float input[2];

        read_frame(self, frame, input);
        for (channel = 0; channel < self->channels; ++channel) {
            float sample = input[channel] * LEGACY_FULL_SCALE;
            if (!isfinite(sample))
                sample = 0.0f;
            self->outputs[channel][frame] = gnuitar_overdrive_sample(
                self->overdrive, sample, channel, resistance, diode_current,
                level, treble) / LEGACY_FULL_SCALE;
        }
    }
}

static Plugin *
create_overdrive(double rate, uint32_t channels)
{
    Plugin *self = create_instance(rate, channels, 3, false, false);
    void *memory;

    if (!self)
        return NULL;
    if (posix_memalign(&memory, 16, sizeof(*self->overdrive))) {
        free(self);
        return NULL;
    }
    self->overdrive = memory;
    return self;
}

static void
run_eqbank(LV2_Handle instance, uint32_t frames)
{
    Plugin *self = instance;
    float gain_db = control_value(self->controls[GNUITAR_EQ_BANDS], 0.0f,
                                  -GNUITAR_EQ_GAIN_MAX, GNUITAR_EQ_GAIN_MAX);
    float gain = gnuitar_amp_gain(gain_db);
    int band;
    uint32_t frame, channel;

    for (band = 0; band < self->eqbank->active_bands; ++band) {
        float db = control_value(self->controls[band], 0.0f,
                                 -GNUITAR_EQ_GAIN_MAX, GNUITAR_EQ_GAIN_MAX);
        gnuitar_eqbank_set_band(self->eqbank, band, db);
    }

    for (frame = 0; frame < frames; ++frame) {
        float input[2];

        read_frame(self, frame, input);
        for (channel = 0; channel < self->channels; ++channel) {
            float sample = input[channel] * LEGACY_FULL_SCALE;
            if (!isfinite(sample))
                sample = 0.0f;
            self->outputs[channel][frame] = gnuitar_eqbank_sample(
                self->eqbank, sample, channel, gain) / LEGACY_FULL_SCALE;
        }
    }
}

static Plugin *
create_eqbank(double rate, uint32_t channels)
{
    Plugin *self = create_instance(rate, channels, GNUITAR_EQ_BANDS + 1, false, false);
    void *memory;

    if (!self)
        return NULL;
    if (posix_memalign(&memory, 16, sizeof(*self->eqbank))) {
        free(self);
        return NULL;
    }
    self->eqbank = memory;
    return self;
}

static Plugin *
create_for_descriptor(const LV2_Descriptor *descriptor, double rate, uint32_t channels)
{
    if (descriptor->run == run_tremolo)
        return create_instance(rate, channels, 2, true, false);
    if (descriptor->run == run_vibrato)
        return create_instance(rate, channels, 3, true, true);
    if (descriptor->run == run_noise_gate)
        return create_instance(rate, channels, 5, false, false);
    if (descriptor->run == run_sustain)
        return create_instance(rate, channels, 3, false, false);
    if (descriptor->run == run_distort)
        return create_distort(rate, channels);
    if (descriptor->run == run_overdrive)
        return create_overdrive(rate, channels);
    if (descriptor->run == run_eqbank)
        return create_eqbank(rate, channels);
    return create_instance(rate, channels, 1, false, false);
}

static void
cleanup(LV2_Handle instance)
{
    Plugin *self = instance;
    free(self->eqbank);
    free(self->overdrive);
    free(self->distort);
    free(self->vibrato);
    free(self->sine_table);
    free(self);
}

static LV2_Handle
instantiate_mono(const LV2_Descriptor *descriptor, double rate, const char *bundle_path,
                 const LV2_Feature *const *features)
{
    (void) bundle_path;
    (void) features;
    return create_for_descriptor(descriptor, rate, 1);
}

static LV2_Handle
instantiate_stereo(const LV2_Descriptor *descriptor, double rate, const char *bundle_path,
                   const LV2_Feature *const *features)
{
    (void) bundle_path;
    (void) features;
    return create_for_descriptor(descriptor, rate, 2);
}

static const LV2_Descriptor descriptors[] = {
    {GNUITAR_URI "digital-amp-mono", instantiate_mono, connect_port, activate, run_amp, NULL, cleanup, NULL},
    {GNUITAR_URI "digital-amp-stereo", instantiate_stereo, connect_port, activate, run_amp, NULL, cleanup, NULL},
    {GNUITAR_URI "tremolo-mono", instantiate_mono, connect_port, activate, run_tremolo, NULL, cleanup, NULL},
    {GNUITAR_URI "tremolo-stereo", instantiate_stereo, connect_port, activate, run_tremolo, NULL, cleanup, NULL},
    {GNUITAR_URI "tremolo-bar-mono", instantiate_mono, connect_port, activate, run_vibrato, NULL, cleanup, NULL},
    {GNUITAR_URI "tremolo-bar-stereo", instantiate_stereo, connect_port, activate, run_vibrato, NULL, cleanup, NULL},
    {GNUITAR_URI "noise-gate-mono", instantiate_mono, connect_port, activate, run_noise_gate, NULL, cleanup, NULL},
    {GNUITAR_URI "noise-gate-stereo", instantiate_stereo, connect_port, activate, run_noise_gate, NULL, cleanup, NULL},
    {GNUITAR_URI "sustain-mono", instantiate_mono, connect_port, activate, run_sustain, NULL, cleanup, NULL},
    {GNUITAR_URI "sustain-stereo", instantiate_stereo, connect_port, activate, run_sustain, NULL, cleanup, NULL},
    {GNUITAR_URI "distort-mono", instantiate_mono, connect_port, activate, run_distort, NULL, cleanup, NULL},
    {GNUITAR_URI "distort-stereo", instantiate_stereo, connect_port, activate, run_distort, NULL, cleanup, NULL},
    {GNUITAR_URI "overdrive-mono", instantiate_mono, connect_port, activate, run_overdrive, NULL, cleanup, NULL},
    {GNUITAR_URI "overdrive-stereo", instantiate_stereo, connect_port, activate, run_overdrive, NULL, cleanup, NULL},
    {GNUITAR_URI "eq-bank-mono", instantiate_mono, connect_port, activate, run_eqbank, NULL, cleanup, NULL},
    {GNUITAR_URI "eq-bank-stereo", instantiate_stereo, connect_port, activate, run_eqbank, NULL, cleanup, NULL},
};

const LV2_Descriptor *gnuitar_time_effect_descriptor(uint32_t index);

LV2_SYMBOL_EXPORT
const LV2_Descriptor *
lv2_descriptor(uint32_t index)
{
    const uint32_t count = sizeof(descriptors) / sizeof(descriptors[0]);
    return index < count ? &descriptors[index] : gnuitar_time_effect_descriptor(index - count);
}
