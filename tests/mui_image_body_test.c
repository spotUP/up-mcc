/* Host test of the UP-Image body unpack (map/mui_aa.c): raw and ByteRun1 rows, the mask plane, bad input. */
#include <stdio.h>
#include <string.h>
#include "map/mui_aa.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    unsigned char p0[2 * 4], p1[2 * 4], m[2 * 4];
    unsigned char* pl[2] = {p0, p1};
    /* 16 x 2, depth 2, raw: row 0 plane 0, plane 1, row 1 plane 0, plane 1 (2 bytes each) */
    static const unsigned char raw[] = {1, 2, 3, 4, 5, 6, 7, 8};
    /* same picture ByteRun1 with a mask plane: literal runs, a repeat run, and a -128 no-op */
    static const unsigned char rle[] = {0x01, 1, 2, 0xFF, 9, 0x01, 3, 4, 0x80, 0xFF, 0xAA, 0x01, 5, 6, 0xFF, 7, 0x01, 0x0F, 0xF0};
    memset(p0, 0, sizeof p0), memset(p1, 0, sizeof p1);
    CHECK(mui_aa_body_unpack(raw, 0, 16, 2, 2, 0, pl, 4, NULL, 0) == 1);
    CHECK(p0[0] == 1 && p0[1] == 2 && p1[0] == 3 && p1[1] == 4 && p0[4] == 5 && p0[5] == 6 && p1[4] == 7 && p1[5] == 8);
    memset(p0, 0, sizeof p0), memset(p1, 0, sizeof p1), memset(m, 0, sizeof m);
    CHECK(mui_aa_body_unpack(rle, 1, 16, 2, 1, 1, pl, 4, m, 4) == 1); /* depth 1 + mask: plane, mask, plane, mask */
    CHECK(p0[0] == 1 && p0[1] == 2 && m[0] == 9 && m[1] == 9);
    CHECK(p0[4] == 3 && p0[5] == 4 && m[4] == 0xAA && m[5] == 0xAA);
    memset(p0, 0, sizeof p0);
    CHECK(mui_aa_body_unpack(rle, 1, 16, 1, 1, 1, pl, 4, NULL, 0) == 1); /* mask skipped */
    CHECK(p0[0] == 1 && p0[1] == 2);
    CHECK(mui_aa_body_unpack(raw, 2, 16, 2, 1, 0, pl, 4, NULL, 0) == 0); /* unknown compression */
    CHECK(mui_aa_body_unpack(raw, 0, 40, 2, 1, 0, pl, 4, NULL, 0) == 0); /* row wider than the plane */
    CHECK(mui_aa_body_unpack(NULL, 0, 16, 2, 1, 0, pl, 4, NULL, 0) == 0);
    if (fails == 0) {
        printf("mui_image_body_test OK\n");
    }
    return fails != 0;
}
