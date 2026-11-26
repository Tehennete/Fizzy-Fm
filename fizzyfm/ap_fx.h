/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Hugo Gilbert */
/* Fizzy-Fm's effects: seven slots in a row after the voices, each with one of fourteen effects and three knobs, in the
 * order you put them. An effect runs once: picked in two slots, it is the first one's. Then the way out: a limiter
 * that sees a little ahead, so that a big chord is turned down instead of cut flat. */
#ifndef AP_FX_H
#define AP_FX_H

#define CHO_N 1024u
#define FLA_N 512u
#define DIM_N 1024u
#define DLY_N 24000u                            /* half-rate frames: 1.09 s */
static int16_t cho_l[CHO_N], cho_r[CHO_N], fla_l[FLA_N], fla_r[FLA_N], dim_l[DIM_N], dim_r[DIM_N];
static int16_t dly_l[DLY_N], dly_r[DLY_N];
static const uint16_t AP_LEN[4] = {71, 53, 191, 139};            /* the reverb's diffusers */
static const uint16_t RV_LEN[4] = {709, 853, 967, 1103};         /* and its four lines (half-rate frames) */
static int16_t ap_buf[4][192], rv_buf[4][1104];
static struct {
    uint32_t live;                              /* a bit per effect that ran last time (its memory is not clean) */
    int32_t drv_lp[2];
    int32_t crush[2];
    uint32_t crush_n;
    int32_t cmp_env, cmp_g;
    int32_t eq_lo[2], eq_hi[2];
    int32_t flt_ic[2][2], flt_cut;
    uint32_t pha_ph;
    int32_t pha_x[2][4], pha_y[2][4], pha_fb[2];
    uint32_t fla_w, fla_ph;
    int32_t fla_fb[2];
    uint32_t cho_w, cho_ph;
    uint32_t dim_w, dim_ph;
    uint32_t trm_ph;
    uint32_t pmp_ph;
    int32_t pmp_g;
    uint32_t dly_w, dly_odd;
    int32_t dly_d;                              /* the delay now, frames Q8 (it slides to the tempo's) */
    int32_t dly_lp[2], dly_in, dly_ol, dly_or, dly_pl, dly_pr;
    uint32_t ap_w[4], rv_w[4], rv_odd;
    int32_t rv_lp[4], rv_in, rv_ol, rv_or;
} fx;

static inline int16_t sat16(int32_t v) { return (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v); }
/* the bus is held on its way through the effects: their products stay in 32 bits. The bound is one whose multiples
 * by a power of two never read as an address in the chip's ROM (0xFFC00000..0xFFCFFFFF, -4194304..-3145729): JieLi's
 * compiler folds a bound and the scaling after it into one constant, and fm1-dock's build refuses such a constant
 * (260000 x 16 was one). tools/precheck.py checks every bound of this kind. */
#define BUS(x) clampi(x, -300000, 300000)
/* dry to wet by m (0..127) */
#define MIXIN(dry, wet, m) ((dry) + ((((wet) - (dry)) * (m)) >> 7))
/* a triangle from a phase: 0..65535 */
static inline int32_t tri16(uint32_t ph)
{
    int32_t t = (int32_t)(ph >> 15) - 65536;
    return t < 0 ? -t - 1 : t;
}

/* DRIVE: a soft curve; TONE dulls what it adds */
static void fx_drive(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
    int32_t g8 = (4096 + a * 226) >> 4, make = (4096 * 256) / (256 + a * 7), tc = 3000 + b * 234;
    uint32_t i, ch;
    for (ch = 0; ch < 2u; ch++) {
        int32_t *x = ch ? r : l, lp = fx.drv_lp[ch];
        for (i = 0; i < n; i++) {
            int32_t d = BUS(x[i]), t = clampi((d * g8) >> 8, -49150, 49150), u = (t * 21845) >> 15;
            int32_t u2 = (u * u) >> 15;
            t = ((t - ((((t * u2) >> 15) * 10923) >> 15)) * make) >> 12;
            lp += ((t - lp) * tc) >> 15;
            x[i] = MIXIN(d, lp, c);
        }
        fx.drv_lp[ch] = lp;
    }
}

