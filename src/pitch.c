/*
 * GNUitar
 * Pitch shifter effect
 * Copyright (c) 2005 Antti S. Lankila  <alankila@bel.fi>
 *
 * See COPYING about details for license.
 *
 * $Id$
 */

#include <math.h>
#include <stdlib.h>

#include "pitch.h"
#include "gui.h"

static void
update_pitch_halfnote(GtkAdjustment *adj, struct pitch_params *params)
{
    params->halfnote = adj->value;
}

static void
update_pitch_finetune(GtkAdjustment *adj, struct pitch_params *params)
{
    params->finetune = adj->value;
}

static void
update_pitch_drywet(GtkAdjustment *adj, struct pitch_params *params)
{
    params->drywet = adj->value;
}

static void
pitch_init(struct effect *p)
{
    struct pitch_params *params = p->params;

    GtkWidget      *halfnote_label;
    GtkWidget      *halfnote;
    GtkObject      *adj_halfnote;

    GtkWidget      *finetune_label;
    GtkWidget      *finetune;
    GtkObject      *adj_finetune;

    GtkWidget      *drywet_label;
    GtkWidget      *drywet;
    GtkObject      *adj_drywet;

    GtkWidget      *parmTable, *button;
    
    /*
     * GUI Init
     */
    p->control = gtk_window_new(GTK_WINDOW_DIALOG);
    gtk_signal_connect(GTK_OBJECT(p->control), "delete_event",
		       GTK_SIGNAL_FUNC(delete_event), p);

    parmTable = gtk_table_new(3, 3, FALSE);

    adj_halfnote = gtk_adjustment_new(params->halfnote, -24.0,
                               12.0, 1.0, 1.0, 0.0);
    halfnote_label = gtk_label_new("Pitch\n(halfnotes)");
    gtk_table_attach(GTK_TABLE(parmTable), halfnote_label, 0, 1, 0, 1,
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK),
		     __GTKATTACHOPTIONS(GTK_FILL |
					GTK_SHRINK), 0, 0);

    gtk_signal_connect(GTK_OBJECT(adj_halfnote), "value_changed",
		       GTK_SIGNAL_FUNC(update_pitch_halfnote), params);

    halfnote = gtk_vscale_new(GTK_ADJUSTMENT(adj_halfnote));
    gtk_scale_set_digits(GTK_SCALE(halfnote), 0);
    gtk_widget_set_size_request(GTK_WIDGET(halfnote),0,100);
    gtk_table_attach(GTK_TABLE(parmTable), halfnote, 0, 1, 1, 2,
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK),
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK), 0, 0);

    adj_finetune = gtk_adjustment_new(params->finetune,
				     -0.5, 0.5, 0.1, 0.1, 0.0);
    finetune_label = gtk_label_new("Finetune\n(halfnotes)");
    gtk_table_attach(GTK_TABLE(parmTable), finetune_label, 1, 2, 0, 1,
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK),
		     __GTKATTACHOPTIONS(GTK_FILL |
					GTK_SHRINK), 0, 0);


    gtk_signal_connect(GTK_OBJECT(adj_finetune), "value_changed",
		       GTK_SIGNAL_FUNC(update_pitch_finetune), params);

    finetune = gtk_vscale_new(GTK_ADJUSTMENT(adj_finetune));
    gtk_scale_set_digits(GTK_SCALE(finetune), 2);
    gtk_table_attach(GTK_TABLE(parmTable), finetune, 1, 2, 1, 2,
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK),
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK), 0, 0);

    adj_drywet = gtk_adjustment_new(params->drywet,
				     0, 100.0, 1.0, 1.0, 0.0);
    drywet_label = gtk_label_new("Dry / Wet\n(%)");
    gtk_table_attach(GTK_TABLE(parmTable), drywet_label, 3, 4, 0, 1,
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK),
		     __GTKATTACHOPTIONS(GTK_FILL |
					GTK_SHRINK), 0, 0);


    gtk_signal_connect(GTK_OBJECT(adj_drywet), "value_changed",
		       GTK_SIGNAL_FUNC(update_pitch_drywet), params);

    drywet = gtk_vscale_new(GTK_ADJUSTMENT(adj_drywet));
    gtk_table_attach(GTK_TABLE(parmTable), drywet, 3, 4, 1, 2,
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK),
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK), 0, 0);

    button = gtk_check_button_new_with_label("On");
    if (p->toggle == 1)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    gtk_signal_connect(GTK_OBJECT(button), "toggled",
		       GTK_SIGNAL_FUNC(toggle_effect), p);

    gtk_table_attach(GTK_TABLE(parmTable), button, 0, 1, 2, 3,
		     __GTKATTACHOPTIONS(GTK_EXPAND |
					GTK_SHRINK),
		     __GTKATTACHOPTIONS(GTK_FILL |
					GTK_SHRINK), 0, 0);

    gtk_window_set_title(GTK_WINDOW(p->control), (gchar *) ("Pitch shift"));
    gtk_container_add(GTK_CONTAINER(p->control), parmTable);

    gtk_widget_show_all(p->control);
}

