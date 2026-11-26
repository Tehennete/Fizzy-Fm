/* SPDX-License-Identifier: GPL-3.0-only */
/* APLAT: a wavetable synth for the M-VAVE FM-1, a module of fm1-dock (firmware/dock/hb_module.h).
 * Copyright (C) 2026 Hugo
 *
 * Eight notes; each has two wavetable oscillators with unison and warps, a sub, noise, its own filter, three
 * envelopes, two LFOs and an eight-slot modulation matrix: every note is shaped on its own, by its velocity and
 * by the pressure of its own key. Drive, chorus, a tempo echo and a reverb after them; an arpeggiator; 23 factory
 * sounds and 32 of your own.
 *
 *   SELECT      the pages, one after the other        PRESETS   the sounds
 *   ALGORITHM   the wavetable of oscillator A (B on its pages)
 *   KNOB 1..4   the four tiles of the page
 *   PLAY        the PLAY page: four macros            EDIT      OSC A > OSC B > MIX
 *   SEL         FILTER      ENV   ENV 1 > 2 > 3       LFO       LFO 1 > 2 > 3
 *   SEQ         MATRIX      ARP   the arpeggiator     FX        DRIVE CHORUS > DELAY > REVERB
 *   GLO         VOICE       REC   compare with the stored sound
 *   SAVE        pick a user sound with PRESETS, SAVE again (any other button: no) */
#include "hb_module.h"
#include "ap_gen.h"

static const hb_host_t *H;

#include "ap_def.h"
#include "ap_eng.h"
#include "ap_seq.h"
#include "ap_fx.h"
#include "ap_ui.h"

#define NAME "APLAT"

/* ---- the sounds */
static patch_t user[NUSER];
static patch_t stored;                          /* the sound as picked, for REC (compare) */
static patch_t edit;                            /* ... and the edited one meanwhile */
typedef struct {
    uint32_t magic;
    uint32_t preset;
} settings_t;
#define SET_MAGIC 0x314C5041u                   /* "APL1" */
static uint32_t set_saved, set_t;

static void patch_check(patch_t *q)
{
    uint32_t i;
    for (i = 0; i < NP; i++)
        if (q->p[i] > PDEF[i].max)
            q->p[i] = PDEF[i].def;
    q->name[NAME_LEN - 1u] = 0;
}

static void preset_load(uint32_t idx)
{
    if (idx >= NFACTORY + NUSER)
        idx = 0;
    ev_put(EV_HUSH, 0, 0);
    if (idx < NFACTORY)
        patch_init(&cur, &FACTORY[idx]);
    else
        cur = user[idx - NFACTORY];
    patch_check(&cur);
    stored = cur;
    ui_s.preset = (uint16_t)idx;
    ui_s.edited = ui_s.compare = ui_s.save = 0;
    ui_s.slot = 0;
    ui_s.msg = 0;
    set_t = H->ms();
    ui_dirty_all();
}

/* ---- the wavetables: the table an oscillator asks for is built into the buffer nobody plays, a frame a pass of
 * the main loop, then the oscillator moves to it */
static uint8_t wt_tab[3] = {255, 255, 255};
static struct { uint8_t on, osc, tab, buf, frame; } wj;
static uint32_t wt_service(void)
{
    uint32_t o;
    if (!wj.on)
        for (o = 0; o < 2u; o++) {
            uint32_t want = cur.p[o ? P_B_WAVE : P_A_WAVE], b;
            if (wt_tab[wt_cur[o]] == want)
                continue;
            if (wt_tab[wt_cur[1u - o]] == want) {       /* the other oscillator has it: both play that buffer */
                wt_cur[o] = wt_cur[1u - o];
                ui_s.d_disp = 1;
                continue;
            }
            for (b = 0; b < 3u; b++)
                if (b != wt_cur[0] && b != wt_cur[1])
                    break;
            wj.on = 1;
            wj.osc = (uint8_t)o;
            wj.tab = (uint8_t)want;
            wj.buf = (uint8_t)b;
            wj.frame = 0;
            wt_tab[b] = 255;
            break;
        }
    if (!wj.on)
        return 0;
    wt_frame(wt_buf[wj.buf] + wj.frame * WT_FRM, wj.tab, wj.frame);
    if (++wj.frame >= AP_NF) {
        wt_tab[wj.buf] = wj.tab;
        wt_cur[wj.osc] = wj.buf;
        wj.on = 0;
        ui_s.d_disp = 1;
    }
    return 1;
}