/* FOLD: what goes over the top comes back down; BIAS leans it to one side */
static void fx_fold(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
    int32_t g8 = (4096 + a * 226) >> 4, bias = (b - 64) * 380;
    uint32_t i, ch;
    for (ch = 0; ch < 2u; ch++) {
        int32_t *x = ch ? r : l;
        for (i = 0; i < n; i++) {
            int32_t d = BUS(x[i]), t = ((d * g8) >> 8) + bias;
            uint32_t g = 12;
            t = clampi(t, -380000, 380000);
            while ((t > 30000 || t < -30000) && g--)
                t = t > 0 ? 60000 - t : -60000 - t;
            x[i] = MIXIN(d, ((t - bias / 2) * 22000) >> 15, c);
        }
    }
}

/* CRUSH: fewer bits, fewer frames */
static void fx_crush(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
    uint32_t bits = (uint32_t)(a * 13) >> 7, hold = 1u + (uint32_t)b / 4u, k = fx.crush_n, i;
    int32_t hl = fx.crush[0], hr = fx.crush[1];
    for (i = 0; i < n; i++) {
        int32_t dl = BUS(l[i]), dr = BUS(r[i]);
        if (!k) {
            hl = (clampi(dl, -32767, 32767) >> bits) * (1 << bits);
            hr = (clampi(dr, -32767, 32767) >> bits) * (1 << bits);
            k = hold;
        }
        k--;
        l[i] = MIXIN(dl, hl, c);
        r[i] = MIXIN(dr, hr, c);
    }
    fx.crush[0] = hl;
    fx.crush[1] = hr;
    fx.crush_n = k;
}

/* COMP: what is over the threshold is let through at a quarter; the level lost is given back */
static void fx_comp(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
    int32_t T = 22000 - a * 150, rel = 2 + ((127 - b) * (127 - b)) / 200, env = fx.cmp_env, g = fx.cmp_g, to, dg;
    int32_t make = (22000 << 10) / (T + ((22000 - T) >> 2));    /* Q10 */
    uint32_t i;
    if (!g)
        g = 1024 << 8;
    to = env > T ? (int32_t)((((uint32_t)(T + ((env - T) >> 2))) << 10) / (uint32_t)env) : 1024;
    to = ((to * make) >> 10) << 8;
    dg = per_frame(to - g, n);
    for (i = 0; i < n; i++) {
        int32_t dl = BUS(l[i]), dr = BUS(r[i]), pk = dl < 0 ? -dl : dl, q = dr < 0 ? -dr : dr;
        if (q > pk)
            pk = q;
        if (pk > env)
            env += (pk - env) >> 3;
        else
            env -= (env >> 8) * rel >> 8;
        g += dg;
        l[i] = MIXIN(dl, (dl * (g >> 8)) >> 10, c);
        r[i] = MIXIN(dr, (dr * (g >> 8)) >> 10, c);
    }
    fx.cmp_env = env;
    fx.cmp_g = to;
}

/* EQ: three bands (under 200 Hz, over 3.5 kHz, and between), each from -12 to +12 dB */
static void fx_eq(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
#define EQ_G(v) ((v) >= 64 ? 256 + ((v) - 64) * 12 : 64 + (v) * 3)      /* Q8: 0.25 .. 1 .. 4 */
    int32_t gl = EQ_G(a), gm = EQ_G(b), gh = EQ_G(c);
    uint32_t i, ch;
    for (ch = 0; ch < 2u; ch++) {
        int32_t *x = ch ? r : l, lo = fx.eq_lo[ch], hi = fx.eq_hi[ch];
        for (i = 0; i < n; i++) {
            int32_t d = BUS(x[i]) * 16;         /* (16 times finer: the low band's slow filter needs it) */
            lo += mulh(d - lo, 61200000) * 2;   /* 200 Hz */
            hi += mulh(d - hi, 843000000) * 2;  /* 3.5 kHz */
            x[i] = (((lo >> 4) * gl) + (((hi - lo) >> 4) * gm) + (((d - hi) >> 4) * gh)) >> 8;
        }
        fx.eq_lo[ch] = lo;
        fx.eq_hi[ch] = hi;
    }
}

