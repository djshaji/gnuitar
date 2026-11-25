/*
 * GNUitar
 * Distortion effect 3 -- Tube amplifier
 * Copyright (C) 2006 Antti S. Lankila  <alankila@bel.fi>
 *
 * GPL license.
 *
 * $Id$
 *
 * $Log$
 * Revision 1.45  2006/11/05 18:50:59  alankila
 * - add tone controls into tubeamp and change default speaker type
 *
 * Revision 1.44  2006/11/05 14:59:44  alankila
 * - new GUI
 * - allow specifying speaker model & model precision
 * - reduce CPU drain for default settings
 *
 * Revision 1.43  2006/08/10 16:18:36  alankila
 * - improve const correctness and make gnuitar compile cleanly under
 *   increasingly pedantic warning models.
 *
 * Revision 1.42  2006/08/08 21:05:31  alankila
 * - optimize gnuitar: this breaks dsound, I'll fix it later
 *
 * Revision 1.41  2006/08/06 20:14:55  alankila
 * - split pump.h into several domain-specific headers to reduce file
 *   interdependencies (everyone included pump.h). New files are:
 *   - effect.h for effect definitions
 *   - audio-driver.h for work relating to audio drivers
 *   - audio-midi.h for MIDI interaction.
 *
 * Revision 1.40  2006/07/27 19:24:42  alankila
 * - aligned memory needs aligned free operation.
 *
 * Revision 1.39  2006/07/20 22:32:04  alankila
 * - reduce cpu drain somewhat
 * - prepare effect for different amp models
 *
 * Revision 1.38  2006/07/03 12:08:15  alankila
 * - remove alignment requirement from DSP_SAMPLE; it's not likely we can ever
 *   really make significant use of the procbuf's alignment due to channel
 *   interleaving issues.
 * - move aligned(16) from the datatype to variable declaration; this seems to
 *   work or at least it didn't crash on me when I tried it on x86-32.
 *
 * Revision 1.37  2006/06/20 20:41:08  anarsoul
 * Added some kind of status window. Now we can use gnuitar_printf(char *fmt, ...) that redirects debug information in this window.
 *
 * Revision 1.36  2006/06/16 17:03:09  alankila
 * - there was some blocking distortion, so I had to enlarge the table range
 * - on 4-stage operation the system showed noise due to the nearest value
 *   approximation of the lookup table. I had to move back to interpolating
 *   the table.
 * - I reduced oversampling slightly to compensate for added CPU drain.
 *
 * Revision 1.35  2006/06/01 16:12:19  fonin
 * Fixed wrong include quotes
 *
 * Revision 1.34  2006/05/31 13:53:58  fonin
 * powf() does not exist on Windows; replaced with pow()
 *
 * Revision 1.33  2006/05/29 23:46:02  alankila
 * - move _GNU_SOURCE into Makefile
 * - align memory for x86-32; x86-64 already aligned memory for us in glibc
 *   so we didn't crash. This is done through new gnuitar_memalign().
 * - cater to further restrictions in SSE instructions for x86 arhictecture:
 *   it appears that mulps memory must be aligned to 16 too. This crashed
 *   all biquad-using functions and tubeamp. :-(
 *
 * Revision 1.32  2006/05/27 11:27:20  alankila
 * - make the 4th stage less noisy
 * - add resolution to nonlinearity table, we are too thin
 *
 * Revision 1.31  2006/05/26 13:45:33  alankila
 * - use chebyshev instead of stock lpf
 * - remove 2nd decimation filter, make the 1st stronger instead
 * - build larger nonlinearity table, with good SNR we can skip doing
 *   linear interpolation. CPU usage is reduced to almost third of what it
 *   was a week ago.
 *
 * Revision 1.30  2006/05/25 17:14:30  alankila
 * - rebalance against new waveshaper equation
 *
 * Revision 1.29  2006/05/25 16:54:12  alankila
 * - use 12 dB/oct lowpass filter between stages to help with our treble
 *   problem. Sounds less aliased now.
 *
 * Revision 1.28  2006/05/25 11:32:51  alankila
 * - the simulated tube "hit bottom". Apply waveshaping on both sides of
 *   waveform, but differently.
 *
 * Revision 1.27  2006/05/25 09:03:05  alankila
 * - replace the SSE code with even faster version. Tubeamp effect now runs
 *   20 % faster on my computer. Add some alignment directives to make future
 *   use of SSE potentially easier.
 *
 * Revision 1.26  2006/05/24 19:49:45  alankila
 * - save some CPU by using only float as the DSP accuracy. We can do this
 *   because even floats give us > 100 dB SNR -- that simply has to be enough.
 * - implement tubeamp.c convolution with SSE where available. The floats are
 *   not one bit faster than the integer version of the code, though, but
 *   SSE2 should do the convolution bit faster. I'll see what I can do to
 *   optimize it further.
 *
 * Revision 1.25  2006/05/23 15:28:45  alankila
 * - allow negative parameters for more sound. Positive parameters largerly
 *   distort only treble, while negatives also distort bass.
 *
 * Revision 1.24  2006/05/20 22:19:04  alankila
 * - rebalance 4th stage
 *
 * Revision 1.23  2006/05/20 12:18:48  alankila
 * - tabularize the nonlinearity
 * - new asymmetric nonlinearity based on solving x - y = exp(y) - exp(-y)
 *   type equation
 *
 * Revision 1.22  2006/05/20 10:22:31  alankila
 * - simplify the convolution loop to extreme
 *   (it doesn't appear to vectorize no matter what I do, though)
 *
 * Revision 1.21  2006/05/19 16:16:18  alankila
 * - make the gui scale properly
 *
 * Revision 1.20  2006/05/19 07:30:58  alankila
 * - rebalance to Princeton II that I basically like
 *
 * Revision 1.19  2006/05/18 22:14:29  alankila
 * - more models (commented out)
 *
 * Revision 1.18  2006/05/18 20:58:05  alankila
 * - mimick Marshall Pro Jr
 *
 * Revision 1.17  2006/05/17 10:22:38  alankila
 * - further parameter tuning
 * - flip bias term to drive the distortion to occur the other way
 *
 * Revision 1.16  2006/05/15 19:08:26  alankila
 * - remove several multiplier terms, collect them under F_tube
 * - some remaining constants such as 250 indicate original tube mains voltage
 * - increase feedback strength by some 10x -- warmer now
 *
 * Revision 1.15  2006/05/15 16:00:59  alankila
 * - move middle control into the inner loop
 *
 * Revision 1.14  2006/05/15 11:32:18  alankila
 * - rebalance bias for "punch"
 *
 * Revision 1.13  2006/05/15 10:55:46  alankila
 * - make it sound sweeter
 * - make initial lowpass IIR stronger
 *
 * Revision 1.12  2006/05/15 09:45:01  alankila
 * - new model according to Antti Huoviala et al. "Virtual Air Guitar",
 *   presented at 117th Audio Engineering Society conference.
 * - still missing: proper F_tube.
 *
 * Revision 1.11  2006/05/13 16:20:23  alankila
 * - further small tuning of the formula and the defaults
 *
 * Revision 1.10  2006/05/13 13:33:38  alankila
 * - new biasfreq, more tubish sound
 *
 * Revision 1.9  2006/05/13 08:53:05  alankila
 * - load/save functions
 *
 * Revision 1.8  2006/05/13 08:04:23  alankila
 * - parameter lickup
 *
 * Revision 1.7  2006/05/08 07:29:14  alankila
 * - improve nonlinearity crunch a bit -- add gain -- get by with less stages
 *
 * Revision 1.6  2006/05/07 22:39:18  alankila
 * - correct some errors in the model
 *
 * Revision 1.5  2006/05/07 21:53:26  alankila
 * - remove some multiplications in the waveshaper path
 *
 * Revision 1.4  2006/05/07 18:58:53  alankila
 * - knob to tone the midrange distortion fatness
 *
 * Revision 1.3  2006/05/07 18:22:23  alankila
 * - produce controls and more aggressive defaults
 *
 * Revision 1.2  2006/05/07 14:38:15  alankila
 * - reimplement shelve as partial highpass
 * - use less stages
 *
 * Revision 1.1  2006/05/07 13:22:12  alankila
 * - new bare bones distortion effect: tubeamp
 *
 *
 */

