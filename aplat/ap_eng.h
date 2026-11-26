/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Hugo Gilbert */
/* APLAT's engine: eight notes, each with two wavetable oscillators (up to five unison voices each), a sub,
 * noise, a filter of its own, three envelopes and two LFOs of its own, and a modulation matrix worked out per note.
 * Integers only (docs/RULES.md of fm1-dock): Q15 samples, 32-bit phases, 64-bit products but no 64-bit division.
 *
 * render() owns the voices. The main loop talks to it through the event queue (notes) and through single bytes
 * and words (the sound's parameters, the wheel ...), each written whole. */
#ifndef AP_ENG_H
#define AP_ENG_H

#define NV 8u                                   /* notes */
#define MAXU 5u                                 /* unison voices per oscillator */
#define SUBN AP_SUB                             /* frames per control tick (32: 1378 times a second) */
/* What the notes of a sound may cost together, in instructions a frame as counted on a stand-in processor
 * (tools/load/count.py): about 40 % of the FM-1's time at fm1-emu's rate, the effects and the way out on top.
 * A sound with more unison, a stereo or a 24 dB filter gets fewer notes at once. Measure on the unit (the bar on
 * the PLAY page) before raising it. */
#define COST_BUDGET 1900u                       /* (with 350 more for the effects: what they take comes off it) */

static patch_t cur;                             /* the sound being played and edited */
#define PV(id) ((int32_t)cur.p[P_##id])

/* ---- the wavetables in RAM: 3 buffers (oscillator A's, B's and one being built), 8 frames each, every frame
 * at 12 levels, half an octave apart, of fewer harmonics for higher notes (none ever passes half the sample rate);
 * each level has one more sample, a copy of its first */
#define WT_FRM 2060u
#define WT_LEVELS 12u
static const uint16_t WT_OFF[WT_LEVELS] = {0, 513, 1026, 1283, 1540, 1669, 1798, 1863, 1928, 1961, 1994, 2027};
static const uint16_t WT_LEN[WT_LEVELS] = {512, 512, 256, 256, 128, 128, 64, 64, 32, 32, 32, 32};
static const uint8_t WT_NH[WT_LEVELS] = {64, 45, 32, 22, 16, 11, 8, 5, 4, 3, 2, 1};
static const uint8_t WT_SH[WT_LEVELS] = {23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 27, 27};
/* the highest phase step a level may play at: 2^31 / its harmonics */
static const uint32_t WT_LIM[WT_LEVELS] = {33554432u, 47721858u, 67108864u, 97612893u, 134217728u, 195225786u,
                                           268435456u, 429496729u, 536870912u, 715827882u, 1073741824u, 2147483648u};
static int16_t wt_buf[3][AP_NF * WT_FRM];
static volatile uint8_t wt_cur[2] = {0, 1};     /* the buffer oscillator A / B plays */
static int16_t sinr[1025];                      /* the sine, in RAM */

static int16_t uwt[NUWT][AP_NF * AP_NH * 2u];  /* the tables of your own, as the built-in ones are kept */

/* one frame of table `tab` into dst (WT_FRM samples), from its 64 harmonics */
static void wt_frame(int16_t *dst, uint32_t tab, uint32_t frame)
{
    static int32_t acc[512];
    const int16_t *spec = (tab < AP_NTAB ? AP_WT + tab * AP_NF * AP_NH * 2u : uwt[(tab - AP_NTAB) % NUWT])
                          + frame * AP_NH * 2u;
    uint32_t k, i, h;
    for (k = 0; k < WT_LEVELS; k++) {
        uint32_t len = WT_LEN[k], step = 1024u / len;
        int16_t *d = dst + WT_OFF[k];
        for (i = 0; i < len; i++)
            acc[i] = 0;
        for (h = 1; h <= WT_NH[k]; h++) {
            int32_t s = spec[2u * (h - 1u)], c = spec[2u * (h - 1u) + 1u];
            uint32_t ph = 0, dph = (h * step) & 1023u;
            if (!s && !c)
                continue;
            for (i = 0; i < len; i++) {
                acc[i] += (s * sinr[ph] + c * sinr[(ph + 256u) & 1023u]) >> 8;
                ph = (ph + dph) & 1023u;
            }
        }
        for (i = 0; i < len; i++) {
            int32_t v = acc[i] >> 7;
            d[i] = (int16_t)(v > 32000 ? 32000 : v < -32000 ? -32000 : v);
        }
        d[len] = d[0];
    }
}

/* ---- small maths */
static inline int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static uint32_t rng_s = 0x1234567u;
static inline uint32_t rnd32(void)
{
    rng_s ^= rng_s << 13;
    rng_s ^= rng_s >> 17;
    rng_s ^= rng_s << 5;
    return rng_s;
}

/* a pitch (semitones, Q8, MIDI numbering) to a phase step */
static uint32_t pitch_inc(int32_t p)
{
    uint32_t oct, rem, i, f, a;
    p = clampi(p, 0, 131 * 256 + 255);
    oct = (uint32_t)p / 3072u;
    rem = (uint32_t)p - oct * 3072u;
    i = rem >> 2;
    f = rem & 3u;
    a = AP_PITCH[i];
    a += ((AP_PITCH[i + 1u] - a) * f) >> 2;
    return a >> (10u - oct);
}

/* the level whose harmonics stay under half the sample rate at this phase step */
static inline uint32_t wt_level(uint32_t inc)
{
    uint32_t k = 0;
    while (k < WT_LEVELS - 1u && inc > WT_LIM[k])
        k++;
    return k;
}

/* ---- envelopes: one-pole segments, the level in Q24 */
#define ENV_ONE (1 << 24)
typedef struct {
    int32_t lvl;
    uint8_t stage;                              /* 0 off, 1 attack, 2 decay / sustain, 3 release */
} env_t;
static inline int32_t env_tick(env_t *e, const uint8_t *adsr)
{
    switch (e->stage) {
    case 1:
        e->lvl += (int32_t)(((int64_t)((ENV_ONE + ENV_ONE / 4) - e->lvl) * AP_ENVC[adsr[0]]) >> 24) + 1;
        if (e->lvl >= ENV_ONE) {
            e->lvl = ENV_ONE;
            e->stage = 2;
        }
        break;
    case 2: {
        int32_t sus = (int32_t)adsr[2] * (ENV_ONE / 127);
        e->lvl += (int32_t)(((int64_t)(sus - e->lvl) * AP_ENVC[adsr[1]]) >> 24);
        break;
    }
    case 3:
        e->lvl -= (int32_t)(((int64_t)e->lvl * AP_ENVC[adsr[3]]) >> 24) + 48;
        if (e->lvl < 4096) {
            e->lvl = 0;
            e->stage = 0;
        }
        break;
    default:
        e->lvl = 0;
        break;
    }
    return e->lvl >> 9;                         /* Q15 */
}

/* ---- an LFO: Q15, -32767..32767 */
typedef struct {
    uint32_t ph;
    int32_t a, b;                               /* S+H / DRIFT: the value held, the next one */
} lfo_t;
static int32_t lfo_tick(lfo_t *l, uint32_t shape, uint32_t inc)
{
    uint32_t prev = l->ph, ph;
    l->ph = ph = prev + inc;
    if (ph < prev) {                            /* a new cycle */
        l->a = l->b;
        l->b = (int32_t)(rnd32() >> 16) - 32768;
    }
    switch (shape) {
    case 0: return sinr[ph >> 22];
    case 1: { int32_t t = (int32_t)(ph >> 15) - 65536; return (t < 0 ? -t : t) - 32768; }   /* down, then up */
    case 2: return 32767 - (int32_t)(ph >> 16);
    case 3: return ph < 0x80000000u ? 32767 : -32767;
    case 4: return l->a;
    default: return l->a + (int32_t)(((int64_t)(l->b - l->a) * (int32_t)(ph >> 17)) >> 15);
    }
}

