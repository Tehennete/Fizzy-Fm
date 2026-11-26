/* SPDX-License-Identifier: GPL-3.0-only */
/* Fizzy-Fm: a wavetable synth for the M-VAVE FM-1, a module of fm1-dock (firmware/dock/hb_module.h).
 * Copyright (C) 2026 Hugo Gilbert
 *
 * Eight notes; each has two wavetable oscillators with unison and warps (64 tables, 3 of your own), a sub, noise,
 * its own filter (20 types), three envelopes, two LFOs and an eight-slot modulation matrix: every note is shaped on
 * its own, by its velocity and by the pressure of its own key. Seven effect slots after them (14 effects); an
 * arpeggiator; factory sounds and 32 of your own. Every parameter is also an NRPN, in and out (the editor).
 *
 *   SELECT      the pages, one after the other        PRESETS   the sounds
 *   ALGORITHM   the wavetable of oscillator A (B on its pages)
 *   KNOB 1..4   the four tiles of the page
 *   PLAY        the PLAY page: four macros            EDIT      OSC A > OSC B > MIX
 *   SEL         FILTER      ENV   ENV 1 > 2 > 3       LFO       LFO 1 > 2 > 3
 *   SEQ         MATRIX      ARP   the arpeggiator     FX        the seven effect slots (SELECT goes on)
 *   GLO         VOICE       REC   compare with the stored sound
 *   SAVE        pick a user sound with PRESETS, SAVE again (any other button: no) */
#include "hb_module.h"
#include "ap_gen.h"

static const hb_host_t *H;

#include "ap_def.h"
#include "ap_rand.h"
#include "ap_eng.h"
#include "ap_seq.h"
#include "ap_fx.h"
#include "ap_ui.h"

#define NAME "FIZZY-FM"

/* ---- the link to an editor: every parameter is an NRPN (its number the parameter's, 0..131; its value 14 bits,
 * controllers 99 / 98 then 6 / 38), in and out, over whatever MIDI the host has: USB, the DIN jack, Bluetooth.
 * Nothing goes out until an editor has asked once (NR_DUMP): a FM-1 on stage stays quiet.
 *   in    0..NP-1 a parameter          1000..1011 the name's letters      2000 send me everything
 *         2001 store as user sound N   2002 pick sound N
 *         2003 a table of your own begins (N = which), 3000..3003 its 1024 numbers in order (16 bits each: the
 *         top two in the NRPN's number), 2005 it ends (the low 14 bits of their sum)
 *   out   the same numbers for what changes on the FM-1; after "everything": 2010 the sound's number, 2011 the low
 *         14 bits of the sum of the parameters; 2012 a table arrived whole (1) or not (0); 2013 this list's version */
enum { NR_NAME = 1000, NR_DUMP = 2000, NR_STORE, NR_LOAD, NR_WT_BEGIN, NR_WT_RES, NR_WT_END, NR_PRESET = 2010, NR_SUM,
       NR_WT_OK, NR_VERSION, NR_WT_DATA = 3000 };
#define LINK_VERSION 1u
#define TXQ 256u
static uint32_t txq[TXQ], tx_w, tx_r;
static uint8_t tx_linked, nr_pm, nr_pl, nr_dm, wt_slot;
static int32_t tx_dump = -1;                    /* where "everything" has got to, -1: not sending it */
static uint32_t wt_pos, wt_sum;
static uint8_t wt_dirty;                        /* a table of your own to write to flash when nothing sounds */

static void tx_put(uint32_t param, uint32_t value)
{
    if (tx_w - tx_r < TXQ)
        txq[tx_w++ % TXQ] = param << 16 | (value & 0x3FFFu);
}
static void tx_param(uint32_t id)
{
    if (tx_linked)
        tx_put(id, cur.p[id]);
}

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
    ui_s.rnd = 0;
    ui_s.slot = 0;
    ui_s.msg = 0;
    set_t = H->ms();
    ui_dirty_all();
    if (tx_linked)
        tx_dump = 0;                            /* the editor follows */
}

/* ---- the wavetables: the table an oscillator asks for is built into the buffer nobody plays, a frame a pass of
 * the main loop, then the oscillator moves to it */
static uint8_t wt_tab[3] = {255, 255, 255};     /* (said above too: the link marks a table to build again) */
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
 * module that is late 64 blocks in a row: this keeps Fizzy-Fm well away from that, whatever the sound asks for. */
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

