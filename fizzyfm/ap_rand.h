/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Hugo Gilbert */
/* Fizzy-Fm's dice: a sound made from a number (the seed), a kind and how wild it may be.
 * Not every parameter thrown at random (that is noise, or silence, most of the time): a recipe for each kind of
 * sound (which tables suit it, how its envelopes go, what moves what, which effects follow), with the dice
 * choosing inside the recipe. CHAOS widens the recipe: odd tables, odd filters, warps, more modulation.
 * rand_mutate() is the other throw: the sound there is, moved a little.
 * The same seed, kind and chaos give the same sound (tests/run_tests.py "random" plays hundreds of them). */
#ifndef AP_RAND_H
#define AP_RAND_H

enum { RK_ANY, RK_BASS, RK_LEAD, RK_PAD, RK_PLUCK, RK_KEYS, RK_BELL, RK_WEIRD, NRK };
static const char *const RK_NAME[NRK] = {"ANY", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "BELL", "WEIRD"};

/* the tables a kind picks from */
static const uint8_t RW_BASS[] = {T_SAW, T_SQUARE, T_PWM, T_BASIC, T_FM_1_1, T_FM_1_2, T_FM_FB, T_GROWL, T_RESO, T_PD,
                                  T_DRIVE, T_FOLD, T_PULSE2, T_TRISAW, T_BUZZ, T_REED, T_SAW, T_SQUARE};
static const uint8_t RW_LEAD[] = {T_SAW, T_SQUARE, T_SYNC, T_SINSYNC, T_PWM, T_TRISAW, T_BRASS, T_REED, T_RESO, T_PD,
                                  T_FM_2_1, T_NASAL, T_BUZZ, T_FLUTE, T_FIN, T_PARABOL, T_SAW};
static const uint8_t RW_PAD[] = {T_GLASS, T_VOCAL, T_CHOIR, T_STRING, T_FORMANT, T_HARMO, T_SINES, T_ODDS, T_EVENS,
                                 T_FIFTHS, T_OCTAVES, T_HOLLOW, T_PHASES, T_BOWED, T_CELLO, T_PIPES, T_SAW, T_PWM,
                                 T_THROAT, T_TALK};
static const uint8_t RW_PLUCK[] = {T_HARMO, T_TINE, T_PLUCK, T_CLAV, T_COMB, T_DIGI, T_SQUARE, T_SAW, T_FM_1_2,
                                   T_MALLET, T_ODDEVEN, T_PD, T_STEPS};
static const uint8_t RW_KEYS[] = {T_TINE, T_PIANO, T_CLAV, T_ORGAN, T_FM_1_1, T_FM_1_3, T_MALLET, T_PIPES, T_PURE,
                                  T_FLUTE, T_REED};
static const uint8_t RW_BELL[] = {T_BELL, T_CHIME, T_GONG, T_GLASS, T_METAL, T_FM_3OP, T_FM_1_3, T_MALLET, T_TINE,
                                  T_SINES};
static const uint8_t RW_WEIRD[] = {T_BITS, T_METAL, T_GLITCH, T_8_BIT, T_GRIT, T_TALK, T_GROWL, T_DIGI, T_STEPS,
                                   T_IMPULSE, T_FOLD, T_PHASES, T_COMB, T_GONG};
static const struct {
    const uint8_t *w;
    uint8_t n;
} RW[NRK] = {{RW_PAD, sizeof RW_PAD}, {RW_BASS, sizeof RW_BASS}, {RW_LEAD, sizeof RW_LEAD}, {RW_PAD, sizeof RW_PAD},
             {RW_PLUCK, sizeof RW_PLUCK}, {RW_KEYS, sizeof RW_KEYS}, {RW_BELL, sizeof RW_BELL},
             {RW_WEIRD, sizeof RW_WEIRD}};

/* a name: a word of the first list, then one of the kind's (five letters each at most: eleven with the space) */
static const char *const RN_A[32] = {"NEON", "DUSK", "MOSS", "FIZZY", "SODA", "LUNAR", "AMBER", "IVORY", "RUSTY",
                                     "SOLAR", "MISTY", "FROST", "EMBER", "HAZY", "VIVID", "DEEP", "WARM", "COLD",
                                     "SOFT", "WILD", "PALE", "DARK", "TINY", "GIANT", "LAZY", "ACID", "MINT", "CORAL",
                                     "ONYX", "OPAL", "ZINC", "JADE"};
