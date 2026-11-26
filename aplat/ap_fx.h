/* SPDX-License-Identifier: GPL-3.0-only */
/* APLAT's effects, in a row after the voices: drive, chorus, an echo in time with the tempo, a reverb, and a
 * soft ceiling so that a big chord rounds off instead of cutting flat. */
#ifndef AP_FX_H
#define AP_FX_H

#define CHO_N 1024u
#define DLY_N 24000u                            /* half-rate frames: 1.09 s */
static int16_t cho_l[CHO_N], cho_r[CHO_N];
static int16_t dly_l[DLY_N], dly_r[DLY_N];
static const uint16_t AP_LEN[4] = {71, 53, 191, 139};            /* the reverb's diffusers */
static const uint16_t RV_LEN[4] = {709, 853, 967, 1103};         /* and its four lines (half-rate frames) */
static int16_t ap_buf[4][192], rv_buf[4][1104];
static struct {
    uint32_t cho_w, cho_ph;
    uint32_t dly_w;
    int32_t dly_d;                              /* the delay now, frames Q8 (it slides to the tempo's) */
    int32_t dly_lp[2];
    int32_t dly_in, dly_ol, dly_or, dly_pl, dly_pr;
    uint32_t dly_odd;
    uint32_t ap_w[4], rv_w[4];
    int32_t rv_lp[4];
    int32_t rv_in, rv_ol, rv_or;
    uint32_t rv_odd;
    int32_t crush_l, crush_r;
    uint32_t crush_n;
} fx;

static inline int16_t sat16(int32_t v) { return (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v); }

static void fx_drive(int32_t *l, int32_t *r, uint32_t n)
{
    int32_t amt = PV(DRV_AMT), gain = 4096 + amt * 226;         /* up to 8 x in, and less out to match */
    int32_t make = (32767 * 4096) / (4096 + amt * 113);
    uint32_t type = (uint32_t)PV(DRV_TYPE), i, c;
    if (!amt)
        return;
    for (c = 0; c < 2u; c++) {
        int32_t *x = c ? r : l;
        switch (type) {
        case 0:                                 /* SOFT: the curve of the filter's input */
            for (i = 0; i < n; i++) {
                int32_t t = clampi((int32_t)(((int64_t)x[i] * gain) >> 12), -49150, 49150), u = (t * 21845) >> 15;
                int32_t u2 = (u * u) >> 15;
                x[i] = ((t - ((((t * u2) >> 15) * 10923) >> 15)) * make) >> 15;
            }
            break;
        case 1:                                 /* HARD */
            for (i = 0; i < n; i++)
                x[i] = (clampi((int32_t)(((int64_t)x[i] * gain) >> 12), -26000, 26000) * make) >> 15;
            break;
        case 2:                                 /* FOLD: what goes over the top comes back down */
            for (i = 0; i < n; i++) {
                int32_t t = clampi((int32_t)(((int64_t)x[i] * gain) >> 12), -250000, 250000);
                uint32_t g = 8;
                while ((t > 30000 || t < -30000) && g--)
                    t = t > 0 ? 60000 - t : -60000 - t;
                x[i] = (t * 22000) >> 15;
            }
            break;
        default: {                              /* CRUSH: fewer bits, fewer frames */
            uint32_t bits = 2u + (uint32_t)amt / 14u, hold = 1u + (uint32_t)amt / 6u, k = fx.crush_n;
            int32_t h = c ? fx.crush_r : fx.crush_l;
            for (i = 0; i < n; i++) {
                if (!k) {
                    h = (clampi(x[i], -32767, 32767) >> bits) * (1 << bits);
                    k = hold;
                }
                k--;
                x[i] = h;
            }
            if (c) { fx.crush_r = h; fx.crush_n = k; } else fx.crush_l = h;   /* (both sides count alike) */
            break;
        }
        }
    }
}

