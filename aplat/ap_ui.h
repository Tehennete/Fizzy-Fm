/* SPDX-License-Identifier: GPL-3.0-only */
/* APLAT's screen and panel: flat blocks of colour, one colour per section.
 *   24..55    the header: the page on its colour, the sound's number and name
 *   56..143   the picture of what the page edits (a wave, the filter, an envelope ...)
 *   144..239  four tiles, one per knob: name, value, a bar
 * The screen keeps what is drawn: draw() sends one strip a pass, only what changed. */
#ifndef AP_UI_H
#define AP_UI_H

#define U_BG    HB_RGB(26, 26, 26)
#define U_TILE  HB_RGB(52, 52, 52)
#define U_TRACK HB_RGB(34, 34, 34)
#define U_EDGE  HB_RGB(84, 84, 84)
#define U_TXT   HB_RGB(236, 236, 236)
#define U_DIM   HB_RGB(150, 150, 150)
#define U_INK   HB_RGB(18, 18, 18)
#define U_WARN  HB_RGB(255, 80, 70)
enum { K_PLAY, K_OSCA, K_OSCB, K_MIX, K_FILT, K_ENV, K_LFO, K_MOD, K_ARP, K_FX, K_VOICE, NCOL };
static const uint16_t COL[NCOL] = {
    HB_RGB(255, 148, 40), HB_RGB(255, 172, 48), HB_RGB(56, 196, 240), HB_RGB(236, 220, 70), HB_RGB(255, 96, 104),
    HB_RGB(104, 214, 108), HB_RGB(178, 140, 255), HB_RGB(255, 122, 196), HB_RGB(52, 214, 178), HB_RGB(94, 156, 255),
    HB_RGB(200, 200, 200)};

