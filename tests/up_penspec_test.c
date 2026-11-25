/* Host test of the MUI pen specification text (map/up_penspec.c): parse, format, round trip, gun conversion. */
#include <stdio.h>
#include <string.h>
#include "map/up_penspec.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    up_penspec_t p;
    char buf[UP_PENSPEC_LEN];
    CHECK(up_penspec_parse("rffffffff,80808080,00000000", &p) == UP_PEN_RGB && p.r == 0xFFFFFFFFu && p.g == 0x80808080u && p.b == 0);
    CHECK(up_penspec_rgb24(p.r, p.g, p.b) == 0xFF8000u);
    CHECK(up_penspec_parse("rABCDEF01,00000000,12345678", &p) == UP_PEN_RGB && p.r == 0xABCDEF01u && p.b == 0x12345678u);
    CHECK(up_penspec_parse("p12", &p) == UP_PEN_CMAP && p.n == 12);
    CHECK(up_penspec_parse("m5", &p) == UP_PEN_MUI && p.n == 5);
    CHECK(up_penspec_parse("rffffffff,8080", &p) == UP_PEN_NONE && p.r == 0); /* truncated RGB */
    CHECK(up_penspec_parse("p", &p) == UP_PEN_NONE);
    CHECK(up_penspec_parse("x3", &p) == UP_PEN_NONE);
    CHECK(up_penspec_parse("", &p) == UP_PEN_NONE);
    CHECK(up_penspec_parse(NULL, &p) == UP_PEN_NONE);

    p.kind = UP_PEN_RGB, p.r = up_penspec_gun32(0xFF), p.g = up_penspec_gun32(0x80), p.b = 0;
    up_penspec_format(&p, buf);
    CHECK(strcmp(buf, "rffffffff,80808080,00000000") == 0 && strlen(buf) < UP_PENSPEC_LEN);
    p.kind = UP_PEN_CMAP, p.n = 255;
    up_penspec_format(&p, buf);
    CHECK(strcmp(buf, "p255") == 0);
    p.kind = UP_PEN_MUI, p.n = 0;
    up_penspec_format(&p, buf);
    CHECK(strcmp(buf, "m0") == 0);
    p.kind = UP_PEN_NONE;
    up_penspec_format(&p, buf);
    CHECK(buf[0] == 0);
    CHECK(up_penspec_parse("p255", &p) == UP_PEN_CMAP && p.n == 255); /* round trip of the formatted text */
    CHECK(up_penspec_gun32(0xAB) == 0xABABABABu);

    if (fails == 0) {
        printf("up_penspec_test: all passed\n");
    }
    return fails != 0;
}
