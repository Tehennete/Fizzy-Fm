/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Hugo Gilbert */
/* Fizzy-Fm's sequencer: a looper you play into, after SLOOP and ChoralRoot. PLAY starts and stops it; REC records what
 * the keys (or MIDI) play into the pattern as it goes round, a pass over the one before (stopped, REC counts a bar
 * in first, a click on the beats); notes land on the sixteenths (QUANT) or where they were played. The STEPS page
 * writes a step at a time from the keys. Four patterns (A..D), 64 steps and 192 notes each, chords and ties too.
 *
 * Time is counted in sub-steps, 24 a sixteenth (96 a quarter note, 4 to a pulse of the clock): TEMPO's own, or
 * MIDI's clock when one comes in (its START and STOP too). A note is one word: where it starts (sub-steps), the key,
 * its velocity and how long it lasts (len_code). The player runs in render(); the keys record from the main loop
 * with one store a note, so the two never wait on each other. */
#ifndef AP_PAT_H
#define AP_PAT_H

#define SQ_PAT 4u
#define SQ_EV 192u
#define SQ_SUB 24u
typedef struct {
    uint8_t len;                                /* steps, 1..64 */
    uint8_t quant;                              /* 1: a note recorded lands on its step */
    uint8_t n;                                  /* notes in ev */
    uint8_t res;
    uint32_t ev[SQ_EV];                         /* pos << 21 | key << 14 | vel << 7 | len_code */
} pat_t;
/* what is kept in the flash (bank 2): the settings and the four patterns */
static struct {
    uint8_t magic, click;                       /* the click: 0 off, 1 when recording, 2 always */
    uint8_t cur, res;
    pat_t p[SQ_PAT];
} pb;
#define PB_MAGIC 0xA6u
#define pats pb.p
static pat_t pat_undo;                          /* the pattern before the last pass recorded, a clear or an edit */
static uint8_t pat_undo_of = 0xFFu;             /* (which one it was) */
static volatile uint32_t pat_dirty;             /* (saved when the sequencer stops) */
static volatile uint32_t pat_ver;               /* (the screen: something in a pattern changed) */
static void pb_init(void)
{
    uint32_t i;
    pb.magic = PB_MAGIC;
    pb.click = 1;
    pb.cur = 0;
    for (i = 0; i < SQ_PAT; i++) {
        pats[i].len = 16;
        pats[i].quant = 1;
        pats[i].n = 0;
    }
}

#define EV_POS(e) ((e) >> 21)
#define EV_KEY(e) ((e) >> 14 & 127u)
#define EV_VEL(e) ((e) >> 7 & 127u)
static uint32_t ev_make(uint32_t pos, uint32_t key, uint32_t vel, uint32_t lc)
{
    return pos << 21 | (key & 127u) << 14 | (vel & 127u) << 7 | (lc & 127u);
}
static uint32_t len_sub(uint32_t lc)            /* code -> sub-steps: 1..48 (two steps) one by one, then whole steps */
{
    return lc < 48u ? lc + 1u : (lc - 46u) * SQ_SUB;
}
static uint32_t len_code(uint32_t t)
{
    if (t <= 48u)
        return t ? t - 1u : 0u;
    t = (t + SQ_SUB / 2u) / SQ_SUB + 46u;
    return t > 127u ? 127u : t;
}

/* ---- the player (render) */
static struct {
    volatile uint8_t run;                       /* playing */
    volatile uint8_t rec;                       /* 1 counting a bar in, 2 recording */
    volatile uint8_t pat, next;                 /* the pattern playing; the one to play when it comes round */
    volatile uint32_t sp;                       /* the sub-step it is at */
    volatile uint32_t beat;                     /* beats since it started (the PLAY light, the screen) */
    uint32_t count;                             /* sub-steps of the count-in left */
    uint32_t acc;                               /* frames toward the next sub-step, Q16 */
    uint8_t key[16], nsound;                    /* the notes it is sounding, */
    uint16_t left[16];                          /* and the sub-steps until each ends */
    volatile uint8_t req;                       /* from the main loop: 1 start, 2 stop, 3 start counting in */
    uint32_t click_n, click_ph, click_inc;      /* a click: frames left, its phase */
} pl;

