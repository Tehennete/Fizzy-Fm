/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Hugo Gilbert */
/* Fizzy-Fm's clock and arpeggiator, and the notes on their way from the main loop to render(). */
#ifndef AP_SEQ_H
#define AP_SEQ_H

/* ---- events: the main loop writes, render() reads */
enum { EV_ON = 1, EV_OFF, EV_PRESS, EV_PEDAL, EV_PANIC, EV_HUSH };
typedef struct { uint8_t type, a, b; } ev_t;
#define EVQ 64u
static ev_t evq[EVQ];
static volatile uint32_t ev_w, ev_r;
static void ev_put(uint32_t type, uint32_t a, uint32_t b)
{
    uint32_t w = ev_w;
    if (w - ev_r >= EVQ)
        return;
    evq[w % EVQ].type = (uint8_t)type;
    evq[w % EVQ].a = (uint8_t)a;
    evq[w % EVQ].b = (uint8_t)b;
    ev_w = w + 1u;
}

/* ---- the clock: 24 pulses a quarter note, its own (TEMPO) or MIDI's when one comes in */
static struct {
    uint32_t acc;                               /* frames toward the next pulse, Q16 */
    uint32_t ext_pulses;                        /* MIDI clocks not yet played (set in the audio interrupt) */
    uint32_t ext_start, ext_stop;
    uint32_t ext_left;                          /* frames until MIDI's clock is taken for gone */
    uint32_t ext_frames, ext_n;                 /* its tempo: frames over 24 pulses */
    /* the arpeggio */
    uint8_t held[16], hvel[16], nheld, down;    /* its notes in the order played; keys down now */
    uint8_t playing[16], nplaying;
    uint8_t was_on;
    uint32_t tick, step;
    int32_t gate;                               /* frames until the notes playing are let go */
} sq;
static volatile uint32_t seq_ext;               /* MIDI's clock leads (the screen) */
static void pl_pulse(void);                     /* (the sequencer's, ap_pat.h) */

static void arp_stop(void)
{
    uint32_t i;
    for (i = 0; i < sq.nplaying; i++)
        eng_note_off(sq.playing[i]);
    sq.nplaying = 0;
    sq.gate = 0;
}

static void arp_note_on(uint32_t key, uint32_t vel)
{
    uint32_t i;
    if (PV(ARP_LATCH) && !sq.down)              /* a new chord takes the latched one's place */
        sq.nheld = 0;
    sq.down++;
    for (i = 0; i < sq.nheld; i++)
        if (sq.held[i] == key)
            return;
    if (!sq.nheld) {
        sq.tick = 0;
        sq.step = 0;
    }
    if (sq.nheld < 16u) {
        sq.held[sq.nheld] = (uint8_t)key;
        sq.hvel[sq.nheld++] = (uint8_t)vel;
    }
}

static void arp_note_off(uint32_t key)
{
    uint32_t i;
    if (sq.down)
        sq.down--;
    if (PV(ARP_LATCH))
        return;
    for (i = 0; i < sq.nheld; i++)
        if (sq.held[i] == key) {
            for (; i + 1u < sq.nheld; i++) {
                sq.held[i] = sq.held[i + 1u];
                sq.hvel[i] = sq.hvel[i + 1u];
            }
            sq.nheld--;
            break;
        }
}

/* step s of the pattern over n notes and their octaves: which note (its place in the list), which octave */
static uint32_t arp_pick(uint32_t mode, uint32_t s, uint32_t n, uint32_t octs, uint32_t *oct)
{
    uint32_t total = n * octs, i, per;
    switch (mode) {
    case ARP_DOWN: i = total - 1u - s % total; break;
    case ARP_UPDOWN:
        per = total > 1u ? 2u * total - 2u : 1u;
        i = s % per;
        if (i >= total)
            i = per - i;
        break;
    case ARP_RANDOM: i = rnd32() % total; break;
    default: i = s % total; break;
    }
    *oct = i / n;
    return i % n;
}