/* ---- sound */
/* The time a block took, against the time it has (544 ticks of 24 MHz a frame). Over three quarters twice running:
 * the oldest note goes and one note fewer may sound; under half for two seconds: one more again. fm1-dock stops a
 * module that is late 64 blocks in a row: this keeps APLAT well away from that, whatever the sound asks for. */
static void load_guard(uint32_t dt, uint32_t n, uint32_t act)
{
    static uint32_t over, calm;
    uint32_t has = n * 544u, i, cnt = 0, old = 0, oa = 0xFFFFFFFFu;
    if (dt > has - (has >> 2)) {
        calm = 0;
        if (++over < 2u)
            return;
        over = 0;
        for (i = 0; i < NV; i++)
            if (act >> i & 1u) {
                cnt++;
                if (voice[i].age < oa) {
                    oa = voice[i].age;
                    old = i;
                }
            }
        if (cnt > 1u) {
            voice[old].on = 2;
            voice[old].held = 0;
            voice[old].kill = 1;
            guard_max = cnt - 1u;
        }
    } else {
        over = 0;
        if (dt < has >> 1 && guard_max < NV && ++calm > 350u) {
            calm = 0;
            guard_max++;
        }
    }
}

static void render(int32_t *out, uint32_t n)
{
    static int32_t bl[SUBN], br[SUBN];
    uint32_t t0 = H->ticks(), done = 0, act = 0, dt;
    seq_begin(n);
    while (done < n) {
        uint32_t m = n - done > SUBN ? SUBN : n - done, i, v;
        seq_run(m);
        eng_tick_global();
        for (i = 0; i < m; i++)
            bl[i] = br[i] = 0;
        act = 0;
        for (v = 0; v < NV; v++)
            if (voice[v].on)
                act |= voice_run(&voice[v], bl, br, m) << v;
        if (!act)
            lfo3_fade = 0;                      /* silence: LFO 3's fade starts again with the next note */
        fx_run(bl, br, m);
        master_run(bl, br, out + 2u * done, m);
        done += m;
    }
    eng_active = act;
    dt = H->ticks() - t0;
    eng_load = dt > eng_load ? dt : eng_load - (eng_load >> 5);
    load_guard(dt, n, act);
}

/* ---- MIDI */
static void param_set(uint32_t id, uint32_t v127)
{
    cur.p[id] = (uint8_t)((v127 * PDEF[id].max + 63u) / 127u);
    ui_ext_dirty = 1;
}

static void handle_cc(uint32_t num, uint32_t val)
{
    switch (num) {
    case 1: in_wheel = (int32_t)val; break;
    case 5: param_set(P_GLIDE, val); break;
    case 16: case 17: case 18: case 19: param_set(P_M1 + num - 16u, val); break;
    case 64: ev_put(EV_PEDAL, val >= 64u, 0); break;
    case 71: param_set(P_F_RES, val); break;
    case 72: param_set(P_E1_R, val); break;
    case 73: param_set(P_E1_A, val); break;
    case 74: param_set(P_F_CUT, val); break;
    case 75: param_set(P_E1_D, val); break;
    case 91: param_set(P_REV_MIX, val); break;
    case 93: param_set(P_CHO_MIX, val); break;
    case 94: param_set(P_DLY_MIX, val); break;
    case 120: case 123: ev_put(EV_PANIC, 0, 0); break;
    default: break;
    }
}

static void note(uint32_t ch, uint32_t key, uint32_t vel)
{
    (void)ch;
    if (key < 128u)
        ev_put(vel ? EV_ON : EV_OFF, key, vel > 127u ? 127u : vel);
}

static void cc(uint32_t ch, uint32_t num, uint32_t val)
{
    (void)ch;
    handle_cc(num, val & 127u);
}

static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (st >= 0xF8u) {                          /* the clock: in the audio interrupt, just before render() */
        if (st == 0xF8u) sq.ext_pulses++;
        else if (st == 0xFAu) sq.ext_start = 1;
        else if (st == 0xFCu) sq.ext_stop = 1;
        return;
    }
    switch (st & 0xF0u) {
    case 0x90: note(0, d1, d2); break;
    case 0x80: note(0, d1, 0); break;
    case 0xA0: ev_put(EV_PRESS, d1 & 127u, d2 & 127u); break;
    case 0xB0: handle_cc(d1, d2 & 127u); break;
    case 0xC0: ui_pc_req = (int32_t)d1; break;
    case 0xD0: in_chanat = (int32_t)(d1 & 127u); break;
    case 0xE0: in_bend = (int32_t)((d2 & 127u) << 7 | (d1 & 127u)) - 8192; break;
    default: break;
    }
}

