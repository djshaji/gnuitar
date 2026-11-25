/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "tubeamp.h"
#include <dlfcn.h>
#include <math.h>
#include <lv2/core/lv2.h>

/* The reference effect's GTK callbacks are unused in this offline host. */
unsigned int sample_rate = 44100;
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