/* FILTER: one for the whole sound; LP>HP goes from low-pass by band-pass to high-pass */
static void fx_filter(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
    fcoef_t f;
    int32_t k29, wa, wb;
    uint32_t i, ch;
    fx.flt_cut += ((16 * 256 + a * 240) - fx.flt_cut) >> 2;
    filter_coef(&f, fx.flt_cut, 131072 - b * 1011);
    k29 = f.k << 13;
    wa = c < 64 ? c * 2 : (c - 64) * 2;         /* 0..127: from the first of the pair to the second */
    wb = c < 64;
    for (ch = 0; ch < 2u; ch++) {
        int32_t *x = ch ? r : l, ic1 = fx.flt_ic[ch][0], ic2 = fx.flt_ic[ch][1];
        for (i = 0; i < n; i++) {
            int32_t v0 = BUS(x[i]) * 16, v3 = v0 - ic2, v1, v2, bp, y;
            v1 = (mulh(f.a1, ic1) + mulh(f.a2, v3)) * 4;
            v2 = ic2 + (mulh(f.a2, ic1) + mulh(f.a3, v3)) * 4;
            ic1 = 2 * v1 - ic1;
            ic2 = 2 * v2 - ic2;
            bp = mulh(k29, v1) * 8;
            y = wb ? v2 + (((bp - v2) >> 7) * wa) : bp + (((v0 - bp - v2 - bp) >> 7) * wa);
            x[i] = y >> 4;
        }
        fx.flt_ic[ch][0] = clampi(ic1, -(1 << 29), 1 << 29);
        fx.flt_ic[ch][1] = clampi(ic2, -(1 << 29), 1 << 29);
    }
}

/* PHASER: four all-passes swept between 200 Hz and 4 kHz, the right side a quarter turn behind */
static void fx_phaser(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
    uint32_t i, ch, k;
    int32_t fb = b * 230;
    fx.pha_ph += (uint32_t)(4000 + a * a * 28) * n;             /* 0.04 .. 4.6 Hz */
    for (ch = 0; ch < 2u; ch++) {
        int32_t *x = ch ? r : l, last = fx.pha_fb[ch], *px = fx.pha_x[ch], *py = fx.pha_y[ch], cf;
        int32_t pitch = 55 * 256 + (((tri16(fx.pha_ph + (ch ? 0x40000000u : 0u)) >> 2) * 13) >> 6);   /* 200 Hz .. 4 kHz */
        uint32_t ix = (uint32_t)(pitch - 2048) >> 7, g = AP_GTAB[ix] >> 8;                    /* tan, Q16 */
        cf = (int32_t)(((65536u - g) * 16384u) / ((65536u + g) >> 1));                         /* (1-g)/(1+g), Q15 */
        cf = (cf > 32767 ? 32767 : cf) * 65536;
        for (i = 0; i < n; i++) {
            int32_t d = BUS(x[i]), y = d + (((last >> 3) * fb) >> 12);
            for (k = 0; k < 4u; k++) {
                int32_t o = px[k] + mulh(py[k] - y, cf) * 2;
                px[k] = y;
                py[k] = o;
                y = o;
            }
            last = clampi(y, -180000, 180000);
            x[i] = MIXIN(d, last, c);
        }
        fx.pha_fb[ch] = last;
    }
}