/* ---- the panel */
static void say(const char *s)
{
    ui_s.msg = s;
    ui_s.msg_until = H->ms() + 1500u;
    ui_s.d_head = 1;
}

static void knob(uint32_t k, int32_t clicks)
{
    const page_t *pg = &PAGES[ui_s.page];
    uint32_t id;
    int32_t v, nv;
    if (pg->par[k] == NOP)
        return;
    if (ui_s.touched != k) {
        if (ui_s.touched < 4u)
            ui_s.d_tile |= (uint8_t)(1u << ui_s.touched);
        ui_s.touched = (uint8_t)k;
        ui_s.d_tile |= (uint8_t)(1u << k);
    }
    if (pg->par[k] == PG_SLOT) {
        nv = clampi((int32_t)ui_s.slot + clicks, 0, 7);
        if (nv != ui_s.slot) {
            ui_s.slot = (uint8_t)nv;
            ui_s.d_tile = 15;
            ui_s.d_disp = 1;
        }
        return;
    }
    id = page_param(pg, k);
    if (PDEF[id].max >= 100u && (clicks > 1 || clicks < -1))
        clicks *= clicks > 3 || clicks < -3 ? 4 : 2;        /* a fast turn goes further */
    v = cur.p[id];
    nv = clampi(v + clicks, 0, PDEF[id].max);
    if (nv == v)
        return;
    if (ui_s.compare) {                         /* editing the stored sound: it is the sound now */
        ui_s.compare = 0;
        ui_leds();
    }
    cur.p[id] = (uint8_t)nv;
    if (!ui_s.edited)
        ui_s.d_head = 1;
    ui_s.edited = 1;
    ui_s.d_tile |= (uint8_t)(1u << k);
    ui_s.d_disp = 1;
    if (pg->disp == DS_MOD)
        ui_s.d_tile |= 8u;
}

static void save_now(void)
{
    uint32_t i;
    for (i = 0; i < NP; i++)
        user[ui_s.save_to].p[i] = cur.p[i];
    for (i = 0; i < NAME_LEN; i++)
        user[ui_s.save_to].name[i] = cur.name[i];
    ui_s.save = 0;
    ui_s.preset = (uint16_t)(NFACTORY + ui_s.save_to);
    stored = cur;
    ui_s.edited = ui_s.compare = 0;
    set_t = H->ms();
    say(H->bank_save(user, sizeof user) ? "NOT SAVED" : "SAVED");
    ui_dirty_all();
    ui_leds();
}

static void ui(uint32_t ev, int32_t arg)
{
    uint32_t id = HB_EV_ID(ev), i;
    if (HB_EV_KIND(ev) == HB_EV_BTN_DOWN) {
        if (ui_s.save) {
            if (id == HB_BTN_SAVE) {
                save_now();
                return;
            }
            ui_s.save = 0;                      /* any other button: no */
            ui_s.d_head = 1;
            ui_s.d_disp = 1;
            ui_leds();
        }
        if (id == HB_BTN_SAVE) {
            ui_s.save = 1;
            ui_s.save_to = (uint8_t)(ui_s.preset >= NFACTORY ? ui_s.preset - NFACTORY : 0u);
            ui_s.d_head = 1;
            ui_leds();
        } else if (id == HB_BTN_REC) {          /* the stored sound and the edited one, by turns */
            if (!ui_s.compare && ui_s.edited) {
                edit = cur;
                cur = stored;
                ui_s.compare = 1;
            } else if (ui_s.compare) {
                cur = edit;
                ui_s.compare = 0;
            }
            ui_dirty_all();
            ui_leds();
        } else {
            for (i = 1; i <= NPAGES; i++) {     /* the next page this button stops at */
                uint32_t p = (ui_s.page + i) % NPAGES;
                if (PAGES[p].btn == id && PAGES[p].head) {
                    ui_go(p);
                    break;
                }
            }
        }
        return;
    }
    if (HB_EV_KIND(ev) != HB_EV_ENC || !arg)
        return;
    switch (id) {
    case HB_ENC_SELECT:
        ui_go((uint32_t)(((int32_t)ui_s.page + arg) % (int32_t)NPAGES + (int32_t)NPAGES) % NPAGES);
        break;
    case HB_ENC_ALGORITHM: {
        uint32_t p = PAGES[ui_s.page].col == K_OSCB ? P_B_WAVE : P_A_WAVE;
        int32_t nv = clampi((int32_t)cur.p[p] + arg, 0, (int32_t)AP_NTAB - 1);
        if (nv != cur.p[p]) {
            cur.p[p] = (uint8_t)nv;
            ui_s.edited = 1;
            ui_dirty_all();
            say(AP_WT_NAME[nv]);
        }
        break;
    }
    case HB_ENC_PRESETS:
        if (ui_s.save) {
            ui_s.save_to = (uint8_t)clampi((int32_t)ui_s.save_to + arg, 0, (int32_t)NUSER - 1);
            ui_s.d_head = 1;
        } else {
            int32_t n = (int32_t)(NFACTORY + NUSER);
            preset_load((uint32_t)((((int32_t)ui_s.preset + arg) % n + n) % n));
        }
        break;
    case HB_ENC_KNOB1: case HB_ENC_KNOB2: case HB_ENC_KNOB3: case HB_ENC_KNOB4:
        knob(id - HB_ENC_KNOB1, arg);
        break;
    default: break;
    }
}