static const char *const RN_B[NRK][8] = {
    {"SOUND", "TONE", "THING", "WAVE", "PATCH", "SHAPE", "VOICE", "FIZZ"},
    {"BASS", "SUB", "GROWL", "THUMP", "ROOT", "BOOM", "WOMP", "LOW"},
    {"LEAD", "SOLO", "BEAM", "LASER", "HORN", "ARROW", "FLARE", "LINE"},
    {"PAD", "CLOUD", "MIST", "TIDE", "HALO", "DRIFT", "GLOW", "SKY"},
    {"PLUCK", "PICK", "DROP", "SPARK", "PING", "HARP", "BEAD", "DOT"},
    {"KEYS", "TINE", "ORGAN", "PIANO", "REED", "CLAV", "EP", "PIPE"},
    {"BELL", "CHIME", "GLASS", "GONG", "STAR", "ICE", "TING", "DEW"},
    {"BUG", "ALIEN", "NOISE", "GHOST", "CHAOS", "ROBOT", "ZAP", "VOID"}};

/* a kind's level (VOLUME), and what a sound of it that only plucks gets on top (Q7) */
static const uint8_t RK_VOL[NRK] = {90, 84, 89, 107, 95, 88, 72, 98};
static const uint8_t RK_SHORT[NRK] = {170, 238, 170, 170, 170, 122, 170, 212};

/* ---- the dice (xorshift32). (R_FN: these are called a few hundred times below; inlined, they are 25 KiB) */
#define R_FN static __attribute__((noinline))
static uint32_t rg, r_chaos, r_nslot, r_nfx;
R_FN uint32_t r_next(void)
{
    rg ^= rg << 13;
    rg ^= rg >> 17;
    rg ^= rg << 5;
    return rg;
}
static void r_seed(uint32_t seed)
{
    uint32_t i;
    rg = seed * 2654435761u + 12345u;
    if (!rg)
        rg = 1u;
    for (i = 0; i < 6u; i++)
        r_next();
}
R_FN uint32_t r_n(uint32_t n) { return ((r_next() >> 8) * n) >> 24; }          /* 0 .. n - 1 (n up to 255) */
R_FN uint8_t r_in(int32_t lo, int32_t hi) { return (uint8_t)(lo + (int32_t)r_n((uint32_t)(hi - lo + 1))); }
R_FN uint32_t r_pct(uint32_t p) { return r_n(100u) < p; }
/* p % of the time with CHAOS in the middle, never with none, twice as often with all of it */
R_FN uint32_t r_wild(uint32_t p) { return r_n(200u) < ((p * r_chaos) >> 5); }

R_FN void r_mod(patch_t *q, uint32_t src, uint32_t dst, int32_t amt)
{
    if (r_nslot >= 8u)
        return;
    q->p[P_X1_SRC + 3u * r_nslot] = (uint8_t)src;
    q->p[P_X1_DST + 3u * r_nslot] = (uint8_t)dst;
    q->p[P_X1_AMT + 3u * r_nslot] = (uint8_t)(100 + amt);
    r_nslot++;
}
R_FN void r_fx(patch_t *q, uint32_t type, uint32_t a, uint32_t b, uint32_t c)
{
    if (r_nfx >= NFX)
        return;
    q->p[P_FX1_TYPE + 4u * r_nfx] = (uint8_t)type;
    q->p[P_FX1_A + 4u * r_nfx] = (uint8_t)a;
    q->p[P_FX1_B + 4u * r_nfx] = (uint8_t)b;
    q->p[P_FX1_C + 4u * r_nfx] = (uint8_t)c;
    r_nfx++;
}
/* an effect as it starts when picked on the panel, each knob moved a little */
R_FN void r_fx_any(patch_t *q, uint32_t type)
{
    uint8_t v[3];
    uint32_t k;
    for (k = 0; k < 3u; k++) {
        int32_t x = (int32_t)FXDEF[type].def[k] + (int32_t)r_n(41u) - 20;
        v[k] = (uint8_t)(x < 0 ? 0 : x > 127 ? 127 : x);
    }
    r_fx(q, type, v[0], v[1], v[2]);
}
R_FN void r_env(patch_t *q, uint32_t e, int32_t a0, int32_t a1, int32_t d0, int32_t d1, int32_t s0, int32_t s1,
                  int32_t rl0, int32_t rl1)
{
    q->p[e] = r_in(a0, a1);
    q->p[e + 1u] = r_in(d0, d1);
    q->p[e + 2u] = r_in(s0, s1);
    q->p[e + 3u] = r_in(rl0, rl1);
}
R_FN void r_lfo(patch_t *q, uint32_t l, uint32_t shape, int32_t r0, int32_t r1, int32_t fade)
{
    q->p[l] = (uint8_t)shape;
    q->p[l + 1u] = r_in(r0, r1);
    q->p[l + 2u] = 0;
    q->p[l + 3u] = (uint8_t)fade;
}
/* an oscillator (o: P_A_WAVE or P_B_WAVE): a table of the kind's, anywhere in it, oct octaves from the key */
R_FN void r_osc(patch_t *q, uint32_t o, uint32_t kind, int32_t oct, uint32_t lvl, uint32_t uni_max)
{
    q->p[o] = r_wild(16u) ? (uint8_t)r_n(AP_NTAB) : RW[kind].w[r_n(RW[kind].n)];
    q->p[o + 1u] = r_in(0, 127);
    q->p[o + 2u] = W_OFF;
    q->p[o + 3u] = 0;
    q->p[o + 4u] = (uint8_t)(3 + oct);
    q->p[o + 5u] = 12;
    q->p[o + 6u] = 50;
    q->p[o + 7u] = (uint8_t)lvl;
    q->p[o + 8u] = (uint8_t)r_n(uni_max + 1u);
    q->p[o + 9u] = r_in(10, 44);
    q->p[o + 10u] = r_in(60, 127);
    q->p[o + 11u] = 64;
}
R_FN void r_name(patch_t *q, uint32_t kind)
{
    const char *a = RN_A[r_n(32u)], *b = RN_B[kind][r_n(8u)];
    uint32_t i = 0;
    while (*a && i < NAME_LEN - 1u)
        q->name[i++] = *a++;
    if (i < NAME_LEN - 1u)
        q->name[i++] = ' ';
    while (*b && i < NAME_LEN - 1u)
        q->name[i++] = *b++;
    while (i < NAME_LEN)
        q->name[i++] = 0;
}

