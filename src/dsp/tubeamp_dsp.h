/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2006 Antti S. Lankila <alankila@bel.fi>
 */

#ifndef GNUITAR_TUBEAMP_DSP_H
#define GNUITAR_TUBEAMP_DSP_H

#include "../biquad.h"
#include "tubeamp_transfer.h"

#include <math.h>
#include <string.h>

#define GNUITAR_TUBE_STAGES 4
#define GNUITAR_TUBE_MODELS 2
#define GNUITAR_TUBE_QUALITIES 4
#define GNUITAR_TUBE_OVERSAMPLE 6
#define GNUITAR_TUBE_SOURCE_RATE 44100.0
#define GNUITAR_TUBE_SOURCE_TAPS 512
#define GNUITAR_TUBE_CAPACITY 8192
#define GNUITAR_TUBE_MIN_RATE 8000.0
#define GNUITAR_TUBE_MAX_RATE 384000.0

/* Marshall G15R */
static const DSP_SAMPLE __attribute__((aligned(16))) impulse_g15r[GNUITAR_TUBE_SOURCE_TAPS] = {
  4405,  17364,  30475,  33517,  28810,  20846,   9309,  -4045, -13421, -13737,  -6939,
  -644,   2381,   4726,   4890,   -577,  -8708, -13224, -11835,  -6840,   -805,   5847,
 11158,  10895,   3963,  -5524, -11923, -13616, -12717,  -9577,  -4247,   -180,    568,
  -647,  -3434,  -7154,  -9508,  -9045,  -5524,   -436,   3582,   4809,   2857,   -600,
 -3441,  -5228,  -5474,  -3608,   -936,    568,    785,    961,   1268,    763,   -657,
 -1941,  -2769,  -3712,  -4174,  -3405,  -2362,  -2122,  -2521,  -2793,  -2499,  -1808,
 -1174,  -1051,  -1339,  -1579,  -1982,  -2796,  -3538,  -3834,  -3492,  -2315,   -813,
   461,   1832,   3235,   3535,   1981,   -688,  -2823,  -3401,  -2683,  -1290,    290,
  1276,    924,   -613,  -2299,  -3054,  -2582,  -1428,   -275,    411,    317,   -452,
 -1440,  -2337,  -2950,  -3028,  -2519,  -1646,   -584,    580,   1663,   2431,   2623,
  2135,   1272,    563,    325,    470,    861,   1568,   2341,   2490,   1838,    954,
   227,   -215,   -167,    182,    305,    140,    -70,   -334,   -565,   -404,    123,
   592,    852,   1030,   1158,   1222,   1264,   1225,    932,    389,   -103,   -322,
  -347,   -283,    -81,    280,    683,   1030,   1335,   1549,   1539,   1323,   1070,
   928,   1095,   1687,   2403,   2755,   2615,   2172,   1513,    736,    167,    -33,
   -77,    -72,     70,    261,    311,    206,     41,   -127,   -177,     79,    635,
  1166,   1339,   1139,    812,    641,    707,    913,   1170,   1414,   1597,   1697,
  1667,   1492,   1250,    993,    687,    328,      7,   -170,   -125,    118,    346,
   425,    483,    579,    577,    465,    405,    358,    138,   -141,   -242,   -189,
  -124,    -41,    136,    373,    533,    603,    652,    682,    710,    728,    673,
   560,    468,    378,    222,     39,    -20,    117,    391,    740,   1171,   1582,
  1737,   1543,   1113,    601,    108,   -273,   -479,   -516,   -482,   -554,   -802,
 -1038,  -1068,   -905,   -618,   -236,    137,    338,    388,    463,    582,    567,
   325,     52,    -29,     22,    141,    347,    481,    382,    206,    121,     38,
  -141,   -394,   -684,   -914,   -923,   -729,   -536,   -457,   -459,   -499,   -510,
  -412,   -254,   -135,   -115,   -243,   -509,   -723,   -695,   -588,   -668,   -874,
 -1000,   -994,   -927,   -852,   -743,   -587,   -456,   -436,   -472,   -439,   -281,
   -41,    140,    152,     17,   -204,   -454,   -649,   -740,   -707,   -573,   -403,
  -268,   -229,   -307,   -460,   -685,  -1015,  -1384,  -1657,  -1755,  -1664,  -1434,
 -1150,   -859,   -555,   -255,    -60,    -17,    -32,    -22,     -2,    -20,    -83,
  -169,   -288,   -388,   -351,   -173,     47,    273,    483,    573,    444,     86,
  -348,   -621,   -701,   -706,   -631,   -489,   -438,   -519,   -577,   -562,   -513,
  -373,   -123,    113,    269,    380,    438,    421,    343,    202,     -1,   -229,
  -401,   -450,   -374,   -220,    -65,     23,     47,     17,    -60,   -135,   -151,
   -83,     72,    264,    398,    415,    295,     73,   -134,   -237,   -268,   -307,
  -370,   -414,   -411,   -308,    -64,    233,    424,    422,    267,     72,    -23,
    63,    282,    545,    824,   1069,   1165,   1098,   1012,    931,    752,    577,
   612,    765,    831,    816,    783,    684,    537,    484,    564,    671,    759,
   843,    860,    742,    569,    459,    432,    459,    541,    680,    798,    812,
   741,    611,    453,    352,    382,    527,    728,    923,   1016,    928,    707,
   491,    393,    467,    670,    900,   1073,   1115,   1000,    778,    577,    520,
   562,    574,    536,    494,    407,    222,     15,    -98,   -111,    -97,    -68,
     6,     73,    109,    224,    442,    592,    535,    306,     37,   -127,   -114,
    23,    183,    268,    193,     30,    -61,    -74,    -46,     45,    156,    201,
   217,    275,    317,    268,    146,    -16,   -186,   -257,   -148,     74,    275,
   372,    338,    207,     53,    -63,   -131,   -187,   -265,   -372,   -482,   -551,
  -587,   -642,   -689,   -662,   -577,   -485
};

