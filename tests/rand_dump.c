/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Hugo Gilbert */
/* The dice (fizzyfm/ap_rand.h) on a computer, for tests/run_tests.py "random": rand_dump N KIND CHAOS [m]
 * throws seeds 1..N and writes each sound as a line "kind hex-of-the-144-bytes"; m: each one mutated as well, on
 * the next line. It stops with an error if a parameter is out of its range, a name is not a name, or the same seed
 * gives two sounds. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../fizzyfm/ap_gen.h"
#include "../fizzyfm/ap_def.h"
#include "../fizzyfm/ap_rand.h"

static void check(const patch_t *q, uint32_t seed, const char *what)
{
    uint32_t i, n = 0;
    for (i = 0; i < NP; i++)
        if (q->p[i] > PDEF[i].max) {
            fprintf(stderr, "seed %u (%s): parameter %u is %u, over %u\n", seed, what, i, q->p[i], PDEF[i].max);
            exit(2);
        }
    while (n < NAME_LEN && q->name[n])
        n++;
    for (i = 0; i < n; i++)
        if (q->name[i] < 32 || q->name[i] > 95)
            n = 0;
    if (!n || n >= NAME_LEN) {
        fprintf(stderr, "seed %u (%s): the name\n", seed, what);
        exit(2);
    }
}
static void put(uint32_t kind, const patch_t *q)
{
    uint32_t i;
    printf("%u ", kind);
    for (i = 0; i < sizeof *q; i++)
        printf("%02x", ((const uint8_t *)q)[i]);
    printf("\n");
}
int main(int argc, char **argv)
{
    uint32_t n = argc > 1 ? (uint32_t)atoi(argv[1]) : 10, kind = argc > 2 ? (uint32_t)atoi(argv[2]) : 0;
    uint32_t chaos = argc > 3 ? (uint32_t)atoi(argv[3]) : 48, mut = argc > 4, seed, i;
    patch_t from, a, b;
    memset(&from, 0, sizeof from);
    for (i = 0; i < NP; i++)
        from.p[i] = PDEF[i].def;
    for (seed = 1; seed <= n; seed++) {
        uint32_t k;
        memset(&a, 0xEE, sizeof a);
        memset(&b, 0x11, sizeof b);
        k = rand_patch(&a, &from, seed, kind, chaos);
        if (rand_patch(&b, &from, seed, kind, chaos) != k || memcmp(&a, &b, sizeof a)) {
            fprintf(stderr, "seed %u: two sounds from one seed\n", seed);
            return 2;
        }
        check(&a, seed, "thrown");
        put(k, &a);
        if (mut) {
            rand_mutate(&a, seed + 7777u, chaos);
            check(&a, seed, "mutated");
            if (memcmp(a.name, b.name, NAME_LEN)) {
                fprintf(stderr, "seed %u: a mutation changed the name\n", seed);
                return 2;
            }
            put(k, &a);
        }
    }
    return 0;
}