/* might be defined by compiler or some included thingy, might be not... */
static void
pitch_filter(effect_t *p, data_block_t *db)
{
    struct pitch_params *params = p->params;
    double ratio = gnuitar_pitch_ratio(params->halfnote, params->finetune);
    float wet = (float) fmin(1.0, fmax(0.0, params->drywet / 100.0));
    int_fast32_t frame;
    unsigned int channel, channels = db->channels;

    if (sample_rate < GNUITAR_PITCH_MIN_RATE || sample_rate > GNUITAR_PITCH_MAX_RATE)
        return;
    if (params->rate != sample_rate || params->channels != channels) {
        params->rate = sample_rate;
        params->channels = channels;
        gnuitar_pitch_reset(params->dsp, sample_rate);
    }
    for (frame = 0; frame < db->len / db->channels; ++frame) {
        float input[MAX_CHANNELS], output[MAX_CHANNELS];
        DSP_SAMPLE *samples = db->data + frame * db->channels;

        for (channel = 0; channel < channels; ++channel)
            input[channel] = samples[channel] / (float) MAX_SAMPLE;
        gnuitar_pitch_frame(params->dsp, input, output, channels, ratio, wet);
        for (channel = 0; channel < channels; ++channel)
            samples[channel] = output[channel] * MAX_SAMPLE;
    }
}

static void
pitch_done(struct effect *p)
{
    struct pitch_params *params = p->params;

    gnuitar_pitch_destroy(params->dsp);
    free(params);
    gtk_widget_destroy(p->control);
    free(p);
}

static void
pitch_save(effect_t *p, SAVE_ARGS)
{
    struct pitch_params *params = p->params;

    SAVE_INT("halfnote", params->halfnote);
    SAVE_DOUBLE("finetune", params->finetune);
    SAVE_DOUBLE("drywet", params->drywet);
}

static void
pitch_load(effect_t *p, LOAD_ARGS)
{
    struct pitch_params *params = p->params;
    
    LOAD_INT("halfnote", params->halfnote);
    LOAD_DOUBLE("finetune", params->finetune);
    LOAD_DOUBLE("drywet", params->drywet);
}

effect_t *
pitch_create()
{
    effect_t           *p;
    struct pitch_params *params;

    p = calloc(1, sizeof(effect_t));
    if (!p)
        return NULL;
    params = p->params = calloc(1, sizeof(*params));
    if (!params) {
        free(p);
        return NULL;
    }
    params->dsp = gnuitar_pitch_create(GNUITAR_PITCH_MAX_RATE, MAX_CHANNELS);
    if (!params->dsp) {
        free(params);
        free(p);
        return NULL;
    }
    p->proc_init = pitch_init;
    p->proc_filter = pitch_filter;
    p->toggle = 0;
    p->proc_done = pitch_done;
    p->proc_save = pitch_save;
    p->proc_load = pitch_load;

    params->drywet = 100;
    return p;
}