static void draw(uint32_t full)
{
    uint32_t now = H->ms(), k;
    if (full) {
        ui_dirty_all();
        ui_leds();
    }
    if (ui_pc_req >= 0) {
        uint32_t pc = (uint32_t)ui_pc_req;
        ui_pc_req = -1;
        preset_load(pc);
    }
    if (ui_ext_dirty) {
        ui_ext_dirty = 0;
        ui_s.edited = 1;
        ui_dirty_all();
    }
    if (ui_s.msg && (int32_t)(now - ui_s.msg_until) >= 0) {
        ui_s.msg = 0;
        ui_s.d_head = 1;
    }
    if (set_t && now - set_t > 3000u && !eng_active) {   /* the sound picked, kept for the next power-on */
        static settings_t st;
        st.magic = SET_MAGIC;
        st.preset = ui_s.preset;
        set_t = 0;
        if (st.preset != set_saved) {
            set_saved = st.preset;
            H->save(&st, sizeof st);
        }
    }
    if (wt_service())
        return;                                 /* a frame of a table took this pass */
    if (ui_s.d_tile) {
        for (k = 0; k < 4u; k++)
            if (ui_s.d_tile >> k & 1u) {
                ui_s.d_tile &= (uint8_t)~(1u << k);
                draw_tile(k);
                return;
            }
    }
    if (ui_s.d_head) {
        ui_s.d_head = 0;
        draw_head();
        return;
    }
    if (ui_s.d_disp) {
        ui_s.d_disp = 0;
        draw_disp();
        return;
    }
    if (now - ui_s.t_meter >= 80u) {
        static uint8_t slow;
        ui_s.t_meter = now;
        draw_meter(0);
        if (PAGES[ui_s.page].disp == DS_VOICE && ++slow >= 8u) {    /* VOICE: its LOAD line, now and then */
            slow = 0;
            ui_s.d_disp = 1;
        }
    }
}

static int init(const hb_host_t *h)
{
    static settings_t st;
    uint32_t i;
    H = h;
    for (i = 0; i < 1025u; i++)
        sinr[i] = AP_SINE[i];
    if (H->bank_load(user, sizeof user)) {      /* no sounds of your own yet: 32 copies of INIT */
        for (i = 0; i < NUSER; i++) {
            patch_init(&user[i], &FACTORY[0]);
            put_s(user[i].name, "USER");
        }
    }
    for (i = 0; i < NUSER; i++)
        patch_check(&user[i]);
    st.preset = 1;
    if (H->load(&st, sizeof st) || st.magic != SET_MAGIC)
        st.preset = 1;
    set_saved = st.preset;
    preset_load(st.preset);
    set_t = 0;
    ev_r = ev_w;                                /* (nothing sounds yet: no need to hush) */
    ui_s.touched = 255;                         /* (the tables are built by draw(), a frame a pass: 16 passes) */
    H->log("aplat: ready");
    return 0;
}

static void release(void)
{
    uint32_t i;
    for (i = 0; i < NV; i++)
        voice[i].on = 0;
    sq.nheld = sq.nplaying = 0;
}

HB_DESC = {HB_ABI, NAME, init, release, note, cc, render, ui, draw, 0, 0, HB_F_MIDI_RT, midi};