/* ---- a voice */
typedef struct {
    uint8_t on;                                 /* 0 free, 1 key down, 2 let go (ringing out) */
    uint8_t note, vel, held;                    /* held: its key is down but for the pedal */
    uint8_t kill;                               /* a new sound was picked: out in a few ms */
    uint32_t age;
    int32_t pitch;                              /* Q16 semitones, gliding to its note */
    int32_t press;                              /* its key's pressure, Q15, smoothed */
    int32_t press_to;
    int32_t rnd;                                /* a random number per note, Q15 */
    int32_t side;                               /* -32768 / 32767 by turns: WIDTH */
    uint32_t pha[MAXU], phb[MAXU], phs, phm;    /* oscillator phases: A, B, the sub, B as A's FM source */
    env_t env[3];
    lfo_t lfo[2];
    int32_t lfade[2];                           /* 0..65536 */
    uint32_t nz, nzph;                          /* noise */
    int32_t nzf[3], nzh;
    int32_t ic[2][4];                           /* the filter's state: [left, right][two stages of two] */
    int32_t fcur[2][3];                         /* its two stages' coefficients now, Q30 (they move every frame) */
    int32_t pg, ps[2];                          /* the 6 dB stage: its coefficient now (Q31), its state */
    int32_t cd, clp[2];                         /* COMB: its delay now (frames Q8), the loop's damping */
    uint32_t cw;
    uint32_t dph, rph;                          /* DECIM's clock, RING's carrier */
    int32_t dh[2];
    int32_t gl, gr;                             /* the gains now, Q23 */
    int32_t sgl, sgr;                           /* the direct sub's */
} voice_t;
static voice_t voice[NV];
#define COMB_N 1024u
static int16_t comb_buf[NV][2][COMB_N];         /* COMB's memory: a line a voice and a side (down to 43 Hz) */
static uint32_t ages;
static int32_t last_pitch = 60 << 16;           /* the last note played (GLIDE starts there) */
static volatile uint32_t eng_active;            /* a bit per voice sounding (the screen) */
static volatile uint32_t eng_load;              /* the worst render of late, in ticks of 24 MHz */

/* what the main loop sets, a word each */
static volatile int32_t in_wheel, in_bend, in_chanat;   /* 0..127, -8192..8191, 0..127 */
/* values on their way to where the knob is (Q16): no steps in the sound */
enum { SM_CUT, SM_RES, SM_APOS, SM_BPOS, SM_AWARP, SM_BWARP, SM_M1, SM_M2, SM_M3, SM_M4, SM_WHEEL, SM_BEND, SM_AT,
       SM_ALVL, SM_BLVL, NSM };
static int32_t sm[NSM];

/* the tick's values common to every voice */
static struct {
    int32_t lfo3;
    uint32_t linc[3];                           /* the LFOs' phase steps */
    int32_t lfade[2];
    int32_t tune[2];                            /* A, B: octave, semitone, fine, Q8 */
    int32_t sub_tune;
    int32_t lvl[2], sub_lvl, nz_lvl, vol;       /* Q15 */
    uint32_t uni[2];
    int32_t glide;                              /* Q16 step toward the note per tick, 65536: at once */
    int32_t bend;                               /* Q8 semitones */
    uint32_t stereo;
    int32_t amt[8];                             /* the matrix amounts, Q15 */
    int32_t drive;                              /* Q12 gain into the filter, 4096: none */
    int32_t makeup;                             /* Q15: what DRIVE adds in level, taken off again after the filter */
} gt;
static lfo_t lfo3;
static int32_t lfo3_fade;

/* ---- the oscillator loops. One macro, a loop per warp: the phase p read from the table is WARP of ph */
typedef struct {
    uint32_t k;                                 /* SYNC, PWM, MIRROR: the factor, Q12 */
    uint32_t d, s1, s2;                         /* BEND: the knee, the two slopes Q16 */
    uint32_t mask;                              /* QUANT */
    int32_t fm;                                 /* FM: the amount, Q12 */
} warp_t;

#define OSC_OUT(P, STEREO) { \
        uint32_t idx_ = (P) >> sh; \
        int32_t fr_ = (int32_t)(((P) >> shf) & 0xFFFu); \
        int32_t a_ = t0[idx_], b_ = t1[idx_]; \
        a_ += ((t0[idx_ + 1u] - a_) * fr_) >> 12; \
        b_ += ((t1[idx_ + 1u] - b_) * fr_) >> 12; \
        a_ += ((b_ - a_) * mixf) >> 12; \
        vl[i] += (a_ * gl) >> 15; \
        STEREO(vr[i] += (a_ * gr) >> 15;) \
    }
#define ST_YES(x) x
#define ST_NO(x)
#define OSC_LOOPS(STEREO) \
    switch (mode) { \
    default: \
        for (i = 0; i < n; i++) { ph += inc; OSC_OUT(ph, STEREO) } \
        break; \
    case W_SYNC: \
        for (i = 0; i < n; i++) { \
            uint32_t p; \
            ph += inc; \
            p = (uint32_t)(((uint64_t)ph * w->k) >> 12); \
            OSC_OUT(p, STEREO) \
        } \
        break; \
    case W_BENDP: \
    case W_BENDM: \
        for (i = 0; i < n; i++) { \
            uint32_t p; \
            ph += inc; \
            p = ph < w->d ? (uint32_t)(((uint64_t)ph * w->s1) >> 16) \
                          : 0x80000000u + (uint32_t)(((uint64_t)(ph - w->d) * w->s2) >> 16); \
            OSC_OUT(p, STEREO) \
        } \
        break; \
    case W_PWM: \
        for (i = 0; i < n; i++) { \
            uint64_t q; \
            uint32_t p; \
            ph += inc; \
            q = ((uint64_t)ph * w->k) >> 12; \
            p = q > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)q; \
            OSC_OUT(p, STEREO) \
        } \
        break; \
    case W_MIRROR: \
        for (i = 0; i < n; i++) { \
            uint64_t q; \
            uint32_t p; \
            ph += inc; \
            q = ((uint64_t)ph * w->k) >> 12; \
            p = (uint32_t)q; \
            if ((uint32_t)(q >> 32) & 1u) \
                p = ~p; \
            OSC_OUT(p, STEREO) \
        } \
        break; \
    case W_FM: \
        for (i = 0; i < n; i++) { \
            uint32_t p; \
            ph += inc; \
            p = ph + ((uint32_t)(mod[i] * w->fm) << 6); \
            OSC_OUT(p, STEREO) \
        } \
        break; \
    case W_QUANT: \
        for (i = 0; i < n; i++) { \
            uint32_t p; \
            ph += inc; \
            p = ph & w->mask; \
            OSC_OUT(p, STEREO) \
        } \
        break; \
    }

/* n frames of one oscillator voice, added into vl (and vr, when it is not 0) */
static void osc_run(uint32_t mode, const warp_t *w, uint32_t *php, uint32_t inc, const int16_t *t0, const int16_t *t1,
                    uint32_t sh, int32_t mixf, int32_t gl, int32_t gr, int32_t *vl, int32_t *vr, const int32_t *mod,
                    uint32_t n)
{
    uint32_t ph = *php, i, shf = sh - 12u;
    if (vr) {
        OSC_LOOPS(ST_YES)
    } else {
        OSC_LOOPS(ST_NO)
    }
    *php = ph;
}