#include <math.h>
#include "gui.h"
#include <stdlib.h>
#include <stdint.h>

#include "biquad.h"
#include "tubeamp.h"

static const char *const ampmodels[] = {
    "Marshall G15R", "Princeton II", NULL
};

static const char *const ampqualities[] = {
    "Lowest", "Normal", "High", "Extreme", NULL
};

static void
update_quality(GtkWidget *w, struct tubeamp_params *params)
{
    int i;
    const char *tmp;
    
    tmp = gtk_entry_get_text(GTK_ENTRY(w));
    if (tmp == NULL)
        return;

    for (i = 0; ampqualities[i] != NULL; i += 1) {
        if (strcmp(tmp, ampqualities[i]) == 0) {
            params->impulse_quality = i;
            break;
        }
    }
}

static void
update_model(GtkWidget *w, struct tubeamp_params *params)
{
    int i;
    const char *tmp;
    
    tmp = gtk_entry_get_text(GTK_ENTRY(w));
    if (tmp == NULL)
        return;

    for (i = 0; ampmodels[i] != NULL; i += 1) {
        if (strcmp(tmp, ampmodels[i]) == 0) {
            params->impulse_model = i;
            break;
        }
    }
}

static void
update_tone(GtkAdjustment *adj, float *ptr)
{
    *ptr = adj->value;
}

