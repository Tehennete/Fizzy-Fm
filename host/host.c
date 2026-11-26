/* SPDX-License-Identifier: GPL-3.0-only */
/* APLAT on a computer: the module (aplat/aplat.c) unchanged, on a stand-in for fm1-dock's host. A script plays
 * notes, turns knobs and presses buttons over time; out come the sound (WAV) and the screen (PPM) at the moments
 * the script asks. The screen is drawn with fm1-dock's own gfx.c and font.
 *
 *   cc -O2 -I$DOCK/firmware/dock -I$DOCK/firmware/src -I$DOCK/firmware/gen -o aplat_host host/host.c
 *   aplat_host OUT script.txt
 * Script lines: "T on KEY VEL", "T off KEY", "T btn ID", "T enc ID CLICKS", "T midi ST D1 D2", "T shot NAME",
 * "T end"; T in ms. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the screen, as tests/host.c of fm1-dock has it */
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

#define HB_DESC const hb_module_t hb_module
#include "../aplat/aplat.c"

static uint32_t now_ms, blits, leds;
static uint32_t h_ms(void) { return now_ms; }
/* APLAT_LOAD=N: every voice sounding takes N % of a block's time (the guard that sheds notes can be tried) */
static uint32_t load_pct, tick_calls, tick_acc;
static uint32_t h_ticks(void)
{
    if (load_pct && (tick_calls++ & 1u)) {      /* (render() asks twice: before and after) */
        uint32_t i, nv = 0;
        for (i = 0; i < NV; i++)
            nv += voice[i].on != 0;
        tick_acc += 256u * 544u * load_pct / 100u * nv;
    }
    return now_ms * 24000u + tick_acc;
}
static void h_log(const char *s) { printf("%6u ms  log: %s\n", now_ms, s); }
static void h_log_u32(const char *s, uint32_t v) { printf("%6u ms  log: %s %08x\n", now_ms, s, v); }
static void h_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > 240) w = 240 - x;
    if (y + h > 240) h = 240 - y;
    if (y < HB_TOP) { fprintf(stderr, "fill above HB_TOP\n"); exit(2); }
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
    if ((uint32_t)(w * h) > CV_MAX) { fprintf(stderr, "canvas %d x %d over CV_MAX\n", w, h); exit(2); }
    cv_begin((uint32_t)w, (uint32_t)h, bg);
}
static void h_cv_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { cv_rect(x, y, w, h, c); }
static void h_cv_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c) { cv_line(x0, y0, x1, y1, c); }
static void h_cv_pixel(int32_t x, int32_t y, uint16_t c) { cv_pset(x, y, c); }
static void h_cv_text(int32_t x, int32_t y, uint32_t big, const char *s, uint16_t fg) { cv_text(x, y, big ? &FONT_L : &FONT_S, s, fg); }
static void h_cv_blit(int32_t x, int32_t y)
{
    if (y < HB_TOP) { fprintf(stderr, "blit above HB_TOP\n"); exit(2); }
    if (x >= 0 && y >= 0 && x + (int32_t)cv_w <= 240 && y + (int32_t)cv_h <= 240) {
        cv_blit((uint32_t)x, (uint32_t)y);
        blits += cv_w * cv_h;
    } else {
        fprintf(stderr, "blit off screen %d %d %u %u\n", x, y, cv_w, cv_h);
        exit(2);
    }
}
static void h_led(uint32_t id, uint32_t on) { leds = on ? leds | 1u << id : leds & ~(1u << id); }
static uint32_t h_rand(void) { return (uint32_t)rand(); }
static uint8_t store[240], bank[HB_BANK_MAX];
static uint32_t store_n, bank_n;
static int h_load(void *p, uint32_t n) { if (store_n != n) return -1; memcpy(p, store, n); return 0; }
static void h_save(const void *p, uint32_t n) { if (n > 240u) { fprintf(stderr, "settings too big\n"); exit(2); } memcpy(store, p, n); store_n = n; printf("%6u ms  settings saved (%u B)\n", now_ms, n); }
static void h_midi_send(uint32_t st, uint32_t d1, uint32_t d2) { printf("%6u ms  MIDI out %02X %u %u\n", now_ms, st, d1, d2); }
static uint32_t h_master(void) { return 4096; }
static void h_blit(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *px) { lcd_blit((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, px); }
static int32_t h_text_w(uint32_t big, const char *s) { return text_w(big ? &FONT_L : &FONT_S, s); }
static int h_bank_load(void *p, uint32_t n) { if (!bank_n || bank_n != n) return -1; memcpy(p, bank, n); return 0; }
static int h_bank_save(const void *p, uint32_t n)
{
    if (n > HB_BANK_MAX) { fprintf(stderr, "bank too big (%u)\n", n); return -1; }
    memcpy(bank, p, n);
    bank_n = n;
    printf("%6u ms  bank saved (%u B)\n", now_ms, n);
    return 0;
}
static hb_host_t host = {
    .abi = HB_ABI, .size = sizeof(hb_host_t), .ms = h_ms, .ticks = h_ticks, .log = h_log, .log_u32 = h_log_u32,
    .fill = h_fill, .text = h_text, .cv_begin = h_cv_begin, .cv_rect = h_cv_rect, .cv_line = h_cv_line,
    .cv_pixel = h_cv_pixel, .cv_text = h_cv_text, .cv_blit = h_cv_blit, .led = h_led, .rand = h_rand, .load = h_load,
    .save = h_save, .midi_send = h_midi_send, .master = h_master, .blit = h_blit, .text_w = h_text_w,
    .bank_load = h_bank_load, .bank_save = h_bank_save,
};

typedef struct { uint32_t t; char what[8]; int a, b, c; char name[64]; } hev_t;
static hev_t ev[20000];
static uint32_t nev, end_ms = 3000;

static void shot(const char *out, const char *name)
{
    char path[600];
    FILE *f;
    uint32_t i;
    snprintf(path, sizeof path, "%s-%s.ppm", out, name);
    if (!(f = fopen(path, "wb"))) { perror(path); exit(1); }
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = fb[i];
        fputc((c >> 11) * 255 / 31, f);
        fputc((c >> 5 & 63) * 255 / 63, f);
        fputc((c & 31) * 255 / 31, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "out";
    char line[256], path[600];
    FILE *f;
    int16_t *wav = 0;
    uint32_t wav_n = 0, step = 0, owed = 0, i, clipped = 0, worst_blit = 0;
    static int32_t blk[512];
    int32_t peak = 0;
    if (getenv("APLAT_LOAD")) load_pct = (uint32_t)atoi(getenv("APLAT_LOAD"));
    if (argc < 3 || !(f = fopen(argv[2], "r"))) { fprintf(stderr, "usage: aplat_host OUT script.txt\n"); return 1; }
    while (fgets(line, sizeof line, f) && nev < 20000u) {
        hev_t e;
        memset(&e, 0, sizeof e);
        if (line[0] == '#' || sscanf(line, "%u %7s", &e.t, e.what) != 2) continue;
        if (!strcmp(e.what, "end")) { end_ms = e.t; continue; }
        if (!strcmp(e.what, "shot")) sscanf(line, "%*u %*s %63s", e.name);
        else sscanf(line, "%*u %*s %i %i %i", &e.a, &e.b, &e.c);
        ev[nev++] = e;
    }
    fclose(f);
    /* the top line is the host's: a grey band, so the pictures show where the module's part starts */
    lcd_fill(0, 0, 240, 24, 0x0000);
    draw_text_box(4, 4, 120, &FONT_S, "APLAT", 0xFFFF, 0);
    if (hb_module.abi != HB_ABI || hb_module.init(&host)) { fprintf(stderr, "init failed\n"); return 1; }
    hb_module.draw(1);
    for (now_ms = 1; now_ms <= end_ms; now_ms++) {
        uint32_t b0;
        while (step < nev && ev[step].t <= now_ms) {
            hev_t *e = &ev[step++];
            if (!strcmp(e->what, "on")) hb_module.note(0, (uint32_t)e->a, (uint32_t)e->b);
            else if (!strcmp(e->what, "off")) hb_module.note(0, (uint32_t)e->a, 0);
            else if (!strcmp(e->what, "btn")) { hb_module.ui(HB_EV_BTN_DOWN | (uint32_t)e->a, 0); hb_module.ui(HB_EV_BTN_UP | (uint32_t)e->a, 0); }
            else if (!strcmp(e->what, "enc")) hb_module.ui(HB_EV_ENC | (uint32_t)e->a, e->b);
            else if (!strcmp(e->what, "midi")) hb_module.midi((uint32_t)e->a, (uint32_t)e->b, (uint32_t)e->c);
            else if (!strcmp(e->what, "shot")) { for (i = 0; i < 40u; i++) hb_module.draw(0); shot(out, e->name); }
        }
        owed += 44118u;
        while (owed >= 256000u) {
            owed -= 256000u;
            memset(blk, 0, sizeof blk);
            hb_module.render(blk, 256);
            wav = realloc(wav, (wav_n + 256u) * 4u);
            for (i = 0; i < 512u; i++) {
                int32_t v = blk[i];
                if (v > peak) peak = v;
                if (-v > peak) peak = -v;
                if (v > 32767 || v < -32767) clipped++;
                wav[2u * wav_n + i] = (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v);
            }
            wav_n += 256u;
        }
        b0 = blits;
        hb_module.draw(0);
        if (blits - b0 > worst_blit) worst_blit = blits - b0;
    }
    snprintf(path, sizeof path, "%s.wav", out);
    if (!(f = fopen(path, "wb"))) { perror(path); return 1; }
    {
        uint32_t bytes = wav_n * 4u, v[] = {16, 0x00020001u, 44118u, 44118u * 4u, 0x00100004u}, riff = bytes + 36u;
        fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f);
        fwrite("WAVEfmt ", 1, 8, f); fwrite(v, 4, 5, f);
        fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
        fwrite(wav, 4, wav_n, f);
    }
    fclose(f);
    free(wav);
    printf("notes allowed at the end: %u\n", (unsigned)eng_max_notes());
    printf("sound -> %s  %u frames, peak %d, over full scale %u, most pixels sent in one draw() %u\n", path, wav_n, peak,
           clipped, worst_blit);
    return 0;
}