/* the warp's numbers from its amount (0..4095) */
static void warp_set(warp_t *w, uint32_t mode, int32_t amt)
{
    uint32_t a = (uint32_t)clampi(amt, 0, 4095);
    switch (mode) {
    case W_SYNC: w->k = 4096u + a * 7u; break;
    case W_MIRROR: w->k = 4096u + a * 7u; break;
    case W_PWM: w->k = 4096u + a * 15u; break;
    case W_BENDP:
    case W_BENDM: {
        uint32_t off = a * 30000u;                  /* up to 0.46 of a cycle from the middle, in 2^-28 */
        uint32_t d28 = mode == W_BENDP ? (1u << 27) - off : (1u << 27) + off;
        w->d = d28 << 4;
        w->s1 = 0x80000000u / (w->d >> 16);
        w->s2 = 0x80000000u / ((0u - w->d) >> 16);
        break;
    }
    case W_FM: w->fm = (int32_t)a; break;
    case W_QUANT: w->mask = ~0u << (24u + ((a * 6u) >> 12)); break;
    default: break;
    }
}

/* ---- the filter: a state-variable filter that stays stable up to the top (trapezoidal integrators) */
typedef struct {
    int32_t a1, a2, a3;                         /* Q30 */
    int32_t k;                                  /* Q16: 2 = no resonance .. 0.04 */
} fcoef_t;
static void filter_coef(fcoef_t *f, int32_t cut_q8, int32_t k_q16)
{
    uint32_t i, fr, g, den, a1;
    cut_q8 = clampi(cut_q8, 8 * 256, 135 * 256);
    i = (uint32_t)(cut_q8 - 2048) >> 7;
    fr = (uint32_t)(cut_q8 - 2048) & 127u;
    g = AP_GTAB[i];
    g += (uint32_t)(((uint64_t)(AP_GTAB[i + 1u] - g) * fr) >> 7);
    f->k = k_q16;
    den = 65536u + (uint32_t)(((uint64_t)g * ((g >> 8) + (uint32_t)f->k)) >> 24);
    a1 = 0xFFFFFFFFu / den;                     /* Q16 */
    f->a1 = (int32_t)(a1 << 14);
    f->a2 = (int32_t)(((uint64_t)g * a1) >> 10);
    f->a3 = (int32_t)(((uint64_t)g * (uint32_t)f->a2) >> 24);
}

enum { SM_LP, SM_BP, SM_BPN, SM_HP, SM_NOTCH, SM_PEAK };      /* what a stage gives out */
/* the high word of a product: one instruction on a 32-bit processor */
static inline int32_t mulh(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 32); }

/* one stage over a block: the coefficients go from (a1, a2, a3) by (d1, d2, d3) a frame. x in, the type's output
 * out. Inside, the signal is 256 times finer than a sample: the input's gain g8 (Q8) is that step too, held to the
 * filter's range (+-1.5). A loop per type */
#define SVF_IN 12582400                         /* 49150 * 256 */
#define SVF_LOOP(OUT) \
    for (i = 0; i < n; i++) { \
        int32_t v0 = x[i] * g8, v3, v1, v2; \
        v0 = v0 > SVF_IN ? SVF_IN : v0 < -SVF_IN ? -SVF_IN : v0; \
        v3 = v0 - ic2; \
        v1 = (mulh(a1, ic1) + mulh(a2, v3)) * 4; \
        v2 = ic2 + (mulh(a2, ic1) + mulh(a3, v3)) * 4; \
        ic1 = 2 * v1 - ic1; \
        ic2 = 2 * v2 - ic2; \
        x[i] = (OUT) >> 8; \
        a1 += d1; \
        a2 += d2; \
        a3 += d3; \
    }
static void svf_run(int32_t *ic, int32_t *x, uint32_t n, uint32_t type, int32_t a1, int32_t a2, int32_t a3,
                    int32_t d1, int32_t d2, int32_t d3, int32_t k, int32_t g8)
{
    int32_t ic1 = ic[0], ic2 = ic[1], k29 = k << 13;
    uint32_t i;
    switch (type) {
    case SM_BP: SVF_LOOP(v1) break;
    case SM_BPN: SVF_LOOP(mulh(k29, v1) * 8) break;             /* a band-pass whose peak is at 1 */
    case SM_HP: SVF_LOOP(v0 - mulh(k29, v1) * 8 - v2) break;
    case SM_NOTCH: SVF_LOOP(v0 - mulh(k29, v1) * 8) break;
    case SM_PEAK: SVF_LOOP(2 * v2 - v0 + mulh(k29, v1) * 8) break;
    default: SVF_LOOP(v2) break;
    }
    ic[0] = clampi(ic1, -(1 << 29), 1 << 29);
    ic[1] = clampi(ic2, -(1 << 29), 1 << 29);
}

/* a soft ceiling on the way into the filter: gain Q12, then a curve that ends flat at +-1.0 (32767) */
static void pre_run(int32_t *x, uint32_t n, int32_t g8, uint32_t curve)
{
    uint32_t i;
    if (!curve) {                               /* DRIVE at 0: clean, only held within the filter's range */
        for (i = 0; i < n; i++)
            x[i] = clampi((x[i] * g8) >> 8, -49150, 49150);
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t t = clampi((x[i] * g8) >> 8, -49150, 49150), u, u2;    /* 1.5 */
        u = (t * 21845) >> 15;                  /* t / 1.5: -32767..32767 */
        u2 = (u * u) >> 15;
        x[i] = t - ((((t * u2) >> 15) * 10923) >> 15);
    }
}

/* ---- notes */
static uint32_t guard_max = NV;                 /* notes the measured time of a block leaves (aplat.c, render) */

/* what a filter type costs a frame and a side, in the units of COST_BUDGET */
static const uint8_t FT_COST[NFT] = {0, 12, 26, 38, 52, 72, 12, 26, 52, 26, 52, 26, 52, 26, 52, 58, 58, 26, 26, 10, 12};

static uint32_t eng_note_cost(void)
{
    uint32_t i, la = PV(A_LVL) != 0, lb = PV(B_LVL) != 0, pa, pb, stereo, stages, cost;
    for (i = 0; i < 8u; i++) {                  /* (a level the matrix moves counts as on) */
        la |= cur.p[P_X1_SRC + 3u * i] && cur.p[P_X1_DST + 3u * i] == D_ALVL;
        lb |= cur.p[P_X1_SRC + 3u * i] && cur.p[P_X1_DST + 3u * i] == D_BLVL;
    }
    pa = la ? (uint32_t)PV(A_UNI) + 1u : 0u;
    pb = lb ? (uint32_t)PV(B_UNI) + 1u : 0u;
    stereo = (pa > 1u && PV(A_SPR)) || (pb > 1u && PV(B_SPR)) || (pa && pb && PV(A_PAN) != PV(B_PAN));
    stages = FT_COST[PV(F_TYPE)];
    cost = 60u + (pa + pb) * (stereo ? 40u : 34u) + (stereo ? 2u : 1u) * (stages + (PV(F_DRV) ? 20u : 4u));
    if (pa && PV(A_WMODE) != W_OFF)
        cost += pa * 6u + (PV(A_WMODE) == W_FM ? 34u : 0u);
    if (pb && PV(B_WMODE) != W_OFF)
        cost += pb * 6u;
    if (PV(SUB_LVL) || PV(B_WMODE) == W_FM)
        cost += 16u;
    if (PV(NZ_LVL))
        cost += 26u;
    return cost;
}

/* what the effects picked cost a frame, in the same units (each kind counts once: it runs once) */
static const uint8_t FX_COST[NFXT] = {0, 34, 30, 16, 40, 44, 50, 110, 60, 80, 110, 24, 16, 60, 125};
static uint32_t eng_fx_cost(void)
{
    uint32_t i, seen = 0, c = 0;
    for (i = 0; i < NFX; i++) {
        uint32_t t = cur.p[P_FX1_TYPE + 4u * i];
        if (t && t < NFXT && !(seen >> t & 1u)) {
            seen |= 1u << t;
            c += FX_COST[t];
        }
    }
    return c;
}