static void
update_stages(GtkAdjustment *adj, struct tubeamp_params *params)
{
    params->stages = adj->value;
}

static void
update_gain(GtkAdjustment *adj, struct tubeamp_params *params)
{
    params->gain = adj->value;
}

static void
update_asymmetry(GtkAdjustment *adj, struct tubeamp_params *params)
{
    params->asymmetry = adj->value;
}

static void
update_biasfactor(GtkAdjustment *adj, struct tubeamp_params *params)
{
    params->biasfactor = adj->value;
}

/* lame func that violates layering */    
static void tblattach(GtkWidget *table, GtkWidget *widget, int x, int y) {
    int attachopts = GTK_EXPAND;
    if ((x == 0 || x == 2)
        && !(y == 0 && x == 2)) { /* omit On toggle */
        attachopts = 0;
        gtk_misc_set_alignment(GTK_MISC(widget), 0, 0.5); /* left */
    }

    gtk_table_attach(GTK_TABLE(table), (widget), (x), (x)+1, (y), (y)+1,
                     __GTKATTACHOPTIONS(GTK_FILL | GTK_SHRINK | attachopts),
                     __GTKATTACHOPTIONS(GTK_FILL | GTK_SHRINK), 3, 3);
}

static void
tubeamp_init(struct effect *p)
{
    int i;
    GtkWidget      *w, *table;
    GtkObject      *o;
    GList          *list;
    struct tubeamp_params *params = p->params;
    
    p->control = gtk_window_new(GTK_WINDOW_DIALOG);
    gtk_signal_connect(GTK_OBJECT(p->control), "delete_event",
		       GTK_SIGNAL_FUNC(delete_event), p);
    table = gtk_table_new(3, 8, FALSE);

    tblattach(table, gtk_label_new("Speaker model"), 0, 0);
    list = NULL;
    for (i = 0; ampmodels[i] != NULL; i += 1)
        list = g_list_append(list, (gchar *) ampmodels[i]);
    w = gtk_combo_new();
    gtk_combo_set_popdown_strings(GTK_COMBO(w), list);
    g_list_free(list);
    gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(w)->entry), FALSE);
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(w)->entry), (gchar *) ampmodels[params->impulse_model]);
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(w)->entry), "changed",
                       GTK_SIGNAL_FUNC(update_model), params);
    tblattach(table, w, 1, 0);
    
    tblattach(table, gtk_label_new("Model quality"), 0, 1);
    list = NULL;
    for (i = 0; ampqualities[i] != NULL; i += 1)
        list = g_list_append(list, (gchar *) ampqualities[i]);
    w = gtk_combo_new();
    gtk_combo_set_popdown_strings(GTK_COMBO(w), list);
    g_list_free(list);
    gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(w)->entry), FALSE);
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(w)->entry), (gchar *) ampqualities[params->impulse_quality]);
    gtk_signal_connect(GTK_OBJECT(GTK_COMBO(w)->entry), "changed",
                       GTK_SIGNAL_FUNC(update_quality), params);
    tblattach(table, w, 1, 1);

    tblattach(table, gtk_label_new("Stages"), 0, 2);
    o = gtk_adjustment_new(params->stages, 2, MAX_STAGES, 1, 1, 0);
    gtk_signal_connect(GTK_OBJECT(o), "value_changed",
                       GTK_SIGNAL_FUNC(update_stages), params);
    w = gtk_hscale_new(GTK_ADJUSTMENT(o));
    gtk_scale_set_digits(GTK_SCALE(w), 0);
    tblattach(table, w, 1, 2);
    
    tblattach(table, gtk_label_new("Gain"), 0, 3);
    o = gtk_adjustment_new(params->gain, 30.0, 50.0, 0.1, 1, 0);
    gtk_signal_connect(GTK_OBJECT(o), "value_changed",
                       GTK_SIGNAL_FUNC(update_gain), params);
    w = gtk_hscale_new(GTK_ADJUSTMENT(o));
    tblattach(table, w, 1, 3);
    tblattach(table, gtk_label_new("dB"), 2, 3);
    
    tblattach(table, gtk_label_new("Absolute bias"), 0, 4);
    o = gtk_adjustment_new(params->asymmetry, -5000.0, 5000.0, 0.1, 1, 0);
    gtk_signal_connect(GTK_OBJECT(o), "value_changed",
                       GTK_SIGNAL_FUNC(update_asymmetry), params);
    w = gtk_hscale_new(GTK_ADJUSTMENT(o));
    tblattach(table, w, 1, 4);
    
    tblattach(table, gtk_label_new("Dynamic bias"), 0, 5);
    o = gtk_adjustment_new(params->biasfactor, -25.0, 25.0, 0.1, 1, 0);
    gtk_signal_connect(GTK_OBJECT(o), "value_changed",
                       GTK_SIGNAL_FUNC(update_biasfactor), params);
    w = gtk_hscale_new(GTK_ADJUSTMENT(o));
    tblattach(table, w, 1, 5);

    tblattach(table, gtk_label_new("Tone bass"), 0, 6);
    o = gtk_adjustment_new(params->tone_bass, -10, 10, 0.1, 1, 0);
    gtk_signal_connect(GTK_OBJECT(o), "value_changed",
                       GTK_SIGNAL_FUNC(update_tone), &params->tone_bass);
    tblattach(table, gtk_hscale_new(GTK_ADJUSTMENT(o)), 1, 6);
    tblattach(table, gtk_label_new("dB"), 2, 6);

    tblattach(table, gtk_label_new("Tone middle"), 0, 7);
    o = gtk_adjustment_new(params->tone_middle, -20, 0, 0.1, 1, 0);
    gtk_signal_connect(GTK_OBJECT(o), "value_changed",
                       GTK_SIGNAL_FUNC(update_tone), &params->tone_middle);
    tblattach(table, gtk_hscale_new(GTK_ADJUSTMENT(o)), 1, 7);
    tblattach(table, gtk_label_new("dB"), 2, 7);

    tblattach(table, gtk_label_new("Tone treble"), 0, 8);
    o = gtk_adjustment_new(params->tone_treble, -10, 10, 0.1, 1, 0);
    gtk_signal_connect(GTK_OBJECT(o), "value_changed",
                       GTK_SIGNAL_FUNC(update_tone), &params->tone_treble);
    tblattach(table, gtk_hscale_new(GTK_ADJUSTMENT(o)), 1, 8);
    tblattach(table, gtk_label_new("dB"), 2, 8);

    w = gtk_check_button_new_with_label("On");
    if (p->toggle == 1)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
    gtk_signal_connect(GTK_OBJECT(w), "toggled",
		       GTK_SIGNAL_FUNC(toggle_effect), p);
    tblattach(table, w, 2, 0);
    
    gtk_window_set_title(GTK_WINDOW(p->control), (gchar *) ("Tube amplifier"));
    gtk_container_add(GTK_CONTAINER(p->control), table);

    gtk_widget_show_all(p->control);
}

