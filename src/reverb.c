/*
 * GNUitar
 * Reverberation effect
 * Copyright (C) 2000,2001,2003 Max Rudensky         <fonin@ziet.zhitomir.ua>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id$
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#    include <io.h>
#else
#    include <unistd.h>
#endif

#include "reverb.h"
#include "gui.h"

#define MAX_REVERB_SIZE GNUITAR_REVERB_MAX_MS

static void
update_reverb_drywet(GtkAdjustment *adj, struct reverb_params *params)
{
    params->drywet = adj->value;
}

static void
update_reverb_delay(GtkAdjustment *adj, struct reverb_params *params)
{
    params->delay = adj->value;
}

static void
update_reverb_regen(GtkAdjustment *adj, struct reverb_params *params)
{
    params->regen = adj->value;
}

static void
reverb_init(struct effect *p)
{
    struct reverb_params *preverb;

    GtkWidget      *drywet;
    GtkWidget      *drywet_label;
    GtkObject      *adj_drywet;

    GtkWidget      *delay;
    GtkWidget      *delay_label;
    GtkObject      *adj_delay;

    GtkWidget      *regen;
    GtkWidget      *regen_label;
    GtkObject      *adj_regen;

    GtkWidget      *button;

    GtkWidget      *parmTable;

    preverb = (struct reverb_params *) p->params;

    /*
     * GUI Init
     */
    p->control = gtk_window_new(GTK_WINDOW_DIALOG);

    gtk_signal_connect(GTK_OBJECT(p->control), "delete_event",
		       GTK_SIGNAL_FUNC(delete_event), p);

    parmTable = gtk_table_new(4, 3, FALSE);

    adj_delay = gtk_adjustment_new(preverb->delay, 1.0, MAX_REVERB_SIZE, 1.0, 1.0, 0.0);
    delay_label = gtk_label_new("delay\nms");
    gtk_table_attach(GTK_TABLE(parmTable), delay_label, 0, 1, 0, 1,
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_EXPAND | GTK_SHRINK),
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_SHRINK), 0, 0);


    gtk_signal_connect(GTK_OBJECT(adj_delay), "value_changed",
		       GTK_SIGNAL_FUNC(update_reverb_delay), preverb);

    delay = gtk_vscale_new(GTK_ADJUSTMENT(adj_delay));
    gtk_widget_set_size_request(GTK_WIDGET(delay),0,100);

    gtk_table_attach(GTK_TABLE(parmTable), delay, 0, 1, 1, 2,
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_EXPAND | GTK_SHRINK),
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_EXPAND | GTK_SHRINK), 0, 0);

    adj_drywet = gtk_adjustment_new(preverb->drywet, 0.0, 100.0, 1.0, 1.0, 0.0);
    drywet_label = gtk_label_new("Dry/Wet\n%");
    gtk_table_attach(GTK_TABLE(parmTable), drywet_label, 2, 3, 0, 1,
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_EXPAND | GTK_SHRINK),
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_SHRINK), 0, 0);


    gtk_signal_connect(GTK_OBJECT(adj_drywet), "value_changed",
		       GTK_SIGNAL_FUNC(update_reverb_drywet), preverb);

    drywet = gtk_vscale_new(GTK_ADJUSTMENT(adj_drywet));

    gtk_table_attach(GTK_TABLE(parmTable), drywet, 2, 3, 1, 2,
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_EXPAND | GTK_SHRINK),
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_EXPAND | GTK_SHRINK), 0, 0);


    adj_regen = gtk_adjustment_new(preverb->regen, 0.0, 100.0, 1.0, 1.0, 0.0);
    regen_label = gtk_label_new("regen\n%");
    gtk_table_attach(GTK_TABLE(parmTable), regen_label, 3, 4, 0, 1,
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_EXPAND | GTK_SHRINK),
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_SHRINK), 0, 0);


    gtk_signal_connect(GTK_OBJECT(adj_regen), "value_changed",
		       GTK_SIGNAL_FUNC(update_reverb_regen), preverb);

    regen = gtk_vscale_new(GTK_ADJUSTMENT(adj_regen));

    gtk_table_attach(GTK_TABLE(parmTable), regen, 3, 4, 1, 2,
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_EXPAND | GTK_SHRINK),
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_EXPAND | GTK_SHRINK), 0, 0);

    button = gtk_check_button_new_with_label("On");
    gtk_signal_connect(GTK_OBJECT(button), "toggled",
		       GTK_SIGNAL_FUNC(toggle_effect), p);

    gtk_table_attach(GTK_TABLE(parmTable), button, 0, 1, 2, 3,
		     __GTKATTACHOPTIONS
		     (GTK_EXPAND | GTK_SHRINK),
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_SHRINK), 0, 0);
    if (p->toggle == 1) {
	p->toggle = 0;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    }

    gtk_window_set_title(GTK_WINDOW(p->control),
			 (gchar *) ("Reverberator"));
    gtk_container_add(GTK_CONTAINER(p->control), parmTable);

    gtk_widget_show_all(p->control);

}

static void
reverb_filter(struct effect *p, data_block_t *db)
{
    struct reverb_params *params = p->params;
    ReverbTaps taps = gnuitar_reverb_taps(sample_rate, params->delay,
                                          params->drywet, params->regen);
    int frame, channel;

    for (frame = 0; frame < db->len / db->channels; ++frame) {
        for (channel = 0; channel < db->channels; ++channel) {
            DSP_SAMPLE *sample = &db->data[frame * db->channels + channel];
            *sample = gnuitar_reverb_sample(params->history[channel], *sample,
                                            &taps, INFINITY);
        }
    }
}

static void
reverb_done(struct effect *p)
{
    struct reverb_params *dr;
    int channel, line;

    dr = p->params;
    for (channel = 0; channel < MAX_CHANNELS; ++channel) {
        for (line = 0; line < GNUITAR_REVERB_LINES; ++line)
            del_Backbuf(dr->history[channel][line]);
    }

    free(p->params);
    gtk_widget_destroy(p->control);
    free(p);
}

static void
reverb_save(struct effect *p, SAVE_ARGS)
{
    struct reverb_params *params = p->params;

    SAVE_DOUBLE("drywet", params->drywet);
    SAVE_DOUBLE("regen", params->regen);
    SAVE_DOUBLE("delay", params->delay);
}

static void
reverb_load(struct effect *p, LOAD_ARGS)
{
    struct reverb_params *params = p->params;

    LOAD_DOUBLE("drywet", params->drywet);
    LOAD_DOUBLE("regen", params->regen);
    LOAD_DOUBLE("delay", params->delay);
}

effect_t *
reverb_create()
{
    effect_t       *p;
    struct reverb_params *dr;
    unsigned int rate = sample_rate > MAX_SAMPLE_RATE ? sample_rate : MAX_SAMPLE_RATE;
    int i, line;

    p = calloc(1, sizeof(effect_t));
    p->params = calloc(1, sizeof(struct reverb_params));

    p->proc_init = reverb_init;
    p->proc_filter = reverb_filter;
    p->toggle = 0;
    p->proc_done = reverb_done;
    p->proc_load = reverb_load;
    p->proc_save = reverb_save;
    dr = (struct reverb_params *) p->params;
    for (i = 0; i < MAX_CHANNELS; ++i) {
        for (line = 0; line < GNUITAR_REVERB_LINES; ++line)
            dr->history[i][line] = new_Backbuf(gnuitar_reverb_capacity(line, rate));
    }
    dr->delay  = 130;    /* ms */
    dr->drywet = 25.0;
    dr->regen  = 25.0;
    return p;
}