static uint32_t eng_max_notes(void)
{
    uint32_t fxc = eng_fx_cost(), n = (COST_BUDGET + 350u - (fxc > 1400u ? 1400u : fxc)) / eng_note_cost();
    if (PV(MODE) != MODE_POLY)
        return 1u;
    if (n > guard_max)
        n = guard_max;                          /* (the chip ran short of time: fewer for now) */
    return n > NV ? NV : n < 1u ? 1u : n;
}

static void voice_start(voice_t *v, uint32_t key, uint32_t vel, uint32_t legato)
{
    uint32_t i, was = v->on || v->env[0].lvl > (ENV_ONE >> 6);
    v->note = (uint8_t)key;
    v->vel = (uint8_t)vel;
    v->on = 1;
    v->held = 0;
    v->kill = 0;
    v->age = ++ages;
    if (!PV(GLIDE) || (PV(MODE) == MODE_LEGATO && !legato))
        v->pitch = (int32_t)key << 16;
    else if (!legato)
        v->pitch = last_pitch;
    last_pitch = (int32_t)key << 16;
    if (legato)
        return;                                 /* the same voice goes on: only its note changes */
    v->press = v->press_to = 0;
    v->rnd = (int32_t)(rnd32() >> 16) - 32768;
    v->side = (ages & 1u) ? 32767 : -32768;
    for (i = 0; i < 3u; i++)
        v->env[i].stage = 1;
    v->env[1].lvl = v->env[2].lvl = 0;
    for (i = 0; i < 2u; i++) {
        v->lfo[i].ph = 0;
        v->lfo[i].a = (int32_t)(rnd32() >> 16) - 32768;
        v->lfo[i].b = (int32_t)(rnd32() >> 16) - 32768;
        v->lfade[i] = 0;
    }
    if (!was) {                                 /* from silence: the phases and the filter start clean */
        uint32_t ra = PV(A_UNI) ? 1u : 0u, rb = PV(B_UNI) ? 1u : 0u;
        for (i = 0; i < MAXU; i++) {
            v->pha[i] = ra ? rnd32() : 0u;
            v->phb[i] = rb ? rnd32() : 0u;
        }
        v->phs = v->phm = 0;
        v->nz = rnd32() | 1u;
        for (i = 0; i < 4u; i++)
            v->ic[0][i] = v->ic[1][i] = 0;
        v->nzf[0] = v->nzf[1] = v->nzf[2] = 0;
        v->gl = v->gr = v->sgl = v->sgr = 0;
        for (i = 0; i < 3u; i++)
            v->fcur[0][i] = v->fcur[1][i] = 0;
        v->pg = v->ps[0] = v->ps[1] = 0;
        v->cd = v->clp[0] = v->clp[1] = 0;
        v->dph = v->rph = 0;
        v->dh[0] = v->dh[1] = 0;
        if (PV(F_TYPE) == FT_COMBP || PV(F_TYPE) == FT_COMBM) {
            int16_t *cb = comb_buf[v - voice][0];
            for (i = 0; i < 2u * COMB_N; i++)
                cb[i] = 0;
        }
    }
}

static void voice_release(voice_t *v)
{
    uint32_t i;
    if (v->on != 1u)
        return;
    v->on = 2;
    v->held = 0;
    for (i = 0; i < 3u; i++)
        if (v->env[i].stage)
            v->env[i].stage = 3;
}

/* the keys held in MONO / LEGATO, the newest last */
static uint8_t mono_stack[16], mono_n;
static uint8_t pedal;

static void eng_note_on(uint32_t key, uint32_t vel)
{
    uint32_t i, pick = 0, best = 0xFFFFFFFFu, maxn = eng_max_notes(), sounding = 0;
    if (PV(MODE) != MODE_POLY) {
        voice_t *v = &voice[0];
        uint32_t legato = PV(MODE) == MODE_LEGATO && mono_n && v->on == 1u;
        for (i = 0; i < mono_n; i++)
            if (mono_stack[i] == key) {
                for (; i + 1u < mono_n; i++)
                    mono_stack[i] = mono_stack[i + 1u];
                mono_n--;
                break;
            }
        if (mono_n < 16u)
            mono_stack[mono_n++] = (uint8_t)key;
        for (i = 1; i < NV; i++)
            voice_release(&voice[i]);
        voice_start(v, key, vel, legato);
        return;
    }
    (void)best;
    for (i = 0; i < NV; i++)
        sounding += voice[i].on != 0u;
    for (i = 0; i < NV; i++)                    /* the same note again: its voice */
        if (voice[i].on && voice[i].note == key) {
            voice_start(&voice[i], key, vel, 0);
            return;
        }
    if (sounding < maxn) {                      /* a silent voice */
        for (i = 0; i < NV; i++)
            if (!voice[i].on) {
                pick = i;
                break;
            }
    } else {                                    /* none left (unison trades for notes): the oldest one let go, */
        uint32_t oa = 0xFFFFFFFFu;              /* else the oldest one held */
        for (i = 0; i < NV; i++) {
            uint32_t a = (voice[i].on == 1u ? 0x80000000u : 0u) + (voice[i].age & 0x7FFFFFFFu);
            if (voice[i].on && a < oa) {
                oa = a;
                pick = i;
            }
        }
    }
    voice_start(&voice[pick], key, vel, 0);
}

static void eng_note_off(uint32_t key)
{
    uint32_t i;
    if (PV(MODE) != MODE_POLY) {
        voice_t *v = &voice[0];
        for (i = 0; i < mono_n; i++)
            if (mono_stack[i] == key) {
                for (; i + 1u < mono_n; i++)
                    mono_stack[i] = mono_stack[i + 1u];
                mono_n--;
                break;
            }
        if (v->on == 1u && v->note == key) {
            if (mono_n)                         /* back to the key still held */
                voice_start(v, mono_stack[mono_n - 1u], v->vel, 1);
            else if (pedal)
                v->held = 1;
            else
                voice_release(v);
        }
        return;
    }
    for (i = 0; i < NV; i++)
        if (voice[i].on == 1u && voice[i].note == key && !voice[i].held) {
            if (pedal)
                voice[i].held = 1;
            else
                voice_release(&voice[i]);
        }
}

static void eng_pedal(uint32_t down)
{
    uint32_t i;
    pedal = (uint8_t)down;
    if (!down)
        for (i = 0; i < NV; i++)
            if (voice[i].held)
                voice_release(&voice[i]);
}

static void eng_all_off(void)
{
    uint32_t i;
    mono_n = 0;
    pedal = 0;
    for (i = 0; i < NV; i++)
        voice_release(&voice[i]);
}

static void eng_pressure(uint32_t key, uint32_t pr)
{
    uint32_t i;
    for (i = 0; i < NV; i++)
        if (voice[i].on == 1u && voice[i].note == key)
            voice[i].press_to = (int32_t)pr * 258;
}

/* ---- the tick: what is common to all voices */
static volatile uint32_t clk_spt = 5514u << 16; /* frames per MIDI clock, Q16 (the tempo; ap_seq.h keeps it) */

static inline int32_t lvl_q15(int32_t v) { return (v * v * 32767) / 16129; }   /* 0..127, a square law */

static uint32_t lfo_rate(uint32_t rate, uint32_t sync)
{
    uint32_t per;
    if (!sync)
        return AP_LFOINC[rate & 127u];
    per = (uint32_t)(((uint64_t)DIV_TICKS[(rate * 12u) >> 7] * clk_spt) >> 21);   /* its cycle in control ticks */
    return 0xFFFFFFFFu / (per ? per : 1u);
}

