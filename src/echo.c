/*
 * GNUitar
 * Echo effect
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
 *
 * $Log$
 * Revision 1.26  2006/09/04 14:25:10  alankila
 * - hide multichannel GUI if input channels != 1
 *
 * Revision 1.25  2006/08/02 19:07:57  alankila
 * - add missing static declarations
 *
 * Revision 1.24  2005/09/16 20:40:51  alankila
 * - reduce total voice count to spare memory and constrain effect
 * - complete circular mixing and increase cross-mixing attenuation a lot
 *
 * Revision 1.23  2005/09/06 23:21:40  alankila
 * - tabularize pow, set some constants, do crossmixing, better defaults
 *
 * Revision 1.22  2005/09/06 14:54:32  alankila
 * - set button states at loadup
 * - make echo multichannel aware. Echo currently can do almost everything
 *   reverb can do, so we could remove reverb.
 *
 * Revision 1.21  2005/09/04 23:05:17  alankila
 * - delete the repeated toggle_foo functions, use one global from gui.c
 *
 * Revision 1.20  2005/09/04 14:40:17  alankila
 * - get rid of effect->id and associated enumeration
 *
 * Revision 1.19  2005/09/04 12:12:35  alankila
 * - make create() and done() symmetric in memory allocation/free
 *
 * Revision 1.18  2005/09/04 11:16:59  alankila
 * - destroy passthru function, move the toggle logic higher up
 *
 * Revision 1.17  2005/09/04 01:51:09  alankila
 * - GKeyFile-based preset load/save
 * - still need locale-immune %lf for printf and sscanf
 *
 * Revision 1.16  2005/09/03 16:36:51  alankila
 * - reworked effect to use backbuff
 * - removed some output scaling that made this effect almost inaudible
 * - reinstated a decay based on delay % and echo delay
 * - echo_size adjustment works again
 *
 * Revision 1.15  2005/09/02 11:58:49  alankila
 * - remove #ifdef HAVE_GTK2 entirely from all effect code
 *
 * Revision 1.14  2005/09/01 23:52:15  alankila
 * - make window delete event do something useful
 *
 * Revision 1.13  2005/08/18 23:54:32  alankila
 * - use GTK_WINDOW_DIALOG instead of TOPLEVEL, however #define them the same
 *   for GTK2.
 *
 * Revision 1.12  2005/08/13 12:06:08  alankila
 * - removed bunch of #ifdef HAVE_GTK/HAVE_GTK2 regarding window type
 *
 * Revision 1.11  2004/08/10 15:07:31  fonin
 * Support processing in float/int - type DSP_SAMPLE
 *
 * Revision 1.10  2004/07/07 19:18:42  fonin
 * GTK2 port
 *
 * Revision 1.9  2003/03/14 19:20:31  fonin
 * Unreferenced variables fixed.
 *
 * Revision 1.8  2003/03/12 20:53:54  fonin
 * - meaningful sliders measure units;
 * - code cleanup.
 *
 * Revision 1.7  2003/02/03 11:39:25  fonin
 * Copyright year changed.
 *
 * Revision 1.6  2003/02/01 19:15:12  fonin
 * Use sizeof(variable) instead sizeof(type) in load/save procedures,
 * when reading/writing from file.
 *
 * Revision 1.5  2003/01/30 21:35:29  fonin
 * Got rid of rnd_window_pos().
 *
 * Revision 1.4  2003/01/29 19:34:00  fonin
 * Win32 port.
 *
 * Revision 1.3  2001/06/02 14:05:59  fonin
 * Added GNU disclaimer.
 *
 * Revision 1.2  2001/03/25 12:10:49  fonin
 * Effect window control ignores delete event.
 *
 * Revision 1.1.1.1  2001/01/11 13:21:41  fonin
 * Version 0.1.0 Release 1 beta
 *
 */

#include "echo.h"
#include "gui.h"
#include <math.h>
#include <assert.h>
#include <stdlib.h>
#ifndef _WIN32
#    include <unistd.h>
#else
#    include <io.h>
#endif

static void
update_echo_decay(GtkAdjustment *adj, struct echo_params *params)
{
    params->echo_decay = adj->value;
}

static void
update_echo_count(GtkAdjustment *adj, struct echo_params *params)
{
    params->echoes = adj->value;
}