/* ---- the link to an editor, its two ends (the list of numbers is at the top of this file) */
static void tx_service(void)                    /* from draw(): a message a millisecond, no more */
{
    static uint32_t last;
    uint32_t now = H->ms(), n;
    if (now == last)
        return;
    last = now;
    for (n = 0; tx_dump >= 0 && tx_w - tx_r < TXQ - 8u && n < 4u; n++) {
        uint32_t i = (uint32_t)tx_dump++, k, sum = 0;
        if (i < NP) {
            tx_put(i, cur.p[i]);
        } else if (i < NP + NAME_LEN) {
            tx_put(NR_NAME + i - NP, (uint8_t)cur.name[i - NP]);
        } else {
            for (k = 0; k < NP; k++)
                sum += cur.p[k];
            tx_put(NR_VERSION, LINK_VERSION);
            tx_put(NR_PRESET, ui_s.preset);
            tx_put(NR_SUM, sum);
            tx_dump = -1;
        }
    }
    if (tx_r != tx_w) {
        uint32_t q = txq[tx_r++ % TXQ], p = q >> 16, v = q & 0x3FFFu;
        H->midi_send(0xB0, 99, p >> 7);
        H->midi_send(0xB0, 98, p & 127u);
        H->midi_send(0xB0, 6, v >> 7);
        H->midi_send(0xB0, 38, v & 127u);
    }
}

static void save_now(void);

static void nrpn_in(uint32_t p, uint32_t v)
{
    if (p < NP) {
        cur.p[p] = (uint8_t)(v > PDEF[p].max ? PDEF[p].max : v);
        ui_ext_dirty = 1;
    } else if (p >= NR_NAME && p < NR_NAME + NAME_LEN - 1u) {
        cur.name[p - NR_NAME] = (char)(v >= 32u && v < 127u ? v : 0u);
        ui_ext_dirty = 1;
    } else if (p == NR_DUMP) {
        tx_linked = 1;
        tx_dump = 0;
    } else if (p == NR_STORE && v < NUSER) {
        ui_s.save_to = (uint8_t)v;
        save_now();
    } else if (p == NR_LOAD) {
        ui_pc_req = (int32_t)v;
    } else if (p == NR_WT_BEGIN && v < NUWT) {
        wt_slot = (uint8_t)v;
        wt_pos = wt_sum = 0;
    } else if (p >= NR_WT_DATA && p < NR_WT_DATA + 4u) {
        uint32_t u = (p - NR_WT_DATA) << 14 | v;
        if (wt_pos < AP_NF * AP_NH * 2u) {
            uint32_t lo = u & 0xFFFFu, t = (lo + 64u) & 0xFFFFu;
            if (t < 16u)                        /* -64..-49 (as the built-in tables: tools/gen_tables.py says why) */
                lo = t > 7u ? lo + 16u - t : lo - 1u - t;
            uwt[wt_slot][wt_pos++] = (int16_t)lo;
        }
        wt_sum += u;
    } else if (p == NR_WT_END) {
        uint32_t ok = wt_pos == AP_NF * AP_NH * 2u && (wt_sum & 0x3FFFu) == v, b;
        if (ok) {
            for (b = 0; b < 3u; b++)            /* whoever plays it builds it again */
                if (wt_tab[b] == AP_NTAB + wt_slot)
                    wt_tab[b] = 255;
            wt_dirty = 1;
        }
        tx_put(NR_WT_OK, ok);
    }
}

/* ---- MIDI */
static void param_set(uint32_t id, uint32_t v127)
{
    cur.p[id] = (uint8_t)((v127 * PDEF[id].max + 63u) / 127u);
    ui_ext_dirty = 1;
}

static void fx_mix(uint32_t type, uint32_t val)
{
    uint32_t i;
    for (i = 0; i < NFX; i++)
        if (cur.p[P_FX1_TYPE + 4u * i] == type) {
            param_set(P_FX1_C + 4u * i, val);
            return;
        }
}