static void eng_tick_global(void)
{
    static const uint8_t SMP[NSM] = {P_F_CUT, P_F_RES, P_A_POS, P_B_POS, P_A_WARP, P_B_WARP, P_M1, P_M2, P_M3, P_M4,
                                     0, 0, 0, P_A_LVL, P_B_LVL};
    uint32_t i;
    for (i = 0; i < NSM; i++) {
        int32_t to = i == SM_WHEEL ? in_wheel << 16 : i == SM_BEND ? in_bend * 256 : i == SM_AT ? in_chanat << 16
                   : (int32_t)cur.p[SMP[i]] << 16;
        sm[i] += (to - sm[i]) >> 3;
    }
    gt.linc[0] = lfo_rate((uint32_t)PV(L1_RATE), (uint32_t)PV(L1_SYNC));
    gt.linc[1] = lfo_rate((uint32_t)PV(L2_RATE), (uint32_t)PV(L2_SYNC));
    gt.linc[2] = lfo_rate((uint32_t)PV(L3_RATE), (uint32_t)PV(L3_SYNC));
    gt.lfade[0] = PV(L1_FADE) ? (int32_t)(AP_ENVC[PV(L1_FADE)] >> 10) + 1 : 65536;
    gt.lfade[1] = PV(L2_FADE) ? (int32_t)(AP_ENVC[PV(L2_FADE)] >> 10) + 1 : 65536;
    lfo3_fade += PV(L3_FADE) ? (int32_t)(AP_ENVC[PV(L3_FADE)] >> 10) + 1 : 65536;
    if (lfo3_fade > 65536)
        lfo3_fade = 65536;
    gt.lfo3 = (lfo_tick(&lfo3, (uint32_t)PV(L3_SHAPE), gt.linc[2]) * (lfo3_fade >> 1)) >> 15;
    gt.tune[0] = (PV(A_OCT) - 3) * 3072 + (PV(A_SEMI) - 12) * 256 + ((PV(A_FINE) - 50) * 256) / 100;
    gt.tune[1] = (PV(B_OCT) - 3) * 3072 + (PV(B_SEMI) - 12) * 256 + ((PV(B_FINE) - 50) * 256) / 100;
    gt.sub_tune = (PV(SUB_OCT) - 2) * 3072;
    gt.lvl[0] = lvl_q15(sm[SM_ALVL] >> 16);
    gt.lvl[1] = lvl_q15(sm[SM_BLVL] >> 16);
    gt.sub_lvl = lvl_q15(PV(SUB_LVL));
    gt.nz_lvl = lvl_q15(PV(NZ_LVL));
    gt.vol = lvl_q15(PV(VOL));
    gt.uni[0] = (uint32_t)PV(A_UNI) + 1u;
    gt.uni[1] = (uint32_t)PV(B_UNI) + 1u;
    gt.glide = PV(GLIDE) ? (int32_t)(AP_ENVC[PV(GLIDE) / 2 + 30] >> 8) : 65536;
    gt.bend = ((sm[SM_BEND] >> 8) * PV(BEND) * 256) / 8192;
    gt.stereo = (gt.lvl[0] && gt.uni[0] > 1u && PV(A_SPR)) || (gt.lvl[1] && gt.uni[1] > 1u && PV(B_SPR))
             || (gt.lvl[0] && gt.lvl[1] && PV(A_PAN) != PV(B_PAN));
    for (i = 0; i < 8u; i++)
        gt.amt[i] = ((int32_t)cur.p[P_X1_AMT + 3u * i] - 100) * 328;
    gt.drive = 4096 + PV(F_DRV) * 200;          /* up to 7.2 x */
    gt.makeup = (23600 * 4096) / (4096 + (PV(F_DRV) * 70));     /* 0.72, down to 0.23 */
}

/* unison: where voice i of u sits, -128..128 (its detune and its place left to right) */
/* (no -64..-49 in a 16-bit table: with the word before it, it would read as an address in the chip's ROM, which
 * fm1-dock's build refuses, tools/gen_tables.py) */
static const int16_t UNI_POS[MAXU][MAXU] = {{0}, {-128, 128}, {-128, 0, 128}, {-128, -43, 43, 128}, {-128, -66, 0, 66, 128}};
static const int16_t UNI_GAIN[MAXU] = {32767, 23170, 18918, 16384, 14654};

/* a place left to right (-32768..32767) as two gains, Q15: the middle is both at full */
static inline void pan_gains(int32_t pan, int32_t *l, int32_t *r)
{
    pan = clampi(pan, -32767, 32767);
    *l = pan > 0 ? 32767 - pan : 32767;
    *r = pan < 0 ? 32767 + pan : 32767;
}

/* the filter types that start with a 12 dB stage (it takes the input's gain), and those whose resonance is loud */
#define FT_SVF1 (1u << FT_LP12 | 1u << FT_LP18 | 1u << FT_LP24 | 1u << FT_LPHOT | 1u << FT_HP12 | 1u << FT_HP24 \
                 | 1u << FT_BP12 | 1u << FT_BP24 | 1u << FT_NOTCH | 1u << FT_NOTCH2 | 1u << FT_PEAK | 1u << FT_BANDW)
#define FT_RESO (FT_SVF1 | 1u << FT_COMBP | 1u << FT_COMBM)

/* a step that reaches d in n frames (n is a whole tick but for the end of an odd block) */
static inline int32_t per_frame(int32_t d, uint32_t n) { return n == SUBN ? d >> 5 : d / (int32_t)n; }
typedef char subn_is_32[SUBN == 32u ? 1 : -1];

/* ---- the filter's parts. A 12 dB stage (the first or the second of a voice), left and right */
static void flt_stage(voice_t *v, uint32_t st, int32_t *l, int32_t *r, uint32_t n, uint32_t mode, int32_t cut,
                      int32_t k, int32_t g8)
{
    fcoef_t f;
    int32_t *c = v->fcur[st], d1, d2, d3;
    filter_coef(&f, cut, k);
    if (!c[0] && !c[1]) {                       /* a new voice: no glide from nothing */
        c[0] = f.a1;
        c[1] = f.a2;
        c[2] = f.a3;
    }
    d1 = per_frame(f.a1 - c[0], n);
    d2 = per_frame(f.a2 - c[1], n);
    d3 = per_frame(f.a3 - c[2], n);
    svf_run(v->ic[0] + 2u * st, l, n, mode, c[0], c[1], c[2], d1, d2, d3, f.k, g8);
    if (r)
        svf_run(v->ic[1] + 2u * st, r, n, mode, c[0], c[1], c[2], d1, d2, d3, f.k, g8);
    c[0] += d1 * (int32_t)n;
    c[1] += d2 * (int32_t)n;
    c[2] += d3 * (int32_t)n;
}

/* a 6 dB stage, low-pass or high-pass */
static void flt_pole(voice_t *v, int32_t *l, int32_t *r, uint32_t n, uint32_t hp, int32_t cut)
{
    uint32_t i, ix, fr, g, ch;
    int32_t G, dG;
    cut = clampi(cut, 8 * 256, 135 * 256);
    ix = (uint32_t)(cut - 2048) >> 7;
    fr = (uint32_t)(cut - 2048) & 127u;
    g = AP_GTAB[ix];
    g += (uint32_t)(((uint64_t)(AP_GTAB[ix + 1u] - g) * fr) >> 7);
    G = (int32_t)(((uint64_t)g * (0xFFFFFFFFu / (65536u + (g >> 8)))) >> 9);     /* g / (1 + g), Q31 */
    if (!v->pg)
        v->pg = G;
    dG = per_frame(G - v->pg, n);
    for (ch = 0; ch < (r ? 2u : 1u); ch++) {
        int32_t *x = ch ? r : l, s = v->ps[ch], gg = v->pg;
        for (i = 0; i < n; i++) {
            int32_t v0 = x[i] * 256, t = mulh(v0 - s, gg) * 2, y = t + s;
            s = y + t;
            x[i] = (hp ? v0 - y : y) >> 8;
            gg += dG;
        }
        v->ps[ch] = s;
    }
    v->pg += dG * (int32_t)n;
}