static void pl_on(uint32_t key, uint32_t vel)
{
    if (PV(ARP_MODE) != ARP_OFF) arp_note_on(key, vel); else eng_note_on(key, vel);
}
static void pl_off(uint32_t key)
{
    if (PV(ARP_MODE) != ARP_OFF) arp_note_off(key); else eng_note_off(key);
}
static void pl_silence(void)
{
    uint32_t i;
    for (i = 0; i < pl.nsound; i++)
        pl_off(pl.key[i]);
    pl.nsound = 0;
}
static void pl_click(uint32_t loud)
{
    pl.click_n = 900u;                          /* 20 ms */
    pl.click_ph = 0;
    pl.click_inc = loud ? 0x0B000000u : 0x07400000u;   /* about 2 kHz on the bar, 1.4 kHz on the beats */
}

/* a sub-step: the notes that end, the notes that start (late by SWING on the off-beat sixteenths) */
static void pl_sub(void)
{
    const pat_t *p;
    uint32_t i, L, sw, k;
    if (pl.rec == 1u) {                         /* the bar counted in */
        if (pl.count % (4u * SQ_SUB) == 0u)
            pl_click(pl.count == 16u * SQ_SUB);
        if (--pl.count)
            return;
        pl.rec = 2u;
        pl.sp = 0;
        pl.beat = 0;
    }
    p = &pats[pl.pat];
    L = (uint32_t)p->len * SQ_SUB;
    for (i = 0; i < pl.nsound;) {
        if (--pl.left[i] == 0u) {
            pl_off(pl.key[i]);
            pl.nsound--;
            pl.key[i] = pl.key[pl.nsound];
            pl.left[i] = pl.left[pl.nsound];
        } else {
            i++;
        }
    }
    if (pl.sp % (4u * SQ_SUB) == 0u) {
        if (pl.sp)
            pl.beat++;
        if (pb.click == 2u || (pb.click == 1u && pl.rec))
            pl_click(pl.sp == 0u);
    }
    sw = ((uint32_t)PV(ARP_SWING) * SQ_SUB) / 150u;    /* (75 %: half a sixteenth) */
    for (k = 0; k < p->n; k++) {
        uint32_t e = p->ev[k], pos = EV_POS(e);
        if ((pos / SQ_SUB) & 1u)
            pos += sw;
        if (pos >= L)
            pos -= L;
        if (pos != pl.sp)
            continue;
        if (pl.nsound >= 16u) {                 /* (sixteen at once at most: the oldest goes) */
            pl_off(pl.key[0]);
            pl.nsound--;
            pl.key[0] = pl.key[pl.nsound];
            pl.left[0] = pl.left[pl.nsound];
        }
        for (i = 0; i < pl.nsound; i++)         /* the same key again: it starts over */
            if (pl.key[i] == EV_KEY(e)) {
                pl_off(pl.key[i]);
                pl.nsound--;
                pl.key[i] = pl.key[pl.nsound];
                pl.left[i] = pl.left[pl.nsound];
                break;
            }
        pl_on(EV_KEY(e), EV_VEL(e) ? EV_VEL(e) : 1u);
        pl.key[pl.nsound] = (uint8_t)EV_KEY(e);
        pl.left[pl.nsound++] = (uint16_t)len_sub(e & 127u);
    }
    if (++pl.sp >= L) {                         /* round: the pattern asked for next */
        pl.sp = 0;
        pl.beat++;
        if (pl.next != pl.pat && pl.next < SQ_PAT)
            pl.pat = pl.next;
    }
}

static void pl_start(uint32_t count_in)
{
    pl_silence();
    pl.sp = 0;
    pl.beat = 0;
    pl.acc = 0;
    pl.pat = pl.next;
    pl.rec = count_in ? 1u : pl.rec;
    pl.count = 16u * SQ_SUB;
    pl.run = 1;
}
static void pl_stop(void)
{
    pl_silence();
    pl.run = 0;
    pl.rec = 0;
    pat_dirty = 1;
}

/* at the start of render(): what the main loop asked, MIDI's START and STOP */
static void pl_begin(uint32_t ext_start, uint32_t ext_stop)
{
    uint32_t r = pl.req;
    pl.req = 0;
    if (ext_start)
        r = 1u;
    if (ext_stop)
        r = 2u;
    if (r == 1u || r == 3u)
        pl_start(r == 3u);
    else if (r == 2u)
        pl_stop();
}
/* every control tick, with TEMPO's clock; MIDI's clock calls pl_pulse() instead */
static void pl_run(uint32_t n)
{
    if (pl.run && !sq.ext_left) {
        pl.acc += n << 16;
        while (pl.acc >= (clk_spt >> 2)) {
            pl.acc -= clk_spt >> 2;
            pl_sub();
        }
    }
}
static void pl_pulse(void)
{
    uint32_t i;
    if (pl.run)
        for (i = 0; i < 4u; i++)
            pl_sub();
}
/* the click, over what goes out */
static void pl_click_mix(int32_t *out, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n && pl.click_n; i++, pl.click_n--) {
        int32_t a = (int32_t)(pl.click_n * 7u), s = (sinr[pl.click_ph >> 22] * a) >> 15;
        pl.click_ph += pl.click_inc;
        out[2u * i] += s;
        out[2u * i + 1u] += s;
    }
}