enum { DS_PLAY, DS_WAVE_A, DS_WAVE_B, DS_MIX, DS_FILT, DS_ENV, DS_LFO, DS_MOD, DS_ARP, DS_FX, DS_VOICE };
#define NOP 0xFFu
#define PG_SLOT 0xFEu                           /* the matrix: this tile picks the slot */
typedef struct {
    const char *name;
    uint8_t btn;                                /* the button that goes to it */
    uint8_t head;                               /* 1: that button stops here (else only SELECT does) */
    uint8_t col, disp, arg;
    uint8_t par[4];
} page_t;
static const page_t PAGES[] = {
    {"PLAY", HB_BTN_PLAY, 1, K_PLAY, DS_PLAY, 0, {P_M1, P_M2, P_M3, P_M4}},
    {"OSC A", HB_BTN_EDIT, 1, K_OSCA, DS_WAVE_A, 0, {P_A_WAVE, P_A_POS, P_A_WMODE, P_A_WARP}},
    {"OSC A", HB_BTN_EDIT, 0, K_OSCA, DS_WAVE_A, 0, {P_A_OCT, P_A_SEMI, P_A_FINE, P_A_LVL}},
    {"OSC A", HB_BTN_EDIT, 0, K_OSCA, DS_WAVE_A, 0, {P_A_UNI, P_A_DET, P_A_SPR, P_A_PAN}},
    {"OSC B", HB_BTN_EDIT, 1, K_OSCB, DS_WAVE_B, 0, {P_B_WAVE, P_B_POS, P_B_WMODE, P_B_WARP}},
    {"OSC B", HB_BTN_EDIT, 0, K_OSCB, DS_WAVE_B, 0, {P_B_OCT, P_B_SEMI, P_B_FINE, P_B_LVL}},
    {"OSC B", HB_BTN_EDIT, 0, K_OSCB, DS_WAVE_B, 0, {P_B_UNI, P_B_DET, P_B_SPR, P_B_PAN}},
    {"MIX", HB_BTN_EDIT, 1, K_MIX, DS_MIX, 0, {P_SUB_WAVE, P_SUB_OCT, P_SUB_LVL, P_SUB_DIR}},
    {"MIX", HB_BTN_EDIT, 0, K_MIX, DS_MIX, 0, {P_NZ_TYPE, P_NZ_LVL, P_VOL, P_VSPREAD}},
    {"FILTER", HB_BTN_SEL, 1, K_FILT, DS_FILT, 0, {P_F_TYPE, P_F_CUT, P_F_RES, P_F_ENV}},
    {"FILTER", HB_BTN_SEL, 0, K_FILT, DS_FILT, 0, {P_F_KEY, P_F_DRV, P_F_VEL, P_F_LFO}},
    {"ENV 1 AMP", HB_BTN_ENV, 1, K_ENV, DS_ENV, P_E1_A, {P_E1_A, P_E1_D, P_E1_S, P_E1_R}},
    {"ENV 2", HB_BTN_ENV, 1, K_ENV, DS_ENV, P_E2_A, {P_E2_A, P_E2_D, P_E2_S, P_E2_R}},
    {"ENV 3", HB_BTN_ENV, 1, K_ENV, DS_ENV, P_E3_A, {P_E3_A, P_E3_D, P_E3_S, P_E3_R}},
    {"LFO 1", HB_BTN_LFO, 1, K_LFO, DS_LFO, P_L1_SHAPE, {P_L1_SHAPE, P_L1_RATE, P_L1_SYNC, P_L1_FADE}},
    {"LFO 2", HB_BTN_LFO, 1, K_LFO, DS_LFO, P_L2_SHAPE, {P_L2_SHAPE, P_L2_RATE, P_L2_SYNC, P_L2_FADE}},
    {"LFO 3 ALL", HB_BTN_LFO, 1, K_LFO, DS_LFO, P_L3_SHAPE, {P_L3_SHAPE, P_L3_RATE, P_L3_SYNC, P_L3_FADE}},
    {"MATRIX", HB_BTN_SEQ, 1, K_MOD, DS_MOD, 0, {P_X1_SRC, P_X1_DST, P_X1_AMT, PG_SLOT}},
    {"ARP", HB_BTN_ARP, 1, K_ARP, DS_ARP, 0, {P_ARP_MODE, P_ARP_RATE, P_ARP_GATE, P_ARP_OCT}},
    {"ARP", HB_BTN_ARP, 0, K_ARP, DS_ARP, 0, {P_ARP_SWING, P_ARP_LATCH, P_TEMPO, NOP}},
    {"DRIVE CHO", HB_BTN_FX, 1, K_FX, DS_FX, 0, {P_DRV_TYPE, P_DRV_AMT, P_CHO_DEPTH, P_CHO_MIX}},
    {"DELAY", HB_BTN_FX, 1, K_FX, DS_FX, 2, {P_DLY_TIME, P_DLY_FB, P_DLY_TONE, P_DLY_MIX}},
    {"REVERB", HB_BTN_FX, 1, K_FX, DS_FX, 3, {P_REV_DECAY, P_REV_DAMP, P_REV_MIX, NOP}},
    {"VOICE", HB_BTN_GLO, 1, K_VOICE, DS_VOICE, 0, {P_MODE, P_GLIDE, P_BEND, P_VELSENS}},
};
#define NPAGES (sizeof PAGES / sizeof PAGES[0])

static struct {
    uint8_t page, slot, touched;
    uint8_t d_head, d_disp, d_tile;             /* what to draw again (d_tile: a bit per tile) */
    uint8_t save, save_to;                      /* SAVE pressed once: the user sound it will go to */
    uint8_t compare, edited;
    uint16_t preset;
    uint32_t msg_until;
    const char *msg;
    uint32_t act_drawn, load_drawn, t_meter;
} ui_s;
static volatile uint8_t ui_ext_dirty;           /* a parameter changed from MIDI */
static volatile int32_t ui_pc_req = -1;         /* a program change to do in the main loop */

/* ---- text */
static char *put_s(char *d, const char *s)
{
    while (*s)
        *d++ = *s++;
    *d = 0;
    return d;
}
static char *put_i(char *d, int32_t v, uint32_t sign)
{
    char t[12];
    uint32_t n = 0, u = (uint32_t)(v < 0 ? -v : v);
    if (v < 0)
        *d++ = '-';
    else if (sign && v > 0)
        *d++ = '+';
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
    } while (u);
    while (n)
        *d++ = t[--n];
    *d = 0;
    return d;
}