static void
tubeamp_filter(struct effect *p, data_block_t *db)
{
    struct tubeamp_params *params = p->params;
    TubeAmpControls controls;
    int_fast32_t i;
    int channel = 0;

    if (sample_rate < GNUITAR_TUBE_MIN_RATE || sample_rate > GNUITAR_TUBE_MAX_RATE)
        return;
    if (params->dsp.rate != sample_rate) {
        gnuitar_tube_prepare(&params->dsp, sample_rate);
        gnuitar_tube_reset(&params->dsp);
    }
    gnuitar_tube_tone(&params->dsp, params->tone_bass, params->tone_middle, params->tone_treble);
    controls.stages = params->stages;
    controls.model = params->impulse_model;
    controls.quality = params->impulse_quality;
    controls.gain = (float) pow(10.0, params->gain / 20.0f);
    controls.biasfactor = params->biasfactor;
    controls.asymmetry = params->asymmetry;

    for (i = 0; i < db->len; ++i) {
        db->data[i] = gnuitar_tube_sample(&params->dsp, db->data[i], channel, &controls);
        channel = (channel + 1) % db->channels;
    }
}

static void
tubeamp_done(struct effect *p)
{
    gtk_widget_destroy(p->control);
    gnuitar_free(p->params);
    free(p);
}