static void fx_chorus(int32_t *l, int32_t *r, uint32_t n)
{
    int32_t mix = PV(CHO_MIX) * 200, depth = PV(CHO_DEPTH);      /* the wet up to 0.78 */
    uint32_t i, w = fx.cho_w, ph = fx.cho_ph;
    if (!mix) {
        if (fx.cho_w != 0xFFFFFFFFu) {          /* off: its memory cleared once, so it starts clean */
            for (i = 0; i < CHO_N; i++)
                cho_l[i] = cho_r[i] = 0;
            fx.cho_w = 0xFFFFFFFFu;
        }
        return;
    }
    if (w == 0xFFFFFFFFu)
        w = 0;
    for (i = 0; i < n; i++) {
        int32_t tl, tr, dl, dr, a, wl, wr;
        uint32_t p;
        ph += 58000u + (uint32_t)depth * 300u;  /* 0.6 .. 1 Hz */
        tl = (int32_t)(ph >> 15) - 65536;       /* two triangles a quarter apart, 0..65535 */
        tl = tl < 0 ? -tl : tl;
        tr = (int32_t)((ph + 0x40000000u) >> 15) - 65536;
        tr = tr < 0 ? -tr : tr;
        dl = (400 << 8) + ((tl * depth) >> 7);  /* 9 ms + up to 5.8 ms, frames Q8 */
        dr = (400 << 8) + ((tr * depth) >> 7);
        cho_l[w] = sat16(l[i]);
        cho_r[w] = sat16(r[i]);
        p = (w - (uint32_t)(dl >> 8)) & (CHO_N - 1u);
        a = cho_l[p];
        wl = a + (((cho_l[(p - 1u) & (CHO_N - 1u)] - a) * (dl & 255)) >> 8);
        p = (w - (uint32_t)(dr >> 8)) & (CHO_N - 1u);
        a = cho_r[p];
        wr = a + (((cho_r[(p - 1u) & (CHO_N - 1u)] - a) * (dr & 255)) >> 8);
        l[i] += ((wl - (l[i] >> 1)) * mix) >> 15;       /* (and a little less of the dry: the level stays) */
        r[i] += ((wr - (r[i] >> 1)) * mix) >> 15;
        w = (w + 1u) & (CHO_N - 1u);
    }
    fx.cho_w = w;
    fx.cho_ph = ph;
}

/* the echo: left, then right, then left again. Its memory runs at half the sample rate (a darker echo, twice the
 * time): two frames in are one frame kept, one frame read is two frames out */
static void fx_delay(int32_t *l, int32_t *r, uint32_t n)
{
    int32_t mix = PV(DLY_MIX) * 258, fb = PV(DLY_FB) * 240, tone = 4000 + PV(DLY_TONE) * 226, d = fx.dly_d;
    int32_t to, lpl = fx.dly_lp[0], lpr = fx.dly_lp[1];
    uint32_t i, w = fx.dly_w;
    uint64_t t = ((uint64_t)DIV_TICKS[PV(DLY_TIME)] * clk_spt) >> 17;   /* half-rate frames */
    if (!mix) {
        if (w != 0xFFFFFFFFu) {
            for (i = 0; i < DLY_N; i++)
                dly_l[i] = dly_r[i] = 0;
            fx.dly_w = 0xFFFFFFFFu;
            fx.dly_lp[0] = fx.dly_lp[1] = 0;
            fx.dly_in = fx.dly_ol = fx.dly_or = fx.dly_pl = fx.dly_pr = 0;
            fx.dly_odd = 0;
        }
        return;
    }
    while (t > DLY_N - 4u)                      /* longer than its memory: half, and half again */
        t >>= 1;
    to = (int32_t)t << 8;
    if (w == 0xFFFFFFFFu) {
        w = 0;
        d = to;
    }
    for (i = 0; i < n; i++) {
        int32_t in = (l[i] + r[i]) >> 1;
        if (fx.dly_odd) {
            int32_t a, b, rl, rr, p, q;
            d += (to - d) >> 10;                /* a new time: it slides there, as a tape would */
            p = (int32_t)w - (d >> 8);
            if (p < 0)
                p += (int32_t)DLY_N;
            q = p ? p - 1 : (int32_t)DLY_N - 1;
            a = dly_l[p];
            b = dly_l[q];
            rl = a + (((b - a) * (d & 255)) >> 8);
            a = dly_r[p];
            b = dly_r[q];
            rr = a + (((b - a) * (d & 255)) >> 8);
            lpl += ((rl - lpl) * tone) >> 15;
            lpr += ((rr - lpr) * tone) >> 15;
            dly_l[w] = sat16(((fx.dly_in + in) >> 1) + ((lpr * fb) >> 15));
            dly_r[w] = sat16(lpl);
            if (++w >= DLY_N)
                w = 0;
            fx.dly_pl = fx.dly_ol;
            fx.dly_pr = fx.dly_or;
            fx.dly_ol = rl;
            fx.dly_or = rr;
            l[i] += (((fx.dly_pl + rl) >> 1) * mix) >> 15;      /* halfway to the new frame, then the frame */
            r[i] += (((fx.dly_pr + rr) >> 1) * mix) >> 15;
        } else {
            fx.dly_in = in;
            l[i] += (fx.dly_ol * mix) >> 15;
            r[i] += (fx.dly_or * mix) >> 15;
        }
        fx.dly_odd ^= 1u;
    }
    fx.dly_w = w;
    fx.dly_d = d;
    fx.dly_lp[0] = lpl;
    fx.dly_lp[1] = lpr;
}