static uint32_t same_s(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static uint32_t page_param(const page_t *pg, uint32_t k)
{
    uint32_t id = pg->par[k];
    if (pg->disp == DS_MOD && k < 3u)
        id += 3u * ui_s.slot;
    return id;
}

static uint32_t is_bipolar(const pdef_t *d) { return d->fmt == F_NUM && d->aux > 0 && d->max == 2 * d->aux; }

static void param_text(char *out, uint32_t id)
{
    const pdef_t *d = &PDEF[id];
    int32_t v = cur.p[id];
    if (d->fmt == F_LIST)
        put_s(out, LISTS[d->aux][v]);
    else if (d->fmt == F_PCT)
        put_s(put_i(out, v, 0), "%");
    else
        put_i(out, v - d->aux, is_bipolar(d) || id == P_F_ENV || id == P_F_LFO || id == P_A_PAN || id == P_B_PAN
                               || id == P_SUB_OCT);
}

static void preset_label(char *out, uint32_t idx)
{
    char *p = out;
    uint32_t n = idx < NFACTORY ? idx + 1u : idx - NFACTORY + 1u;
    *p++ = idx < NFACTORY ? 'F' : 'U';
    *p++ = (char)('0' + n / 10u);
    *p++ = (char)('0' + n % 10u);
    *p = 0;
}

/* ---- the strips */
static void draw_head(void)
{
    const page_t *pg = &PAGES[ui_s.page];
    char s[40], *p;
    uint32_t i, n = 0, k = 0;
    uint16_t c = COL[pg->col];
    H->cv_begin(240, 32, U_BG);
    if (ui_s.save) {
        H->cv_rect(0, 2, 240, 28, U_WARN);
        p = put_s(s, "SAVE TO ");
        preset_label(p, NFACTORY + ui_s.save_to);
        put_s(s + 11, "?  SAVE = YES");
        H->cv_text(8, 8, 0, s, U_INK);
        H->cv_blit(0, 24);
        return;
    }
    for (i = 0; i < NPAGES; i++)                /* "OSC A 2/3": its place among the pages of that name */
        if (same_s(PAGES[i].name, pg->name)) {
            n++;
            if (i <= ui_s.page)
                k++;
        }
    p = put_s(s, pg->name);
    if (n > 1u) {
        *p++ = ' ';
        p = put_i(p, (int32_t)k, 0);
        *p++ = '/';
        put_i(p, (int32_t)n, 0);
    }
    H->cv_rect(0, 2, 112, 28, c);
    H->cv_text(8, 8, 0, s, U_INK);
    if (ui_s.msg) {
        H->cv_text(120, 8, 0, ui_s.msg, U_TXT);
    } else {
        preset_label(s, ui_s.preset);
        H->cv_text(120, 8, 0, s, ui_s.compare ? U_WARN : c);
        p = put_s(s, cur.name);
        if (ui_s.edited && !ui_s.compare)
            put_s(p, "*");
        H->cv_text(148, 8, 0, ui_s.compare ? "STORED" : s, U_TXT);
    }
    H->cv_blit(0, 24);
}

static void draw_tile(uint32_t k)
{
    const page_t *pg = &PAGES[ui_s.page];
    uint32_t id = page_param(pg, k);
    uint16_t c = COL[pg->col];
    char s[16];
    H->cv_begin(60, 96, U_BG);
    if (pg->par[k] == NOP) {
        H->cv_rect(1, 2, 58, 92, U_TRACK);
        H->cv_blit((int32_t)k * 60, 144);
        return;
    }
    H->cv_rect(1, 2, 58, 92, U_TILE);
    H->cv_rect(1, 2, 58, 4, ui_s.touched == k ? c : U_EDGE);
    if (pg->par[k] == PG_SLOT) {
        uint32_t i;
        s[0] = (char)('1' + ui_s.slot);
        s[1] = 0;
        H->cv_text(3, 12, 0, "SLOT", U_DIM);
        H->cv_text(3, 34, 0, s, U_TXT);
        for (i = 0; i < 8u; i++)                /* eight small blocks: the slot, and which ones are in use */
            H->cv_rect(6 + (int32_t)i * 6, 64, 5, 18,
                       i == ui_s.slot ? c : cur.p[P_X1_SRC + 3u * i] && cur.p[P_X1_DST + 3u * i] ? U_DIM : U_TRACK);
    } else {
        const pdef_t *d = &PDEF[id];
        int32_t v = cur.p[id], w;
        const char *nm = d->name;
        if (id <= P_M4) {                       /* a macro: named after the first thing it moves */
            uint32_t i;
            nm = id == P_M1 ? "M1" : id == P_M2 ? "M2" : id == P_M3 ? "M3" : "M4";
            for (i = 0; i < 8u; i++)
                if (cur.p[P_X1_SRC + 3u * i] == S_M1 + id && cur.p[P_X1_DST + 3u * i]) {
                    nm = LS_DST[cur.p[P_X1_DST + 3u * i]];
                    break;
                }
        }
        H->cv_text(3, 12, 0, nm, U_DIM);
        param_text(s, id);
        H->cv_text(3, 34, 0, s, U_TXT);
        H->cv_rect(6, 64, 48, 18, U_TRACK);
        if (d->fmt == F_LIST) {                 /* a list: a block at its place */
            int32_t bw = 48 / (d->max + 1);
            if (bw < 4)
                bw = 4;
            H->cv_rect(6 + ((48 - bw) * v) / (d->max ? d->max : 1), 64, bw, 18, c);
        } else if (is_bipolar(d) || id == P_F_ENV || id == P_F_LFO || id == P_A_PAN || id == P_B_PAN) {
            int32_t mid = d->aux, half = d->max - mid > mid ? d->max - mid : mid;
            w = ((v - mid) * 24) / (half ? half : 1);
            if (w >= 0)
                H->cv_rect(30, 64, w ? w : 1, 18, c);
            else
                H->cv_rect(30 + w, 64, -w, 18, c);
            H->cv_rect(29, 62, 2, 22, U_EDGE);
        } else {
            w = (v * 48) / (d->max ? d->max : 1);
            if (w)
                H->cv_rect(6, 64, w, 18, c);
        }
    }
    H->cv_blit((int32_t)k * 60, 144);
}

/* the phase a warp reads the table at (the picture of the wave; the engine has its own loops) */
static uint32_t warp_map(uint32_t mode, const warp_t *w, uint32_t ph)
{
    uint64_t q;
    switch (mode) {
    case W_SYNC: return (uint32_t)(((uint64_t)ph * w->k) >> 12);
    case W_BENDP:
    case W_BENDM:
        return ph < w->d ? (uint32_t)(((uint64_t)ph * w->s1) >> 16)
                         : 0x80000000u + (uint32_t)(((uint64_t)(ph - w->d) * w->s2) >> 16);
    case W_PWM:
        q = ((uint64_t)ph * w->k) >> 12;
        return q > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)q;
    case W_MIRROR:
        q = ((uint64_t)ph * w->k) >> 12;
        return (uint32_t)(q >> 32) & 1u ? ~(uint32_t)q : (uint32_t)q;
    case W_QUANT: return ph & w->mask;
    default: return ph;
    }
}