/* Princeton II */
static const DSP_SAMPLE __attribute__((aligned(16))) impulse_princeton2[GNUITAR_TUBE_SOURCE_TAPS] = {
  2799,  11631,  23881,  32811,  34786,  30693,  22401,  12097,   3608,    333,   1986,
  5050,   5906,   3149,  -2263,  -7957, -11151,  -9808,  -4421,   1179,   2345,  -1974,
 -8064, -11426, -10826,  -7845,  -4476,  -2085,  -1307,  -1743,  -2306,  -2291,  -1539,
  -317,    118,  -1496,  -4272,  -5880,  -5257,  -2844,    410,   2743,   1949,  -2161,
 -6821,  -9053,  -8010,  -4596,   -871,    603,  -1322,  -5235,  -8392,  -8852,  -6539,
 -2851,    514,   2273,   1827,   -491,  -3243,  -4689,  -4452,  -3242,  -1617,   -302,
  -339,  -1949,  -3846,  -4448,  -3687,  -3003,  -3510,  -4938,  -6335,  -7114,  -7239,
 -6769,  -5656,  -4182,  -2937,  -2249,  -1904,  -1544,  -1072,   -366,    566,   1098,
   532,  -1043,  -2570,  -2751,  -1216,   1249,   3294,   3731,   2278,   -206,  -2371,
 -3195,  -2402,   -639,    959,   1533,    852,   -597,  -1887,  -2102,   -820,   1549,
  3846,   4837,   4143,   2398,    624,   -298,    -81,    793,   1552,   1605,    889,
   -26,   -445,   -138,    631,   1455,   1937,   1740,    745,   -735,  -2002,  -2435,
 -1966,  -1092,   -495,   -539,  -1109,  -1708,  -1762,  -1079,     21,    935,   1103,
   489,   -400,   -960,   -718,    427,   2035,   3310,   3636,   3115,   2310,   1690,
  1475,   1623,   1770,   1452,    548,   -577,  -1348,  -1424,   -922,   -242,    234,
   357,    244,     66,      9,    208,    450,    376,    -69,   -688,  -1261,  -1605,
 -1569,  -1160,   -563,    -59,    110,    -19,   -277,   -570,   -774,   -798,   -733,
  -693,   -715,   -705,   -525,   -217,     69,    263,    338,    298,    199,     98,
    28,     83,    344,    774,   1293,   1772,   2010,   1960,   1771,   1555,   1321,
  1015,    609,    246,     59,    -23,    -17,    164,    447,    666,    749,    681,
   472,    145,   -206,   -444,   -476,   -389,   -386,   -535,   -683,   -692,   -618,
  -550,   -459,   -335,   -222,   -121,    -20,     42,     19,   -109,   -284,   -432,
  -588,   -803,   -925,   -743,   -296,    158,    419,    486,    393,    140,   -150,
  -305,   -249,    -36,    209,    377,    353,    143,    -27,     72,    414,    789,
  1050,   1150,   1066,    861,    697,    716,    850,    914,    803,    592,    463,
   502,    652,    831,    933,    826,    398,   -277,   -895,  -1136,   -937,   -460,
    54,    374,    374,    147,    -70,   -119,    -51,     13,     48,     69,     99,
   171,    305,    515,    759,    919,    904,    753,    606,    589,    731,    945,
  1125,   1182,   1069,    824,    600,    546,    682,    890,   1042,   1070,    917,
   588,    228,     58,    181,    439,    561,    488,    369,    277,    186,    150,
   218,    306,    309,    228,    161,    210,    355,    474,    526,    538,    479,
   352,    292,    356,    429,    432,    369,    220,     33,     -9,    149,    329,
   374,    347,    353,    356,    312,    369,    588,    735,    650,    497,    459,
   465,    419,    401,    447,    408,    218,     75,    122,    298,    550,    802,
   914,    793,    468,    134,     48,    265,    538,    603,    401,    -11,   -499,
  -805,   -752,   -451,   -167,   -106,   -314,   -666,   -922,   -896,   -621,   -313,
  -169,   -249,   -522,   -850,   -981,   -774,   -374,    -45,     49,    -87,   -320,
  -443,   -281,    207,    831,   1269,   1309,    999,    586,    307,    276,    451,
   608,    527,    265,     86,    145,    403,    710,    869,    762,    456,    139,
   -46,    -62,     53,    189,    214,     73,   -149,   -301,   -285,   -111,    104,
   202,    101,    -96,   -195,   -102,    106,    263,    252,     62,   -223,   -442,
  -446,   -214,    166,    549,    811,    893,    791,    564,    360,    344,    506,
   676,    744,    719,    635,    511,    382,    289,    245,    207,    124,     35,
     6,     -1,    -50,    -83,    -24,    115,    247,    331,    411,    479,    436,
   235,    -15,   -148,   -130,    -84,    -98,   -135,   -147,   -160,   -220,   -304,
  -396,   -515,   -664,   -772,   -733,   -528,   -267,    -96,   -105,   -307,   -610,
  -870,   -939,   -708,   -263,    109,    199
};