/* FLANGER: a very short echo, its time swept from 1 to 7 ms, fed back */
static void fx_flanger(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
    uint32_t i, w = fx.fla_w, ph = fx.fla_ph;
    int32_t fb = b * 240, fl = fx.fla_fb[0], fr = fx.fla_fb[1];
    for (i = 0; i < n; i++) {
        int32_t dl = BUS(l[i]), dr = BUS(r[i]), d1, d2, p, q, s;
        ph += 2000u + (uint32_t)(a * a) * 30u;  /* 0.02 .. 5 Hz */
        d1 = (44 << 8) + ((tri16(ph) * 266) >> 8);              /* frames Q8 */
        d2 = (44 << 8) + ((tri16(ph + 0x30000000u) * 266) >> 8);
        fla_l[w] = sat16((dl >> 2) + (((fl >> 2) * fb) >> 15));         /* (its memory holds a quarter of the level) */
        fla_r[w] = sat16((dr >> 2) + (((fr >> 2) * fb) >> 15));
        p = (int32_t)((w - (uint32_t)(d1 >> 8)) & (FLA_N - 1u));
        q = (int32_t)((uint32_t)(p - 1) & (FLA_N - 1u));
        s = fla_l[p];
        fl = (s + (((fla_l[q] - s) * (d1 & 255)) >> 8)) * 4;
        p = (int32_t)((w - (uint32_t)(d2 >> 8)) & (FLA_N - 1u));
        q = (int32_t)((uint32_t)(p - 1) & (FLA_N - 1u));
        s = fla_r[p];
        fr = (s + (((fla_r[q] - s) * (d2 & 255)) >> 8)) * 4;
        l[i] = dl + (((fl - (dl >> 1)) * c) >> 7);
        r[i] = dr + (((fr - (dr >> 1)) * c) >> 7);
        w = (w + 1u) & (FLA_N - 1u);
    }
    fx.fla_w = w;
    fx.fla_ph = ph;
    fx.fla_fb[0] = fl;
    fx.fla_fb[1] = fr;
}

/* CHORUS: 9 ms away, moving by up to 6 ms, the two sides a quarter turn apart */
static void fx_chorus(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
    uint32_t i, w = fx.cho_w, ph = fx.cho_ph;
    for (i = 0; i < n; i++) {
        int32_t dl = BUS(l[i]), dr = BUS(r[i]), d1, d2, s, wl, wr;
        uint32_t p;
        ph += 20000u + (uint32_t)a * 1500u;     /* 0.2 .. 2.2 Hz */
        d1 = (400 << 8) + ((tri16(ph) * b) >> 7);
        d2 = (400 << 8) + ((tri16(ph + 0x40000000u) * b) >> 7);
        cho_l[w] = sat16(dl >> 2);
        cho_r[w] = sat16(dr >> 2);
        p = (w - (uint32_t)(d1 >> 8)) & (CHO_N - 1u);
        s = cho_l[p];
        wl = (s + (((cho_l[(p - 1u) & (CHO_N - 1u)] - s) * (d1 & 255)) >> 8)) * 4;
        p = (w - (uint32_t)(d2 >> 8)) & (CHO_N - 1u);
        s = cho_r[p];
        wr = (s + (((cho_r[(p - 1u) & (CHO_N - 1u)] - s) * (d2 & 255)) >> 8)) * 4;
        l[i] = dl + ((((wl - (dl >> 1)) >> 4) * c * 100) >> 10);        /* (a little less of the dry: the level stays) */
        r[i] = dr + ((((wr - (dr >> 1)) >> 4) * c * 100) >> 10);
        w = (w + 1u) & (CHO_N - 1u);
    }
    fx.cho_w = w;
    fx.cho_ph = ph;
}

/* DIMENS: two slow, opposite movements, each side taking a little of the other's the wrong way up: width without
 * the wobble of a chorus */
