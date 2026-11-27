/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Hugo Gilbert */
/* Fizzy-Fm's screen and panel: flat blocks of colour, one colour per section, lettered in Inter Tight (ap_font.h).
 * All of the screen is the module's (HB_F_SCREEN):
 *   0..23     the top line: the name, the keys' octave, the level going out
 *   24..47    the header: the parts of the section as tabs, this part's pages as dots, the level going out
 *   48..175   the display: the sound's line, the page's title, the picture of what the page edits
 *   176..239  four cards, one per knob: its name, its value, a bar; the one last turned on the section's colour
 * The screen keeps what is drawn: draw() sends one strip a pass, only what changed. */
#ifndef AP_UI_H
#define AP_UI_H
#include "ap_font.h"

#define U_BG    HB_RGB(27, 27, 27)
#define U_VOID  HB_RGB(33, 33, 33)              /* a card with nothing under its knob */
#define U_PANEL HB_RGB(40, 40, 40)
#define U_TRACK HB_RGB(60, 60, 60)
#define U_FAINT HB_RGB(92, 92, 92)
#define U_DIM   HB_RGB(146, 146, 146)
#define U_TXT   HB_RGB(236, 236, 236)
#define U_INK   HB_RGB(20, 20, 20)
#define U_WARN  HB_RGB(255, 80, 70)
enum { K_PLAY, K_OSCA, K_OSCB, K_MIX, K_FILT, K_ENV, K_LFO, K_MOD, K_ARP, K_FX, K_VOICE, K_RAND, NCOL };
static const uint16_t COL[NCOL] = {
    HB_RGB(255, 148, 40), HB_RGB(255, 172, 48), HB_RGB(56, 196, 240), HB_RGB(236, 220, 70), HB_RGB(255, 96, 104),
    HB_RGB(104, 214, 108), HB_RGB(178, 140, 255), HB_RGB(255, 122, 196), HB_RGB(52, 214, 178), HB_RGB(94, 156, 255),
    HB_RGB(200, 200, 200), HB_RGB(186, 236, 64)};

enum { DS_PLAY, DS_WAVE_A, DS_WAVE_B, DS_MIX, DS_FILT, DS_ENV, DS_LFO, DS_MOD, DS_ARP, DS_FX, DS_VOICE, DS_RAND, DS_SEQ };
#define NOP 0xFFu
#define PG_SLOT 0xFEu                           /* the matrix: this card picks the slot */
#define PG_X 0xF0u                              /* the cards from here are the page's own, not a parameter's: */
#define PG_SLOTS 0xF0u                          /* RANDOM: the sounds kept, the one before the dice first */
#define PG_KIND 0xF1u                           /* ... which kind SEL throws */
#define PG_CHAOS 0xF2u                          /* ... how wild */
#define PG_WHAT 0xF3u                           /* ... NEW sound or a VARIATION of this one */
#define PG_PAT 0xF4u                            /* SEQUENCER: the pattern (A..D) */
#define PG_LEN 0xF5u                            /* ... its length in steps */
#define PG_QUANT 0xF6u                          /* ... notes recorded land on the sixteenths */
#define PG_CLICK 0xF7u                          /* ... the click */
#define PG_STEP 0xF8u                           /* STEPS: the step */
#define PG_SNOTE 0xF9u                          /* ... its notes, up and down */
#define PG_SVEL 0xFAu                           /* ... their velocity */
#define PG_SLEN 0xFBu                           /* ... their length in steps */
typedef struct {
    const char *name;
    uint8_t btn;                                /* the button that goes to it */
    uint8_t head;                               /* 1: the first page of a part (a tab of the header) */
    uint8_t col, disp, arg;
    uint8_t par[4];
} page_t;
static const page_t PAGES[] = {
    {"PLAY", HB_BTN_HOME, 1, K_PLAY, DS_PLAY, 0, {P_M1, P_M2, P_M3, P_M4}},
    {"RANDOM", HB_BTN_HOME, 1, K_RAND, DS_RAND, 0, {PG_SLOTS, PG_KIND, PG_CHAOS, PG_WHAT}},
    {"OSC A", HB_BTN_EDIT, 1, K_OSCA, DS_WAVE_A, 0, {P_A_WAVE, P_A_POS, P_A_WMODE, P_A_WARP}},
    {"OSC A", HB_BTN_EDIT, 0, K_OSCA, DS_WAVE_A, 0, {P_A_OCT, P_A_SEMI, P_A_FINE, P_A_LVL}},
    {"OSC A", HB_BTN_EDIT, 0, K_OSCA, DS_WAVE_A, 0, {P_A_UNI, P_A_DET, P_A_SPR, P_A_PAN}},
    {"OSC B", HB_BTN_EDIT, 1, K_OSCB, DS_WAVE_B, 0, {P_B_WAVE, P_B_POS, P_B_WMODE, P_B_WARP}},
    {"OSC B", HB_BTN_EDIT, 0, K_OSCB, DS_WAVE_B, 0, {P_B_OCT, P_B_SEMI, P_B_FINE, P_B_LVL}},
    {"OSC B", HB_BTN_EDIT, 0, K_OSCB, DS_WAVE_B, 0, {P_B_UNI, P_B_DET, P_B_SPR, P_B_PAN}},
    {"FILTER", HB_BTN_ENV, 1, K_FILT, DS_FILT, 0, {P_F_TYPE, P_F_CUT, P_F_RES, P_F_ENV}},
    {"FILTER", HB_BTN_ENV, 0, K_FILT, DS_FILT, 0, {P_F_KEY, P_F_DRV, P_F_VEL, P_F_VAR}},
    {"ENV 1", HB_BTN_ENV, 1, K_ENV, DS_ENV, P_E1_A, {P_E1_A, P_E1_D, P_E1_S, P_E1_R}},
    {"ENV 2", HB_BTN_ENV, 1, K_ENV, DS_ENV, P_E2_A, {P_E2_A, P_E2_D, P_E2_S, P_E2_R}},
    {"ENV 3", HB_BTN_ENV, 1, K_ENV, DS_ENV, P_E3_A, {P_E3_A, P_E3_D, P_E3_S, P_E3_R}},
    {"LFO 1", HB_BTN_LFO, 1, K_LFO, DS_LFO, P_L1_SHAPE, {P_L1_SHAPE, P_L1_RATE, P_L1_SYNC, P_L1_FADE}},
    {"LFO 2", HB_BTN_LFO, 1, K_LFO, DS_LFO, P_L2_SHAPE, {P_L2_SHAPE, P_L2_RATE, P_L2_SYNC, P_L2_FADE}},
    {"LFO 3", HB_BTN_LFO, 1, K_LFO, DS_LFO, P_L3_SHAPE, {P_L3_SHAPE, P_L3_RATE, P_L3_SYNC, P_L3_FADE}},
    {"MATRIX", HB_BTN_LFO, 1, K_MOD, DS_MOD, 0, {P_X1_SRC, P_X1_DST, P_X1_AMT, PG_SLOT}},
    {"MIX", HB_BTN_GLO, 1, K_MIX, DS_MIX, 0, {P_SUB_WAVE, P_SUB_OCT, P_SUB_LVL, P_SUB_DIR}},
    {"MIX", HB_BTN_GLO, 0, K_MIX, DS_MIX, 0, {P_NZ_TYPE, P_NZ_LVL, P_VOL, P_VSPREAD}},
    {"VOICE", HB_BTN_GLO, 1, K_VOICE, DS_VOICE, 0, {P_MODE, P_GLIDE, P_BEND, P_VELSENS}},
    {"ARP", HB_BTN_ARP, 1, K_ARP, DS_ARP, 0, {P_ARP_MODE, P_ARP_RATE, P_ARP_GATE, P_ARP_OCT}},
    {"ARP", HB_BTN_ARP, 0, K_ARP, DS_ARP, 0, {P_ARP_SWING, P_ARP_LATCH, P_TEMPO, NOP}},
    {"FX RACK", HB_BTN_FX, 1, K_FX, DS_FX, 0, {P_FX1_TYPE, P_FX1_A, P_FX1_B, P_FX1_C}},
    {"FX RACK", HB_BTN_FX, 0, K_FX, DS_FX, 1, {P_FX2_TYPE, P_FX2_A, P_FX2_B, P_FX2_C}},
    {"FX RACK", HB_BTN_FX, 0, K_FX, DS_FX, 2, {P_FX3_TYPE, P_FX3_A, P_FX3_B, P_FX3_C}},
    {"FX RACK", HB_BTN_FX, 0, K_FX, DS_FX, 3, {P_FX4_TYPE, P_FX4_A, P_FX4_B, P_FX4_C}},
    {"FX RACK", HB_BTN_FX, 0, K_FX, DS_FX, 4, {P_FX5_TYPE, P_FX5_A, P_FX5_B, P_FX5_C}},
    {"FX RACK", HB_BTN_FX, 0, K_FX, DS_FX, 5, {P_FX6_TYPE, P_FX6_A, P_FX6_B, P_FX6_C}},
    {"FX RACK", HB_BTN_FX, 0, K_FX, DS_FX, 6, {P_FX7_TYPE, P_FX7_A, P_FX7_B, P_FX7_C}},
    {"SEQUENCER", HB_BTN_SEQ, 1, K_ARP, DS_SEQ, 0, {PG_PAT, PG_LEN, PG_QUANT, PG_CLICK}},
    {"STEPS", HB_BTN_SEQ, 1, K_ARP, DS_SEQ, 1, {PG_STEP, PG_SNOTE, PG_SVEL, PG_SLEN}},
};
#define NPAGES (sizeof PAGES / sizeof PAGES[0])