static const DSP_SAMPLE *const gnuitar_tube_sources[] = {
    impulse_g15r, impulse_princeton2
};
static const int gnuitar_tube_lengths[] = {64, 128, 256, 512};

typedef struct {
    Biquad_t highpass[GNUITAR_TUBE_STAGES], lowpass[GNUITAR_TUBE_STAGES];
    Biquad_t biaslowpass[GNUITAR_TUBE_STAGES], bass, middle, treble, decimation;
    float bias[MAX_CHANNELS][GNUITAR_TUBE_STAGES], input[MAX_CHANNELS];
    DSP_SAMPLE cabinet[GNUITAR_TUBE_MODELS][GNUITAR_TUBE_QUALITIES][GNUITAR_TUBE_CAPACITY];
    DSP_SAMPLE history[MAX_CHANNELS][2 * GNUITAR_TUBE_CAPACITY];
    unsigned int cursor[MAX_CHANNELS];
    int lengths[GNUITAR_TUBE_QUALITIES];
    double rate;
    float tone_bass, tone_middle, tone_treble;
} TubeAmpDSP;

typedef struct {
    int stages, model, quality;
    float gain, biasfactor, asymmetry;
} TubeAmpControls;

static inline void
gnuitar_tube_resample(DSP_SAMPLE *output, int length, const DSP_SAMPLE *source,
                      int source_length, double rate)
{
    double step = GNUITAR_TUBE_SOURCE_RATE / rate;
    double cutoff = fmin(1.0, 1.0 / step);
    const double pi = 3.14159265358979323846;
    int i;

    if (rate == GNUITAR_TUBE_SOURCE_RATE) {
        memcpy(output, source, (size_t) source_length * sizeof(*output));
        return;
    }

    /* Resample the truncated IR with a Hann-windowed low-pass sinc.
     * Scaling by step preserves the continuous-time convolution gain. */
    for (i = 0; i < length; ++i) {
        double position = i * step;
        double sum = 0.0;
        double radius = 16.0 / cutoff;
        int first = (int) ceil(position - radius);
        int last = (int) floor(position + radius);
        int tap;

        if (first < 0)
            first = 0;
        if (last >= source_length)
            last = source_length - 1;
        for (tap = first; tap <= last; ++tap) {
            double offset = position - tap;
            double angle = pi * offset;
            double sinc = offset == 0.0 ? cutoff : sin(cutoff * angle) / angle;
            double window = 0.5 + 0.5 * cos(pi * offset / radius);
            sum += source[tap] * sinc * window;
        }
        output[i] = (DSP_SAMPLE) (sum * step);
    }
}