static void
update_echo_size(GtkAdjustment *adj, struct echo_params *params)
{
    params->echo_size = adj->value;
}

static void
toggle_echo_multichannel(void *bullshit, struct echo_params *params)
{
    params->multichannel = !params->multichannel;
}

static void
echo_init(struct effect *p)
{
    struct echo_params *params;

    GtkWidget      *decay;
    GtkWidget      *decay_label;
    GtkObject      *adj_decay;

    GtkWidget      *count;
    GtkWidget      *count_label;
    GtkObject      *adj_count;

    GtkWidget      *size;
    GtkWidget      *size_label;
    GtkObject      *adj_size;

    GtkWidget      *button, *mcbutton;
    GtkWidget      *parmTable;

    params = p->params;

    /*
     * GUI Init
     */
    p->control = gtk_window_new(GTK_WINDOW_DIALOG);

    gtk_signal_connect(GTK_OBJECT(p->control), "delete_event",
		       GTK_SIGNAL_FUNC(delete_event), p);

    parmTable = gtk_table_new(2, 8, FALSE);

    adj_decay = gtk_adjustment_new(params->echo_decay,
				   1.0, 80.0, 1.0, 10.0, 0.0);
    decay_label = gtk_label_new("Decay\n%");
    gtk_table_attach(GTK_TABLE(parmTable), decay_label, 0, 1, 0, 1,
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK),
		     __GTKATTACHOPTIONS(GTK_SHRINK), 0, 0);


    gtk_signal_connect(GTK_OBJECT(adj_decay), "value_changed",
		       GTK_SIGNAL_FUNC(update_echo_decay), params);

    decay = gtk_vscale_new(GTK_ADJUSTMENT(adj_decay));
    gtk_widget_set_size_request(GTK_WIDGET(decay),0,100);

    gtk_table_attach(GTK_TABLE(parmTable), decay, 0, 1, 1, 2,
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK),
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK), 0, 0);


    adj_count = gtk_adjustment_new(params->echoes,
				   1.0, MAX_ECHO_COUNT, 1.0, 1.0, 0.0);
    count_label = gtk_label_new("Voices\n#");
    gtk_table_attach(GTK_TABLE(parmTable), count_label, 1, 2, 0, 1,
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK),
		     __GTKATTACHOPTIONS(GTK_SHRINK), 0, 0);


    gtk_signal_connect(GTK_OBJECT(adj_count), "value_changed",
		       GTK_SIGNAL_FUNC(update_echo_count), params);

    count = gtk_vscale_new(GTK_ADJUSTMENT(adj_count));
    gtk_scale_set_digits(GTK_SCALE(count), 0);

    gtk_table_attach(GTK_TABLE(parmTable), count, 1, 2, 1, 2,
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_EXPAND | GTK_SHRINK),
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_EXPAND | GTK_SHRINK), 0, 0);

    adj_size = gtk_adjustment_new(params->echo_size, 1.0,
                                  MAX_ECHO_LENGTH, 1.0, 1.0, 0.0);
    size_label = gtk_label_new("Delay\nms");
    gtk_table_attach(GTK_TABLE(parmTable), size_label, 2, 3, 0, 1,
		     __GTKATTACHOPTIONS(GTK_FILL | GTK_EXPAND |
					GTK_SHRINK),
		     __GTKATTACHOPTIONS(GTK_SHRINK), 0, 0);


    gtk_signal_connect(GTK_OBJECT(adj_size), "value_changed",
		       GTK_SIGNAL_FUNC(update_echo_size), params);

    size = gtk_vscale_new(GTK_ADJUSTMENT(adj_size));

    gtk_table_attach(GTK_TABLE(parmTable), size, 2, 3, 1, 2,
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_EXPAND | GTK_SHRINK),
		     __GTKATTACHOPTIONS
		     (GTK_FILL | GTK_EXPAND | GTK_SHRINK), 0, 0);

    if (n_output_channels > 1 && n_input_channels == 1) {
        mcbutton = gtk_check_button_new_with_label("Multichannel");
        if (params->multichannel)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mcbutton), TRUE);
        gtk_signal_connect(GTK_OBJECT(mcbutton), "toggled",
                           GTK_SIGNAL_FUNC(toggle_echo_multichannel), params);

        gtk_table_attach(GTK_TABLE(parmTable), mcbutton, 1, 3, 2, 3,
                         __GTKATTACHOPTIONS(GTK_SHRINK),
                         __GTKATTACHOPTIONS(GTK_SHRINK), 0, 0);
    }
    
    button = gtk_check_button_new_with_label("On");
    if (p->toggle == 1)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    gtk_signal_connect(GTK_OBJECT(button), "toggled",
		       GTK_SIGNAL_FUNC(toggle_effect), p);

    gtk_table_attach(GTK_TABLE(parmTable), button, 0, 1, 2, 3,
		     __GTKATTACHOPTIONS(GTK_SHRINK),
		     __GTKATTACHOPTIONS(GTK_SHRINK), 0, 0);

    gtk_window_set_title(GTK_WINDOW(p->control), (gchar *) ("Echo"));
    gtk_container_add(GTK_CONTAINER(p->control), parmTable);

    gtk_widget_show_all(p->control);

}
    