static struct {
    uint8_t page, slot, touched;
    uint8_t d_head, d_disp, d_tile;             /* what to draw again (d_tile: a bit per tile) */
    uint8_t save, save_to;                      /* SAVE pressed once: the user sound it will go to */
    uint8_t compare, edited;
    uint8_t rnd;                                /* the sound is the dice's, not the preset whose number shows */
    uint16_t preset;
    uint32_t msg_until;
    const char *msg;
    uint8_t d_strip;                            /* the display's two strips still to draw (d_disp: both) */
    uint8_t d_top;                              /* the top line (the octave) */
    uint8_t last[16];                           /* by button: the page of its section seen last */
    uint8_t lvl_drawn, lvl_shown;
    uint8_t step;                               /* STEPS: the step the knobs and the keys edit */
    uint8_t sq_cell, sq_state;                  /* the step lit as the playhead, the state shown (the screen) */
    uint32_t sq_ver;
    uint32_t act_drawn, t_meter;
} ui_s;
static volatile uint8_t ui_ext_dirty;           /* a parameter changed from MIDI */
static volatile int32_t ui_pc_req = -1;         /* a program change to do in the main loop */

/* the dice (ap_rand.h). SEL throws a sound, NEW or a VARIATION of this one, over the one thrown before it: nothing is
 * kept until SEL is held. KEPT goes back through the sounds kept (eight, the oldest goes) and the one before the
 * dice. how[i]: the kind of a sound kept (RK_BASS ..), or RD_MUT */
#define RH 8u
#define RD_MUT 0x80u
static patch_t rh[RH], rbefore;
static struct {
    uint8_t n, pos;                             /* the sounds kept; pos 0 the one before the dice, i the i-th kept */
    uint8_t kind, chaos, what;                  /* what SEL throws: its kind, how wild, 1: a variation */
    uint8_t trying, tkind;                      /* the sound is a throw not kept (of this kind) */
    uint8_t before;                             /* rbefore holds the sound before the dice */
    uint8_t how[RH];
    uint32_t count;
} rd = {0, 0, RK_ANY, 48, 0, 0, 0, 0, {0}, 0};

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

static uint32_t is_bipolar(const pdef_t *d) { return d->fmt == F_NUM && d->aux > 0 && d->max == 2 * d->aux; }

static uint32_t page_param(const page_t *pg, uint32_t k)
{
    uint32_t id = pg->par[k];
    if (pg->disp == DS_MOD && k < 3u)
        id += 3u * ui_s.slot;
    return id;
}

/* an effect's knob: which of the three (0..2), -1 when id is not one; *type its effect */
static int32_t fx_knob(uint32_t id, uint32_t *type)
{
    uint32_t o = id - P_FX1_TYPE;
    if (id < P_FX1_TYPE || o >= 4u * NFX || !(o & 3u))
        return -1;
    *type = cur.p[id - (o & 3u)] < NFXT ? cur.p[id - (o & 3u)] : 0u;
    return (int32_t)(o & 3u) - 1;
}

static const char *param_name(uint32_t id)
{
    uint32_t t = 0;
    int32_t k = fx_knob(id, &t);
    return k < 0 ? PDEF[id].name : FXDEF[t].lab[k];
}

static void wave_name(char *out, uint32_t w)
{
    if (w < AP_NTAB) {
        put_s(out, AP_WT_NAME[w]);
    } else {
        char *p = put_s(out, "USER ");
        *p++ = (char)('1' + (w - AP_NTAB) % NUWT);
        *p = 0;
    }
}

static uint32_t is_bipolar(const pdef_t *d);
static uint32_t param_bipolar(uint32_t id)
{
    uint32_t t = 0;
    int32_t k = fx_knob(id, &t);
    if (k >= 0)
        return FXDEF[t].kind[k] == 1u;
    return is_bipolar(&PDEF[id]) || id == P_F_ENV || id == P_A_PAN || id == P_B_PAN;
}

