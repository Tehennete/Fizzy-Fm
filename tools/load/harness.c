/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Hugo Gilbert */
/* APLAT's module on a bare 32-bit RISC-V (rv32im), for tools/load/count.py: it counts the instructions a block of
 * sound takes. Not the FM-1's chip, but a 32-bit processor with a multiplier and no FPU, as it is. */
#include <stdint.h>
#define HB_DESC const hb_module_t hb_module
#include "../../aplat/aplat.c"

static uint32_t t_ms;
static uint32_t s_ms(void) { return t_ms; }
static uint32_t s_ticks(void) { return 0; }
static void s_log(const char *s) { (void)s; }
static void s_log_u32(const char *s, uint32_t v) { (void)s; (void)v; }
static void s_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
static void s_text(int32_t x, int32_t y, int32_t w, uint32_t b, const char *s, uint16_t f, uint32_t c) { (void)x; (void)y; (void)w; (void)b; (void)s; (void)f; (void)c; }
static void s_cvb(int32_t w, int32_t h, uint16_t bg) { (void)w; (void)h; (void)bg; }
static void s_cvl(int32_t a, int32_t b, int32_t c, int32_t d, uint16_t e) { (void)a; (void)b; (void)c; (void)d; (void)e; }
static void s_cvp(int32_t x, int32_t y, uint16_t c) { (void)x; (void)y; (void)c; }
static void s_cvt(int32_t x, int32_t y, uint32_t b, const char *s, uint16_t f) { (void)x; (void)y; (void)b; (void)s; (void)f; }
static void s_cvx(int32_t x, int32_t y) { (void)x; (void)y; }
static void s_led(uint32_t i, uint32_t o) { (void)i; (void)o; }
static uint32_t s_rand(void) { return 4; }
static int s_load(void *p, uint32_t n) { (void)p; (void)n; return -1; }
static void s_save(const void *p, uint32_t n) { (void)p; (void)n; }
static void s_msend(uint32_t a, uint32_t b, uint32_t c) { (void)a; (void)b; (void)c; }
static uint32_t s_master(void) { return 4096; }
static void s_blit(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *p) { (void)x; (void)y; (void)w; (void)h; (void)p; }
static int32_t s_tw(uint32_t b, const char *s) { (void)b; (void)s; return 0; }
static int s_bload(void *p, uint32_t n) { (void)p; (void)n; return -1; }
static int s_bsave(const void *p, uint32_t n) { (void)p; (void)n; return 0; }
static const hb_host_t host = {HB_ABI, (uint32_t)__builtin_offsetof(hb_host_t, sense_keys), s_ms, s_ticks, s_log, s_log_u32, s_fill, s_text, s_cvb, s_fill,
                               s_cvl, s_cvp, s_cvt, s_cvx, s_led, s_rand, s_load, s_save, s_msend, s_master, s_blit, s_tw,
                               s_bload, s_bsave};
static int32_t blk[512];
int32_t peak;

void x_init(void) { hb_module.init(&host); while (wt_service()) ; }
void x_preset(uint32_t i) { preset_load(i); while (wt_service()) ; }
void x_set(uint32_t id, uint32_t v) { cur.p[id] = (uint8_t)v; while (wt_service()) ; }
void x_note(uint32_t key, uint32_t vel) { hb_module.note(0, key, vel); }
void x_render(void)
{
    uint32_t i;
    for (i = 0; i < 512u; i++) blk[i] = 0;
    hb_module.render(blk, 256);
    for (i = 0; i < 512u; i++) { int32_t a = blk[i] < 0 ? -blk[i] : blk[i]; if (a > peak) peak = a; }
    t_ms += 6;
}
void x_table(void) { wt_frame(wt_buf[2], 4, 3); }       /* one frame of a table: what a pass of the main loop takes */
void x_draw(void) { ui_dirty_all(); while (ui_s.d_tile || ui_s.d_head || ui_s.d_disp) hb_module.draw(0); }
uint32_t x_np(void) { return NP; }