/* COMB: a short echo tuned to CUTOFF, fed back by RESO (the other way round for COMB -); SHAPE dulls the loop */
static void flt_comb(voice_t *v, int32_t *l, int32_t *r, uint32_t n, int32_t cut, int32_t res, int32_t var, uint32_t neg)
{
    uint32_t inc = pitch_inc(cut), i, ch, w0 = v->cw;
    int32_t to = (int32_t)(0xFFFFFFFFu / ((inc >> 8) + 1u)), fb = res * 250, damp = 32767 - var * 240, dd;
    to = clampi(to, 2 << 8, (int32_t)(COMB_N - 4u) << 8);
    if (neg)
        fb = -fb;
    if (!v->cd)
        v->cd = to;
    dd = per_frame(to - v->cd, n);
    for (ch = 0; ch < (r ? 2u : 1u); ch++) {
        int16_t *buf = comb_buf[v - voice][ch];
        int32_t *x = ch ? r : l, d = v->cd, lp = v->clp[ch];
        uint32_t w = w0;
        for (i = 0; i < n; i++) {
            uint32_t p = (w - (uint32_t)(d >> 8)) & (COMB_N - 1u);
            int32_t a = buf[p], y, in;
            y = (a + (((buf[(p - 1u) & (COMB_N - 1u)] - a) * (d & 255)) >> 8)) * 2;
            lp += ((y - lp) * damp) >> 15;
            in = clampi(x[i] + ((lp * fb) >> 15), -65000, 65000);
            buf[w] = (int16_t)(in >> 1);
            x[i] = in;
            w = (w + 1u) & (COMB_N - 1u);
            d += dd;
        }
        v->clp[ch] = lp;
    }
    v->cw = (w0 + n) & (COMB_N - 1u);
    v->cd += dd * (int32_t)n;
}