static void fx_dimens(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
    uint32_t i, w = fx.dim_w, ph = fx.dim_ph;
    int32_t base = (220 + a * 3) << 8;          /* 5 .. 13.6 ms */
    for (i = 0; i < n; i++) {
        int32_t dl = BUS(l[i]), dr = BUS(r[i]), t = (tri16(ph) - 32768) * b, d1 = base + (t >> 7), d2 = base - (t >> 7);
        int32_t s, wl, wr;
        uint32_t p;
        ph += 24000u;                           /* 0.25 Hz */
        dim_l[w] = sat16(dl >> 2);
        dim_r[w] = sat16(dr >> 2);
        p = (w - (uint32_t)(d1 >> 8)) & (DIM_N - 1u);
        s = dim_l[p];
        wl = (s + (((dim_l[(p - 1u) & (DIM_N - 1u)] - s) * (d1 & 255)) >> 8)) * 4;
        p = (w - (uint32_t)(d2 >> 8)) & (DIM_N - 1u);
        s = dim_r[p];
        wr = (s + (((dim_r[(p - 1u) & (DIM_N - 1u)] - s) * (d2 & 255)) >> 8)) * 4;
        l[i] = dl + (((wl - (wr >> 1) - (dl >> 2)) * c) >> 7);
        r[i] = dr + (((wr - (wl >> 1) - (dr >> 2)) * c) >> 7);
        w = (w + 1u) & (DIM_N - 1u);
    }
    fx.dim_w = w;
    fx.dim_ph = ph;
}

/* TREMOLO: the level moves; with PAN the two sides move apart, up to opposite */
static void fx_trem(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
    uint32_t i, ph = fx.trm_ph, inc = 48000u + (uint32_t)(a * a) * 105u, off = (uint32_t)c << 24;   /* 0.5 .. 18 Hz */
    for (i = 0; i < n; i++) {
        int32_t gl, gr;
        ph += inc;
        gl = 4096 - ((b * (32767 - sinr[ph >> 22])) >> 11);     /* Q12, down to 1 - depth */
        gr = 4096 - ((b * (32767 - sinr[(ph + off) >> 22])) >> 11);
        l[i] = (BUS(l[i]) * gl) >> 12;
        r[i] = (BUS(r[i]) * gr) >> 12;
    }
    fx.trm_ph = ph;
}

/* PUMP: the level ducks on the beat and comes back, as a compressor keyed by a kick would do it */
static void fx_pump(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
    uint32_t per = (uint32_t)(((uint64_t)DIV_TICKS[(a * 12) >> 7] * clk_spt) >> 16), inc = 0xFFFFFFFFu / (per ? per : 1u);
    uint32_t i, ph = fx.pmp_ph;
    int32_t g = fx.pmp_g ? fx.pmp_g : 4096 << 4;
    for (i = 0; i < n; i++) {
        int32_t e, to;
        ph += inc;
        e = 4095 - (int32_t)(ph >> 20);         /* 1 at the beat, down to 0 before the next */
        e += ((((e * e) >> 12) - e) * c) >> 7;  /* CURVE: back sooner */
        to = (4096 - ((b * e) >> 7)) << 4;
        g += (to - g) >> 5;                     /* (no click when it drops) */
        l[i] = (BUS(l[i]) * (g >> 4)) >> 12;
        r[i] = (BUS(r[i]) * (g >> 4)) >> 12;
    }
    fx.pmp_ph = ph;
    fx.pmp_g = g;
}

/* DELAY: left, then right, then left again, in time with the tempo. Its memory runs at half the sample rate (a
 * darker echo, twice the time): two frames in are one frame kept, one frame read is two frames out */