/* a column of the picture: from the middle (or the bottom) to y */
static void col_fill(int32_t x, int32_t y0, int32_t y1, uint16_t c)
{
    if (y1 < y0) {
        int32_t t = y0;
        y0 = y1;
        y1 = t;
    }
    H->cv_rect(x, y0, 1, y1 - y0 + 1, c);
}

static void disp_wave(uint32_t o, uint16_t c)
{
    uint32_t po = o ? P_B_WAVE : P_A_WAVE, mode = cur.p[po + 2u], x;
    int32_t pos = (int32_t)cur.p[po + 1u] * 7 * 4096 / 127, mixf = pos & 4095;
    uint32_t f0 = (uint32_t)pos >> 12;
    const int16_t *t0, *t1;
    warp_t w = {4096u, 0x80000000u, 65536u, 65536u, ~0u, 0};
    char s[24], *p;
    if (f0 >= AP_NF - 1u) {
        f0 = AP_NF - 1u;
        mixf = 0;
    }
    t0 = wt_buf[wt_cur[o]] + f0 * WT_FRM;
    t1 = t0 + (mixf ? WT_FRM : 0u);
    warp_set(&w, mode, (int32_t)cur.p[po + 3u] * 4095 / 127);
    H->cv_rect(0, 43, 240, 1, U_EDGE);
    for (x = 0; x < 240u; x++) {
        uint32_t ph = warp_map(mode, &w, x * 17895697u), i = ph >> 23;
        int32_t a = t0[i], b = t1[i];
        a += ((b - a) * mixf) >> 12;
        col_fill((int32_t)x, 43, 43 - (a * 38) / 32768, c);
    }
    p = put_s(s, AP_WT_NAME[cur.p[po]]);
    H->cv_text(4, 0, 0, s, U_TXT);
    for (x = 0; x < AP_NF; x++)                 /* the eight frames, and where POS is among them */
        H->cv_rect(176 + (int32_t)x * 8, 5, 6, 6, x == f0 || (mixf && x == f0 + 1u) ? c : U_TILE);
    (void)p;
}