static void
echo_filter(effect_t *p, data_block_t *db)
{
    struct echo_params *params = p->params;
    int voices = params->echoes;
    EchoTaps taps;
    DSP_SAMPLE *input = db->data;
    int frame, channel;

    if (voices < 0)
        voices = 0;
    if (voices > MAX_ECHO_COUNT)
        voices = MAX_ECHO_COUNT;
    taps = gnuitar_echo_taps(sample_rate, params->echo_size, params->echo_decay, voices);

    if (params->multichannel && db->channels == 1 && n_output_channels > 1) {
        DSP_SAMPLE *output = db->data_swap;
        for (frame = 0; frame < db->len; ++frame) {
            for (channel = 0; channel < n_output_channels; ++channel) {
                *output++ = gnuitar_echo_channel(params->history[0], params->history[0],
                                                 input[frame], &taps, channel, n_output_channels);
            }
        }
        db->channels = n_output_channels;
        db->len *= n_output_channels;
        db->data = db->data_swap;
        db->data_swap = input;
    } else {
        for (frame = 0; frame < db->len / db->channels; ++frame) {
            for (channel = 0; channel < db->channels; ++channel) {
                DSP_SAMPLE *sample = input + frame * db->channels + channel;
                *sample = gnuitar_echo_channel(params->history[channel], params->history[0],
                                               *sample, &taps, 0, 1);
            }
        }
    }
}

static void
echo_done(struct effect *p)
{
    struct echo_params *params;
    int             i, j;

    params = p->params;
    for (i = 0; i < MAX_ECHO_COUNT; i += 1) {
        for (j = 0; j < MAX_CHANNELS; j += 1) {
            del_Backbuf(params->history[j][i]);
        }
    }

    gtk_widget_destroy(p->control);
    free(params);
    free(p);
}

static void
echo_save(struct effect *p, SAVE_ARGS)
{
    struct echo_params *params = p->params;

    SAVE_DOUBLE("echo_size", params->echo_size);
    SAVE_DOUBLE("echo_decay", params->echo_decay);
    SAVE_INT("echoes", params->echoes);
    SAVE_INT("multichannel", params->multichannel);
}

static void
echo_load(struct effect *p, LOAD_ARGS)
{
    struct echo_params *params = p->params;

    LOAD_DOUBLE("echo_size", params->echo_size);
    LOAD_DOUBLE("echo_decay", params->echo_decay);
    LOAD_INT("echoes", params->echoes);
    LOAD_INT("multichannel", params->multichannel);
}

effect_t *
echo_create()
{
    effect_t       *p;
    struct echo_params *params;
    int             i, j;

    p = calloc(1, sizeof(effect_t));
    p->params = calloc(1, sizeof(*params));
    p->proc_init = echo_init;
    p->proc_filter = echo_filter;
    p->proc_save = echo_save;
    p->proc_load = echo_load;
    p->toggle = 0;
    p->proc_done = echo_done;

    params = p->params;
    params->multichannel = 0;
    params->echo_size = 200;
    params->echo_decay = 30.0;
    params->echoes = MAX_ECHO_COUNT;

    for (i = 0; i < MAX_ECHO_COUNT; i += 1) {
        for (j = 0; j < MAX_CHANNELS; j += 1) {
            params->history[j][i] = new_Backbuf(MAX_ECHO_LENGTH / 1000.0 * MAX_SAMPLE_RATE * gnuitar_echo_ratio(i));
        }
    }
    return p;
}
