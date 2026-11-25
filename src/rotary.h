/*
 * GNUitar
 * Rotary effect
 * Copyright (C) 2006 Antti S. Lankila  <alankila@bel.fi>
 *
 * $Id$
 */

#ifndef _ROTARY_H_
#define _ROTARY_H_ 1

#include "effect.h"
#include "dsp/spatial_dsp.h"

effect_t *rotary_create(void);

struct rotary_params {
    int         speed;
    RotaryDSP   dsp;
};

#endif