static void disp_mix(void)
{
    static const char *const NM[4] = {"A", "B", "SUB", "NOISE"};
    static const uint8_t ID[4] = {P_A_LVL, P_B_LVL, P_SUB_LVL, P_NZ_LVL};
    static const uint8_t CL[4] = {K_OSCA, K_OSCB, K_MIX, K_VOICE};
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        int32_t y = (int32_t)i * 22 + 2, w = (cur.p[ID[i]] * 176) / 127;
        H->cv_text(4, y + 1, 0, NM[i], U_DIM);
        H->cv_rect(56, y, 176, 18, U_TRACK);
        if (w)
            H->cv_rect(56, y, w, 18, COL[CL[i]]);
    }
}

static void disp_filter(uint16_t c)
{
    int32_t type = PV(F_TYPE), xc = 20 + (PV(F_CUT) * 200) / 127, pk = (PV(F_RES) * 30) / 127, x;
    int32_t slope = type == FT_LP24 ? 30 : 60;
    if (type == FT_OFF) {
        H->cv_text(0, 36, 0, "            FILTER OFF", U_DIM);
        return;
    }
    for (x = 0; x < 240; x++) {
        int32_t d = x - xc, y, flat = 44, low = 86;
        int32_t bump = d > -24 && d < 24 ? (pk * (24 - (d < 0 ? -d : d))) / 24 : 0;
        switch (type) {
        case FT_HP: y = d >= 0 ? flat : flat - (d * (low - flat)) / slope; break;
        case FT_BP: y = flat + ((d < 0 ? -d : d) * (low - flat)) / 50 - 14; break;
        case FT_NOTCH: y = (d < 0 ? -d : d) < 14 ? low - ((d < 0 ? -d : d) * (low - flat)) / 14 : flat; bump = 0; break;
        default: y = d <= 0 ? flat : flat + (d * (low - flat)) / slope; break;
        }
        y -= bump;
        if (y > low) y = low;
        if (y < 2) y = 2;
        col_fill(x, 87, y, c);
    }
    H->cv_rect(xc, 0, 1, 88, U_EDGE);
}

static void disp_env(uint32_t base, uint16_t c)
{
    int32_t a = 6 + (cur.p[base] * 52) / 127, d = 6 + (cur.p[base + 1u] * 52) / 127, r = 6 + (cur.p[base + 3u] * 52) / 127;
    int32_t sl = 84 - (cur.p[base + 2u] * 78) / 127, x, hold = 52, x0 = 4;
    for (x = 0; x < a + d + hold + r; x++) {
        int32_t y;
        if (x < a) y = 84 - (x * 78) / a;
        else if (x < a + d) y = 6 + ((x - a) * (sl - 6)) / d;
        else if (x < a + d + hold) y = sl;
        else y = sl + ((x - a - d - hold) * (84 - sl)) / r;
        col_fill(x0 + x, 86, y, c);
    }
    H->cv_rect(x0 + a + d + hold, 0, 1, 88, U_EDGE);    /* where the key is let go */
}

