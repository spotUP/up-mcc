/* MUI pen specifications (struct MUI_PenSpec text) for UP-Pendisplay, UP-Poppen and UP-Penadjust. Pure code, host tested
 * (tests/up_penspec_test.c). Spellings: "rRRRRRRRR,GGGGGGGG,BBBBBBBB" (32 bit hex per gun), "pN" colormap pen N, "mN" MUI pen N (MPEN_*). */
#ifndef UP_PENSPEC_H
#define UP_PENSPEC_H

#include <stdint.h>

enum { UP_PEN_NONE, UP_PEN_RGB, UP_PEN_CMAP, UP_PEN_MUI };
#define UP_PENSPEC_LEN 32 /* sizeof(struct MUI_PenSpec) */

typedef struct up_penspec_t {
    int kind;          /* UP_PEN_* */
    uint32_t r, g, b;  /* UP_PEN_RGB: 32 bit guns */
    long n;            /* UP_PEN_CMAP / UP_PEN_MUI: the pen */
} up_penspec_t;

int up_penspec_parse(const char* s, up_penspec_t* out);    /* kind; UP_PEN_NONE (out zeroed) when s is NULL or not a spec */
void up_penspec_format(const up_penspec_t* p, char* buf); /* buf of UP_PENSPEC_LEN; UP_PEN_NONE gives "" */
uint32_t up_penspec_rgb24(uint32_t r, uint32_t g, uint32_t b); /* 32 bit guns to 0xRRGGBB */
uint32_t up_penspec_gun32(uint32_t v8);                        /* 8 bit gun to 32 bit (0xAB -> 0xABABABAB) */

#endif