/* ---- recording (the main loop: the keys; the audio interrupt: MIDI) */
static uint16_t rec_at[128];                    /* where a key held was put (sub-step), +1; 0: not recorded */
static void pat_snapshot(uint32_t p)
{
    pat_undo = pats[p];
    pat_undo_of = (uint8_t)p;
}
static void pat_put(pat_t *p, uint32_t e)       /* a note in: over one of the same key at the same place */
{
    uint32_t k;
    for (k = 0; k < p->n; k++)
        if ((p->ev[k] & 0xFFFFC000u) == (e & 0xFFFFC000u)) {
            p->ev[k] = e;
            return;
        }
    if (p->n < SQ_EV) {
        p->ev[p->n] = e;
        p->n++;
    }
}
static void rec_note_on(uint32_t key, uint32_t vel)
{
    pat_t *p = &pats[pl.pat];
    uint32_t L = (uint32_t)p->len * SQ_SUB, pos = pl.sp;
    if (!pl.run || pl.rec != 2u || key > 127u)
        return;
    if (p->quant)
        pos = ((pos + SQ_SUB / 2u) / SQ_SUB) * SQ_SUB;
    if (pos >= L)
        pos = 0;
    rec_at[key] = (uint16_t)(pos + 1u);
    pat_put(p, ev_make(pos, key, vel ? vel : 1u, SQ_SUB - 1u));
    pat_dirty = 1;
    pat_ver++;
}
static void rec_note_off(uint32_t key)
{
    pat_t *p = &pats[pl.pat];
    uint32_t L = (uint32_t)p->len * SQ_SUB, at, t, k;
    if (key > 127u || !rec_at[key])
        return;
    at = rec_at[key] - 1u;
    rec_at[key] = 0;
    t = (pl.sp + L - at) % L;                   /* how long it was held */
    if (p->quant)
        t = ((t + SQ_SUB / 2u) / SQ_SUB) * SQ_SUB;
    if (!t)
        t = p->quant ? SQ_SUB : 6u;
    for (k = 0; k < p->n; k++)
        if (EV_POS(p->ev[k]) == at && EV_KEY(p->ev[k]) == key) {
            p->ev[k] = (p->ev[k] & ~127u) | len_code(t);
            pat_ver++;
            break;
        }
}

/* ---- editing a step (the STEPS page) */
static void pat_del(pat_t *p, uint32_t k)
{
    p->n--;
    p->ev[k] = p->ev[p->n];
}
static uint32_t step_notes(const pat_t *p, uint32_t s, uint32_t *keys, uint32_t max)   /* a step's keys, low first */
{
    uint32_t k, n = 0, i, j;
    for (k = 0; k < p->n && n < max; k++)
        if (EV_POS(p->ev[k]) / SQ_SUB == s)
            keys[n++] = EV_KEY(p->ev[k]);
    for (i = 1; i < n; i++)
        for (j = i; j && keys[j - 1u] > keys[j]; j--) {
            uint32_t t = keys[j];
            keys[j] = keys[j - 1u];
            keys[j - 1u] = t;
        }
    return n;
}
static void step_clear(pat_t *p, uint32_t s)
{
    uint32_t k;
    for (k = p->n; k--;)
        if (EV_POS(p->ev[k]) / SQ_SUB == s)
            pat_del(p, k);
}
/* the notes of step s: d semitones up, a velocity, a length in steps (0: as they are) */
static void step_edit(pat_t *p, uint32_t s, int32_t d, uint32_t vel, uint32_t steps)
{
    uint32_t k;
    for (k = 0; k < p->n; k++) {
        uint32_t e = p->ev[k];
        int32_t key = (int32_t)EV_KEY(e) + d;
        if (EV_POS(e) / SQ_SUB != s)
            continue;
        if (key < 0) key = 0;
        if (key > 127) key = 127;
        p->ev[k] = ev_make(EV_POS(e), (uint32_t)key, vel ? vel : EV_VEL(e), steps ? len_code(steps * SQ_SUB) : e & 127u);
    }
}

#endif
