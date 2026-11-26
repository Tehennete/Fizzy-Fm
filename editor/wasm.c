/* SPDX-License-Identifier: GPL-3.0-only */
/* Fizzy-Fm in a browser: the module (fizzyfm/fizzyfm.c) unchanged, with fm1-dock's screen code and font under it, built as
 * WebAssembly (tools/build_editor.py). The editor's page plays this, shows its screen, and takes the parameters'
 * names, ranges and texts from it: the page and the FM-1 cannot disagree. Copyright (C) 2026 Hugo Gilbert */
#include <stdint.h>
#define EXPORT(n) __attribute__((export_name(#n)))

static uint16_t fb[240 * 240];
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = c;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u) {
                uint16_t c = px[j * w + i];
                fb[(y + j) * 240u + x + i] = (uint16_t)(c >> 8 | c << 8);
            }
}
#define CV_MAX (240u * 96u)
#include "gfx.c"
#include "hb_lib.c"

#define HB_DESC const hb_module_t hb_module
#include "../fizzyfm/fizzyfm.c"

static uint32_t now_ms, leds;
static uint32_t h_ms(void) { return now_ms; }
static uint32_t h_ticks(void) { return 0; }
static void h_log(const char *s) { (void)s; }
static void h_log_u32(const char *s, uint32_t v) { (void)s; (void)v; }
static void h_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > 240) w = 240 - x;
    if (y + h > 240) h = 240 - y;
    if (w > 0 && h > 0)
        lcd_fill((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, c);
}
static void h_text(int32_t x, int32_t y, int32_t w, uint32_t big, const char *s, uint16_t fg, uint32_t center)
{
    const felucca_font_t *f = big ? &FONT_L : &FONT_S;
    if (x < 0 || y < 0 || w <= 0 || x + w > 240 || y + (int32_t)f->h > 240)
        return;
    draw_text_box((uint32_t)x, (uint32_t)y, (uint32_t)w, f, s, fg, center ? 1 : 0);
}
static void h_cv_begin(int32_t w, int32_t h, uint16_t bg)
{
    if (w < 1) w = 1;
    if (w > 240) w = 240;
    if (h < 1) h = 1;
    if ((uint32_t)(w * h) > CV_MAX) h = (int32_t)(CV_MAX / (uint32_t)w);
    cv_begin((uint32_t)w, (uint32_t)h, bg);
}
static void h_cv_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { cv_rect(x, y, w, h, c); }
static void h_cv_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c) { cv_line(x0, y0, x1, y1, c); }
static void h_cv_pixel(int32_t x, int32_t y, uint16_t c) { cv_pset(x, y, c); }
static void h_cv_text(int32_t x, int32_t y, uint32_t big, const char *s, uint16_t fg) { cv_text(x, y, big ? &FONT_L : &FONT_S, s, fg); }
static void h_cv_blit(int32_t x, int32_t y)
{
    if (x >= 0 && y >= 0 && x + (int32_t)cv_w <= 240 && y + (int32_t)cv_h <= 240)
        cv_blit((uint32_t)x, (uint32_t)y);
}
static void h_led(uint32_t id, uint32_t on) { if (id < 32u) leds = on ? leds | 1u << id : leds & ~(1u << id); }
static uint32_t rs = 12345;
static uint32_t h_rand(void) { rs = rs * 1664525u + 1013904223u; return rs; }
static int h_load(void *p, uint32_t n) { (void)p; (void)n; return -1; }
static void h_save(const void *p, uint32_t n) { (void)p; (void)n; }
static void h_midi_send(uint32_t a, uint32_t b, uint32_t c) { (void)a; (void)b; (void)c; }
static uint32_t h_master(void) { return 4096; }
static void h_blit(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *px) { lcd_blit((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, px); }
static int32_t h_text_w(uint32_t big, const char *s) { return text_w(big ? &FONT_L : &FONT_S, s); }
static int h_bank_load(void *p, uint32_t n) { (void)p; (void)n; return -1; }
static int h_bank_save(const void *p, uint32_t n) { (void)p; (void)n; return 0; }
static const hb_host_t host = {HB_ABI, (uint32_t)__builtin_offsetof(hb_host_t, sense_keys), h_ms, h_ticks, h_log,
                               h_log_u32, h_fill, h_text, h_cv_begin, h_cv_rect, h_cv_line, h_cv_pixel, h_cv_text,
                               h_cv_blit, h_led, h_rand, h_load, h_save, h_midi_send, h_master, h_blit, h_text_w,
                               h_bank_load, h_bank_save};

static int32_t blk[512];
static char txt[32];
#define X(id, name, max, def, fmt, aux) #id,
static const char *const IDS[NP] = {AP_PARAMS(X)};
#undef X

EXPORT(w_init) void w_init(void)
{
    lcd_fill(0, 0, 240, 24, 0);
    draw_text_box(4, 4, 120, &FONT_S, "FIZZY-FM", 0xFFFF, 0);
    hb_module.init(&host);
    hb_module.draw(1);
}
/* 256 frames, left and right by turns, +-32767; ms: the time now */
EXPORT(w_render) int32_t *w_render(uint32_t ms)
{
    uint32_t i;
    now_ms = ms;
    for (i = 0; i < 512u; i++)
        blk[i] = 0;
    hb_module.render(blk, 256);
    return blk;
}
EXPORT(w_draw) uint16_t *w_draw(uint32_t ms) { now_ms = ms; hb_module.draw(0); return fb; }
EXPORT(w_midi) void w_midi(uint32_t st, uint32_t d1, uint32_t d2) { hb_module.midi(st, d1, d2); }
EXPORT(w_ui) void w_ui(uint32_t ev, int32_t arg) { hb_module.ui(ev, arg); }
EXPORT(w_leds) uint32_t w_leds(void) { return leds; }
EXPORT(w_active) uint32_t w_active(void) { return eng_active; }
EXPORT(w_max_notes) uint32_t w_max_notes(void) { return eng_max_notes(); }

/* the parameters, as the FM-1 has them */
EXPORT(w_np) uint32_t w_np(void) { return NP; }
EXPORT(w_patch) uint8_t *w_patch(void) { return (uint8_t *)&cur; }             /* NP bytes, then the name */
EXPORT(w_patch_size) uint32_t w_patch_size(void) { return sizeof cur; }
EXPORT(w_changed) void w_changed(void) { patch_check(&cur); ui_s.edited = 1; ui_dirty_all(); }
EXPORT(w_hush) void w_hush(void) { ev_put(EV_HUSH, 0, 0); }
EXPORT(w_id) const char *w_id(uint32_t i) { return i < NP ? IDS[i] : ""; }
EXPORT(w_name) const char *w_name(uint32_t i) { return i < NP ? param_name(i) : ""; }
EXPORT(w_max) uint32_t w_max(uint32_t i) { return i < NP ? PDEF[i].max : 0; }
EXPORT(w_def) uint32_t w_def(uint32_t i) { return i < NP ? PDEF[i].def : 0; }
EXPORT(w_is_list) uint32_t w_is_list(uint32_t i) { return i < NP && PDEF[i].fmt == F_LIST; }
EXPORT(w_bipolar) uint32_t w_bipolar(uint32_t i) { return i < NP ? param_bipolar(i) : 0; }
EXPORT(w_centre) uint32_t w_centre(uint32_t i) { uint32_t t; return i < NP ? (fx_knob(i, &t) >= 0 ? 64u : (uint32_t)PDEF[i].aux) : 0; }
/* the text of value v of parameter i (as the screen writes it) */
EXPORT(w_text) const char *w_text(uint32_t i, uint32_t v)
{
    uint8_t keep;
    if (i >= NP)
        return "";
    keep = cur.p[i];
    cur.p[i] = (uint8_t)(v > PDEF[i].max ? PDEF[i].max : v);
    param_text(txt, i);
    cur.p[i] = keep;
    return txt;
}
EXPORT(w_fx_def) uint32_t w_fx_def(uint32_t type, uint32_t k) { return type < NFXT && k < 3u ? FXDEF[type].def[k] : 0; }
EXPORT(w_nfactory) uint32_t w_nfactory(void) { return NFACTORY; }
EXPORT(w_nuser) uint32_t w_nuser(void) { return NUSER; }
EXPORT(w_factory_name) const char *w_factory_name(uint32_t i) { return i < NFACTORY ? FACTORY[i].name : ""; }
EXPORT(w_preset) void w_preset(uint32_t i) { preset_load(i); }
EXPORT(w_preset_now) uint32_t w_preset_now(void) { return ui_s.preset; }
/* the dice (fizzyfm/ap_rand.h), as the RANDOM page throws them: a new sound of this kind, or (mutate) the one
 * there is, moved a little; the kind thrown is returned */
EXPORT(w_nrk) uint32_t w_nrk(void) { return NRK; }
EXPORT(w_rk_name) const char *w_rk_name(uint32_t i) { return i < NRK ? RK_NAME[i] : ""; }
EXPORT(w_random) uint32_t w_random(uint32_t kind, uint32_t chaos, uint32_t mutate)
{
    rd.kind = (uint8_t)(kind < NRK ? kind : RK_ANY);
    rd.chaos = (uint8_t)(chaos > 127u ? 127u : chaos);
    if (rd.n)
        rd.pos = (uint8_t)(rd.n - 1u);          /* (always a new one: the page has its own undo) */
    rand_turn(mutate, 1);
    return rd.how[rd.pos];
}
/* the tables of one's own: NUWT of 8 frames x 64 harmonics x (sine part, cosine part), 16 bits */
EXPORT(w_uwt) int16_t *w_uwt(void) { return uwt[0]; }
EXPORT(w_uwt_changed) void w_uwt_changed(uint32_t slot)
{
    uint32_t b;
    for (b = 0; b < 3u; b++)
        if (wt_tab[b] == AP_NTAB + slot)
            wt_tab[b] = 255;
}
EXPORT(w_ntab) uint32_t w_ntab(void) { return AP_NTAB; }
/* the wave oscillator o plays now, as the screen draws it: 240 points, -127..127 */
static int8_t wave[240];
EXPORT(w_wave) int8_t *w_wave(uint32_t o)
{
    uint32_t po = o ? P_B_WAVE : P_A_WAVE, mode = cur.p[po + 2u], x;
    int32_t pos = (int32_t)cur.p[po + 1u] * 7 * 4096 / 127, mixf = pos & 4095;
    uint32_t f0 = (uint32_t)pos >> 12;
    const int16_t *t0, *t1;
    warp_t w = {4096u, 0x80000000u, 65536u, 65536u, ~0u, 0};
    if (f0 >= AP_NF - 1u) { f0 = AP_NF - 1u; mixf = 0; }
    t0 = wt_buf[wt_cur[o]] + f0 * WT_FRM;
    t1 = t0 + (mixf ? WT_FRM : 0u);
    warp_set(&w, mode, (int32_t)cur.p[po + 3u] * 4095 / 127);
    for (x = 0; x < 240u; x++) {
        uint32_t ph = warp_map(mode, &w, x * 17895697u), i = ph >> 23;
        int32_t a = t0[i], b = t1[i];
        a += ((b - a) * mixf) >> 12;
        wave[x] = (int8_t)(a / 258);
    }
    return wave;
}
