/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "tubeamp.h"
#include <dlfcn.h>
#include <math.h>
#include "dsp/tubeamp_dsp.h"
#include <lv2/core/lv2.h>

/* The reference effect's GTK callbacks are unused in this offline host. */
unsigned int sample_rate = 44100;
const float *legacy_transfer_table(void);
gint delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) { return 0; }
void toggle_effect(GtkWidget *widget, effect_t *effect) {}
void gnuitar_printf(const char *format, ...) {}

int
main(int argc, char **argv)
{
    const char *uri = "https://github.com/djshaji/gnuitar#tube-amplifier-mono";
    const LV2_Feature *const features[] = {NULL};
    const LV2_Descriptor *(*describe)(uint32_t);
    const LV2_Descriptor *plugin;
    enum { frames = 8193 };
    float input[frames], output[frames];
    DSP_SAMPLE original[frames];
    void *library;
    uint32_t index;
    int cabinet, quality;

    if (argc != 2 || !(library = dlopen(argv[1], RTLD_NOW)))
        return 1;
    describe = (const LV2_Descriptor *(*)(uint32_t)) dlsym(library, "lv2_descriptor");
    if (!describe)
        return 1;
    for (index = 0; (plugin = describe(index)); ++index) {
        if (strcmp(plugin->URI, uri) == 0)
            break;
    }
    if (!plugin)
        return 1;

    for (cabinet = 0; cabinet < 2; ++cabinet) {
        for (quality = 0; quality < 4; ++quality) {
            effect_t *legacy = tubeamp_create();
            struct tubeamp_params *params = legacy->params;
            LV2_Handle instance = plugin->instantiate(plugin, sample_rate, "", features);
            data_block_t block = {.data = original, .len = frames, .channels = 1};
            float controls[] = {4, 35, -7, -3500, cabinet, quality, 3, -10, 0};
            uint32_t frame, control;
            int channel;
            double max_error = 0.0;

            if (!instance)
                return 1;
            params->impulse_model = cabinet;
            params->impulse_quality = quality;
            for (frame = 0; frame < frames; ++frame) {
                input[frame] = (float) (0.2 * sin(frame * 0.035) + 0.1 * cos(frame * 0.081));
                original[frame] = input[frame] * 8388608.0f;
            }
            if (!cabinet && !quality) {
                const float *old_table = legacy_transfer_table();
                TubeAmpDSP *current = gnuitar_memalign(1, sizeof(*current));
                TubeAmpControls direct = {4, cabinet, quality, (float) pow(10.0, 35.0 / 20.0), -7, -3500};
                double maximum = 0.0;
                float original_first = original[0];
                data_block_t first = {.data = &original_first, .len = 1, .channels = 1};
                for (frame = 0; frame < 16384; ++frame)
                    maximum = fmax(maximum, fabs(old_table[frame] - gnuitar_tube_response[frame]));
                gnuitar_tube_prepare(current, 44100.0);
                gnuitar_tube_reset(current);
                gnuitar_tube_tone(current, 3.0f, -10.0f, 0.0f);
                legacy->proc_filter(legacy, &first);
                float new_first = gnuitar_tube_sample(current, original[0], 0, &direct);
                fprintf(stderr, "table max difference %g, direct output %g vs %g, input %g/%g, decimation %g/%g, FIR %g/%g\n",
                        maximum, new_first, original_first, current->input[0], params->in[0],
                        current->decimation.mem[0][2], params->decimation_filter.mem[0][2],
                        current->cabinet[0][0][0], impulse_g15r[0]);
                for (channel = 0; channel < 4; ++channel)
                    fprintf(stderr, "Stage %d: bias %g/%g lowpass %g/%g highpass %g/%g coeff %g/%g\n",
                            channel, current->bias[0][channel], params->bias[channel],
                            current->lowpass[channel].mem[0][2], params->lowpass[channel].mem[0][2],
                            current->highpass[channel].mem[0][2], params->highpass[channel].mem[0][2],
                            current->lowpass[channel].b0, params->lowpass[channel].b0);
                gnuitar_free(current);
                for (channel = 0; channel < MAX_CHANNELS; ++channel)
                    gnuitar_free(params->buf[channel]);
                gnuitar_free(params);
                free(legacy);
                legacy = tubeamp_create();
                params = legacy->params;
                params->impulse_model = cabinet;
                params->impulse_quality = quality;
            }
            legacy->proc_filter(legacy, &block);
            plugin->connect_port(instance, 0, input);
            plugin->connect_port(instance, 1, output);
            for (control = 0; control < sizeof(controls) / sizeof(controls[0]); ++control)
                plugin->connect_port(instance, 2 + control, &controls[control]);
            plugin->activate(instance);
            plugin->run(instance, frames);

            for (frame = 0; frame < frames; ++frame) {
                double expected = original[frame] / 8388608.0f;
                double error = fabs(output[frame] - expected);
                double tolerance = 1.0e-6 + 1.0e-5 * fabs(expected);
                max_error = fmax(error, max_error);
                if (!isfinite(output[frame]) || error > tolerance) {
                    fprintf(stderr, "Cabinet %d, quality %d, frame %u: %.9g vs %.9g\n",
                            cabinet, quality, frame, output[frame], expected);
                    return 1;
                }
            }
            printf("Cabinet %d, quality %d: maximum mono reference error %.9g\n",
                   cabinet, quality, max_error);
            plugin->cleanup(instance);
            for (channel = 0; channel < MAX_CHANNELS; ++channel)
                gnuitar_free(params->buf[channel]);
            gnuitar_free(params);
            free(legacy);
        }
    }

    dlclose(library);
    return 0;
}