static void disp_lfo(uint32_t base, uint16_t c)
{
    static const int8_t RN[9] = {40, -70, 10, 90, -30, -100, 60, -10, 40};
    uint32_t shape = cur.p[base], x;
    for (x = 0; x < 232u; x++) {
        uint32_t ph = x * 37025580u;            /* two cycles across */
        int32_t v, t;
        switch (shape) {
        case 0: v = sinr[ph >> 22] >> 8; break;
        case 1: t = (int32_t)(ph >> 23) - 256; v = (t < 0 ? -t : t) - 128; break;
        case 2: v = 127 - (int32_t)(ph >> 24); break;
        case 3: v = ph < 0x80000000u ? 127 : -127; break;
        case 4: v = RN[x / 29u]; break;
        default: v = RN[x / 29u] + ((RN[x / 29u + 1u] - RN[x / 29u]) * (int32_t)(x % 29u)) / 29; break;
        }
        col_fill(4 + (int32_t)x, 44, 44 - (v * 36) / 128, c);
    }
    H->cv_rect(4, 44, 232, 1, U_EDGE);
    if (cur.p[base + 2u])
        H->cv_text(4, 0, 0, LS_DIV[(cur.p[base + 1u] * 12u) >> 7], U_TXT);
}

static void disp_mod(uint16_t c)
{
    uint32_t top = ui_s.slot < 2u ? 0u : ui_s.slot > 5u ? 4u : ui_s.slot - 1u, i;
    for (i = 0; i < 4u; i++) {
        uint32_t k = top + i, s = cur.p[P_X1_SRC + 3u * k], d = cur.p[P_X1_DST + 3u * k];
        int32_t y = (int32_t)i * 22, amt = (int32_t)cur.p[P_X1_AMT + 3u * k] - 100;
        char t[40], *p = t;
        uint16_t fg = k == ui_s.slot ? U_INK : s && d ? U_TXT : U_DIM;
        if (k == ui_s.slot)
            H->cv_rect(0, y + 1, 240, 20, c);
        *p++ = (char)('1' + k);
        *p++ = ' ';
        p = put_s(p, LS_SRC[s]);
        H->cv_text(6, y + 3, 0, t, fg);
        H->cv_text(88, y + 3, 0, ">", fg);
        H->cv_text(104, y + 3, 0, LS_DST[d], fg);
        put_i(t, amt, 1);
        H->cv_text(196, y + 3, 0, t, fg);
    }
}

static void disp_arp(uint16_t c)
{
    uint32_t mode = (uint32_t)PV(ARP_MODE), octs = (uint32_t)PV(ARP_OCT) + 1u, s;
    char t[16], *p;
    p = put_i(t, 60 + PV(TEMPO), 0);
    put_s(p, seq_ext ? " EXT" : " BPM");
    H->cv_text(4, 0, 0, seq_ext ? "MIDI CLOCK" : t, U_TXT);
    if (mode == ARP_OFF) {
        H->cv_text(152, 0, 0, "ARP OFF", U_DIM);
        return;
    }
    for (s = 0; s < 16u; s++) {                 /* sixteen steps of a three-note chord */
        uint32_t oct = 0, k = mode == ARP_RANDOM ? (s * 7u + 3u) % 3u : mode == ARP_CHORD ? 0u
                            : arp_pick(mode == ARP_ORDER ? ARP_UP : mode, s, 3, octs, &oct);
        int32_t h, x = 4 + (int32_t)s * 14 + ((s & 1u) ? (PV(ARP_SWING) * 12) / 150 : 0);
        if (mode == ARP_CHORD)
            oct = s % octs;
        if (mode == ARP_RANDOM)
            oct = (s * 5u) % octs;
        h = 10 + (int32_t)(k + 3u * oct) * 52 / (int32_t)(3u * octs);
        H->cv_rect(x, 84 - h, 2 + (PV(ARP_GATE) * 10) / 100, mode == ARP_CHORD ? h : 8, c);
    }
}

static void disp_fx(uint32_t which, uint16_t c)
{
    static const char *const NM[4] = {"DRV", "CHO", "DLY", "REV"};
    static const uint8_t ID[4] = {P_DRV_AMT, P_CHO_MIX, P_DLY_MIX, P_REV_MIX};
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        int32_t x = 4 + (int32_t)i * 59, h = (cur.p[ID[i]] * 60) / 127;
        uint32_t here = which ? i == which : i < 2u;
        H->cv_rect(x, 22, 55, 62, U_TRACK);
        if (h)
            H->cv_rect(x, 84 - h, 55, h, here ? c : U_EDGE);
        H->cv_text(x + 2, 2, 0, NM[i], here ? U_TXT : U_DIM);
    }
}