/* the dice thrown: q becomes a sound of this kind (RK_ANY: of a kind the dice pick; the kind is returned).
 * The arpeggiator, the tempo and the bend range are the player's: they stay as they are in `from`. */
static uint32_t rand_patch(patch_t *q, const patch_t *from, uint32_t seed, uint32_t kind, uint32_t chaos)
{
    static const uint8_t ANY[] = {RK_BASS, RK_BASS, RK_BASS, RK_LEAD, RK_LEAD, RK_LEAD, RK_PAD, RK_PAD, RK_PAD,
                                  RK_PAD, RK_PLUCK, RK_PLUCK, RK_PLUCK, RK_KEYS, RK_KEYS, RK_BELL, RK_BELL, RK_WEIRD};
    static const uint8_t WARPS[] = {W_SYNC, W_BENDP, W_BENDM, W_PWM, W_MIRROR, W_FM, W_QUANT};
    static const uint8_t ECHO[] = {2, 4, 4, 6, 6, 7};              /* note lengths an echo takes (LS_DIV) */
    uint32_t i, hasb = 0, cut = 100, filt = FT_LP12, mono = 0;
    int32_t v;
    r_seed(seed);
    r_chaos = chaos > 127u ? 127u : chaos;
    r_nslot = r_nfx = 0;
    for (i = 0; i < NP; i++)
        q->p[i] = PDEF[i].def;
    for (i = P_ARP_MODE; i <= P_TEMPO; i++)
        q->p[i] = from->p[i];
    q->p[P_BEND] = from->p[P_BEND];
    if (kind == RK_ANY || kind >= NRK)
        kind = r_wild(10u) ? RK_WEIRD : ANY[r_n(sizeof ANY)];
    q->p[P_VELSENS] = r_in(60, 100);
    q->p[P_E3_A] = 0;
    q->p[P_E3_D] = r_in(50, 100);
    q->p[P_E3_S] = 0;
    q->p[P_E3_R] = r_in(40, 80);

    switch (kind) {
    case RK_BASS:
        r_osc(q, P_A_WAVE, kind, r_pct(30u) ? -2 : -1, 105, 1);
        if (r_pct(50u)) {
            hasb = 1;
            r_osc(q, P_B_WAVE, kind, q->p[P_A_OCT] - 3 + (int32_t)r_n(2u), r_in(40, 80), 1);
        }
        if (r_pct(70u)) {
            q->p[P_SUB_LVL] = r_in(50, 100);
            q->p[P_SUB_WAVE] = r_in(0, 2);
            q->p[P_SUB_OCT] = q->p[P_A_OCT] <= 1 ? 0 : (uint8_t)r_n(2u);
        }
        filt = r_pct(45u) ? FT_LP24 : r_pct(25u) ? FT_LPHOT : FT_LP12;
        cut = r_in(48, 84);
        q->p[P_F_RES] = r_in(0, 64);
        q->p[P_F_ENV] = r_in(80, 112);
        q->p[P_F_DRV] = r_pct(50u) ? r_in(10, 60) : 0;
        q->p[P_F_KEY] = r_in(20, 50);
        q->p[P_F_VEL] = r_in(20, 70);
        if (r_pct(40u))
            r_env(q, P_E1_A, 0, 2, 50, 74, 0, 40, 16, 40);       /* a plucked bass */
        else
            r_env(q, P_E1_A, 0, 4, 50, 80, 70, 127, 14, 36);
        r_env(q, P_E2_A, 0, 0, 36, 66, 0, 40, 20, 40);
        mono = r_pct(60u);
        if (r_pct(40u))
            r_fx(q, FX_DRIVE, r_in(20, 50), r_in(80, 110), 127);
        if (r_pct(30u))
            r_fx_any(q, FX_COMP);
        if (r_pct(20u))
            r_fx(q, FX_CHORUS, r_in(30, 50), r_in(40, 70), r_in(20, 40));
        if (r_pct(15u))
            r_fx(q, FX_DELAY, DIV(ECHO[r_n(sizeof ECHO)]), r_in(30, 50), r_in(14, 26));
        if (r_pct(15u))
            r_fx(q, FX_REVERB, r_in(60, 80), 60, r_in(12, 24));
        q->p[P_VSPREAD] = r_in(0, 20);
        break;
    case RK_LEAD:
        r_osc(q, P_A_WAVE, kind, r_pct(20u) ? 1 : 0, 100, 3);
        if (r_pct(60u)) {
            hasb = 1;
            r_osc(q, P_B_WAVE, kind, (int32_t)r_n(3u) - 1, r_in(40, 80), 2);
            q->p[P_B_FINE] = r_in(44, 56);
            if (r_wild(20u))
                q->p[P_B_SEMI] = 19;                                /* a fifth up */
        }
        filt = r_pct(25u) ? FT_LP24 : r_pct(15u) ? FT_LPHOT : FT_LP12;
        cut = r_in(76, 114);
        q->p[P_F_RES] = r_in(5, 45);
        q->p[P_F_ENV] = r_in(64, 100);
        q->p[P_F_KEY] = r_in(30, 70);
        r_env(q, P_E1_A, 0, 10, 60, 90, 80, 127, 30, 55);
        r_env(q, P_E2_A, 0, 6, 60, 90, 20, 70, 30, 60);
        mono = r_pct(55u);
        r_lfo(q, P_L1_SHAPE, 0, 80, 92, r_in(50, 80));         /* a vibrato that comes in late */
        r_mod(q, S_LFO1, D_FINE, r_in(12, 24));
        r_mod(q, S_AT, D_CUT, r_in(25, 45));
        if (r_pct(30u))
            r_fx(q, FX_DRIVE, r_in(16, 40), r_in(80, 110), 127);
        if (r_pct(25u))
            r_fx(q, FX_CHORUS, r_in(30, 50), r_in(40, 70), r_in(25, 45));
        if (r_pct(70u))
            r_fx(q, FX_DELAY, DIV(ECHO[r_n(sizeof ECHO)]), r_in(35, 60), r_in(25, 45));
        if (r_pct(60u))
            r_fx(q, FX_REVERB, r_in(70, 95), 60, r_in(20, 40));
        break;
    case RK_PAD:
        r_osc(q, P_A_WAVE, kind, 0, 95, 3);
        q->p[P_A_UNI] = r_in(1, 4);
        q->p[P_A_SPR] = r_in(90, 127);
        if (r_pct(85u)) {
            hasb = 1;
            r_osc(q, P_B_WAVE, kind, r_pct(30u) ? 1 : r_pct(15u) ? -1 : 0, r_in(60, 90), 2);
            q->p[P_B_FINE] = r_in(43, 57);
        }
        filt = r_pct(20u) ? FT_LP24 : r_pct(15u) ? FT_LP18 : FT_LP12;
        cut = r_in(66, 100);
        q->p[P_F_RES] = r_in(5, 35);
        q->p[P_F_ENV] = r_in(74, 100);
        q->p[P_F_KEY] = r_in(30, 60);
        r_env(q, P_E1_A, 50, 100, 80, 110, 90, 127, 70, 105);
        r_env(q, P_E2_A, 60, 100, 90, 110, 40, 90, 80, 100);
        r_lfo(q, P_L1_SHAPE, r_pct(50u) ? 5 : r_n(2u), 20, 50, r_in(0, 60));
        r_lfo(q, P_L2_SHAPE, r_n(2u), 16, 44, 0);
        r_mod(q, S_LFO1, D_APOS, r_in(25, 60));
        r_mod(q, S_LFO2, hasb && r_pct(60u) ? D_BPOS : r_pct(50u) ? D_CUT : D_PAN, r_in(20, 45));
        r_mod(q, S_AT, D_CUT, r_in(30, 50));
        if (r_pct(70u))
            r_mod(q, S_RND, D_PAN, r_in(40, 70));
        q->p[P_VSPREAD] = r_in(40, 80);
        if (r_wild(15u))
            r_fx_any(q, FX_PHASER);
        if (r_pct(70u)) {
            if (r_pct(35u))
                r_fx(q, FX_DIMENS, r_in(40, 60), r_in(50, 70), r_in(60, 90));
            else
                r_fx(q, FX_CHORUS, r_in(30, 50), r_in(50, 70), r_in(40, 60));
        }
        if (r_pct(40u))
            r_fx(q, FX_DELAY, DIV(ECHO[r_n(sizeof ECHO)]), r_in(40, 62), r_in(22, 36));
        if (r_pct(95u))
            r_fx(q, FX_REVERB, r_in(90, 115), r_in(50, 70), r_in(50, 80));
        break;
    case RK_PLUCK:
        r_osc(q, P_A_WAVE, kind, 0, 100, 2);
        if (r_pct(60u)) {
            hasb = 1;
            r_osc(q, P_B_WAVE, kind, r_pct(60u) ? 1 : 0, r_in(40, 75), 1);
        }
        filt = r_pct(30u) ? FT_LP24 : FT_LP12;
        cut = r_in(50, 72);
        q->p[P_F_RES] = r_in(10, 45);
        q->p[P_F_ENV] = r_in(94, 114);
        q->p[P_F_VEL] = r_in(50, 100);
        q->p[P_F_KEY] = r_in(40, 70);
        r_env(q, P_E1_A, 0, 0, 52, 76, 0, 20, 46, 70);
        r_env(q, P_E2_A, 0, 0, 40, 62, 0, 0, 40, 60);
        if (r_pct(50u))
            r_mod(q, S_ENV2, D_APOS, r_pct(50u) ? r_in(40, 70) : -(int32_t)r_in(40, 70));
        if (hasb)
            r_mod(q, S_VEL, D_BLVL, r_in(40, 70));
        q->p[P_VSPREAD] = r_in(30, 60);
        if (r_pct(25u))
            r_fx(q, FX_CHORUS, r_in(30, 50), r_in(40, 70), r_in(30, 50));
        if (r_pct(80u))
            r_fx(q, FX_DELAY, DIV(ECHO[r_n(sizeof ECHO)]), r_in(40, 62), r_in(30, 48));
        if (r_pct(70u))
            r_fx(q, FX_REVERB, r_in(70, 95), 60, r_in(30, 50));
        break;
    case RK_KEYS:
        r_osc(q, P_A_WAVE, kind, 0, 100, 1);
        q->p[P_A_DET] = r_in(8, 20);
        if (r_pct(50u)) {
            hasb = 1;
            r_osc(q, P_B_WAVE, kind, r_pct(70u) ? 1 : 0, r_in(30, 60), 0);
        }
        filt = r_pct(15u) ? FT_OFF : FT_LP12;
        cut = r_in(82, 114);
        q->p[P_F_RES] = r_in(0, 20);
        q->p[P_F_ENV] = r_in(64, 90);
        q->p[P_F_VEL] = r_in(30, 80);
        q->p[P_F_KEY] = r_in(30, 60);
        if (r_pct(25u))
            r_env(q, P_E1_A, 0, 3, 60, 90, 127, 127, 12, 24);    /* held like an organ */
        else
            r_env(q, P_E1_A, 0, 3, 82, 105, 0, 70, 40, 70);
        r_env(q, P_E2_A, 0, 0, 60, 90, 0, 50, 40, 70);
        if (r_pct(30u)) {
            r_lfo(q, P_L1_SHAPE, 0, 76, 90, 0);
            r_mod(q, S_LFO1, r_pct(50u) ? D_AMP : D_PAN, r_in(15, 35));
        }
        r_mod(q, S_VEL, D_APOS, r_in(20, 50));
        r_mod(q, S_AT, D_CUT, r_in(20, 40));
        q->p[P_VSPREAD] = r_in(20, 60);
        if (r_pct(15u))
            r_fx_any(q, FX_PHASER);
        if (r_pct(40u))
            r_fx(q, FX_CHORUS, r_in(30, 50), r_in(40, 70), r_in(30, 50));
        if (r_wild(15u))
            r_fx_any(q, FX_TREM);
        if (r_pct(25u))
            r_fx(q, FX_DELAY, DIV(ECHO[r_n(sizeof ECHO)]), r_in(30, 50), r_in(20, 34));
        if (r_pct(70u))
            r_fx(q, FX_REVERB, r_in(70, 95), 60, r_in(25, 45));
        break;
    case RK_BELL:
        r_osc(q, P_A_WAVE, kind, r_pct(25u) ? 1 : 0, 96, 1);
        if (r_pct(70u)) {
            hasb = 1;
            r_osc(q, P_B_WAVE, kind, r_pct(70u) ? 1 : 2, r_in(40, 70), 0);
            q->p[P_B_FINE] = r_in(44, 56);
        }
        if (r_wild(30u)) {
            q->p[P_A_WMODE] = W_FM;
            q->p[P_A_WARP] = r_in(10, 40);
        }
        filt = r_pct(20u) ? FT_OFF : FT_LP12;
        cut = r_in(96, 125);
        q->p[P_F_RES] = r_in(0, 15);
        q->p[P_F_KEY] = r_in(20, 50);
        r_env(q, P_E1_A, 0, 0, 88, 110, 0, 0, 85, 105);
        r_env(q, P_E2_A, 0, 0, 70, 95, 0, 0, 70, 95);
        r_mod(q, S_ENV2, D_APOS, r_pct(60u) ? -(int32_t)r_in(30, 60) : r_in(30, 60));
        r_mod(q, S_VEL, D_APOS, r_in(30, 50));
        r_mod(q, S_RND, D_PAN, r_in(50, 80));
        q->p[P_VSPREAD] = r_in(50, 80);
        if (r_pct(30u))
            r_fx(q, FX_CHORUS, r_in(30, 50), r_in(40, 70), r_in(30, 50));
        if (r_pct(60u))
            r_fx(q, FX_DELAY, DIV(ECHO[r_n(3u)]), r_in(45, 64), r_in(30, 46));
        if (r_pct(95u))
            r_fx(q, FX_REVERB, r_in(100, 118), r_in(50, 70), r_in(55, 85));
        break;
    default:                                    /* RK_WEIRD: little is held back */
        r_osc(q, P_A_WAVE, kind, (int32_t)r_n(3u) - 1, 100, 4);
        hasb = r_pct(80u);
        if (hasb) {
            r_osc(q, P_B_WAVE, kind, (int32_t)r_n(4u) - 2, r_in(50, 100), 3);
            q->p[P_B_SEMI] = r_pct(50u) ? 12 : r_in(0, 24);
            q->p[P_B_FINE] = r_in(30, 70);
        }
        filt = r_in(1, NFT - 1);
        cut = r_in(62, 112);
        q->p[P_F_RES] = r_in(0, 90);
        q->p[P_F_ENV] = r_in(54, 114);
        q->p[P_F_VAR] = r_in(0, 127);
        q->p[P_F_DRV] = r_pct(40u) ? r_in(10, 80) : 0;
        if (r_pct(50u))
            r_env(q, P_E1_A, 30, 100, 60, 110, 60, 127, 50, 110);
        else
            r_env(q, P_E1_A, 0, 6, 40, 100, 0, 127, 20, 90);
        r_env(q, P_E2_A, 0, 90, 40, 110, 0, 100, 30, 100);
        r_lfo(q, P_L1_SHAPE, r_n(6u), 10, 110, 0);
        r_lfo(q, P_L2_SHAPE, r_n(6u), 10, 110, 0);
        r_lfo(q, P_L3_SHAPE, r_n(6u), 10, 100, 0);
        q->p[P_L1_SYNC] = (uint8_t)r_pct(30u);
        q->p[P_L2_SYNC] = (uint8_t)r_pct(30u);
        if (r_pct(40u)) {
            q->p[P_NZ_LVL] = r_in(20, 70);
            q->p[P_NZ_TYPE] = r_in(0, 3);
        }
        for (i = 3u + r_n(3u); i; i--) {         /* three to five things moving */
            static const uint8_t SRC[] = {S_LFO1, S_LFO2, S_LFO3, S_ENV2, S_ENV3, S_RND, S_KEY, S_VEL};
            static const uint8_t DST[] = {D_APOS, D_AWARP, D_BPOS, D_BWARP, D_CUT, D_RES, D_PAN, D_NOISE, D_L1RATE,
                                          D_ALVL, D_BLVL};
            v = r_in(20, 80);
            r_mod(q, SRC[r_n(sizeof SRC)], DST[r_n(sizeof DST)], r_pct(50u) ? v : -v);
        }
        if (r_pct(30u))
            r_mod(q, r_pct(50u) ? S_LFO3 : S_ENV3, D_BPITCH, r_in(4, 30));
        q->p[P_VSPREAD] = r_in(0, 100);
        {
            static const uint8_t PRE[] = {FX_DRIVE, FX_FOLD, FX_CRUSH, FX_FILTER, FX_PHASER, FX_FLANGER, FX_CHORUS,
                                          FX_DIMENS, FX_TREM};
            uint32_t a = r_n(sizeof PRE), b = r_n(sizeof PRE);
            if (b < a) {
                uint32_t t = a;
                a = b;
                b = t;
            }
            r_fx_any(q, PRE[a]);
            if (b != a && r_pct(60u))
                r_fx_any(q, PRE[b]);
        }
        if (r_pct(60u))
            r_fx(q, FX_DELAY, DIV(r_in(2, 9)), r_in(30, 80), r_in(20, 60));
        if (r_pct(60u))
            r_fx(q, FX_REVERB, r_in(60, 120), r_in(30, 90), r_in(20, 70));
        mono = r_pct(25u);
        break;
    }

    /* what CHAOS adds to any kind: a warp, an odd filter, a wave that moves */
    if (kind != RK_WEIRD) {
        if (r_wild(kind == RK_LEAD ? 40u : 18u) && q->p[P_A_WMODE] == W_OFF) {
            q->p[P_A_WMODE] = WARPS[r_n(sizeof WARPS)];
            if (q->p[P_A_WMODE] == W_FM && !hasb)
                q->p[P_A_WMODE] = W_SYNC;
            q->p[P_A_WARP] = r_in(15, 70);
            r_mod(q, S_ENV2, D_AWARP, r_in(20, 60));
        }
        if (r_wild(12u) && filt != FT_OFF) {
            static const uint8_t ODD[] = {FT_NOTCH, FT_NOTCH2, FT_PEAK, FT_BANDW, FT_TWIN, FT_VOWEL, FT_COMBP, FT_COMBM,
                                          FT_HP12, FT_BP24};
            filt = ODD[r_n(sizeof ODD)];
            q->p[P_F_VAR] = r_in(0, 127);
        }
        if (r_wild(20u))
            r_mod(q, S_LFO3, hasb ? D_BPOS : D_APOS, r_in(20, 50));
        if (r_wild(8u)) {
            q->p[P_NZ_LVL] = r_in(10, 40);
            q->p[P_NZ_TYPE] = r_in(0, 2);
        }
    } else if (r_pct(80u)) {
        q->p[P_A_WMODE] = WARPS[r_n(sizeof WARPS)];
        if (q->p[P_A_WMODE] == W_FM && !hasb)
            q->p[P_A_WMODE] = W_BENDP;
        q->p[P_A_WARP] = r_in(20, 110);
        if (hasb && r_pct(50u)) {
            q->p[P_B_WMODE] = WARPS[r_n(sizeof WARPS - 2u)];
            q->p[P_B_WARP] = r_in(20, 110);
        }
    }

    /* a filter that is not a low-pass sits where the notes are, and follows the keys: a band-pass far above
     * the note leaves nothing, a high-pass there neither */
    if (filt >= FT_HP6 && filt <= FT_HP24) {
        q->p[P_F_KEY] = r_in(40, 80);
        cut = r_in(24, 56);
    } else if (filt >= FT_BP12 && filt <= FT_VOWEL) {
        q->p[P_F_KEY] = r_in(60, 100);
        cut = r_in(52, 80);
        if (q->p[P_F_ENV] > 90)
            q->p[P_F_ENV] = 90;
    }

    /* the voice */
    if (mono) {
        q->p[P_MODE] = r_pct(50u) ? MODE_LEGATO : MODE_MONO;
        q->p[P_GLIDE] = r_pct(60u) ? r_in(20, 50) : 0;
    }

    /* the four macros (the knobs of the first page) and the wheel: always something under the hand.
     * MACRO 1 opens and closes the filter around where the dice put it: it starts in the middle */
    q->p[P_F_TYPE] = (uint8_t)filt;
    if (filt != FT_OFF) {
        q->p[P_M1] = 64;
        r_mod(q, S_M1, D_CUT, 60);
        cut = cut > 19u ? cut - 19u : 0u;
    } else {
        r_mod(q, S_M1, D_AMP, -40);
    }
    q->p[P_F_CUT] = (uint8_t)(cut > 127u ? 127u : cut);
    cut += filt != FT_OFF ? 19u : 0u;           /* (where the filter is with the macro in the middle) */
    r_mod(q, S_M2, D_APOS, q->p[P_A_POS] > 64 ? -60 : 60);
    r_mod(q, S_M3, q->p[P_A_WMODE] != W_OFF ? D_AWARP : filt != FT_OFF ? D_RES : D_PAN, 50);
    if (hasb)
        r_mod(q, S_M4, D_BLVL, q->p[P_B_LVL] > 64 ? -70 : 70);
    r_mod(q, S_WHEEL, filt != FT_OFF && kind != RK_LEAD ? D_CUT : D_APOS, 40);

    /* how loud. The dice cannot listen, so the level is worked out from what was thrown: the kind's own level, then
     * what was measured to matter on a few hundred sounds of each kind (tests/run_tests.py "random" plays them and
     * says how far apart they are): a sound that only plucks, a filter that keeps a band or the top, one voice at
     * a time, a second oscillator, a sub, a driven filter, a slow attack, a low-pass nearly shut. Q7 factors. */
    v = RK_VOL[kind];
    if (q->p[P_E1_S] < 30)
        v = (v * RK_SHORT[kind]) >> 7;
    if (filt == FT_BP12 || filt == FT_BP24 || filt == FT_BANDW)
        v = (v * 250) >> 7;
    else if (filt >= FT_HP6 && filt <= FT_HP24)
        v = (v * 186) >> 7;
    else if (filt >= FT_LP6 && filt <= FT_LPHOT && cut < 71u)
        v = (v * (128 + (71 - (int32_t)cut) / 2)) >> 7;
    if (mono)
        v = (v * 145) >> 7;
    if (hasb)
        v = (v * 113) >> 7;
    v = (v * (128 - ((int32_t)q->p[P_SUB_LVL] * 3) / 8)) >> 7;
    v = (v * (128 - ((int32_t)q->p[P_F_DRV] * 5) / 8)) >> 7;
    v = (v * (128 - (int32_t)q->p[P_E1_A] / 4)) >> 7;
    q->p[P_VOL] = (uint8_t)(v < 40 ? 40 : v > 127 ? 127 : v);
    r_name(q, kind);
    return kind;
}