static void fx_delay(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
    int32_t fb = b * 240, tone = 24000, d = fx.dly_d, to, lpl = fx.dly_lp[0], lpr = fx.dly_lp[1];
    uint32_t i, w = fx.dly_w;
    uint64_t t = ((uint64_t)DIV_TICKS[(a * 12) >> 7] * clk_spt) >> 17;   /* half-rate frames */
    while (t > DLY_N - 4u)                      /* longer than its memory: half, and half again */
        t >>= 1;
    to = (int32_t)t << 8;
    if (!d)
        d = to;
    for (i = 0; i < n; i++) {
        int32_t dl = BUS(l[i]), dr = BUS(r[i]), in = (dl + dr) >> 1;
        if (fx.dly_odd) {
            int32_t x, y, rl, rr, p, q;
            d += (to - d) >> 10;                /* a new time: it slides there, as a tape would */
            p = (int32_t)w - (d >> 8);
            if (p < 0)
                p += (int32_t)DLY_N;
            q = p ? p - 1 : (int32_t)DLY_N - 1;
            x = dly_l[p];
            y = dly_l[q];
            rl = x + (((y - x) * (d & 255)) >> 8);              /* (its memory holds a quarter of the level) */
            x = dly_r[p];
            y = dly_r[q];
            rr = x + (((y - x) * (d & 255)) >> 8);
            lpl += ((rl - lpl) * tone) >> 15;
            lpr += ((rr - lpr) * tone) >> 15;
            dly_l[w] = sat16(((fx.dly_in + in) >> 3) + ((lpr * fb) >> 15));
            dly_r[w] = sat16(lpl);
            if (++w >= DLY_N)
                w = 0;
            fx.dly_pl = fx.dly_ol;
            fx.dly_pr = fx.dly_or;
            fx.dly_ol = rl;
            fx.dly_or = rr;
            l[i] = dl + ((((fx.dly_pl + rl) >> 1) * c) >> 5);           /* halfway to the new frame, then the frame */
            r[i] = dr + ((((fx.dly_pr + rr) >> 1) * c) >> 5);
        } else {
            fx.dly_in = in;
            l[i] = dl + ((fx.dly_ol * c) >> 5);
            r[i] = dr + ((fx.dly_or * c) >> 5);
        }
        fx.dly_odd ^= 1u;
    }
    fx.dly_w = w;
    fx.dly_d = d;
    fx.dly_lp[0] = lpl;
    fx.dly_lp[1] = lpr;
}

/* REVERB: four all-passes smear the input, then four lines feed one another, a little duller each time round.
 * Like the echo it runs at half the sample rate */
static void fx_reverb(int32_t *l, int32_t *r, uint32_t n, int32_t a, int32_t b, int32_t c)
{
    int32_t mix = c * 200, g = 19660 + a * 96, damp = 32000 - b * 200;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t dl = BUS(l[i]), dr = BUS(r[i]), in = (dl + dr) >> 4;    /* (a quarter: four voices of headroom) */
        if (fx.rv_odd) {
            int32_t x = (fx.rv_in + in) >> 1, d0, d1, d2, d3, s, f, ol, or_;
#define RV_AP(K) { \
                uint32_t w_ = fx.ap_w[K]; \
                int32_t z_ = ap_buf[K][w_], y_ = z_ - ((x * 19660) >> 15); \
                ap_buf[K][w_] = sat16(x + ((y_ * 19660) >> 15)); \
                fx.ap_w[K] = w_ + 1u >= AP_LEN[K] ? 0u : w_ + 1u; \
                x = y_; \
            }
#define RV_LINE(K, D) { \
                uint32_t w_ = fx.rv_w[K]; \
                f = (((D) - s) * (g >> 1)) >> 14; \
                fx.rv_lp[K] += ((f - fx.rv_lp[K]) * damp) >> 15; \
                rv_buf[K][w_] = sat16(x + fx.rv_lp[K]); \
                fx.rv_w[K] = w_ + 1u >= RV_LEN[K] ? 0u : w_ + 1u; \
            }
            RV_AP(0) RV_AP(1) RV_AP(2) RV_AP(3)
            d0 = rv_buf[0][fx.rv_w[0]];
            d1 = rv_buf[1][fx.rv_w[1]];
            d2 = rv_buf[2][fx.rv_w[2]];
            d3 = rv_buf[3][fx.rv_w[3]];
            s = (d0 + d1 + d2 + d3) >> 1;
            RV_LINE(0, d0) RV_LINE(1, d1) RV_LINE(2, d2) RV_LINE(3, d3)
            ol = d0 - d2;
            or_ = d1 - d3;
            l[i] = dl + ((((fx.rv_ol + ol) >> 1) * mix) >> 13);         /* halfway to the new frame, then the frame */
            r[i] = dr + ((((fx.rv_or + or_) >> 1) * mix) >> 13);
            fx.rv_ol = ol;
            fx.rv_or = or_;
        } else {
            fx.rv_in = in;
            l[i] = dl + ((fx.rv_ol * mix) >> 13);
            r[i] = dr + ((fx.rv_or * mix) >> 13);
        }
        fx.rv_odd ^= 1u;
    }
}