static void
tubeamp_save(struct effect *p, SAVE_ARGS)
{
    struct tubeamp_params *params = p->params;
    SAVE_INT("stages", params->stages);
    SAVE_INT("impulse_quality", params->impulse_quality);
    SAVE_INT("impulse_model", params->impulse_model);
    SAVE_DOUBLE("biasfactor", params->biasfactor);
    SAVE_DOUBLE("asymmetry", params->asymmetry);
    SAVE_DOUBLE("gain", params->gain);
    SAVE_DOUBLE("tone_bass", params->tone_bass);
    SAVE_DOUBLE("tone_middle", params->tone_middle);
    SAVE_DOUBLE("tone_treble", params->tone_treble);
}

static void
tubeamp_load(struct effect *p, LOAD_ARGS)
{
    struct tubeamp_params *params = p->params;
    LOAD_INT("stages", params->stages);
    LOAD_INT("impulse_quality", params->impulse_quality);
    LOAD_INT("impulse_model", params->impulse_model);
    LOAD_DOUBLE("biasfactor", params->biasfactor);
    LOAD_DOUBLE("asymmetry", params->asymmetry);
    LOAD_DOUBLE("gain", params->gain);
    LOAD_DOUBLE("tone_bass", params->tone_bass);
    LOAD_DOUBLE("tone_middle", params->tone_middle);
    LOAD_DOUBLE("tone_treble", params->tone_treble);

    if (params->impulse_model < 0 || params->impulse_model > 1)
        params->impulse_model = 0;
    if (params->impulse_quality < 0 || params->impulse_quality >= GNUITAR_TUBE_QUALITIES)
        params->impulse_quality = 1;
    if (params->stages < 2 || params->stages > MAX_STAGES)
        params->stages = MAX_STAGES;
}

effect_t *
tubeamp_create()
{
    effect_t   *p;
    struct tubeamp_params *params;

    p = calloc(1, sizeof(effect_t)); 
    params = p->params = gnuitar_memalign(1, sizeof(struct tubeamp_params));
    p->proc_init = tubeamp_init;
    p->proc_filter = tubeamp_filter;
    p->proc_save = tubeamp_save;
    p->proc_load = tubeamp_load;
    p->toggle = 0;
    p->proc_done = tubeamp_done;

    params->stages = 4;
    params->gain = 35.0; /* dB */
    params->biasfactor = -7;
    params->asymmetry = -3500;
    params->impulse_model = 0;
    params->impulse_quality = 1;

    params->tone_bass = +3; /* dB */
    params->tone_middle = -10; /* dB */

    if (sample_rate >= GNUITAR_TUBE_MIN_RATE && sample_rate <= GNUITAR_TUBE_MAX_RATE) {
        gnuitar_tube_prepare(&params->dsp, sample_rate);
        gnuitar_tube_reset(&params->dsp);
    }
    return p;
}