static inline void
gnuitar_tube_prepare(TubeAmpDSP *state, double rate)
{
    int stage, quality, model;
    static const int bias_cutoff[] = {86, 132, 194, 250};
    double oversampled = rate * GNUITAR_TUBE_OVERSAMPLE;

    state->rate = rate;
    for (stage = 0; stage < GNUITAR_TUBE_STAGES; ++stage) {
        double cutoff = stage ? 6531.0 : 22570.0;
        set_chebyshev1_biquad(oversampled, cutoff, 0.0, 1, &state->lowpass[stage]);
        set_rc_lowpass_biquad(oversampled, bias_cutoff[stage], &state->biaslowpass[stage]);
        set_rc_highpass_biquad(oversampled, 37, &state->highpass[stage]);
    }
    set_chebyshev1_biquad(oversampled, 12000, 10.0, 1, &state->decimation);
    for (quality = 0; quality < GNUITAR_TUBE_QUALITIES; ++quality) {
        int original = gnuitar_tube_lengths[quality];
        int count = (int) ceil(original * rate / GNUITAR_TUBE_SOURCE_RATE);
        state->lengths[quality] = count;
        for (model = 0; model < GNUITAR_TUBE_MODELS; ++model)
            gnuitar_tube_resample(state->cabinet[model][quality], count,
                                  gnuitar_tube_sources[model], original, rate);
    }
}

