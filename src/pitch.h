/*
 * GNUitar
 * Pitch Shifter effect
 * Copyright (c) 2005 Antti S. Lankila  <alankila@bel.fi>
 *
 * See COPYING for details about license.
 *
 * $Id$
 */

#ifndef _PITCH_H_
#define _PITCH_H_ 1

#include "effect.h"
#include "dsp/pitch_dsp.h"

effect_t *   pitch_create(void);

struct pitch_params {
    PitchDSP *dsp;
    unsigned int rate, channels;

    /* user tunables */
    int             halfnote;
    double          finetune, drywet;
};

#endif
