/* SPDX-License-Identifier: GPL-3.0-only */
/* APLAT: a sound's parameters (one byte each), their names and ranges, and the factory sounds. */
#ifndef AP_DEF_H
#define AP_DEF_H

/* how a value is shown */
enum { F_NUM, F_LIST, F_PCT };
/* the lists (F_LIST: aux is one of these) */
enum { L_WAVE, L_WARP, L_SUBW, L_NOISE, L_FTYPE, L_LFO, L_ONOFF, L_SRC, L_DST, L_ARP, L_DIV, L_DRIVE, L_MODE };

/* X(id, name, max, default, format, aux)   aux: F_NUM the value shown is v - aux; F_LIST the list */
#define AP_OSC(X, o) \
    X(o##_WAVE, "WAVE", AP_NTAB - 1, 0, F_LIST, L_WAVE) \
    X(o##_POS, "POS", 127, 0, F_NUM, 0) \
    X(o##_WMODE, "WARP", 7, 0, F_LIST, L_WARP) \
    X(o##_WARP, "AMOUNT", 127, 0, F_NUM, 0) \
    X(o##_OCT, "OCTAVE", 6, 3, F_NUM, 3) \
    X(o##_SEMI, "SEMI", 24, 12, F_NUM, 12) \
    X(o##_FINE, "FINE", 100, 50, F_NUM, 50) \
    X(o##_LVL, "LEVEL", 127, 0, F_NUM, 0) \
    X(o##_UNI, "UNISON", 4, 0, F_NUM, -1) \
    X(o##_DET, "DETUNE", 127, 30, F_NUM, 0) \
    X(o##_SPR, "SPREAD", 127, 80, F_NUM, 0) \
    X(o##_PAN, "PAN", 127, 64, F_NUM, 64)
#define AP_ENV(X, e) \
    X(e##_A, "ATTACK", 127, 0, F_NUM, 0) \
    X(e##_D, "DECAY", 127, 70, F_NUM, 0) \
    X(e##_S, "SUS", 127, 127, F_NUM, 0) \
    X(e##_R, "REL", 127, 45, F_NUM, 0)
#define AP_LFO(X, l) \
    X(l##_SHAPE, "SHAPE", 5, 0, F_LIST, L_LFO) \
    X(l##_RATE, "RATE", 127, 70, F_NUM, 0) \
    X(l##_SYNC, "SYNC", 1, 0, F_LIST, L_ONOFF) \
    X(l##_FADE, "FADE", 127, 0, F_NUM, 0)
#define AP_MOD(X, m) \
    X(m##_SRC, "SOURCE", 15, 0, F_LIST, L_SRC) \
    X(m##_DST, "DEST", 15, 0, F_LIST, L_DST) \
    X(m##_AMT, "AMOUNT", 200, 100, F_NUM, 100)
#define AP_PARAMS(X) \
    X(M1, "MACRO 1", 127, 0, F_NUM, 0) X(M2, "MACRO 2", 127, 0, F_NUM, 0) \
    X(M3, "MACRO 3", 127, 0, F_NUM, 0) X(M4, "MACRO 4", 127, 0, F_NUM, 0) \
    AP_OSC(X, A) AP_OSC(X, B) \
    X(SUB_WAVE, "SUB", 3, 0, F_LIST, L_SUBW) \
    X(SUB_OCT, "OCTAVE", 2, 1, F_NUM, 2) \
    X(SUB_LVL, "LEVEL", 127, 0, F_NUM, 0) \
    X(SUB_DIR, "DIRECT", 1, 0, F_LIST, L_ONOFF) \
    X(NZ_TYPE, "NOISE", 3, 0, F_LIST, L_NOISE) \
    X(NZ_LVL, "LEVEL", 127, 0, F_NUM, 0) \
    X(VOL, "VOLUME", 127, 100, F_NUM, 0) \
    X(VSPREAD, "WIDTH", 127, 0, F_NUM, 0) \
    X(F_TYPE, "TYPE", 5, 1, F_LIST, L_FTYPE) \
    X(F_CUT, "CUTOFF", 127, 127, F_NUM, 0) \
    X(F_RES, "RESO", 127, 0, F_NUM, 0) \
    X(F_ENV, "ENV 2", 127, 64, F_NUM, 64) \
    X(F_KEY, "KEYTRK", 100, 0, F_PCT, 0) \
    X(F_DRV, "DRIVE", 127, 0, F_NUM, 0) \
    X(F_VEL, "VELO", 127, 0, F_NUM, 0) \
    X(F_LFO, "LFO 1", 127, 64, F_NUM, 64) \
    AP_ENV(X, E1) AP_ENV(X, E2) AP_ENV(X, E3) \
    AP_LFO(X, L1) AP_LFO(X, L2) AP_LFO(X, L3) \
    AP_MOD(X, X1) AP_MOD(X, X2) AP_MOD(X, X3) AP_MOD(X, X4) \
    AP_MOD(X, X5) AP_MOD(X, X6) AP_MOD(X, X7) AP_MOD(X, X8) \
    X(ARP_MODE, "MODE", 6, 0, F_LIST, L_ARP) \
    X(ARP_RATE, "RATE", 11, 9, F_LIST, L_DIV) \
    X(ARP_GATE, "GATE", 100, 60, F_PCT, 0) \
    X(ARP_OCT, "OCTS", 3, 0, F_NUM, -1) \
    X(ARP_SWING, "SWING", 75, 0, F_PCT, 0) \
    X(ARP_LATCH, "LATCH", 1, 0, F_LIST, L_ONOFF) \
    X(TEMPO, "TEMPO", 127, 60, F_NUM, -60) \
    X(RES1, "", 0, 0, F_NUM, 0) \
    X(DRV_TYPE, "DRIVE", 3, 0, F_LIST, L_DRIVE) \
    X(DRV_AMT, "AMOUNT", 127, 0, F_NUM, 0) \
    X(CHO_DEPTH, "CHORUS", 127, 60, F_NUM, 0) \
    X(CHO_MIX, "MIX", 127, 0, F_NUM, 0) \
    X(DLY_TIME, "DELAY", 11, 4, F_LIST, L_DIV) \
    X(DLY_FB, "FEEDBK", 127, 50, F_NUM, 0) \
    X(DLY_TONE, "TONE", 127, 90, F_NUM, 0) \
    X(DLY_MIX, "MIX", 127, 0, F_NUM, 0) \
    X(REV_DECAY, "REVERB", 127, 80, F_NUM, 0) \
    X(REV_DAMP, "DAMP", 127, 60, F_NUM, 0) \
    X(REV_MIX, "MIX", 127, 0, F_NUM, 0) \
    X(RES2, "", 0, 0, F_NUM, 0) \
    X(MODE, "MODE", 2, 0, F_LIST, L_MODE) \
    X(GLIDE, "GLIDE", 127, 0, F_NUM, 0) \
    X(BEND, "BEND", 24, 2, F_NUM, 0) \
    X(VELSENS, "VELO", 127, 80, F_NUM, 0)

#define X(id, name, max, def, fmt, aux) P_##id,
enum { AP_PARAMS(X) NP };
#undef X

typedef struct {
    const char *name;
    uint8_t max, def, fmt;
    int8_t aux;
} pdef_t;
#define X(id, name, max, def, fmt, aux) {name, (uint8_t)(max), def, fmt, aux},
static const pdef_t PDEF[NP] = {AP_PARAMS(X)};
#undef X

#define NAME_LEN 12u
typedef struct {
    uint8_t p[NP];
    char name[NAME_LEN];
} patch_t;                                      /* 128 bytes */
typedef char patch_size_check[sizeof(patch_t) == 128u ? 1 : -1];

/* modulation sources and destinations (the matrix) */
enum { S_NONE, S_ENV2, S_ENV3, S_LFO1, S_LFO2, S_LFO3, S_VEL, S_KEY, S_AT, S_WHEEL, S_BEND, S_RND,
       S_M1, S_M2, S_M3, S_M4, NSRC };
enum { D_NONE, D_PITCH, D_FINE, D_APOS, D_AWARP, D_ALVL, D_BPOS, D_BWARP, D_BLVL, D_BPITCH, D_CUT, D_RES,
       D_AMP, D_PAN, D_NOISE, D_L1RATE, NDST };
enum { W_OFF, W_SYNC, W_BENDP, W_BENDM, W_PWM, W_MIRROR, W_FM, W_QUANT };
enum { FT_OFF, FT_LP12, FT_LP24, FT_BP, FT_HP, FT_NOTCH };
enum { ARP_OFF, ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM, ARP_ORDER, ARP_CHORD };
enum { MODE_POLY, MODE_MONO, MODE_LEGATO };

static const char *const LS_WARP[] = {"OFF", "SYNC", "BEND +", "BEND -", "PWM", "MIRROR", "FM", "QUANT"};
static const char *const LS_SUBW[] = {"SINE", "TRI", "SQUARE", "SAW"};
static const char *const LS_NOISE[] = {"WHITE", "PINK", "DARK", "TUNED"};
static const char *const LS_FTYPE[] = {"OFF", "LP 12", "LP 24", "BAND", "HP 12", "NOTCH"};
static const char *const LS_LFO[] = {"SINE", "TRI", "SAW", "SQUARE", "S+H", "DRIFT"};
static const char *const LS_ONOFF[] = {"OFF", "ON"};
static const char *const LS_SRC[] = {"-", "ENV 2", "ENV 3", "LFO 1", "LFO 2", "LFO 3", "VELO", "KEY", "PRESS",
                                     "WHEEL", "BEND", "RANDOM", "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4"};
static const char *const LS_DST[] = {"-", "PITCH", "FINE", "A POS", "A WARP", "A LEVEL", "B POS", "B WARP",
                                     "B LEVEL", "B PITCH", "CUTOFF", "RESO", "AMP", "PAN", "NOISE", "LFO1 RT"};
static const char *const LS_ARP[] = {"OFF", "UP", "DOWN", "UP DOWN", "RANDOM", "ORDER", "CHORD"};
static const char *const LS_DIV[] = {"1/1", "1/2", "1/4 D", "1/4", "1/8 D", "1/4 T", "1/8", "1/16 D", "1/8 T",
                                     "1/16", "1/16 T", "1/32"};
static const uint8_t DIV_TICKS[12] = {96, 48, 36, 24, 18, 16, 12, 9, 8, 6, 4, 3};   /* MIDI clocks (24 a quarter) */
static const char *const LS_DRIVE[] = {"SOFT", "HARD", "FOLD", "CRUSH"};
static const char *const LS_MODE[] = {"POLY", "MONO", "LEGATO"};
static const char *const *const LISTS[] = {AP_WT_NAME, LS_WARP, LS_SUBW, LS_NOISE, LS_FTYPE, LS_LFO, LS_ONOFF, LS_SRC,
                                           LS_DST, LS_ARP, LS_DIV, LS_DRIVE, LS_MODE};

/* ---- the factory sounds: a name, then (parameter, value) pairs over the INIT sound, ended by NP */
enum { T_BASIC, T_SAW, T_SQUARE, T_PWM, T_SYNC, T_FORMANT, T_VOCAL, T_FM11, T_FM12, T_BELL, T_GLASS, T_ORGAN,
       T_HARMO, T_FOLD, T_DRIVE, T_BITS, T_METAL, T_COMB, T_ODDEVEN, T_DIGI, T_STRING, T_BRASS, T_REED, T_TINE };
#define MODSLOT(n, s, d, a) P_X1_SRC + 3 * (n), s, P_X1_DST + 3 * (n), d, P_X1_AMT + 3 * (n), (uint8_t)(100 + (a))
typedef struct {
    const char *name;
    const uint8_t *set;
} factory_t;

static const uint8_t FS_INIT[] = {P_A_LVL, 100, P_A_WAVE, T_BASIC, P_A_POS, 54, P_VOL, 76, NP};
static const uint8_t FS_SUPER[] = {                   /* a wide detuned saw stack */
    P_A_WAVE, T_SAW, P_A_POS, 80, P_A_LVL, 100, P_A_UNI, 4, P_A_DET, 46, P_A_SPR, 110,
    P_B_WAVE, T_SAW, P_B_POS, 64, P_B_LVL, 70, P_B_OCT, 4, P_B_UNI, 2, P_B_DET, 60, P_B_SPR, 127,
    P_F_TYPE, FT_LP12, P_F_CUT, 96, P_F_RES, 14, P_F_KEY, 40, P_E1_A, 6, P_E1_R, 62,
    MODSLOT(0, S_M1, D_CUT, 55), MODSLOT(1, S_M2, D_APOS, 60), MODSLOT(2, S_WHEEL, D_CUT, 40),
    P_M1, 64, P_CHO_MIX, 40, P_REV_MIX, 34, P_REV_DECAY, 84, P_VOL, 90, NP};
static const uint8_t FS_GLASSPAD[] = {                /* a slow pad whose wave moves on its own in every voice */
    P_A_WAVE, T_GLASS, P_A_POS, 20, P_A_LVL, 90, P_A_UNI, 2, P_A_DET, 24, P_A_SPR, 100,
    P_B_WAVE, T_VOCAL, P_B_POS, 30, P_B_LVL, 80, P_B_UNI, 2, P_B_DET, 34, P_B_SPR, 120,
    P_F_TYPE, FT_LP12, P_F_CUT, 84, P_F_RES, 20, P_F_ENV, 84, P_F_KEY, 50,
    P_E1_A, 78, P_E1_D, 90, P_E1_S, 110, P_E1_R, 90, P_E2_A, 92, P_E2_D, 100, P_E2_S, 60, P_E2_R, 90,
    P_L1_SHAPE, 5, P_L1_RATE, 38, P_L1_FADE, 60, P_L2_SHAPE, 1, P_L2_RATE, 30,
    MODSLOT(0, S_LFO1, D_APOS, 46), MODSLOT(1, S_LFO2, D_BPOS, 40), MODSLOT(2, S_AT, D_CUT, 40),
    MODSLOT(3, S_M1, D_CUT, 50), MODSLOT(4, S_M2, D_APOS, 70), MODSLOT(5, S_RND, D_PAN, 50),
    P_M1, 50, P_VSPREAD, 60, P_CHO_MIX, 50, P_DLY_MIX, 30, P_DLY_FB, 60, P_REV_MIX, 70, P_REV_DECAY, 104, P_VOL, 110, NP};
static const uint8_t FS_ACID[] = {                    /* a mono line: each note its own filter sweep, glide on legato */
    P_A_WAVE, T_SAW, P_A_POS, 72, P_A_LVL, 110, P_SUB_LVL, 60, P_SUB_WAVE, 2,
    P_F_TYPE, FT_LP24, P_F_CUT, 44, P_F_RES, 96, P_F_ENV, 104, P_F_DRV, 50, P_F_KEY, 30, P_F_VEL, 60,
    P_E1_D, 60, P_E1_S, 100, P_E1_R, 20, P_E2_D, 52, P_E2_S, 0, P_E2_R, 30,
    P_MODE, MODE_LEGATO, P_GLIDE, 44, MODSLOT(0, S_M1, D_CUT, 60), MODSLOT(1, S_M2, D_RES, 40),
    P_DRV_AMT, 30, P_DLY_MIX, 26, P_DLY_TIME, 7, P_VOL, 53, NP};
static const uint8_t FS_PLUCK[] = {                   /* a pluck: velocity opens the filter of its own note */
    P_A_WAVE, T_HARMO, P_A_POS, 40, P_A_LVL, 100, P_B_WAVE, T_TINE, P_B_POS, 70, P_B_LVL, 70, P_B_OCT, 4,
    P_F_TYPE, FT_LP12, P_F_CUT, 50, P_F_RES, 30, P_F_ENV, 100, P_F_VEL, 90, P_F_KEY, 60,
    P_E1_D, 66, P_E1_S, 0, P_E1_R, 60, P_E2_D, 54, P_E2_S, 0, P_E2_R, 54,
    MODSLOT(0, S_ENV2, D_APOS, 60), MODSLOT(1, S_M1, D_CUT, 50), MODSLOT(2, S_VEL, D_BLVL, 60),
    P_VSPREAD, 50, P_DLY_MIX, 44, P_DLY_TIME, 4, P_DLY_FB, 56, P_REV_MIX, 40, P_VOL, 101, NP};
static const uint8_t FS_VOX[] = {                     /* a choir that opens with the pressure of each key */
    P_A_WAVE, T_FORMANT, P_A_POS, 0, P_A_LVL, 100, P_A_UNI, 2, P_A_DET, 20, P_A_SPR, 90,
    P_B_WAVE, T_VOCAL, P_B_POS, 60, P_B_LVL, 70, P_B_FINE, 57,
    P_F_TYPE, FT_BP, P_F_CUT, 82, P_F_RES, 10, P_F_KEY, 70,
    P_E1_A, 62, P_E1_R, 76, P_L1_RATE, 62, P_L1_FADE, 74, P_L2_SHAPE, 5, P_L2_RATE, 30,
    MODSLOT(0, S_AT, D_APOS, 90), MODSLOT(1, S_LFO1, D_FINE, 18), MODSLOT(2, S_LFO2, D_BPOS, 30),
    MODSLOT(3, S_M1, D_APOS, 100), MODSLOT(4, S_WHEEL, D_APOS, 80),
    P_CHO_MIX, 60, P_REV_MIX, 76, P_REV_DECAY, 100, P_VOL, 113, NP};
static const uint8_t FS_FMBASS[] = {                  /* A's phase driven by B: an FM bass, the amount on the envelope */
    P_A_WAVE, T_BASIC, P_A_POS, 0, P_A_LVL, 110, P_A_WMODE, W_FM, P_A_WARP, 30, P_A_OCT, 2,
    P_B_WAVE, T_BASIC, P_B_POS, 0, P_B_OCT, 3, P_SUB_LVL, 70, P_SUB_OCT, 1,
    P_F_TYPE, FT_LP12, P_F_CUT, 100, P_E1_D, 70, P_E1_S, 90, P_E1_R, 34, P_E2_D, 58, P_E2_S, 20,
    MODSLOT(0, S_ENV2, D_AWARP, 60), MODSLOT(1, S_VEL, D_AWARP, 40), MODSLOT(2, S_M1, D_AWARP, 70),
    P_MODE, MODE_MONO, P_VOL, 78, NP};
static const uint8_t FS_BELLS[] = {
    P_A_WAVE, T_BELL, P_A_POS, 70, P_A_LVL, 96, P_B_WAVE, T_GLASS, P_B_POS, 50, P_B_LVL, 60, P_B_OCT, 4, P_B_FINE, 54,
    P_F_TYPE, FT_LP12, P_F_CUT, 110, P_F_KEY, 30, P_E1_D, 96, P_E1_S, 0, P_E1_R, 92, P_E2_D, 84, P_E2_S, 0,
    MODSLOT(0, S_ENV2, D_APOS, -50), MODSLOT(1, S_VEL, D_APOS, 40), MODSLOT(2, S_RND, D_PAN, 70),
    P_VSPREAD, 70, P_DLY_MIX, 40, P_DLY_TIME, 2, P_DLY_FB, 60, P_REV_MIX, 80, P_REV_DECAY, 110, P_VOL, 113, NP};
static const uint8_t FS_SYNCLEAD[] = {
    P_A_WAVE, T_SAW, P_A_POS, 70, P_A_LVL, 100, P_A_WMODE, W_SYNC, P_A_WARP, 40,
    P_B_WAVE, T_SQUARE, P_B_POS, 70, P_B_LVL, 50, P_B_OCT, 2,
    P_F_TYPE, FT_LP12, P_F_CUT, 104, P_F_RES, 20, P_E1_R, 40, P_E2_D, 80, P_E2_S, 40,
    P_L1_RATE, 86, P_L1_FADE, 70, P_MODE, MODE_LEGATO, P_GLIDE, 36,
    MODSLOT(0, S_ENV2, D_AWARP, 50), MODSLOT(1, S_WHEEL, D_AWARP, 60), MODSLOT(2, S_LFO1, D_FINE, 22),
    MODSLOT(3, S_M1, D_AWARP, 80), MODSLOT(4, S_AT, D_AWARP, 40),
    P_DRV_AMT, 24, P_DLY_MIX, 40, P_DLY_TIME, 4, P_REV_MIX, 30, P_VOL, 68, NP};
static const uint8_t FS_ORGAN[] = {
    P_A_WAVE, T_ORGAN, P_A_POS, 60, P_A_LVL, 100, P_B_WAVE, T_ORGAN, P_B_POS, 110, P_B_LVL, 50,
    P_F_TYPE, FT_LP12, P_F_CUT, 112, P_E1_R, 16, P_L1_RATE, 96, P_L1_SHAPE, 1,
    MODSLOT(0, S_LFO1, D_FINE, 8), MODSLOT(1, S_LFO1, D_AMP, 14), MODSLOT(2, S_M1, D_APOS, 50),
    MODSLOT(3, S_WHEEL, D_L1RATE, -70), P_M1, 40, P_CHO_MIX, 70, P_CHO_DEPTH, 90, P_DRV_AMT, 20, P_REV_MIX, 30, P_VOL, 68, NP};
static const uint8_t FS_KEYS[] = {                    /* an electric piano: the tine follows the velocity */
    P_A_WAVE, T_TINE, P_A_POS, 20, P_A_LVL, 110, P_B_WAVE, T_FM12, P_B_POS, 20, P_B_LVL, 30, P_B_OCT, 4,
    P_F_TYPE, FT_LP12, P_F_CUT, 86, P_F_KEY, 60, P_F_VEL, 50, P_E1_D, 92, P_E1_S, 30, P_E1_R, 60, P_E2_D, 70, P_E2_S, 0,
    MODSLOT(0, S_VEL, D_APOS, 80), MODSLOT(1, S_ENV2, D_BLVL, 60), MODSLOT(2, S_LFO1, D_PAN, 40),
    P_L1_RATE, 66, P_L1_SHAPE, 1, P_VELSENS, 110, P_CHO_MIX, 40, P_REV_MIX, 30, P_VOL, 101, NP};
static const uint8_t FS_STRINGS[] = {
    P_A_WAVE, T_STRING, P_A_POS, 60, P_A_LVL, 100, P_A_UNI, 4, P_A_DET, 40, P_A_SPR, 120,
    P_B_WAVE, T_STRING, P_B_POS, 100, P_B_LVL, 60, P_B_OCT, 4, P_B_UNI, 2, P_B_DET, 30, P_B_SPR, 110,
    P_F_TYPE, FT_LP12, P_F_CUT, 92, P_F_KEY, 40, P_E1_A, 66, P_E1_R, 72,
    P_L1_RATE, 74, P_L1_FADE, 70, MODSLOT(0, S_LFO1, D_FINE, 10), MODSLOT(1, S_AT, D_AMP, 40),
    MODSLOT(2, S_AT, D_CUT, 30), MODSLOT(3, S_M1, D_CUT, 50), MODSLOT(4, S_VEL, D_CUT, 20),
    P_M1, 60, P_CHO_MIX, 50, P_REV_MIX, 60, P_REV_DECAY, 96, P_VOL, 88, NP};
static const uint8_t FS_BRASS[] = {
    P_A_WAVE, T_BRASS, P_A_POS, 30, P_A_LVL, 110, P_A_UNI, 2, P_A_DET, 18, P_A_SPR, 70,
    P_F_TYPE, FT_LP12, P_F_CUT, 70, P_F_ENV, 90, P_F_KEY, 50, P_F_VEL, 50,
    P_E1_A, 30, P_E1_R, 44, P_E2_A, 44, P_E2_D, 70, P_E2_S, 80,
    MODSLOT(0, S_ENV2, D_APOS, 70), MODSLOT(1, S_VEL, D_APOS, 30), MODSLOT(2, S_AT, D_APOS, 40),
    MODSLOT(3, S_LFO1, D_FINE, 12), P_L1_RATE, 80, P_L1_FADE, 76, P_REV_MIX, 40, P_VOL, 77, NP};
static const uint8_t FS_ARPSEQ[] = {                  /* hold a chord: the arpeggio, in time with the echo */
    P_A_WAVE, T_PWM, P_A_POS, 30, P_A_LVL, 127, P_B_WAVE, T_DIGI, P_B_POS, 40, P_B_LVL, 50, P_B_OCT, 4,
    P_F_TYPE, FT_LP24, P_F_CUT, 70, P_F_RES, 50, P_F_ENV, 92, P_E1_D, 60, P_E1_S, 60, P_E1_R, 44, P_E2_D, 50, P_E2_S, 0,
    P_L3_RATE, 2, P_L3_SYNC, 1, P_L3_SHAPE, 1, MODSLOT(0, S_LFO3, D_CUT, 30), MODSLOT(1, S_LFO3, D_APOS, 50),
    MODSLOT(2, S_M1, D_CUT, 60), P_ARP_MODE, ARP_UPDOWN, P_ARP_OCT, 1, P_ARP_RATE, 9, P_ARP_LATCH, 1,
    P_DLY_MIX, 50, P_DLY_TIME, 4, P_DLY_FB, 60, P_REV_MIX, 30, P_VOL, 98, NP};
static const uint8_t FS_DIGI[] = {                    /* a wave that steps: S+H on the position, per voice */
    P_A_WAVE, T_DIGI, P_A_POS, 60, P_A_LVL, 127, P_A_WMODE, W_QUANT, P_A_WARP, 30,
    P_B_WAVE, T_BITS, P_B_POS, 60, P_B_LVL, 100, P_B_OCT, 2,
    P_F_TYPE, FT_BP, P_F_CUT, 80, P_F_RES, 25, P_F_KEY, 60, P_E1_R, 50,
    P_L1_SHAPE, 4, P_L1_RATE, 6, P_L1_SYNC, 1, MODSLOT(0, S_LFO1, D_APOS, 80), MODSLOT(1, S_LFO1, D_CUT, 30),
    MODSLOT(2, S_M1, D_AWARP, 80), P_DLY_MIX, 40, P_DLY_TIME, 7, P_REV_MIX, 40, P_VOL, 123, NP};
static const uint8_t FS_REED[] = {
    P_A_WAVE, T_REED, P_A_POS, 20, P_A_LVL, 110, P_NZ_LVL, 14, P_NZ_TYPE, 1,
    P_F_TYPE, FT_LP12, P_F_CUT, 84, P_F_KEY, 60, P_E1_A, 34, P_E1_R, 40, P_L1_RATE, 80, P_L1_FADE, 80,
    MODSLOT(0, S_AT, D_APOS, 80), MODSLOT(1, S_LFO1, D_FINE, 14), MODSLOT(2, S_VEL, D_APOS, 40),
    MODSLOT(3, S_AT, D_AMP, 30), P_MODE, MODE_LEGATO, P_GLIDE, 20, P_REV_MIX, 50, P_VOL, 72, NP};
static const uint8_t FS_SUBBASS[] = {
    P_A_WAVE, T_BASIC, P_A_POS, 10, P_A_LVL, 60, P_A_OCT, 2, P_SUB_LVL, 110, P_SUB_OCT, 1, P_SUB_DIR, 1,
    P_F_TYPE, FT_LP12, P_F_CUT, 60, P_F_DRV, 40, P_E1_R, 30, P_MODE, MODE_MONO, P_GLIDE, 24,
    MODSLOT(0, S_M1, D_APOS, 100), MODSLOT(1, S_M2, D_CUT, 50), P_VOL, 62, NP};
static const uint8_t FS_METAL[] = {
    P_A_WAVE, T_METAL, P_A_POS, 20, P_A_LVL, 100, P_A_WMODE, W_MIRROR, P_A_WARP, 20,
    P_B_WAVE, T_COMB, P_B_POS, 50, P_B_LVL, 60, P_B_SEMI, 19,
    P_F_TYPE, FT_HP, P_F_CUT, 40, P_F_RES, 30, P_E1_D, 84, P_E1_S, 20, P_E1_R, 80, P_E2_D, 90, P_E2_S, 0,
    MODSLOT(0, S_ENV2, D_APOS, 80), MODSLOT(1, S_ENV2, D_AWARP, 40), MODSLOT(2, S_KEY, D_BPOS, 60),
    MODSLOT(3, S_M1, D_AWARP, 90), P_VSPREAD, 80, P_DLY_MIX, 40, P_REV_MIX, 70, P_REV_DECAY, 108, P_VOL, 98, NP};
static const uint8_t FS_SWEEP[] = {                   /* B bends while A folds: a slow moving drone pad */
    P_A_WAVE, T_FOLD, P_A_POS, 0, P_A_LVL, 90, P_A_UNI, 2, P_A_DET, 14, P_A_SPR, 110,
    P_B_WAVE, T_COMB, P_B_POS, 0, P_B_LVL, 80, P_B_WMODE, W_BENDP, P_B_WARP, 0, P_B_OCT, 2,
    P_F_TYPE, FT_LP12, P_F_CUT, 76, P_F_RES, 40, P_F_KEY, 30, P_E1_A, 96, P_E1_R, 94,
    P_L1_SHAPE, 1, P_L1_RATE, 14, P_L2_SHAPE, 0, P_L2_RATE, 22, P_L3_SHAPE, 5, P_L3_RATE, 30,
    MODSLOT(0, S_LFO1, D_APOS, 100), MODSLOT(1, S_LFO2, D_BWARP, 90), MODSLOT(2, S_LFO3, D_CUT, 30),
    MODSLOT(3, S_LFO2, D_BPOS, 60), MODSLOT(4, S_M1, D_CUT, 60), MODSLOT(5, S_RND, D_L1RATE, 40),
    P_M1, 50, P_VSPREAD, 100, P_CHO_MIX, 40, P_DLY_MIX, 40, P_DLY_TIME, 2, P_DLY_FB, 50,
    P_REV_MIX, 90, P_REV_DECAY, 108, P_VOL, 112, NP};
static const uint8_t FS_STAB[] = {
    P_A_WAVE, T_SAW, P_A_POS, 70, P_A_LVL, 100, P_A_UNI, 2, P_A_DET, 30, P_A_SPR, 100,
    P_B_WAVE, T_SAW, P_B_POS, 70, P_B_LVL, 80, P_B_SEMI, 19, P_B_UNI, 2, P_B_DET, 24, P_B_SPR, 100,
    P_F_TYPE, FT_LP24, P_F_CUT, 62, P_F_RES, 30, P_F_ENV, 100, P_F_VEL, 40,
    P_E1_D, 70, P_E1_S, 0, P_E1_R, 56, P_E2_D, 50, P_E2_S, 0, MODSLOT(0, S_M1, D_CUT, 60),
    P_DLY_MIX, 50, P_DLY_TIME, 4, P_DLY_FB, 64, P_REV_MIX, 50, P_CHO_MIX, 30, P_VOL, 114, NP};
static const uint8_t FS_NOISE[] = {                   /* tuned noise: it follows the keys */
    P_NZ_LVL, 110, P_NZ_TYPE, 3, P_A_WAVE, T_BITS, P_A_POS, 100, P_A_LVL, 50,
    P_F_TYPE, FT_BP, P_F_CUT, 70, P_F_RES, 70, P_F_KEY, 100, P_E1_D, 70, P_E1_S, 30, P_E1_R, 60, P_E2_D, 60, P_E2_S, 0,
    P_F_ENV, 90, MODSLOT(0, S_M1, D_CUT, 60), MODSLOT(1, S_RND, D_PAN, 80), MODSLOT(2, S_VEL, D_NOISE, 40),
    P_DLY_MIX, 50, P_DLY_TIME, 9, P_DLY_FB, 70, P_REV_MIX, 50, P_VOL, 77, NP};
static const uint8_t FS_PWMPAD[] = {
    P_A_WAVE, T_PWM, P_A_POS, 30, P_A_LVL, 100, P_B_WAVE, T_PWM, P_B_POS, 60, P_B_LVL, 100, P_B_FINE, 58,
    P_F_TYPE, FT_LP12, P_F_CUT, 80, P_F_KEY, 40, P_E1_A, 70, P_E1_R, 80,
    P_L1_SHAPE, 1, P_L1_RATE, 44, P_L2_SHAPE, 1, P_L2_RATE, 52,
    MODSLOT(0, S_LFO1, D_APOS, 60), MODSLOT(1, S_LFO2, D_BPOS, -60), MODSLOT(2, S_M1, D_CUT, 50),
    MODSLOT(3, S_AT, D_CUT, 30), P_M1, 60, P_VSPREAD, 60, P_CHO_MIX, 60, P_REV_MIX, 60, P_VOL, 80, NP};
static const uint8_t FS_TALK[] = {                    /* a formant bass that talks with the wheel or the key's pressure */
    P_A_WAVE, T_FORMANT, P_A_POS, 0, P_A_LVL, 110, P_A_OCT, 2, P_A_WMODE, W_BENDM, P_A_WARP, 20,
    P_SUB_LVL, 70, P_F_TYPE, FT_LP24, P_F_CUT, 80, P_F_RES, 30, P_F_DRV, 60, P_E1_R, 30,
    P_L1_RATE, 4, P_L1_SYNC, 1, P_L1_SHAPE, 1, MODSLOT(0, S_LFO1, D_APOS, 90), MODSLOT(1, S_WHEEL, D_APOS, 100),
    MODSLOT(2, S_AT, D_AWARP, 60), MODSLOT(3, S_M1, D_L1RATE, 80), P_MODE, MODE_MONO, P_GLIDE, 30, P_DRV_AMT, 40, P_VOL, 40, NP};

static const factory_t FACTORY[] = {
    {"INIT", FS_INIT}, {"SUPER STACK", FS_SUPER}, {"GLASS PAD", FS_GLASSPAD}, {"ACID LINE", FS_ACID},
    {"VELO PLUCK", FS_PLUCK}, {"PRESS CHOIR", FS_VOX}, {"FM BASS", FS_FMBASS}, {"COLD BELLS", FS_BELLS},
    {"SYNC LEAD", FS_SYNCLEAD}, {"DRAWBARS", FS_ORGAN}, {"TINE KEYS", FS_KEYS}, {"STRINGS", FS_STRINGS},
    {"BRASS", FS_BRASS}, {"ARP ECHO", FS_ARPSEQ}, {"STEP DIGI", FS_DIGI}, {"REED SOLO", FS_REED},
    {"SUB BASS", FS_SUBBASS}, {"METAL RAIN", FS_METAL}, {"SLOW DRONE", FS_SWEEP}, {"CHORD STAB", FS_STAB},
    {"TUNED NOISE", FS_NOISE}, {"PWM PAD", FS_PWMPAD}, {"TALK BASS", FS_TALK},
};
#define NFACTORY (sizeof FACTORY / sizeof FACTORY[0])
#define NUSER 32u

static void patch_init(patch_t *q, const factory_t *f)
{
    uint32_t i;
    const uint8_t *s = f->set;
    for (i = 0; i < NP; i++)
        q->p[i] = PDEF[i].def;
    for (; s[0] < NP; s += 2)
        q->p[s[0]] = s[1] > PDEF[s[0]].max ? PDEF[s[0]].max : s[1];
    for (i = 0; i < NAME_LEN; i++)
        q->name[i] = 0;
    for (i = 0; i < NAME_LEN - 1u && f->name[i]; i++)
        q->name[i] = f->name[i];
}

#endif