static void disp_voice(void)
{
    char t[40], *p;
    p = put_s(t, "NOTES ");
    p = put_i(p, (int32_t)eng_max_notes(), 0);
    p = put_s(p, "  UNISON ");
    p = put_i(p, PV(A_UNI) + 1, 0);
    p = put_s(p, "+");
    put_i(p, PV(B_UNI) + 1, 0);
    H->cv_text(4, 6, 0, t, U_TXT);
    H->cv_text(4, 30, 0, guard_max < NV ? "THE CHIP IS BUSY: FEWER" : "A HEAVY SOUND HAS FEWER", U_DIM);
    p = put_s(t, "LOAD ");                      /* the worst block of late, of the 5.8 ms it has */
    p = put_i(p, (int32_t)(eng_load / 1392u), 0);
    put_s(p, "% OF A BLOCK");
    H->cv_text(4, 58, 0, t, eng_load > 97000u ? U_WARN : U_DIM);
}

static void draw_meter(uint32_t force);

static void draw_disp(void)
{
    const page_t *pg = &PAGES[ui_s.page];
    uint16_t c = COL[pg->col];
    H->cv_begin(240, 88, U_BG);
    switch (pg->disp) {
    case DS_PLAY: {
        int32_t w = H->text_w(1, cur.name);
        H->cv_text(w < 232 ? (240 - w) / 2 : 4, 6, 1, cur.name, U_TXT);
        break;
    }
    case DS_WAVE_A: disp_wave(0, c); break;
    case DS_WAVE_B: disp_wave(1, c); break;
    case DS_MIX: disp_mix(); break;
    case DS_FILT: disp_filter(c); break;
    case DS_ENV: disp_env(pg->arg, c); break;
    case DS_LFO: disp_lfo(pg->arg, c); break;
    case DS_MOD: disp_mod(c); break;
    case DS_ARP: disp_arp(c); break;
    case DS_FX: disp_fx(pg->arg, c); break;
    default: disp_voice(); break;
    }
    H->cv_blit(0, 56);
    if (pg->disp == DS_PLAY)
        draw_meter(1);
}

/* PLAY: a block per note sounding and the load of the audio, straight to the screen when they change */
static void draw_meter(uint32_t force)
{
    uint32_t act = eng_active, load = eng_load / 1392u, i;      /* % of a block's 5.8 ms (24 ticks a us) */
    if (PAGES[ui_s.page].disp != DS_PLAY || ui_s.save)
        return;
    if (load > 100u)
        load = 100u;
    if (!force && act == ui_s.act_drawn && load == ui_s.load_drawn)
        return;
    for (i = 0; i < NV; i++)
        if (force || ((act ^ ui_s.act_drawn) >> i & 1u))
            H->fill(8 + (int32_t)i * 28, 104, 24, 14, act >> i & 1u ? COL[K_PLAY] : U_TILE);
    if (force || load != ui_s.load_drawn) {
        H->fill(8, 126, 220, 6, U_TRACK);
        if (load)
            H->fill(8, 126, (int32_t)(load * 220u / 100u), 6, load > 70u ? U_WARN : U_DIM);
    }
    ui_s.act_drawn = act;
    ui_s.load_drawn = load;
}

static void ui_dirty_all(void)
{
    ui_s.d_head = ui_s.d_disp = 1;
    ui_s.d_tile = 15;
}

static void ui_leds(void)
{
    static const uint8_t BT[] = {HB_BTN_PLAY, HB_BTN_EDIT, HB_BTN_SEL, HB_BTN_ENV, HB_BTN_LFO, HB_BTN_SEQ, HB_BTN_ARP,
                                 HB_BTN_FX, HB_BTN_GLO};
    uint32_t i;
    for (i = 0; i < sizeof BT; i++)
        H->led(BT[i], PAGES[ui_s.page].btn == BT[i]);
    H->led(HB_BTN_SAVE, ui_s.save);
    H->led(HB_BTN_REC, ui_s.compare);
}

static void ui_go(uint32_t page)
{
    ui_s.page = (uint8_t)page;
    ui_s.touched = 255;
    ui_dirty_all();
    ui_leds();
}

#endif