/* an effect no slot holds any more: its memory is cleared, so that it starts clean next time */
static void fx_clear(uint32_t t)
{
    uint32_t i, k;
    switch (t) {
    case FX_DRIVE: fx.drv_lp[0] = fx.drv_lp[1] = 0; break;
    case FX_COMP: fx.cmp_env = fx.cmp_g = 0; break;
    case FX_EQ: fx.eq_lo[0] = fx.eq_lo[1] = fx.eq_hi[0] = fx.eq_hi[1] = 0; break;
    case FX_FILTER: fx.flt_ic[0][0] = fx.flt_ic[0][1] = fx.flt_ic[1][0] = fx.flt_ic[1][1] = 0; break;
    case FX_PHASER:
        for (i = 0; i < 4u; i++)
            fx.pha_x[0][i] = fx.pha_x[1][i] = fx.pha_y[0][i] = fx.pha_y[1][i] = 0;
        fx.pha_fb[0] = fx.pha_fb[1] = 0;
        break;
    case FX_FLANGER:
        for (i = 0; i < FLA_N; i++)
            fla_l[i] = fla_r[i] = 0;
        fx.fla_fb[0] = fx.fla_fb[1] = 0;
        break;
    case FX_CHORUS:
        for (i = 0; i < CHO_N; i++)
            cho_l[i] = cho_r[i] = 0;
        break;
    case FX_DIMENS:
        for (i = 0; i < DIM_N; i++)
            dim_l[i] = dim_r[i] = 0;
        break;
    case FX_PUMP: fx.pmp_g = 0; break;
    case FX_DELAY:
        for (i = 0; i < DLY_N; i++)
            dly_l[i] = dly_r[i] = 0;
        fx.dly_d = fx.dly_lp[0] = fx.dly_lp[1] = fx.dly_in = fx.dly_ol = fx.dly_or = fx.dly_pl = fx.dly_pr = 0;
        break;
    case FX_REVERB:
        for (k = 0; k < 4u; k++) {
            for (i = 0; i < 192u; i++)
                ap_buf[k][i] = 0;
            for (i = 0; i < 1104u; i++)
                rv_buf[k][i] = 0;
            fx.rv_lp[k] = 0;
        }
        fx.rv_in = fx.rv_ol = fx.rv_or = 0;
        break;
    default: break;
    }
}

static void fx_run(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t s, seen = 0, gone, t;
    for (s = 0; s < NFX; s++) {
        const uint8_t *q = &cur.p[P_FX1_TYPE + 4u * s];
        int32_t a = q[1], b = q[2], c = q[3];
        t = q[0];
        if (!t || t >= NFXT || (seen >> t & 1u))
            continue;
        seen |= 1u << t;
        switch (t) {
        case FX_DRIVE: fx_drive(l, r, n, a, b, c); break;
        case FX_FOLD: fx_fold(l, r, n, a, b, c); break;
        case FX_CRUSH: fx_crush(l, r, n, a, b, c); break;
        case FX_COMP: fx_comp(l, r, n, a, b, c); break;
        case FX_EQ: fx_eq(l, r, n, a, b, c); break;
        case FX_FILTER: fx_filter(l, r, n, a, b, c); break;
        case FX_PHASER: fx_phaser(l, r, n, a, b, c); break;
        case FX_FLANGER: fx_flanger(l, r, n, a, b, c); break;
        case FX_CHORUS: fx_chorus(l, r, n, a, b, c); break;
        case FX_DIMENS: fx_dimens(l, r, n, a, b, c); break;
        case FX_TREM: fx_trem(l, r, n, a, b, c); break;
        case FX_PUMP: fx_pump(l, r, n, a, b, c); break;
        case FX_DELAY: fx_delay(l, r, n, a, b, c); break;
        default: fx_reverb(l, r, n, a, b, c); break;
        }
    }
    gone = fx.live & ~seen;
    for (t = 1; gone && t < NFXT; t++)
        if (gone >> t & 1u)
            fx_clear(t);
    fx.live = seen;
}