/* the reverb: four all-passes smear the input, then four lines feed one another, a little duller each time
 * round. Like the echo it runs at half the sample rate */
static void fx_reverb(int32_t *l, int32_t *r, uint32_t n)
{
    int32_t mix = PV(REV_MIX) * 200, g = 19660 + PV(REV_DECAY) * 96, damp = 32000 - PV(REV_DAMP) * 200;
    uint32_t i, k;
    if (!mix) {
        if (fx.rv_w[0] != 0xFFFFFFFFu) {
            for (k = 0; k < 4u; k++) {
                for (i = 0; i < 192u; i++)
                    ap_buf[k][i] = 0;
                for (i = 0; i < 1104u; i++)
                    rv_buf[k][i] = 0;
                fx.rv_lp[k] = 0;
                fx.ap_w[k] = 0;
                fx.rv_w[k] = 0;
            }
            fx.rv_w[0] = 0xFFFFFFFFu;
            fx.rv_in = fx.rv_ol = fx.rv_or = 0;
            fx.rv_odd = 0;
        }
        return;
    }
    if (fx.rv_w[0] == 0xFFFFFFFFu)
        fx.rv_w[0] = 0;
    for (i = 0; i < n; i++) {
        int32_t in = (l[i] + r[i]) >> 2;
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
                f = (((D) - s) * g) >> 15; \
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
            l[i] += (((fx.rv_ol + ol) >> 1) * mix) >> 15;       /* halfway to the new frame, then the frame */
            r[i] += (((fx.rv_or + or_) >> 1) * mix) >> 15;
            fx.rv_ol = ol;
            fx.rv_or = or_;
        } else {
            fx.rv_in = in;
            l[i] += (fx.rv_ol * mix) >> 15;
            r[i] += (fx.rv_or * mix) >> 15;
        }
        fx.rv_odd ^= 1u;
    }
}

/* ---- the way out: nothing under 13 Hz (a warp can leave a wave off centre), then a limiter that sees a tick
 * ahead: a chord too big for 16 bits is turned down before its peak arrives, not cut off. The sound leaves one
 * tick (0.7 ms) late for it. */
#define LIM_T 24000
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
    } else if (peak > 260000) {                 /* far over the top (the gain's product must fit 32 bits) */
        for (i = 0; i < n; i++) {
            l[i] = clampi(l[i], -260000, 260000);
            r[i] = clampi(r[i], -260000, 260000);
        }
        peak = 260000;
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
}

static void fx_run(int32_t *l, int32_t *r, uint32_t n)
{
    fx_drive(l, r, n);
    fx_chorus(l, r, n);
    fx_delay(l, r, n);
    fx_reverb(l, r, n);
}

#endif