static void param_text(char *out, uint32_t id)
{
    const pdef_t *d = &PDEF[id];
    int32_t v = cur.p[id], k;
    uint32_t t = 0;
    if ((k = fx_knob(id, &t)) >= 0) {
        if (!t)
            put_s(out, "");
        else if (FXDEF[t].kind[k] == 2u)
            put_s(out, LS_DIV[(v * 12) >> 7]);
        else
            put_i(out, FXDEF[t].kind[k] == 1u ? v - 64 : v, FXDEF[t].kind[k] == 1u);
        return;
    }
    if (id == P_A_WAVE || id == P_B_WAVE) {
        wave_name(out, (uint32_t)v);
        return;
    }
    if (d->fmt == F_LIST)
        put_s(out, LISTS[d->aux][v]);
    else if (d->fmt == F_PCT)
        put_s(put_i(out, v, 0), "%");
    else
        put_i(out, v - d->aux, param_bipolar(id) || id == P_SUB_OCT);
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

/* ---- drawing: a strip at a time, in the host's canvas. dr_* draw in the coordinates of the surface being drawn
 * (the header, the display, a card): what falls outside the strip on its way (dv_y0, dv_h) is left out */
#define Y_HEAD 24                               /* (the top line is above: draw_top) */
#define Y_DISP 48
#define H_DISP 128                              /* two strips: its rows 0..95, then 96..127 */
#define Y_DECK 176
static int32_t dv_y0, dv_h, dv_w;
static void dv_begin(int32_t w, int32_t y0, int32_t h, uint16_t bg)
{
    dv_w = w;
    dv_y0 = y0;
    dv_h = h;
    H->cv_begin(w, h, bg);
}
static void dr_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    y -= dv_y0;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > dv_w) w = dv_w - x;
    if (y + h > dv_h) h = dv_h - y;
    if (w > 0 && h > 0)
        H->cv_rect(x, y, w, h, c);
}
static void dr_px(int32_t x, int32_t y, uint16_t c)
{
    y -= dv_y0;
    if ((uint32_t)x < (uint32_t)dv_w && (uint32_t)y < (uint32_t)dv_h)
        H->cv_pixel(x, y, c);
}
/* k fifteenths of a over b */
static uint16_t mix565(uint16_t a, uint16_t b, uint32_t k)
{
    uint32_t j = 15u - k, r = (a >> 11) * k + (b >> 11) * j, g = (a >> 5 & 63u) * k + (b >> 5 & 63u) * j;
    uint32_t bl = (a & 31u) * k + (b & 31u) * j;
    r = (r * 4369u + 32768u) >> 16;             /* (/15) */
    g = (g * 4369u + 32768u) >> 16;
    bl = (bl * 4369u + 32768u) >> 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static uint16_t dark565(uint16_t c)             /* a colour at five eighths */
{
    return (uint16_t)((((c >> 11) * 5u) >> 3) << 11 | ((((c >> 5) & 63u) * 5u) >> 3) << 5 | (((c & 31u) * 5u) >> 3));
}

/* ---- the lettering (ap_font.h): y is the top of the capitals; it draws over bg, a flat colour */
static uint32_t t_code(uint32_t ch)             /* what the faces have: 32..95 (a small letter as its capital) */
{
    if (ch >= 'a' && ch <= 'z')
        ch -= 32u;
    return ch < 32u || ch > 95u ? 32u : ch;
}
static int32_t t_kern(uint32_t f, uint32_t a, uint32_t b)   /* the pair a b closer (or further), in 1/4 px */
{
    const fface_t *F = &FACES[f];
    uint32_t key = (a - 32u) << 6 | (b - 32u), lo = 0, hi = F->nk;
    while (lo < hi) {
        uint32_t m = (lo + hi) >> 1;
        if (F->kk[m] < key)
            lo = m + 1u;
        else
            hi = m;
    }
    return lo < F->nk && F->kk[lo] == key ? F->kd[lo] : 0;
}
static int32_t t_w(uint32_t f, const char *s)
{
    int32_t pen = 0;
    uint32_t prev = 32u;
    for (; *s; s++) {
        uint32_t ch = t_code((uint8_t)*s);
        pen += t_kern(f, prev, ch) + FACES[f].g[ch - 32u].adv4;
        prev = ch;
    }
    return (pen + 2) >> 2;
}
static int32_t t_draw(int32_t x, int32_t y, uint32_t f, const char *s, uint16_t fg, uint16_t bg)
{
    uint16_t tb[16];
    uint32_t i;
    int32_t pen = 0;
    uint32_t prev = 32u;
    for (i = 1; i < 15u; i++)
        tb[i] = mix565(fg, bg, i);
    for (; *s; s++) {
        uint32_t ch = t_code((uint8_t)*s), n = 0, a;
        const fglyph_t *g = &FACES[f].g[ch - 32u];
        const uint8_t *d = FACES[f].d + g->off;
        int32_t gx, gy = y + g->by, r, cx;
        pen += t_kern(f, prev, ch);
        prev = ch;
        gx = x + ((pen + 2) >> 2) + g->bx;
        for (r = 0; r < (int32_t)g->bh; r++)
            for (cx = 0; cx < (int32_t)g->bw;) {
                a = (uint32_t)(d[n >> 1] >> ((n & 1u) ? 0 : 4)) & 15u;
                if (a == 15u) {                 /* a run of full ink: one rectangle */
                    int32_t c0 = cx;
                    do {
                        cx++;
                        n++;
                    } while (cx < (int32_t)g->bw && ((d[n >> 1] >> ((n & 1u) ? 0 : 4)) & 15u) == 15u);
                    dr_rect(gx + c0, gy + r, cx - c0, 1, fg);
                    continue;
                }
                if (a)
                    dr_px(gx + cx, gy + r, tb[a]);
                cx++;
                n++;
            }
        pen += g->adv4;
    }
    return (pen + 2) >> 2;
}
/* in face f, or a smaller one if s is wider than w; set a little lower then, on the same middle */
static int32_t t_fit(int32_t x, int32_t y, uint32_t f, const char *s, int32_t w, uint16_t fg, uint16_t bg)
{
    uint32_t g = f;
    while (g > F_S && t_w(g, s) > w)
        g--;
    return t_draw(x, y + ((int32_t)FACES[f].cap - (int32_t)FACES[g].cap) / 2, g, s, fg, bg);
}
static void t_right(int32_t xr, int32_t y, uint32_t f, const char *s, uint16_t fg, uint16_t bg)
{
    t_draw(xr - t_w(f, s), y, f, s, fg, bg);
}

/* ---- the top line: the name, the keys' octave, the level going out (draw_meter) */
static int8_t kb_oct;                           /* the keys' octave (OCT - / OCT +), -3 .. +3 */
static uint8_t oct_led = 0xFFu;                 /* OCT - and OCT +'s lights as sent (bits 0, 1); 0xFF: send again */
static uint8_t tr_led = 0xFFu;                  /* PLAY and REC's, the same way */
static void draw_top(void)
{
    char s[8] = "OCT 0";
    dv_begin(240, 0, 24, U_INK);
    t_draw(8, 8, F_S, "FIZZY-FM", U_DIM, U_INK);
    if (kb_oct) {
        s[4] = kb_oct < 0 ? '-' : '+';
        s[5] = (char)('0' + (kb_oct < 0 ? -kb_oct : kb_oct));
        s[6] = 0;
    }
    t_right(192, 8, F_S, s, kb_oct ? COL[K_PLAY] : U_FAINT, U_INK);
    H->cv_blit(0, 0);
    ui_s.lvl_drawn = 255;                       /* (the meter, again) */
}

/* ---- the header: the parts of this button's section as tabs, the one open on its colour; this part's pages as
 * dots */
static void draw_head(void)
{
    const page_t *pg = &PAGES[ui_s.page];
    uint32_t i, n = 0, k = 0;
    int32_t x = 0;
    uint16_t c = COL[pg->col];
    char s[48], *p;
    dv_begin(240, 0, 24, U_BG);
    if (ui_s.save) {
        dr_rect(0, 0, 240, 24, U_WARN);
        p = put_s(s, "SAVE TO ");
        preset_label(p, NFACTORY + ui_s.save_to);
        t_draw(8, 8, F_S, s, U_INK, U_WARN);
        t_right(232, 8, F_S, "SAVE: YES   ELSE: NO", U_INK, U_WARN);
        H->cv_blit(0, Y_HEAD);
        return;
    }
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].btn == pg->btn && PAGES[i].head) {
            const char *nm = PAGES[i].name;
            int32_t w = t_w(F_S, nm) + 14;
            uint32_t on = same_s(nm, pg->name);
            if (on)
                dr_rect(x, 0, w, 24, c);
            t_draw(x + 7, 8, F_S, nm, on ? U_INK : U_DIM, on ? c : U_BG);
            x += w;
        }
    for (i = 0; i < NPAGES; i++)                /* this part's pages, and which one this is */
        if (same_s(PAGES[i].name, pg->name)) {
            n++;
            if (i <= ui_s.page)
                k++;
        }
    if (n > 1u)
        for (i = 0; i < n; i++)
            dr_rect(235 - 8 * (int32_t)n + 8 * (int32_t)i, 10, 5, 5, i + 1u == k ? c : U_FAINT);
    H->cv_blit(0, Y_HEAD);
}