static inline void
gnuitar_tube_reset(TubeAmpDSP *state)
{
    int stage;

    for (stage = 0; stage < GNUITAR_TUBE_STAGES; ++stage) {
        memset(state->highpass[stage].mem, 0, sizeof(state->highpass[stage].mem));
        memset(state->lowpass[stage].mem, 0, sizeof(state->lowpass[stage].mem));
        memset(state->biaslowpass[stage].mem, 0, sizeof(state->biaslowpass[stage].mem));
    }
    memset(state->decimation.mem, 0, sizeof(state->decimation.mem));
    memset(state->bass.mem, 0, sizeof(state->bass.mem));
    memset(state->middle.mem, 0, sizeof(state->middle.mem));
    memset(state->treble.mem, 0, sizeof(state->treble.mem));
    memset(state->bias, 0, sizeof(state->bias));
    memset(state->input, 0, sizeof(state->input));
    memset(state->history, 0, sizeof(state->history));
    memset(state->cursor, 0, sizeof(state->cursor));
    state->tone_bass = state->tone_middle = state->tone_treble = 999.0f;
}

static inline void
gnuitar_tube_tone(TubeAmpDSP *state, float bass, float middle, float treble)
{
    double rate = state->rate * GNUITAR_TUBE_OVERSAMPLE;

    if (state->tone_bass != bass) {
        set_lsh_biquad(rate, 500, bass, &state->bass);
        state->tone_bass = bass;
    }
    if (state->tone_middle != middle) {
        set_peq_biquad(rate, 650, 500.0, middle, &state->middle);
        state->tone_middle = middle;
    }
    if (state->tone_treble != treble) {
        set_hsh_biquad(rate, 800, treble, &state->treble);
        state->tone_treble = treble;
    }
}

static inline float
gnuitar_tube_shape(float input, float resistance)
{
    float position = input / resistance * 64.0f + 8192.0f;
    int index;

    position = fminf(16382.0f, fmaxf(0.0f, position));
    index = (int) position;
    position -= index;
    return (gnuitar_tube_response[index] * (1.0f - position) +
            gnuitar_tube_response[index + 1] * position) * resistance;
}

static inline DSP_SAMPLE
gnuitar_tube_sample(TubeAmpDSP *state, DSP_SAMPLE input, int channel, const TubeAmpControls *controls)
{
    static const float resistance[] = {68e3 / 3000, 250e3 / 3000, 250e3 / 3000, 250e3 / 3000};
    static const float bias_gain[] = {2700 / 100000.0, 1500 / 100000.0, 820 / 1000000.0, 820 / 100000.0};
    DSP_SAMPLE *history = state->history[channel] + state->cursor[channel];
    float result = 0.0f;
    int sub, stage;

    for (sub = 0; sub < GNUITAR_TUBE_OVERSAMPLE; ++sub) {
        state->input[channel] = (input + state->input[channel] * (GNUITAR_TUBE_OVERSAMPLE - 1.0f)) /
                                GNUITAR_TUBE_OVERSAMPLE;
        result = state->input[channel] / (float) MAX_SAMPLE;
        for (stage = 0; stage < controls->stages; ++stage) {
            float *bias = &state->bias[channel][stage];
            result = do_biquad(result * controls->gain, &state->lowpass[stage], channel);
            result = gnuitar_tube_shape(*bias - result, resistance[stage]);
            *bias = do_biquad((controls->asymmetry - controls->biasfactor * result) * bias_gain[stage],
                              &state->biaslowpass[stage], channel);
            result = do_biquad(result, &state->highpass[stage], channel);
            if (stage == 1) {
                result = do_biquad(result, &state->bass, channel);
                result = do_biquad(result, &state->middle, channel);
                result = do_biquad(result, &state->treble, channel);
            }
        }
        result = do_biquad(result, &state->decimation, channel);
    }

    history[GNUITAR_TUBE_CAPACITY] = history[0] = result / 500.f * (float) (MAX_SAMPLE >> 13);
    state->cursor[channel] = (state->cursor[channel] - 1) & (GNUITAR_TUBE_CAPACITY - 1);
    return (DSP_SAMPLE) (convolve(state->cabinet[controls->model][controls->quality], history,
                                   state->lengths[controls->quality]) / 32.f);
}

#endif