/* the other throw: the sound there is, moved a little (more with more CHAOS). What gives a sound its place stays:
 * its tables (but for a rare change of one), its octaves, its kind of filter, its effects, what moves what. */
static void rand_mutate(patch_t *q, uint32_t seed, uint32_t chaos)
{
    uint32_t i;
    r_seed(seed);
    r_chaos = chaos > 127u ? 127u : chaos;
    for (i = P_A_WAVE; i < NP; i++) {
        const pdef_t *d = &PDEF[i];
        int32_t span, x;
        uint32_t t = 0;
        if (d->fmt == F_LIST || !d->max || (i >= P_ARP_MODE && i <= P_RES1) || i == P_VOL || i == P_BEND ||
            i == P_VELSENS || i == P_GLIDE)
            continue;
        if (i == P_A_OCT || i == P_A_SEMI || i == P_B_OCT || i == P_B_SEMI || i == P_SUB_OCT || i == P_A_UNI ||
            i == P_B_UNI)
            continue;
        t = i - P_FX1_TYPE;
        if (i >= P_FX1_TYPE && t < 4u * NFX && !q->p[i - (t & 3u)])   /* a knob of an empty slot */
            continue;
        if (i >= P_X1_SRC && i < P_X1_SRC + 24u && !q->p[i - 2u])    /* the amount of a slot nothing is in */
            continue;
        if (!r_pct(40u))
            continue;
        span = 2 + ((int32_t)d->max * (10 + (int32_t)r_chaos / 3)) / 160;     /* 7 % .. 33 % of its range */
        if (i == P_A_FINE || i == P_B_FINE)
            span = 3;
        x = (int32_t)q->p[i] + (int32_t)r_n((uint32_t)(2 * span + 1)) - span;
        q->p[i] = (uint8_t)(x < 0 ? 0 : x > d->max ? d->max : x);
    }
    if (r_wild(10u))
        q->p[r_pct(50u) ? P_A_WAVE : P_B_WAVE] = (uint8_t)r_n(AP_NTAB);
}

#endif