/* ---- a card under each knob: its name, its value, a bar. The one last turned is on the section's colour */
static void card_bar(uint32_t on, uint16_t c, int32_t x, int32_t w)
{
    if (w > 0)
        dr_rect(7 + x, 51, w, 4, on ? U_INK : c);
}
static char *note_name(char *d, uint32_t key)   /* C4 is 60 */
{
    static const char NN[12][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    d = put_s(d, NN[key % 12u]);
    return put_i(d, (int32_t)(key / 12u) - 1, 0);
}
/* a step of the pattern on the STEPS page: its notes, low first; the first one's word */
static uint32_t step_first(uint32_t s, uint32_t *e)
{
    const pat_t *p = &pats[pl.pat];
    uint32_t k, best = 0, lo = 128u;
    for (k = 0; k < p->n; k++)
        if (EV_POS(p->ev[k]) / SQ_SUB == s && EV_KEY(p->ev[k]) < lo) {
            lo = EV_KEY(p->ev[k]);
            best = p->ev[k];
        }
    *e = best;
    return lo < 128u;
}
/* RANDOM's and the sequencer's cards: their name; s gets the value, the bar is drawn */
static const char *card_own(uint32_t what, char *s, uint32_t on, uint16_t c)
{
    static const char *const CLK[3] = {"OFF", "REC", "ON"};
    const pat_t *p = &pats[pl.pat];
    uint32_t i, e;
    char *q;
    switch (what) {
    case PG_SLOTS:                              /* the one before the dice, then the sounds kept */
        if (rd.trying)
            put_s(s, "NEW");
        else if (!rd.pos)
            put_s(s, rd.before ? "BEFORE" : "-");
        else {
            q = put_i(s, rd.pos, 0);
            *q++ = '/';
            put_i(q, rd.n, 0);
        }
        for (i = 0; i <= RH; i++)
            dr_rect(7 + (int32_t)i * 5, 51, 4, 4, i > rd.n || (!i && !rd.before) ? U_TRACK
                                                   : i == rd.pos && !rd.trying ? (on ? U_INK : c) : U_DIM);
        return "KEPT";
    case PG_KIND:
        put_s(s, RK_NAME[rd.kind]);
        card_bar(on, c, (40 * (int32_t)rd.kind) / (int32_t)(NRK - 1u), 6);
        return "KIND";
    case PG_CHAOS:
        put_i(s, rd.chaos, 0);
        card_bar(on, c, 0, ((int32_t)rd.chaos * 46) / 127);
        return "CHAOS";
    case PG_WHAT:
        put_s(s, rd.what ? "VARY" : "NEW");
        card_bar(on, c, rd.what ? 40 : 0, 6);
        return "SEL";
    case PG_PAT:
        s[0] = (char)('A' + pl.pat);
        s[1] = 0;
        if (pl.next != pl.pat) {                /* the one asked for, when this one comes round */
            s[1] = '>';
            s[2] = (char)('A' + pl.next);
            s[3] = 0;
        }
        for (i = 0; i < SQ_PAT; i++)
            dr_rect(7 + (int32_t)i * 12, 51, 10, 4, i == pl.pat ? (on ? U_INK : c) : pats[i].n ? U_DIM : U_TRACK);
        return "PATTERN";
    case PG_LEN:
        put_i(s, p->len, 0);
        card_bar(on, c, 0, ((int32_t)p->len * 46) / 64);
        return "STEPS";
    case PG_QUANT:
        put_s(s, p->quant ? "ON" : "OFF");
        card_bar(on, c, p->quant ? 40 : 0, 6);
        return "QUANTIZE";
    case PG_CLICK:
        put_s(s, CLK[pb.click < 3u ? pb.click : 0u]);
        card_bar(on, c, 20 * (int32_t)pb.click, 6);
        return "CLICK";
    case PG_STEP:
        put_i(s, ui_s.step + 1, 0);
        card_bar(on, c, ((int32_t)ui_s.step * 40) / (p->len > 1u ? p->len - 1 : 1), 6);
        return "STEP";
    default:                                    /* the step's notes: the first one tells */
        if (!step_first(ui_s.step, &e)) {
            put_s(s, "-");
            return what == PG_SNOTE ? "NOTE" : what == PG_SVEL ? "VELO" : "LENGTH";
        }
        if (what == PG_SNOTE) {
            note_name(s, EV_KEY(e));
            card_bar(on, c, 0, ((int32_t)EV_KEY(e) * 46) / 127);
            return "NOTE";
        }
        if (what == PG_SVEL) {
            put_i(s, (int32_t)EV_VEL(e), 0);
            card_bar(on, c, 0, ((int32_t)EV_VEL(e) * 46) / 127);
            return "VELO";
        }
        i = (len_sub(e & 127u) + SQ_SUB / 2u) / SQ_SUB;
        put_i(s, (int32_t)(i ? i : 1u), 0);
        card_bar(on, c, 0, ((int32_t)(i > 16u ? 16u : i) * 46) / 16);
        return "LENGTH";
    }
}
static void draw_tile(uint32_t k)
{
    const page_t *pg = &PAGES[ui_s.page];
    uint32_t id = page_param(pg, k), on = ui_s.touched == k, fxt = 0;
    uint16_t c = COL[pg->col], bg = on ? c : U_PANEL, fg = on ? U_INK : U_TXT, lab = on ? U_INK : U_DIM;
    char s[16];
    const char *nm = 0;
    dv_begin(60, 0, 64, U_BG);
    if (pg->par[k] == NOP || (pg->par[k] < PG_X && fx_knob(id, &fxt) >= 0 && !fxt)) {   /* nothing, or a knob */
        dr_rect(1, 0, 58, 64, U_VOID);                                                    /* of an empty slot */
        H->cv_blit((int32_t)k * 60, Y_DECK);
        return;
    }
    dr_rect(1, 0, 58, 64, bg);
    dr_rect(7, 51, 46, 4, on ? dark565(c) : U_TRACK);
    s[0] = 0;
    if (pg->par[k] == PG_SLOT) {                /* the matrix: the slot, and which ones are in use */
        uint32_t i;
        nm = "SLOT";
        s[0] = (char)('1' + ui_s.slot);
        s[1] = 0;
        for (i = 0; i < 8u; i++)
            dr_rect(7 + (int32_t)i * 6, 51, 4, 4, i == ui_s.slot ? (on ? U_INK : c)
                                                   : cur.p[P_X1_SRC + 3u * i] && cur.p[P_X1_DST + 3u * i] ? U_DIM : U_TRACK);
    } else if (pg->par[k] >= PG_X) {            /* the page's own */
        nm = card_own(pg->par[k], s, on, c);
    } else {
        const pdef_t *d = &PDEF[id];
        int32_t v = cur.p[id], w;
        nm = param_name(id);
        if (id <= P_M4) {                       /* a macro: named after the first thing it moves */
            uint32_t i;
            nm = id == P_M1 ? "MACRO 1" : id == P_M2 ? "MACRO 2" : id == P_M3 ? "MACRO 3" : "MACRO 4";
            for (i = 0; i < 8u; i++)
                if (cur.p[P_X1_SRC + 3u * i] == S_M1 + id && cur.p[P_X1_DST + 3u * i]) {
                    nm = LS_DST[cur.p[P_X1_DST + 3u * i]];
                    break;
                }
        }
        param_text(s, id);
        if (d->fmt == F_LIST) {                 /* a list: a block at its place */
            int32_t bw = 46 / (d->max + 1);
            if (bw < 4)
                bw = 4;
            card_bar(on, c, ((46 - bw) * v) / (d->max ? d->max : 1), bw);
        } else if (param_bipolar(id)) {         /* from the middle */
            int32_t mid = fx_knob(id, &fxt) >= 0 ? 64 : d->aux, half = d->max - mid > mid ? d->max - mid : mid;
            w = ((v - mid) * 23) / (half ? half : 1);
            if (w >= 0)
                card_bar(on, c, 23, w);
            else
                card_bar(on, c, 23 + w, -w);
            dr_rect(29, 48, 1, 10, lab);
        } else {
            card_bar(on, c, 0, (v * 46) / (d->max ? d->max : 1));
        }
    }
    t_fit(7, 9, F_S, nm, 48, lab, bg);
    t_fit(7, 25, F_L, s, 51, fg, bg);
    H->cv_blit((int32_t)k * 60, Y_DECK);
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

/* ---- the display: the sound's line, the page's title, its picture (the rows 40..125 of the display, drawn from
 * the old 88-row geometry: pic_y maps it) */
static const char *const FT_LONG[NFT] = {"FILTER OFF", "LOW-PASS 6", "LOW-PASS 12", "LOW-PASS 18", "LOW-PASS 24",
                                         "LOW-PASS HOT", "HIGH-PASS 6", "HIGH-PASS 12", "HIGH-PASS 24", "BAND-PASS 12",
                                         "BAND-PASS 24", "NOTCH", "TWO NOTCHES", "PEAK", "WIDE BAND", "TWIN BANDS",
                                         "VOWEL", "COMB +", "COMB -", "DECIMATE", "RING"};
static const char *const LFO_LONG[6] = {"SINE", "TRIANGLE", "SAW", "SQUARE", "SAMPLE + HOLD", "DRIFT"};
static const char *const ARP_LONG[7] = {"ARP OFF", "UP", "DOWN", "UP + DOWN", "RANDOM", "AS PLAYED", "CHORD"};
static int32_t pic_y(int32_t y) { return 40 + (y * 86) / 88; }
static void col_fill(int32_t x, int32_t y0, int32_t y1, uint16_t c)   /* a column of the picture, y0 to y1 */
{
    if (y1 < y0) {
        int32_t t = y0;
        y0 = y1;
        y1 = t;
    }
    y0 = pic_y(y0);
    dr_rect(x, y0, 1, pic_y(y1) - y0 + 1, c);
}

/* the sound: its number (or STORED when REC shows it, RND for the dice's), its name, * when edited; a message
 * in its place for a while */
static void disp_line(const page_t *pg, uint16_t c)
{
    char s[24];
    int32_t x = 8;
    if (ui_s.msg) {
        int32_t w = t_w(F_M, ui_s.msg) + 12;
        dr_rect(4, 1, w, 18, c);
        t_draw(10, 6, F_M, ui_s.msg, U_INK, c);
        return;
    }
    if (ui_s.compare)
        put_s(s, "STORED");
    else if (ui_s.rnd)
        put_s(s, "RND");
    else
        preset_label(s, ui_s.preset);
    x += t_draw(x, 6, F_M, s, ui_s.compare ? U_WARN : c, U_BG) + 6;
    if (pg->disp == DS_PLAY || pg->disp == DS_RAND) {     /* (the name is the title there) */
        if (ui_s.edited && !ui_s.compare)
            t_draw(x, 6, F_M, "EDITED", U_DIM, U_BG);
        return;
    }
    x += t_draw(x, 6, F_M, cur.name, U_DIM, U_BG);
    if (ui_s.edited && !ui_s.compare)
        t_draw(x + 1, 6, F_M, "*", U_DIM, U_BG);
}
static void disp_title(const char *s) { t_fit(8, 24, F_L, s, 224, U_TXT, U_BG); }

static void disp_play(void)
{
    uint32_t i, seen = 0;
    int32_t x = 128, y = 74;
    char s[12];
    t_fit(8, 24, F_XL, cur.name, 224, U_TXT, U_BG);
    for (i = 0; i < NV; i++)                    /* (draw_meter keeps them lit) */
        dr_rect(8 + (int32_t)i * 28, 54, 24, 8, U_PANEL);
    for (i = 0; i < 2u; i++) {                  /* the two tables */
        uint32_t w = cur.p[i ? P_B_WAVE : P_A_WAVE], on = cur.p[i ? P_B_LVL : P_A_LVL] != 0;
        uint16_t c = on ? COL[i ? K_OSCB : K_OSCA] : U_TRACK;
        dr_rect(8, 74 + (int32_t)i * 20, 15, 15, c);
        t_draw(12, 78 + (int32_t)i * 20, F_S, i ? "B" : "A", U_INK, c);
        wave_name(s, w);
        t_draw(30, 77 + (int32_t)i * 20, F_M, s, on ? U_TXT : U_DIM, U_BG);
    }
    for (i = 0; i < NFX; i++) {                 /* the effects, in their order (one repeated is off) */
        uint32_t t = cur.p[P_FX1_TYPE + 4u * i];
        int32_t w;
        if (!t || (seen >> t & 1u))
            continue;
        seen |= 1u << t;
        w = t_w(F_S, LS_FX[t]) + 10;
        if (x + w > 236) {
            x = 128;
            y += 18;
        }
        if (y > 112)
            break;
        dr_rect(x, y, w, 15, COL[K_FX]);
        t_draw(x + 5, y + 4, F_S, LS_FX[t], U_INK, COL[K_FX]);
        x += w + 3;
    }
    if (!seen)
        t_draw(128, 78, F_S, "NO EFFECTS", U_FAINT, U_BG);
}

static void disp_wave(uint32_t o, uint16_t c)
{
    uint32_t po = o ? P_B_WAVE : P_A_WAVE, mode = cur.p[po + 2u], x;
    int32_t pos = (int32_t)cur.p[po + 1u] * 7 * 4096 / 127, mixf = pos & 4095;
    uint32_t f0 = (uint32_t)pos >> 12;
    const int16_t *t0, *t1;
    warp_t w = {4096u, 0x80000000u, 65536u, 65536u, ~0u, 0};
    char s[24];
    if (f0 >= AP_NF - 1u) {
        f0 = AP_NF - 1u;
        mixf = 0;
    }
    t0 = wt_buf[wt_cur[o]] + f0 * WT_FRM;
    t1 = t0 + (mixf ? WT_FRM : 0u);
    warp_set(&w, mode, (int32_t)cur.p[po + 3u] * 4095 / 127);
    wave_name(s, cur.p[po]);
    x = (uint32_t)t_fit(8, 24, F_L, s, 150, U_TXT, U_BG);
    if (mode != W_OFF)                          /* the warp, beside it */
        t_draw(8 + (int32_t)x + 8, 27, F_M, LS_WARP[mode], c, U_BG);
    for (x = 0; x < AP_NF; x++)                 /* the eight frames, and where POS is among them */
        dr_rect(176 + (int32_t)x * 7, 27, 5, 5, x == f0 || (mixf && x == f0 + 1u) ? c : U_FAINT);
    dr_rect(0, pic_y(46), 240, 1, U_FAINT);
    for (x = 0; x < 240u; x++) {
        uint32_t ph = warp_map(mode, &w, x * 17895697u), i = ph >> 23;
        int32_t a = t0[i], b = t1[i];
        a += ((b - a) * mixf) >> 12;
        col_fill((int32_t)x, 46, 46 - (a * 38) / 32768, c);
    }
}

static void disp_mix(void)
{
    static const char *const NM[4] = {"A", "B", "SUB", "NOISE"};
    static const uint8_t ID[4] = {P_A_LVL, P_B_LVL, P_SUB_LVL, P_NZ_LVL};
    static const uint8_t CL[4] = {K_OSCA, K_OSCB, K_MIX, K_VOICE};
    uint32_t i;
    char s[8];
    for (i = 0; i < 4u; i++) {
        int32_t y = 28 + (int32_t)i * 24, w = (cur.p[ID[i]] * 156) / 127;
        t_draw(8, y + 4, F_S, NM[i], U_DIM, U_BG);
        dr_rect(56, y, 156, 15, U_PANEL);
        if (w)
            dr_rect(56, y, w, 15, COL[CL[i]]);
        put_i(s, cur.p[ID[i]], 0);
        t_right(234, y + 3, F_M, s, U_DIM, U_BG);
    }
}

/* the filter, drawn as its kind: where CUTOFF is, how steep, where RESO lifts it */
static void disp_filter(uint16_t c)
{
    /* per type: 0 low-pass, 1 high-pass, 2 band, 3 notch, 4 peak, 5 comb, 6 steps, 7 ring; and its slope in px */
    static const uint8_t KIND[NFT] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 2, 2, 2, 5, 5, 6, 7};
    static const uint8_t SLOPE[NFT] = {0, 110, 60, 42, 30, 30, 110, 60, 30, 50, 28, 14, 14, 20, 60, 30, 30, 0, 0, 0, 0};
    int32_t type = PV(F_TYPE), xc = 20 + (PV(F_CUT) * 200) / 127, pk = (PV(F_RES) * 30) / 127, x, var = PV(F_VAR);
    int32_t slope = SLOPE[type], two = type == FT_NOTCH2 || type == FT_TWIN || type == FT_VOWEL ? 16 + var / 3 : 0;
    disp_title(FT_LONG[type]);
    if (type == FT_OFF)
        return;
    for (x = 0; x < 240; x++) {
        int32_t d = x - xc, ad = d < 0 ? -d : d, y, flat = 44, low = 86, d2 = d - two, ad2 = d2 < 0 ? -d2 : d2;
        int32_t bump = ad < 24 ? (pk * (24 - ad)) / 24 : 0;
        switch (KIND[type]) {
        case 1: y = d >= 0 ? flat : flat - (d * (low - flat)) / slope; break;
        case 2:
            y = flat + (ad * (low - flat)) / slope - 14;
            if (type == FT_BANDW)
                y = d > 0 ? flat + (d * (low - flat)) / 60 : d > -(20 + var / 2) ? flat : flat + ((-d - 20 - var / 2) * (low - flat)) / 30;
            if (two && ad2 < ad)
                y = flat + (ad2 * (low - flat)) / slope - 14;
            bump = type == FT_BANDW ? bump : 0;
            break;
        case 3:
            y = ad < slope ? low - (ad * (low - flat)) / slope : flat;
            if (two && ad2 < slope)
                y = low - (ad2 * (low - flat)) / slope;
            bump = 0;
            break;
        case 4: y = flat - (ad < slope ? ((slope - ad) * (14 + pk)) / slope : 0); bump = 0; break;
        case 5: {                               /* a comb: teeth, closer together as CUTOFF goes down */
            int32_t per = 8 + (127 - PV(F_CUT)) / 5, ph = x % per;
            y = flat - ((ph < per / 2 ? ph : per - ph) * (10 + pk)) / (per / 2) + 10;
            bump = 0;
            break;
        }
        case 6: {                               /* steps, wider as CUTOFF goes down */
            int32_t st = 4 + (127 - PV(F_CUT)) / 6;
            y = 70 - ((x / st) * st * 50) / 240;
            bump = 0;
            break;
        }
        case 7: y = flat - (sinr[((uint32_t)x * (2u + (uint32_t)PV(F_CUT) / 8u) * 17u) & 1023u] * 30) / 32768; bump = 0; break;
        default: y = d <= 0 ? flat : flat + (d * (low - flat)) / slope; break;
        }
        y -= bump;
        if (y > low) y = low;
        if (y < 14) y = 14;
        col_fill(x, 87, y, c);
    }
    if (KIND[type] < 5)                         /* where CUTOFF is */
        dr_rect(xc, pic_y(10), 1, pic_y(87) - pic_y(10) + 1, U_BG);
}

static void disp_env(uint32_t base, uint16_t c)
{
    int32_t a = 6 + (cur.p[base] * 52) / 127, d = 6 + (cur.p[base + 1u] * 52) / 127, r = 6 + (cur.p[base + 3u] * 52) / 127;
    int32_t sl = 84 - (cur.p[base + 2u] * 66) / 127, x, hold = 52, x0 = 8;
    disp_title(base == P_E1_A ? "AMPLITUDE" : base == P_E2_A ? "FILTER AND MATRIX" : "MATRIX");
    for (x = 0; x < a + d + hold + r; x++) {
        int32_t y;
        if (x < a) y = 84 - (x * 66) / a;
        else if (x < a + d) y = 18 + ((x - a) * (sl - 18)) / d;
        else if (x < a + d + hold) y = sl;
        else y = sl + ((x - a - d - hold) * (84 - sl)) / r;
        col_fill(x0 + x, 86, y, c);
    }
    dr_rect(x0 + a + d + hold, pic_y(14), 1, pic_y(86) - pic_y(14) + 1, U_BG);    /* where the key is let go */
}

static void disp_lfo(uint32_t base, uint16_t c)
{
    static const int8_t RN[9] = {40, -70, 10, 90, -30, -100, 60, -10, 40};
    uint32_t shape = cur.p[base], x;
    int32_t w = t_fit(8, 24, F_L, LFO_LONG[shape], 150, U_TXT, U_BG);
    if (cur.p[base + 2u])                       /* in time: the note length */
        t_draw(8 + w + 8, 27, F_M, LS_DIV[(cur.p[base + 1u] * 12u) >> 7], c, U_BG);
    if (base == P_L3_SHAPE)
        t_right(232, 27, F_M, "ONE FOR ALL NOTES", U_DIM, U_BG);
    dr_rect(8, pic_y(52), 224, 1, U_FAINT);
    for (x = 0; x < 224u; x++) {
        uint32_t ph = x * 38347923u;            /* two cycles across */
        int32_t v, t;
        switch (shape) {
        case 0: v = sinr[ph >> 22] >> 8; break;
        case 1: t = (int32_t)(ph >> 23) - 256; v = (t < 0 ? -t : t) - 128; break;
        case 2: v = 127 - (int32_t)(ph >> 24); break;
        case 3: v = ph < 0x80000000u ? 127 : -127; break;
        case 4: v = RN[x / 28u]; break;
        default: v = RN[x / 28u] + ((RN[x / 28u + 1u] - RN[x / 28u]) * (int32_t)(x % 28u)) / 28; break;
        }
        col_fill(8 + (int32_t)x, 52, 52 - (v * 32) / 128, c);
    }
}

/* the matrix: its eight slots, the one the knobs edit on the section's colour */
static void disp_mod(uint16_t c)
{
    uint32_t i;
    for (i = 0; i < 8u; i++) {
        uint32_t s = cur.p[P_X1_SRC + 3u * i], d = cur.p[P_X1_DST + 3u * i], sel = i == ui_s.slot;
        int32_t y = 22 + (int32_t)i * 13, amt = (int32_t)cur.p[P_X1_AMT + 3u * i] - 100;
        uint16_t bg = sel ? c : U_BG, fg = sel ? U_INK : s && d ? U_TXT : U_FAINT;
        char t[8];
        if (sel)
            dr_rect(4, y - 1, 232, 13, c);
        t[0] = (char)('1' + i);
        t[1] = 0;
        t_draw(10, y + 2, F_S, t, sel ? U_INK : U_DIM, bg);
        t_draw(26, y + 1, F_M, LS_SRC[s], fg, bg);
        t_draw(104, y + 1, F_M, ">", sel ? U_INK : U_FAINT, bg);
        t_draw(120, y + 1, F_M, LS_DST[d], fg, bg);
        if (s && d) {
            put_i(t, amt, 1);
            t_right(230, y + 1, F_M, t, fg, bg);
        }
    }
}

static void disp_arp(uint16_t c)
{
    uint32_t mode = (uint32_t)PV(ARP_MODE), octs = (uint32_t)PV(ARP_OCT) + 1u, s;
    char t[16], *p;
    disp_title(ARP_LONG[mode < 7u ? mode : 0u]);
    p = put_i(t, 60 + PV(TEMPO), 0);
    put_s(p, " BPM");
    t_right(232, 27, F_M, seq_ext ? "MIDI CLOCK" : t, U_DIM, U_BG);
    if (mode == ARP_OFF)
        return;
    for (s = 0; s < 16u; s++) {                 /* sixteen steps of a three-note chord */
        uint32_t oct = 0, k = mode == ARP_RANDOM ? (s * 7u + 3u) % 3u : mode == ARP_CHORD ? 0u
                            : arp_pick(mode == ARP_ORDER ? ARP_UP : mode, s, 3, octs, &oct);
        int32_t h, x = 8 + (int32_t)s * 14 + ((s & 1u) ? (PV(ARP_SWING) * 12) / 150 : 0);
        if (mode == ARP_CHORD)
            oct = s % octs;
        if (mode == ARP_RANDOM)
            oct = (s * 5u) % octs;
        h = 10 + (int32_t)(k + 3u * oct) * 52 / (int32_t)(3u * octs);
        dr_rect(x, 40 + ((84 - h) * 86) / 88, 2 + (PV(ARP_GATE) * 10) / 100, mode == ARP_CHORD ? (h * 86) / 88 : 7, c);
    }
}

/* the rack: the seven slots in their order, the one the knobs edit on the section's colour; its effect, big */
static void disp_fx(uint32_t which, uint16_t c)
{
    uint32_t i, seen = 0, dup_of = 0, t = cur.p[P_FX1_TYPE + 4u * which];
    char s[24], *p;
    for (i = 0; i < NFX; i++) {
        uint32_t ti = cur.p[P_FX1_TYPE + 4u * i], dup = ti && (seen >> ti & 1u), sel = i == which;
        int32_t x = 8 + (int32_t)i * 32;
        uint16_t bg = sel ? c : ti && !dup ? U_PANEL : U_BG;
        char ab[4];
        if (sel && dup)
            for (dup_of = 0; cur.p[P_FX1_TYPE + 4u * dup_of] != ti; dup_of++) ;
        seen |= 1u << ti;
        if (bg == U_BG) {                       /* an empty slot: its outline */
            dr_rect(x, 22, 29, 1, U_TRACK);
            dr_rect(x, 49, 29, 1, U_TRACK);
            dr_rect(x, 22, 1, 28, U_TRACK);
            dr_rect(x + 28, 22, 1, 28, U_TRACK);
        } else {
            dr_rect(x, 22, 29, 28, bg);
        }
        ab[0] = (char)('1' + i);
        ab[1] = 0;
        t_draw(x + 4, 26, F_S, ab, sel ? U_INK : U_FAINT, bg);
        if (ti) {
            ab[0] = LS_FX[ti][0];
            ab[1] = LS_FX[ti][1];
            ab[2] = ti == FX_EQ ? 0 : LS_FX[ti][2];
            ab[3] = 0;
            t_draw(x + 4, 38, F_S, ab, sel ? U_INK : dup ? U_FAINT : U_TXT, bg);
        }
    }
    t_fit(8, 62, F_XL, t ? LS_FX[t] : "EMPTY", 224, t ? U_TXT : U_FAINT, U_BG);
    p = put_s(s, "SLOT ");
    p = put_i(p, (int32_t)which + 1, 0);
    put_s(p, " OF 7");
    if (t && dup_of < which && cur.p[P_FX1_TYPE + 4u * dup_of] == t) {
        p = put_s(s, "OFF: SLOT ");
        p = put_i(p, (int32_t)dup_of + 1, 0);
        put_s(p, " HAS IT");
    }
    t_draw(8, 94, F_M, s, U_DIM, U_BG);
}

static void disp_voice(uint16_t c)
{
    char t[40], *p;
    int32_t load = (int32_t)(eng_load / 1392u);
    disp_title(LS_MODE[PV(MODE) < 3 ? PV(MODE) : 0]);
    p = put_s(t, "NOTES ");
    p = put_i(p, (int32_t)eng_max_notes(), 0);
    p = put_s(p, "   UNISON ");
    p = put_i(p, PV(A_UNI) + 1, 0);
    p = put_s(p, " + ");
    put_i(p, PV(B_UNI) + 1, 0);
    t_draw(8, 48, F_M, t, U_TXT, U_BG);
    t_draw(8, 66, F_M, guard_max < NV ? "THE CHIP IS BUSY: FEWER NOTES" : "A HEAVY SOUND HAS FEWER NOTES", U_DIM, U_BG);
    if (load > 100)
        load = 100;
    p = put_s(t, "LOAD ");                      /* the worst block of late, of the 5.8 ms it has */
    p = put_i(p, (int32_t)(eng_load / 1392u), 0);
    put_s(p, "%");
    t_draw(8, 92, F_S, t, eng_load > 97000u ? U_WARN : U_DIM, U_BG);
    dr_rect(64, 92, 168, 8, U_PANEL);
    if (load)
        dr_rect(64, 92, (load * 168) / 100, 8, load > 70 ? U_WARN : c);
}

/* RANDOM: the sound's name; the sounds kept; what SEL does */
static void disp_rand(uint16_t c)
{
    uint32_t i;
    t_fit(8, 24, F_XL, cur.name, 224, U_TXT, U_BG);
    for (i = 0; i <= RH; i++)                   /* the one before the dice, then the sounds kept */
        dr_rect(8 + (int32_t)i * 15, 56, 11, 11, i > rd.n || (!i && !rd.before) ? U_PANEL
                                                 : i == rd.pos && !rd.trying ? c : U_DIM);
    t_draw(148, 57, F_M, rd.trying ? "NOT KEPT" : rd.pos ? "KEPT" : rd.before ? "BEFORE" : "", rd.trying ? U_WARN : U_TXT, U_BG);
    t_draw(8, 84, F_M, rd.what ? "SEL: A VARIATION OF THIS" : "SEL: A NEW SOUND", U_DIM, U_BG);
    t_draw(8, 102, F_M, rd.trying ? "HOLD SEL: KEEP IT" : rd.pos ? "HOLD SEL: TAKE IT OUT" : "", U_DIM, U_BG);
}

/* ---- the sequencer: its 64 steps as four rows of sixteen. A step with notes on its colour, a mark for each of
 * them as high as it is; the playhead white; the step STEPS edits framed. Drawn straight to the screen too, a
 * step at a time, as the playhead goes (seq_cells) */
#define SQ_X 8
#define SQ_Y 44
static int32_t seq_rowh(void)                   /* a row's height: the fewer the rows, the taller (a piano roll) */
{
    static const uint8_t RH_[4] = {60, 32, 21, 15};
    return RH_[(pats[pl.pat].len - 1u) >> 4 & 3u];
}
static void seq_cell(uint32_t i, int32_t yoff, uint32_t steps_page, uint16_t c, int32_t lo, int32_t span)
{
    const pat_t *p = &pats[pl.pat];
    int32_t rh = seq_rowh(), x = SQ_X + (int32_t)(i & 15u) * 14, y = SQ_Y + (int32_t)(i >> 4) * (rh + 3) + yoff;
    uint32_t k, has = 0, head = pl.run && pl.rec != 1u && pl.sp / SQ_SUB == i;
    uint16_t bg;
    for (k = 0; k < p->n; k++)
        if (EV_POS(p->ev[k]) / SQ_SUB == i)
            has = 1;
    bg = i >= p->len ? U_BG : head ? U_TXT : has ? c : (i & 3u) ? U_PANEL : U_TRACK;
    if (i >= p->len)
        return;
    H->fill(x, y, 13, rh, bg);
    if (has)
        for (k = 0; k < p->n; k++)
            if (EV_POS(p->ev[k]) / SQ_SUB == i)
                H->fill(x + 2, y + rh - 4 - ((int32_t)EV_KEY(p->ev[k]) - lo) * (rh - 6) / span, 9, 2, U_INK);
    if (steps_page && i == ui_s.step) {         /* the step edited: framed */
        H->fill(x - 1, y - 1, 15, 1, U_TXT);
        H->fill(x - 1, y + rh, 15, 1, U_TXT);
        H->fill(x - 1, y, 1, rh, U_TXT);
        H->fill(x + 13, y, 1, rh, U_TXT);
    }
}
static void seq_range(int32_t *lo, int32_t *span)   /* the lowest note of the pattern, and how far they go */
{
    const pat_t *p = &pats[pl.pat];
    uint32_t k;
    int32_t l = 127, h = 0;
    for (k = 0; k < p->n; k++) {
        int32_t key = (int32_t)EV_KEY(p->ev[k]);
        if (key < l) l = key;
        if (key > h) h = key;
    }
    *lo = l > h ? 60 : l;
    *span = h - *lo < 12 ? 12 : h - *lo;
}
static uint32_t seq_state(void)                 /* what the title says, as a number (it changes: draw again) */
{
    return pl.rec == 1u ? 16u + pl.count / (4u * SQ_SUB) : (uint32_t)pl.run << 1 | (pl.rec == 2u) | (uint32_t)pl.pat << 2;
}
static void disp_seq(uint32_t steps_page, uint16_t c)
{
    const pat_t *p = &pats[pl.pat];
    char t[40], *q;
    uint32_t i;
    if (steps_page) {                           /* the step: its notes */
        uint32_t keys[8], n;
        q = put_s(t, "STEP ");
        q = put_i(q, ui_s.step + 1, 0);
        t_draw(8, 24, F_L, t, U_TXT, U_BG);
        n = step_notes(p, ui_s.step, keys, 4u);
        q = t;
        for (i = 0; i < n; i++) {
            q = note_name(q, keys[i]);
            *q++ = ' ';
        }
        *q = 0;
        t_right(232, 27, F_M, n ? t : "EMPTY", n ? c : U_FAINT, U_BG);
    } else {
        q = put_s(t, "PATTERN ");
        *q++ = (char)('A' + pl.pat);
        *q = 0;
        t_draw(8, 24, F_L, t, U_TXT, U_BG);
        if (pl.rec == 1u) {
            q = put_s(t, "COUNT ");
            put_i(q, (int32_t)((pl.count + 4u * SQ_SUB - 1u) / (4u * SQ_SUB)), 0);
        } else {
            q = put_i(t, p->n, 0);
            put_s(q, p->n == 1u ? " NOTE" : " NOTES");
        }
        t_right(232, 27, F_M, t, pl.rec ? U_WARN : U_DIM, U_BG);
    }
    for (i = 0; i < p->len; i++)                /* (the cells, in the strip: then straight on the screen) */
        dr_rect(SQ_X + (int32_t)(i & 15u) * 14, SQ_Y + (int32_t)(i >> 4) * (seq_rowh() + 3), 13, seq_rowh(), U_PANEL);
    t_draw(8, 118, F_S, pl.rec == 2u ? "RECORDING" : pl.run ? "PLAYING" : "STOPPED", pl.rec ? U_WARN : U_DIM, U_BG);
    if (steps_page)
        t_right(232, 118, F_S, pl.run ? "KEYS: PLAY" : "KEYS: WRITE   SEL: CLEAR", U_FAINT, U_BG);
    else
        t_right(232, 118, F_S, "SEL: UNDO   HOLD REC: CLEAR", U_FAINT, U_BG);
}
static void seq_cells(uint32_t all)             /* straight to the screen: every cell, or the playhead's two */
{
    const page_t *pg = &PAGES[ui_s.page];
    uint32_t now = pl.run && pl.rec != 1u ? pl.sp / SQ_SUB : 255u, i;
    int32_t lo, span;
    if (pg->disp != DS_SEQ || ui_s.save)
        return;
    seq_range(&lo, &span);
    if (all) {
        for (i = 0; i < 64u; i++)
            seq_cell(i, Y_DISP, pg->arg, COL[pg->col], lo, span);
    } else if (now != ui_s.sq_cell) {
        if (ui_s.sq_cell < 64u)
            seq_cell(ui_s.sq_cell, Y_DISP, pg->arg, COL[pg->col], lo, span);
        if (now < 64u)
            seq_cell(now, Y_DISP, pg->arg, COL[pg->col], lo, span);
    }
    ui_s.sq_cell = (uint8_t)now;
}

static void draw_meter(uint32_t force);

/* the display, one strip of it: 0 its rows 0..95, 1 the rest */
static void draw_disp(uint32_t strip)
{
    const page_t *pg = &PAGES[ui_s.page];
    uint16_t c = COL[pg->col];
    int32_t y0 = strip ? 96 : 0;
    dv_begin(240, y0, strip ? H_DISP - 96 : 96, U_BG);
    disp_line(pg, c);
    switch (pg->disp) {
    case DS_PLAY: disp_play(); break;
    case DS_WAVE_A: disp_wave(0, c); break;
    case DS_WAVE_B: disp_wave(1, c); break;
    case DS_MIX: disp_mix(); break;
    case DS_FILT: disp_filter(c); break;
    case DS_ENV: disp_env(pg->arg, c); break;
    case DS_LFO: disp_lfo(pg->arg, c); break;
    case DS_MOD: disp_mod(c); break;
    case DS_ARP: disp_arp(c); break;
    case DS_FX: disp_fx(pg->arg, c); break;
    case DS_RAND: disp_rand(c); break;
    case DS_SEQ: disp_seq(pg->arg, c); break;
    default: disp_voice(c); break;
    }
    H->cv_blit(0, Y_DISP + y0);
    if (!strip && pg->disp == DS_PLAY)
        ui_s.act_drawn = 0xFFFFFFFFu;           /* (the notes, again) */
    if (pg->disp == DS_SEQ) {                   /* (the steps, on it) */
        seq_cells(1);
        ui_s.sq_ver = pat_ver;
        ui_s.sq_state = (uint8_t)seq_state();
    }
}

/* straight to the screen when they change: the level going out (the header), the notes sounding (PLAY) */
static void draw_meter(uint32_t force)
{
    uint32_t act = eng_active, i, lv = 30u;
    int32_t v = out_lvl, x = LIM_T;
    out_lvl = 0;
    while (x > v && lv) {                       /* about 1.2 dB a pixel down from the limiter's */
        x = (x * 7) >> 3;
        lv--;
    }
    if (lv + 2u < ui_s.lvl_shown)               /* down slowly, up at once */
        lv = ui_s.lvl_shown - 2u;
    if (force || lv != ui_s.lvl_drawn) {        /* (on the top line) */
        H->fill(204, 10, 30, 4, U_PANEL);
        if (lv)
            H->fill(204, 10, (int32_t)lv, 4, U_DIM);
        if (lv >= 29u)                          /* the limiter at work */
            H->fill(204 + 27, 10, (int32_t)lv - 27, 4, U_WARN);
        ui_s.lvl_drawn = (uint8_t)lv;
    }
    ui_s.lvl_shown = (uint8_t)lv;
    if (PAGES[ui_s.page].disp == DS_SEQ && !ui_s.save) {   /* the sequencer: the playhead, what changed */
        if (pat_ver != ui_s.sq_ver || seq_state() != ui_s.sq_state)
            ui_s.d_disp = 1;
        else
            seq_cells(0);
    }
    if (PAGES[ui_s.page].disp != DS_PLAY || ui_s.save)
        return;
    if (!force && act == ui_s.act_drawn)
        return;
    for (i = 0; i < NV; i++)
        if (force || ((act ^ ui_s.act_drawn) >> i & 1u))
            H->fill(8 + (int32_t)i * 28, Y_DISP + 54, 24, 8, act >> i & 1u ? COL[K_PLAY] : U_PANEL);
    ui_s.act_drawn = act;
}

static void ui_dirty_all(void)
{
    ui_s.d_top = 1;
    ui_s.d_head = ui_s.d_disp = 1;
    ui_s.d_tile = 15;
}

static void ui_leds(void)
{
    static const uint8_t BT[] = {HB_BTN_HOME, HB_BTN_EDIT, HB_BTN_ENV, HB_BTN_LFO, HB_BTN_SEQ, HB_BTN_ARP, HB_BTN_FX,
                                 HB_BTN_GLO};
    uint32_t i, d = PAGES[ui_s.page].disp;
    for (i = 0; i < sizeof BT; i++)
        H->led(BT[i], PAGES[ui_s.page].btn == BT[i]);
    H->led(HB_BTN_SEL, d == DS_RAND || d == DS_SEQ || d == DS_MOD);   /* SEL does something here */
    H->led(HB_BTN_SAVE, ui_s.save);
    oct_led = 0xFFu;
    tr_led = 0xFFu;
}

static void ui_go(uint32_t page)
{
    ui_s.page = (uint8_t)page;
    ui_s.last[PAGES[page].btn & 15u] = (uint8_t)page;
    ui_s.touched = 255;
    ui_dirty_all();
    ui_leds();
}

#endif