/* ---- the way out: nothing under 13 Hz (a warp can leave a wave off centre), then a limiter that sees a tick
 * ahead: a chord too big for 16 bits is turned down before its peak arrives, not cut off. The sound leaves one
 * tick (0.7 ms) late for it. */
#define LIM_T 24000
static volatile int32_t out_lvl;                /* the loudest a block went out since the screen last looked */
static struct {
    int32_t dc_x[2], dc_y[2];
    int32_t fifo_l[SUBN], fifo_r[SUBN];
    uint32_t pos;
    int32_t g;                                  /* the gain now, Q12 << 8 */
    int32_t need[3];                            /* the gain the last three ticks' peaks ask for, Q12 */
} mst = {{0, 0}, {0, 0}, {0}, {0}, 0, 4096 << 8, {4096, 4096, 4096}};

static void master_run(int32_t *l, int32_t *r, int32_t *out, uint32_t n)
{
    int32_t peak = 0, need, to, dg, g = mst.g, x0 = mst.dc_x[0], x1 = mst.dc_x[1], y0 = mst.dc_y[0], y1 = mst.dc_y[1];
    uint32_t i, pos = mst.pos;
    for (i = 0; i < n; i++) {
        int32_t xl = l[i], xr = r[i], a, b;
        y0 += (xl - x0) * 256 - (y0 >> 9);      /* (kept 256 times finer: it comes to rest within a step of 0) */
        y1 += (xr - x1) * 256 - (y1 >> 9);
        x0 = xl;
        x1 = xr;
        a = y0 >> 8;
        b = y1 >> 8;
        l[i] = a;
        r[i] = b;
        a = a < 0 ? -a : a;
        b = b < 0 ? -b : b;
        if (b > a) a = b;
        if (a > peak) peak = a;
    }
    mst.dc_x[0] = x0;
    mst.dc_x[1] = x1;
    mst.dc_y[0] = y0;
    mst.dc_y[1] = y1;
    if (peak < 3) {                             /* the last crumbs of an echo or a reverb: silence */
        for (i = 0; i < n; i++)
            l[i] = r[i] = 0;
    } else if (peak > 300000) {                 /* far over the top (the gain's product must fit 32 bits) */
        for (i = 0; i < n; i++) {
            l[i] = clampi(l[i], -300000, 300000);
            r[i] = clampi(r[i], -300000, 300000);
        }
        peak = 300000;
    }
    need = peak > LIM_T ? (int32_t)(((uint32_t)LIM_T << 12) / (uint32_t)peak) : 4096;
    mst.need[2] = mst.need[1];
    mst.need[1] = mst.need[0];
    mst.need[0] = need;
    to = g + (((4096 << 8) - g) >> 8) + 1;      /* back up slowly (0.2 s) */
    for (i = 0; i < 3u; i++)
        if (to > mst.need[i] << 8)
            to = mst.need[i] << 8;              /* down at once: there by the end of this tick */
    dg = per_frame(to - g, n);
    for (i = 0; i < n; i++) {
        int32_t yl = mst.fifo_l[pos], yr = mst.fifo_r[pos];
        mst.fifo_l[pos] = l[i];
        mst.fifo_r[pos] = r[i];
        if (++pos >= SUBN)
            pos = 0;
        g += dg;
        out[2u * i] += (yl * (g >> 8)) >> 12;
        out[2u * i + 1u] += (yr * (g >> 8)) >> 12;
    }
    mst.g = to;
    mst.pos = pos;
    peak = (peak * (to >> 8)) >> 12;
    if (peak > out_lvl)
        out_lvl = peak;
}

#endif