static void handle_cc(uint32_t num, uint32_t val)
{
    switch (num) {
    case 99: nr_pm = (uint8_t)val; break;       /* an NRPN: its number, then its value's two halves */
    case 98: nr_pl = (uint8_t)val; break;
    case 6: nr_dm = (uint8_t)val; break;
    case 38: nrpn_in((uint32_t)nr_pm << 7 | nr_pl, (uint32_t)nr_dm << 7 | val); break;
    case 1: in_wheel = (int32_t)val; break;
    case 5: param_set(P_GLIDE, val); break;
    case 16: case 17: case 18: case 19: param_set(P_M1 + num - 16u, val); break;
    case 64: ev_put(EV_PEDAL, val >= 64u, 0); break;
    case 71: param_set(P_F_RES, val); break;
    case 72: param_set(P_E1_R, val); break;
    case 73: param_set(P_E1_A, val); break;
    case 74: param_set(P_F_CUT, val); break;
    case 75: param_set(P_E1_D, val); break;
    case 91: fx_mix(FX_REVERB, val); break;     /* the reverb's, the chorus's, the echo's MIX, in whatever slot */
    case 93: fx_mix(FX_CHORUS, val); break;
    case 94: fx_mix(FX_DELAY, val); break;
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
static char wave_msg[12];
static void say(const char *s)
{
    ui_s.msg = s;
    ui_s.msg_until = H->ms() + 1500u;
    ui_s.d_head = 1;
}

/* ---- the dice on the panel (the RANDOM page): ROLL throws a new sound, MUTATE moves the one there is, and a
 * turn to the left of either goes back: the last eight sounds are kept, the one before the dice among them */
static patch_t rq;
static uint32_t same_patch(const patch_t *a, const patch_t *b)
{
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    uint32_t i;
    for (i = 0; i < sizeof(patch_t); i++)
        if (x[i] != y[i])
            return 0;
    return 1;
}
static void rand_keep(const patch_t *q, uint32_t how)
{
    uint32_t i;
    if (rd.n && rd.pos + 1u < rd.n)
        rd.n = (uint8_t)(rd.pos + 1u);          /* thrown from further back: what was ahead is gone */
    if (rd.n == RH) {
        for (i = 1; i < RH; i++) {
            rh[i - 1u] = rh[i];
            rd.how[i - 1u] = rd.how[i];
        }
        rd.n--;
    }
    rh[rd.n] = *q;
    rd.how[rd.n] = (uint8_t)how;
    rd.pos = rd.n++;
}
static void rand_show(uint32_t hush)
{
    if (hush)
        ev_put(EV_HUSH, 0, 0);
    cur = rh[rd.pos];
    patch_check(&cur);
    ui_s.rnd = rd.how[rd.pos] != 0;
    ui_s.edited = 1;
    ui_s.compare = 0;
    ui_s.slot = 0;
    ui_dirty_all();
    ui_leds();
    if (tx_linked)
        tx_dump = 0;                            /* the editor follows */
}
static void rand_turn(uint32_t mutate, int32_t clicks)
{
    uint32_t seed;
    if (ui_s.compare) {                         /* (REC was showing the stored sound) */
        cur = edit;
        ui_s.compare = 0;
    }
    if (!rd.n || !same_patch(&rh[rd.pos], &cur))
        rand_keep(&cur, 0);                     /* the sound as it is now: a turn back finds it */
    if (clicks < 0) {
        if (!rd.pos) {
            say("THE FIRST");
            return;
        }
        rd.pos--;
        rand_show(1);
        return;
    }
    if (!mutate && rd.pos + 1u < rd.n) {        /* forward again through what was thrown */
        rd.pos++;
        rand_show(1);
        return;
    }
    seed = H->rand() ^ (H->ms() * 2654435761u) ^ (++rd.count << 16);
    if (mutate) {
        rq = cur;
        rand_mutate(&rq, seed, rd.chaos);
        rand_keep(&rq, RD_MUT);
    } else {
        rand_keep(&rq, rand_patch(&rq, &cur, seed, rd.kind, rd.chaos));
    }
    rand_show(!mutate);
}

/* ---- the keys: the panel is the module's (HB_F_PANEL), so it plays them, in its own octave, and sends them on
 * (USB, Bluetooth) as fm1-dock did. A key lets go of the note it started, whatever the octave is by then */
#define KEY0 53                                 /* key 0 is F3 */
static uint8_t kb_note[27];                     /* the note key k started, 0: none */
static uint8_t oct_held;                        /* OCT - and OCT + held (bits 0, 1) */
static void kb_key(uint32_t kind, uint32_t k, int32_t arg)
{
    uint32_t n;
    if (k >= 27u)
        return;
    if (kind == HB_EV_KEY_DOWN) {
        uint32_t v = arg > 0 && arg < 128 ? (uint32_t)arg : 100u;   /* (a key whose force is not sensed: 100) */
        if (kb_note[k]) {
            ev_put(EV_OFF, kb_note[k], 0);
            H->midi_send(0x80u, kb_note[k], 0);
        }
        n = (uint32_t)(KEY0 + 12 * kb_oct + (int32_t)k);
        kb_note[k] = (uint8_t)n;
        ev_put(EV_ON, n, v);
        H->midi_send(0x90u, n, v);
    } else if (kind == HB_EV_KEY_UP) {
        if (!kb_note[k])
            return;
        ev_put(EV_OFF, kb_note[k], 0);
        H->midi_send(0x80u, kb_note[k], 0);
        kb_note[k] = 0;
    } else if (kb_note[k]) {                    /* its pressure */
        n = arg < 0 ? 0u : arg > 127 ? 127u : (uint32_t)arg;
        ev_put(EV_PRESS, kb_note[k], n);
        H->midi_send(0xA0u, kb_note[k], n);
    }
}
/* OCT - / OCT +: lit when the keys are off the middle, blinking the faster the further: one octave lit, two
 * blinking, three blinking fast */
static void oct_leds(uint32_t now)
{
    uint32_t a = (uint32_t)(kb_oct < 0 ? -kb_oct : kb_oct), on = a == 1u ? 1u : a == 2u ? (now / 250u) & 1u : a ? (now / 90u) & 1u : 0u;
    uint32_t want = on << (kb_oct > 0 ? 1 : 0);
    if (want == oct_led)
        return;
    oct_led = (uint8_t)want;
    H->led(HB_BTN_OCT_DOWN, want & 1u);
    H->led(HB_BTN_OCT_UP, want >> 1 & 1u);
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
    if (pg->par[k] >= PG_MUT) {                 /* the dice */
        if (pg->par[k] == PG_ROLL || pg->par[k] == PG_MUT) {
            rand_turn(pg->par[k] == PG_MUT, clicks);
            return;
        }
        if (pg->par[k] == PG_KIND) {
            rd.kind = (uint8_t)clampi((int32_t)rd.kind + clicks, 0, (int32_t)NRK - 1);
        } else {
            if (clicks > 1 || clicks < -1)
                clicks *= clicks > 3 || clicks < -3 ? 4 : 2;
            rd.chaos = (uint8_t)clampi((int32_t)rd.chaos + clicks, 0, 127);
        }
        ui_s.d_tile |= (uint8_t)(1u << k);
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
    tx_param(id);
    if (id >= P_FX1_TYPE && id < P_FX1_TYPE + 4u * NFX && !((id - P_FX1_TYPE) & 3u)) {
        uint32_t j;                             /* another effect in the slot: its knobs where they usually start */
        for (j = 0; j < 3u; j++) {
            cur.p[id + 1u + j] = FXDEF[nv].def[j];
            tx_param(id + 1u + j);
        }
        ui_s.d_tile = 15;
    }
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
    ui_s.rnd = 0;
    set_t = H->ms();
    say(H->bank_save(user, sizeof user) ? "NOT SAVED" : "SAVED");
    ui_dirty_all();
    ui_leds();
}

static void ui(uint32_t ev, int32_t arg)
{
    uint32_t id = HB_EV_ID(ev), i, kind = HB_EV_KIND(ev);
    if (kind == HB_EV_KEY_DOWN || kind == HB_EV_KEY_UP || kind == HB_EV_KEY_PRESSURE) {
        kb_key(kind, id, arg);
        return;
    }
    if (id <= HB_BTN_OCT_UP && (kind == HB_EV_BTN_DOWN || kind == HB_EV_BTN_UP)) {
        if (kind == HB_EV_BTN_UP) {
            oct_held &= (uint8_t)~(1u << id);
            return;
        }
        oct_held |= (uint8_t)(1u << id);
        kb_oct = (int8_t)(oct_held == 3u ? 0 : clampi(kb_oct + (id ? 1 : -1), -3, 3));   /* (both: the middle) */
        ui_s.d_top = 1;
        return;
    }
    if (kind == HB_EV_BTN_DOWN) {
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
        } else if (id == HB_BTN_HOME) {         /* HOME: the PLAY page */
            ui_go(0);
        } else if (PAGES[ui_s.page].btn == id) { /* again: the next page of the section (round) */
            for (i = 1; i <= NPAGES; i++) {
                uint32_t p = (ui_s.page + i) % NPAGES;
                if (PAGES[p].btn == id) {
                    ui_go(p);
                    break;
                }
            }
        } else if (id < 16u && PAGES[ui_s.last[id]].btn == id) {   /* another section: where it was left */
            ui_go(ui_s.last[id]);
        } else {
            for (i = 0; i < NPAGES; i++)
                if (PAGES[i].btn == id) {
                    ui_go(i);
                    break;
                }
        }
        return;
    }
    if (HB_EV_KIND(ev) != HB_EV_ENC || !arg)
        return;
    switch (id) {
    case HB_ENC_SELECT: {                       /* the tempo, from any page (the arpeggiator, the echo, synced LFOs) */
        static char tm[16];
        int32_t nv;
        char *p;
        if (seq_ext) {
            say("MIDI CLOCK LEADS");
            break;
        }
        if (arg > 1 || arg < -1)
            arg *= arg > 3 || arg < -3 ? 4 : 2;
        nv = clampi((int32_t)cur.p[P_TEMPO] + arg, 0, PDEF[P_TEMPO].max);
        if (nv != cur.p[P_TEMPO]) {
            cur.p[P_TEMPO] = (uint8_t)nv;
            ui_s.edited = 1;
            tx_param(P_TEMPO);
            if (PAGES[ui_s.page].disp == DS_ARP)
                ui_dirty_all();
        }
        p = put_s(tm, "TEMPO ");
        put_i(p, 60 + nv, 0);
        say(tm);
        break;
    }
    case HB_ENC_ALGORITHM: {
        uint32_t p = PAGES[ui_s.page].col == K_OSCB ? P_B_WAVE : P_A_WAVE;
        int32_t nv = clampi((int32_t)cur.p[p] + arg, 0, (int32_t)(AP_NTAB + NUWT) - 1);
        if (nv != cur.p[p]) {
            cur.p[p] = (uint8_t)nv;
            ui_s.edited = 1;
            ui_dirty_all();
            wave_name(wave_msg, (uint32_t)nv);
            say(wave_msg);
            tx_param(p);
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
    tx_service();
    if (wt_dirty && !eng_active && HB_HOST_HAS(H, bank_save_n)) {   /* your tables: 6 KiB in a bank of their own */
        wt_dirty = 0;
        if (H->bank_save_n(1, uwt, sizeof uwt) < 0)
            say("NOT SAVED");
    }
    oct_leds(now);
    hb_key0(H, KEY0 + 12 * kb_oct);
    if (wt_service())
        return;                                 /* a frame of a table took this pass */
    if (ui_s.d_top) {
        ui_s.d_top = 0;
        draw_top();
        return;
    }
    if (ui_s.d_tile) {
        for (k = 0; k < 4u; k++)
            if (ui_s.d_tile >> k & 1u) {
                ui_s.d_tile &= (uint8_t)~(1u << k);
                draw_tile(k);
                return;
            }
    }
    if (ui_s.d_head) {                          /* (the sound's line is the display's: it follows) */
        ui_s.d_head = 0;
        ui_s.d_strip |= 1u;
        draw_head();
        return;
    }
    if (ui_s.d_disp) {
        ui_s.d_disp = 0;
        ui_s.d_strip = 3u;
    }
    if (ui_s.d_strip) {
        k = ui_s.d_strip & 1u ? 0u : 1u;
        ui_s.d_strip &= (uint8_t)~(1u << k);
        draw_disp(k);
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
    if (!HB_HOST_HAS(H, bank_load_n) || H->bank_load_n(1, uwt, sizeof uwt) != (int)sizeof uwt)
        for (i = 0; i < NUWT * AP_NF; i++)      /* no tables of your own yet: a sine in every frame */
            uwt[i / AP_NF][(i % AP_NF) * AP_NH * 2u] = 15556;
    st.preset = 1;
    if (H->load(&st, sizeof st) || st.magic != SET_MAGIC)
        st.preset = 1;
    set_saved = st.preset;
    preset_load(st.preset);
    set_t = 0;
    ev_r = ev_w;                                /* (nothing sounds yet: no need to hush) */
    ui_s.touched = 255;                         /* (the tables are built by draw(), a frame a pass: 16 passes) */
    H->log("fizzy-fm: ready");
    return 0;
}

static void release(void)
{
    uint32_t i;
    for (i = 0; i < 27u; i++)
        kb_note[i] = 0;
    for (i = 0; i < NV; i++)
        voice[i].on = 0;
    sq.nheld = sq.nplaying = 0;
}

/* the screen and the panel are the module's (fm1-dock's HOME held 1.5 s still opens its list) */
HB_DESC = {HB_ABI, NAME, init, release, note, cc, render, ui, draw, 0, 0, HB_F_SCREEN | HB_F_PANEL | HB_F_MIDI_RT, midi};