/* ---- one voice, n frames (a control tick), added into outl / outr. Returns 0 when it has gone silent */
static uint32_t voice_run(voice_t *v, int32_t *outl, int32_t *outr, uint32_t n)
{
    static int32_t vl[SUBN], vr[SUBN], sub[SUBN], modb[SUBN];
    int32_t src[NSRC], dst[NDST];
    int32_t e1, key, base, cut, amp, pl, pr, tl, tr, dgl, dgr;
    uint32_t i, o, ftype = (uint32_t)PV(F_TYPE), stereo = gt.stereo;
    int32_t *rbuf = stereo ? vr : 0;

    /* its own envelopes and LFOs */
    if (v->kill) {
        v->env[0].lvl -= (v->env[0].lvl >> 2) + 64;
        if (v->env[0].lvl < 4096) {
            v->env[0].lvl = 0;
            v->env[0].stage = 0;
        }
        e1 = v->env[0].lvl >> 9;
    } else {
        e1 = env_tick(&v->env[0], &cur.p[P_E1_A]);
    }
    src[S_NONE] = 0;
    src[S_ENV2] = env_tick(&v->env[1], &cur.p[P_E2_A]);
    src[S_ENV3] = env_tick(&v->env[2], &cur.p[P_E3_A]);
    for (i = 0; i < 2u; i++) {
        uint32_t inc = gt.linc[i];
        v->lfade[i] += gt.lfade[i];
        if (v->lfade[i] > 65536)
            v->lfade[i] = 65536;
        src[S_LFO1 + i] = inc ? (lfo_tick(&v->lfo[i], (uint32_t)cur.p[i ? P_L2_SHAPE : P_L1_SHAPE], inc)
                                 * (v->lfade[i] >> 1)) >> 15 : 0;
    }
    v->press += (v->press_to - v->press) >> 3;
    key = ((int32_t)v->note - 60) * 546;        /* +-5 octaves: -32768..32767 */
    src[S_LFO3] = gt.lfo3;
    src[S_VEL] = (int32_t)v->vel * 258;
    src[S_KEY] = clampi(key, -32767, 32767);
    src[S_AT] = clampi(v->press + (sm[SM_AT] >> 16) * 258, 0, 32767);
    src[S_WHEEL] = (sm[SM_WHEEL] >> 16) * 258;
    src[S_BEND] = clampi(sm[SM_BEND] >> 6, -32767, 32767);
    src[S_RND] = v->rnd;
    for (i = 0; i < 4u; i++)
        src[S_M1 + i] = (sm[SM_M1 + i] >> 16) * 258;

    /* the matrix */
    for (i = 0; i < NDST; i++)
        dst[i] = 0;
    for (i = 0; i < 8u; i++) {
        uint32_t s = cur.p[P_X1_SRC + 3u * i], d = cur.p[P_X1_DST + 3u * i];
        if (s && d && s < NSRC && d < NDST)
            dst[d] += (src[s] * gt.amt[i]) >> 15;
    }
    if (dst[D_L1RATE]) {                        /* LFO 1 faster or slower, up to 4 times: its phase moved on or back */
        int32_t m = clampi(dst[D_L1RATE], -32767, 32767);
        v->lfo[0].ph += (uint32_t)(int32_t)(((int64_t)gt.linc[0] * m * 3) >> (m > 0 ? 15 : 17));
    }

    /* pitch */
    if (v->pitch != (int32_t)v->note << 16) {
        int32_t to = (int32_t)v->note << 16, d = to - v->pitch;
        int32_t step = gt.glide >= 65536 ? d : (int32_t)(((int64_t)d * gt.glide) >> 16);
        if (!step)
            step = d > 0 ? 1 : -1;
        v->pitch += step;
        if ((d > 0 && v->pitch > to) || (d < 0 && v->pitch < to) || (d > -64 && d < 64))
            v->pitch = to;
    }
    base = (v->pitch >> 8) + gt.bend + ((dst[D_PITCH] * 24 * 256) >> 15) + ((dst[D_FINE] * 256) >> 15);

    /* the gains: envelope 1, the velocity, the matrix, the sound's volume; then left and right */
    amp = e1;
    {
        int32_t vs = PV(VELSENS), vg = 32767 - ((127 - (int32_t)v->vel) * vs * 2) ;
        amp = (amp * clampi(vg, 2000, 32767)) >> 15;
    }
    amp = (amp * clampi(32767 + dst[D_AMP], 0, 65535)) >> 15;
    amp = (amp * gt.vol) >> 15;
    amp = (amp * gt.makeup) >> 15;              /* the half that comes out of the filter, less when DRIVE fills it */
    pan_gains(((v->side * PV(VSPREAD)) >> 7) + dst[D_PAN], &pl, &pr);
    tl = ((amp * pl) >> 15) << 10;              /* Q23, four times over: the filter's input was halved, and */
    tr = ((amp * pr) >> 15) << 10;              /* VOLUME at 100 of 127 is the usual level */
    dgl = per_frame(tl - v->gl, n);
    dgr = per_frame(tr - v->gr, n);

    if (!v->env[0].stage && v->gl < 256 && v->gr < 256 && !tl && !tr) {
        v->on = 0;
        v->gl = v->gr = v->sgl = v->sgr = 0;
        return 0;
    }

    for (i = 0; i < n; i++)
        vl[i] = 0;
    if (stereo)
        for (i = 0; i < n; i++)
            vr[i] = 0;

    /* the sub: also oscillator B's FM source */
    if (gt.sub_lvl || (PV(B_WMODE) == W_FM && gt.lvl[1])) {
        uint32_t inc = pitch_inc(base + gt.sub_tune), ph = v->phs, w = (uint32_t)PV(SUB_WAVE);
        for (i = 0; i < n; i++) {
            int32_t s;
            ph += inc;
            switch (w) {
            case 0: {
                uint32_t ix = ph >> 22;
                int32_t a = sinr[ix];
                s = a + (((sinr[ix + 1u] - a) * (int32_t)((ph >> 10) & 0xFFFu)) >> 12);
                break;
            }
            case 1: { int32_t t = (int32_t)(ph >> 15) - 65536; s = (t < 0 ? -t : t) - 32768; break; }
            case 2: s = ph < 0x80000000u ? 22000 : -22000; break;
            default: s = ((int32_t)(ph >> 16) - 32768) * 3 / 4; break;
            }
            sub[i] = s;
        }
        v->phs = ph;
    }

    /* oscillators B, then A (A's FM source is B's first voice alone, whatever B's level) */
    for (o = 2; o-- > 0;) {
        uint32_t po = o ? P_B_WAVE : P_A_WAVE, mode = cur.p[po + 2u], u = gt.uni[o], k, lev, f0, need_mod;
        int32_t lvl = clampi(gt.lvl[o] + dst[o ? D_BLVL : D_ALVL], 0, 32767);
        int32_t pos, mixf, p;
        uint32_t inc, *phs = o ? v->phb : v->pha;
        const int16_t *tb = wt_buf[wt_cur[o]], *t0, *t1;
        const int32_t *mod = o ? sub : modb;
        warp_t w = {4096u, 0x80000000u, 65536u, 65536u, ~0u, 0};
        need_mod = o == 1u && PV(A_WMODE) == W_FM && gt.lvl[0];
        if (!lvl && !need_mod)
            continue;                           /* silent */
        p = base + gt.tune[o] + (o ? (dst[D_BPITCH] * 24 * 256) >> 15 : 0);
        inc = pitch_inc(p);
        pos = clampi(((sm[o ? SM_BPOS : SM_APOS] >> 8) * 7 * 4096 / 127 >> 8) + ((dst[o ? D_BPOS : D_APOS] * 7 * 4096) >> 15),
                     0, 7 * 4096);
        f0 = (uint32_t)pos >> 12;
        mixf = pos & 4095;
        if (f0 >= AP_NF - 1u) {
            f0 = AP_NF - 1u;
            mixf = 0;
        }
        warp_set(&w, mode, (((sm[o ? SM_BWARP : SM_AWARP] >> 8) * 4095 / 127) >> 8) + ((dst[o ? D_BWARP : D_AWARP] * 4095) >> 15));
        lev = wt_level(u > 1u ? inc + (inc >> 5) : inc);        /* (the sharpest unison voice is 3 % up) */
        if (w.k > 8192u)                        /* a strong SYNC / PWM / MIRROR adds harmonics: an octave duller */
            lev = lev + 2u < WT_LEVELS ? lev + 2u : WT_LEVELS - 1u;
        if (need_mod) {                         /* B's first voice, alone and at full level, for A */
            uint32_t ph = v->phm;
            for (i = 0; i < n; i++)
                modb[i] = 0;
            t0 = tb + f0 * WT_FRM + WT_OFF[lev];
            osc_run(W_OFF, &w, &ph, inc, t0, t0 + (mixf ? WT_FRM : 0u), WT_SH[lev], mixf, 32767, 0, modb, 0, modb, n);
            v->phm = ph;
        }
        if (!lvl)
            continue;
        for (k = 0; k < u; k++) {
            int32_t ps = UNI_POS[u - 1u][k], gl, gr, g;
            uint32_t pinc = inc + (uint32_t)(int32_t)(((int64_t)inc * ((ps * (int32_t)cur.p[po + 9u] * 15) >> 3)) >> 20);
            t0 = tb + f0 * WT_FRM + WT_OFF[lev];
            t1 = t0 + (mixf ? WT_FRM : 0u);
            g = (lvl * UNI_GAIN[u - 1u]) >> 15;
            if (stereo) {
                pan_gains(((int32_t)cur.p[po + 11u] - 64) * 512 + ps * (int32_t)cur.p[po + 10u] * 2, &gl, &gr);
                gl = (g * gl) >> 15;
                gr = (g * gr) >> 15;
            } else {
                gl = g;
                gr = 0;
            }
            osc_run(mode, &w, &phs[k], pinc, t0, t1, WT_SH[lev], mixf, gl, gr, vl, rbuf, mod, n);
        }
    }

    /* noise */
    {
        int32_t g = clampi(gt.nz_lvl + dst[D_NOISE], 0, 32767);
        if (g) {
            uint32_t x = v->nz, type = (uint32_t)PV(NZ_TYPE), tinc = pitch_inc(base + 36 * 256);
            int32_t f0 = v->nzf[0], f1 = v->nzf[1], h = v->nzh;
            for (i = 0; i < n; i++) {
                int32_t w, s;
                x ^= x << 13;
                x ^= x >> 17;
                x ^= x << 5;
                w = (int32_t)(x >> 16) - 32768;
                switch (type) {
                case 1: f0 += (w - f0) >> 2; f1 += (w - f1) >> 5; s = (f0 + f1 * 2) * 2 / 3; break;
                case 2: f0 += (w - f0) >> 4; s = f0 * 3; break;
                case 3: {                       /* a new value 8 times a cycle of the note: noise with a pitch */
                    uint32_t ph = v->nzph;
                    v->nzph = ph + tinc;
                    if (v->nzph < ph)
                        h = w;
                    s = h;
                    break;
                }
                default: s = w * 3 / 4; break;
                }
                s = (s * g) >> 15;
                vl[i] += s;
                if (stereo)
                    vr[i] += s;
            }
            v->nz = x;
            v->nzf[0] = f0;
            v->nzf[1] = f1;
            v->nzh = h;
        }
    }
    if (gt.sub_lvl && !PV(SUB_DIR))
        for (i = 0; i < n; i++) {
            int32_t s = (sub[i] * gt.sub_lvl) >> 15;
            vl[i] += s;
            if (stereo)
                vr[i] += s;
        }

    /* into the filter at half level, under a soft ceiling: DRIVE pushes into it */
    {
        int32_t res = clampi((sm[SM_RES] >> 16) + ((dst[D_RES] * 127) >> 15), 0, 127), var = PV(F_VAR);
        int32_t k = 131072 - res * 1011;        /* 2 (no resonance) .. 0.04 */
        /* (Q8: a voice's sum times it stays in 32 bits. Resonance is loud: less goes in) */
        int32_t g8 = ((FT_RESO >> ftype & 1u ? 4096 - res * 18 : 4096) * gt.drive) >> 17, gin = g8;
        if (PV(F_DRV) || !(FT_SVF1 >> ftype & 1u)) {     /* the curve (or only the ceiling) here; a filter that */
            pre_run(vl, n, g8, PV(F_DRV) != 0);          /* starts with a 12 dB stage takes the gain itself */
            if (stereo)
                pre_run(vr, n, g8, PV(F_DRV) != 0);
            gin = 256;
        }
        /* the cutoff: the knob, the key, envelope 2, the velocity and the matrix */
        cut = 16 * 256 + ((sm[SM_CUT] >> 8) * 15) / 16;             /* 16 .. 135 semitones */
        cut += (((int32_t)v->note - 60) * 256 * PV(F_KEY)) / 100;
        cut += (src[S_ENV2] * (PV(F_ENV) - 64) * 2) >> 8;           /* +-5 octaves */
        cut += ((src[S_VEL] - 16384) * PV(F_VEL)) >> 8;
        cut += (dst[D_CUT] * 64 * 256) >> 15;
        switch (ftype) {
        case FT_OFF: break;
        case FT_LP6: flt_pole(v, vl, rbuf, n, 0, cut); break;
        case FT_HP6: flt_pole(v, vl, rbuf, n, 1, cut); break;
        case FT_LP12: flt_stage(v, 0, vl, rbuf, n, SM_LP, cut, k, gin); break;
        case FT_HP12: flt_stage(v, 0, vl, rbuf, n, SM_HP, cut, k, gin); break;
        case FT_BP12: flt_stage(v, 0, vl, rbuf, n, SM_BP, cut, k, gin); break;
        case FT_NOTCH: flt_stage(v, 0, vl, rbuf, n, SM_NOTCH, cut, k, gin); break;
        case FT_PEAK: flt_stage(v, 0, vl, rbuf, n, SM_PEAK, cut, k, gin); break;
        case FT_LP18:
            flt_stage(v, 0, vl, rbuf, n, SM_LP, cut, k, gin);
            flt_pole(v, vl, rbuf, n, 0, cut);
            break;
        case FT_LP24:                           /* the second stage: the same cutoff, flat */
            flt_stage(v, 0, vl, rbuf, n, SM_LP, cut, k, gin);
            flt_stage(v, 1, vl, rbuf, n, SM_LP, cut, 92682, 256);
            break;
        case FT_LPHOT:                          /* ... and the curve between the two */
            flt_stage(v, 0, vl, rbuf, n, SM_LP, cut, k, gin);
            pre_run(vl, n, 420, 1);
            if (stereo)
                pre_run(vr, n, 420, 1);
            flt_stage(v, 1, vl, rbuf, n, SM_LP, cut, 92682, 256);
            break;
        case FT_HP24:
            flt_stage(v, 0, vl, rbuf, n, SM_HP, cut, k, gin);
            flt_stage(v, 1, vl, rbuf, n, SM_HP, cut, 92682, 256);
            break;
        case FT_BP24:
            flt_stage(v, 0, vl, rbuf, n, SM_BP, cut, k, gin);
            flt_stage(v, 1, vl, rbuf, n, SM_BPN, cut, 65536, 256);
            break;
        case FT_NOTCH2:                         /* a second notch above: SHAPE is how far (5 st .. 3 octaves) */
            flt_stage(v, 0, vl, rbuf, n, SM_NOTCH, cut, k, gin);
            flt_stage(v, 1, vl, rbuf, n, SM_NOTCH, cut + 5 * 256 + var * 62, k, 256);
            break;
        case FT_BANDW:                          /* low-pass, then a high-pass below: SHAPE is the band's width */
            flt_stage(v, 0, vl, rbuf, n, SM_LP, cut, k, gin);
            flt_stage(v, 1, vl, rbuf, n, SM_HP, cut - 12 * 256 - var * 97, 92682, 256);
            break;
        case FT_TWIN:                           /* two band-passes side by side: SHAPE is how far apart */
        case FT_VOWEL: {                        /* ... or on a vowel's two formants: CUTOFF goes A E I O U */
            static int32_t tl[SUBN], tr[SUBN];
            int32_t c1 = cut, c2 = cut + 3 * 256 + var * 66;
            if (ftype == FT_VOWEL) {
                static const int16_t F1[6] = {19907, 18483, 15488, 18816, 15974, 15974};    /* semitones Q8 */
                static const int16_t F2[6] = {21683, 24013, 24986, 20531, 20685, 20685};
                int32_t pos = clampi(((cut - 16 * 256) * 9) >> 8, 0, 1023), ix = pos >> 8, fr = pos & 255;
                c1 = F1[ix] + (((F1[ix + 1] - F1[ix]) * fr) >> 8) + var * 24;       /* SHAPE: a smaller mouth */
                c2 = F2[ix] + (((F2[ix + 1] - F2[ix]) * fr) >> 8) + var * 24;
            }
            for (i = 0; i < n; i++) {
                tl[i] = vl[i];
                if (stereo)
                    tr[i] = vr[i];
            }
            flt_stage(v, 0, vl, rbuf, n, SM_BPN, c1, k, 256);
            flt_stage(v, 1, tl, stereo ? tr : 0, n, SM_BPN, c2, k, 256);
            for (i = 0; i < n; i++) {
                vl[i] += tl[i];
                if (stereo)
                    vr[i] += tr[i];
            }
            break;
        }
        case FT_COMBP:
        case FT_COMBM: flt_comb(v, vl, rbuf, n, cut, res, var, ftype == FT_COMBM); break;
        case FT_DECIM: {                        /* fewer frames (CUTOFF) and fewer bits (SHAPE) */
            uint32_t inc = pitch_inc(cut + 24 * 256), ph = v->dph, sh = (uint32_t)(var * 11) >> 7;
            int32_t hl = v->dh[0], hr = v->dh[1];
            for (i = 0; i < n; i++) {
                uint32_t q = ph + inc;
                if (q < ph) {
                    hl = (vl[i] >> sh) * (1 << sh);
                    if (stereo)
                        hr = (vr[i] >> sh) * (1 << sh);
                }
                ph = q;
                vl[i] = hl;
                if (stereo)
                    vr[i] = hr;
            }
            v->dph = ph;
            v->dh[0] = hl;
            v->dh[1] = hr;
            break;
        }
        default: {                              /* RING: times a sine at CUTOFF; SHAPE is how much */
            uint32_t inc = pitch_inc(cut), ph = v->rph;
            int32_t m = 64 + (var >> 1);
            for (i = 0; i < n; i++) {
                int32_t c;
                ph += inc;
                c = sinr[ph >> 22];
                vl[i] += (((vl[i] * c) >> 15) - vl[i]) * m >> 7;
                if (stereo)
                    vr[i] += (((vr[i] * c) >> 15) - vr[i]) * m >> 7;
            }
            v->rph = ph;
            break;
        }
        }
    }

    /* out, the gains moving to this tick's (Q12 here: with the voice held to +-4.0 the product fits 32 bits) */
    {
        int32_t gl = v->gl, gr = v->gr;
        if (stereo) {
            for (i = 0; i < n; i++) {
                gl += dgl;
                gr += dgr;
                outl[i] += (clampi(vl[i], -131071, 131071) * (gl >> 11)) >> 12;
                outr[i] += (clampi(vr[i], -131071, 131071) * (gr >> 11)) >> 12;
            }
        } else {
            for (i = 0; i < n; i++) {
                int32_t x = clampi(vl[i], -131071, 131071);
                gl += dgl;
                gr += dgr;
                outl[i] += (x * (gl >> 11)) >> 12;
                outr[i] += (x * (gr >> 11)) >> 12;
            }
        }
        v->gl = tl;
        v->gr = tr;
    }
    if (gt.sub_lvl && PV(SUB_DIR)) {            /* the sub past the filter: the bass stays whole */
        int32_t g = (gt.sub_lvl * amp) >> 15, to = g << 9, d = per_frame(to - v->sgl, n), gg = v->sgl;
        for (i = 0; i < n; i++) {
            int32_t s;
            gg += d;
            s = (sub[i] * (gg >> 11)) >> 12;
            outl[i] += s;
            outr[i] += s;
        }
        v->sgl = to;
    }
    return 1;
}

/* a new sound: every voice lets go fast */
static void eng_hush(void)
{
    uint32_t i;
    for (i = 0; i < NV; i++) {
        voice[i].on = voice[i].on ? 2 : 0;
        voice[i].held = 0;
        voice[i].kill = 1;
    }
    mono_n = 0;
}

#endif