static void arp_step(void)
{
    uint8_t list[16], vel[16];
    uint32_t n = sq.nheld, i, j, mode = (uint32_t)PV(ARP_MODE), octs = (uint32_t)PV(ARP_OCT) + 1u, oct = 0, k;
    uint32_t len = DIV_TICKS[PV(ARP_RATE)];
    arp_stop();
    if (!n)
        return;
    for (i = 0; i < n; i++) {
        list[i] = sq.held[i];
        vel[i] = sq.hvel[i];
    }
    if (mode != ARP_ORDER)                      /* low to high */
        for (i = 1; i < n; i++)
            for (j = i; j && list[j - 1u] > list[j]; j--) {
                uint8_t t = list[j]; list[j] = list[j - 1u]; list[j - 1u] = t;
                t = vel[j]; vel[j] = vel[j - 1u]; vel[j - 1u] = t;
            }
    if (mode == ARP_CHORD) {
        oct = sq.step % octs;
        for (i = 0; i < n; i++) {
            uint32_t key = list[i] + 12u * oct;
            if (key < 128u) {
                eng_note_on(key, vel[i]);
                sq.playing[sq.nplaying++] = (uint8_t)key;
            }
        }
    } else {
        k = arp_pick(mode, sq.step, n, octs, &oct);
        if (list[k] + 12u * oct < 128u) {
            eng_note_on(list[k] + 12u * oct, vel[k]);
            sq.playing[sq.nplaying++] = (uint8_t)(list[k] + 12u * oct);
        }
    }
    sq.step++;
    sq.gate = (int32_t)((uint32_t)(((uint64_t)len * clk_spt) >> 16) * (uint32_t)PV(ARP_GATE) / 100u);
    if (PV(ARP_GATE) >= 100)
        sq.gate = 0x7FFFFFFF;                   /* tied: until the next step */
}

static void seq_pulse(void)
{
    uint32_t len = DIV_TICKS[PV(ARP_RATE)], pos;
    if (PV(ARP_MODE) == ARP_OFF || !sq.nheld) {
        sq.tick = 0;
        return;
    }
    pos = sq.tick % (2u * len);                 /* steps in pairs: the second one late by SWING */
    if (pos == 0u || pos == len + (len * (uint32_t)PV(ARP_SWING)) / 200u)
        arp_step();
    sq.tick++;
}

/* at the start of render(): the events and MIDI's pulses since the last one */
static void seq_begin(uint32_t n)
{
    uint32_t arp = PV(ARP_MODE) != ARP_OFF;
    if (arp != sq.was_on) {                     /* the arpeggio switched: nothing left hanging */
        arp_stop();
        if (!arp) {
            sq.nheld = 0;
            sq.down = 0;
        } else {
            eng_all_off();
        }
        sq.was_on = (uint8_t)arp;
    }
    while (ev_r != ev_w) {
        ev_t e = evq[ev_r % EVQ];
        ev_r++;
        switch (e.type) {
        case EV_ON:
            if (arp) arp_note_on(e.a, e.b); else eng_note_on(e.a, e.b);
            break;
        case EV_OFF:
            if (arp) arp_note_off(e.a); else eng_note_off(e.a);
            break;
        case EV_PRESS: eng_pressure(e.a, e.b); break;
        case EV_PEDAL: eng_pedal(e.a); break;
        case EV_PANIC:
            sq.nheld = 0;
            sq.down = 0;
            arp_stop();
            eng_all_off();
            break;
        case EV_HUSH:
            sq.nheld = 0;
            sq.down = 0;
            sq.nplaying = 0;
            eng_hush();
            break;
        default: break;
        }
    }
    if (arp && !sq.nheld && sq.nplaying && sq.gate == 0x7FFFFFFF)
        arp_stop();
    if (sq.ext_start) {
        sq.ext_start = 0;
        sq.tick = 0;
        sq.step = 0;
    }
    if (sq.ext_stop) {
        sq.ext_stop = 0;
        arp_stop();
    }
    if (sq.ext_pulses) {
        sq.ext_left = 22000u;                   /* half a second without one: back to TEMPO */
        seq_ext = 1;
        while (sq.ext_pulses) {
            sq.ext_pulses--;
            seq_pulse();
            pl_pulse();
            if (++sq.ext_n >= 24u) {            /* its tempo, a quarter note at a time */
                if (sq.ext_frames > 4000u && sq.ext_frames < 132000u)
                    clk_spt = ((sq.ext_frames << 12) / 24u) << 4;
                sq.ext_n = 0;
                sq.ext_frames = 0;
            }
        }
    } else if (sq.ext_left) {
        if (sq.ext_left <= n) {
            sq.ext_left = 0;
            seq_ext = 0;
            sq.ext_n = sq.ext_frames = 0;
        } else {
            sq.ext_left -= n;
        }
    }
    if (sq.ext_left)
        sq.ext_frames += n;
}

/* every control tick */
static void seq_run(uint32_t n)
{
    if (!sq.ext_left) {
        uint32_t bpm = 60u + (uint32_t)PV(TEMPO);
        clk_spt = ((110294u << 14) / bpm) << 2;
        sq.acc += n << 16;
        while (sq.acc >= clk_spt) {
            sq.acc -= clk_spt;
            seq_pulse();
        }
    }
    if (sq.gate > 0 && sq.gate != 0x7FFFFFFF) {
        sq.gate -= (int32_t)n;
        if (sq.gate <= 0)
            arp_stop();
    }
}

#endif
