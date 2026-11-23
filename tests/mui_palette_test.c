/* Host test of the UP-Palette grid maths (map/mui_aa.c): cell origin, hit test, cursor steps. */
#include <stdio.h>
#include "map/mui_aa.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    int x, y;
    CHECK(mui_aa_palette_rows(16, 4) == 4 && mui_aa_palette_rows(17, 4) == 5 && mui_aa_palette_rows(0, 4) == 0);

    mui_aa_palette_cell(6, 4, 10, 8, 2, &x, &y);
    CHECK(x == 24 && y == 10);

    /* hit: inside a cell, on a gap, past the last cell, outside */
    CHECK(mui_aa_palette_hit(24, 10, 4, 16, 10, 8, 2) == 6);
    CHECK(mui_aa_palette_hit(33, 17, 4, 16, 10, 8, 2) == 6);
    CHECK(mui_aa_palette_hit(34, 10, 4, 16, 10, 8, 2) == -1); /* horizontal gap */
    CHECK(mui_aa_palette_hit(24, 18, 4, 16, 10, 8, 2) == -1); /* vertical gap */
    CHECK(mui_aa_palette_hit(48, 0, 4, 16, 10, 8, 2) == -1);  /* past the last column */
    CHECK(mui_aa_palette_hit(12, 36, 4, 6, 10, 8, 2) == -1);  /* row 3 col 1 = cell 13 of 6 */
    CHECK(mui_aa_palette_hit(-1, 0, 4, 16, 10, 8, 2) == -1);

    /* keys */
    CHECK(mui_aa_palette_move(5, 10, 4, 1, 0) == 6);
    CHECK(mui_aa_palette_move(4, 10, 4, -1, 0) == 4);  /* no wrap left */
    CHECK(mui_aa_palette_move(3, 10, 4, 1, 0) == 3);   /* no wrap right */
    CHECK(mui_aa_palette_move(1, 10, 4, 0, 1) == 5);
    CHECK(mui_aa_palette_move(1, 10, 4, 0, -1) == 1);  /* top row stays */
    CHECK(mui_aa_palette_move(7, 10, 4, 0, 1) == 9);   /* short last row: land on last cell */
    CHECK(mui_aa_palette_move(9, 10, 4, 0, 1) == 9);   /* off the bottom stays */
    CHECK(mui_aa_palette_move(9, 10, 4, 1, 0) == 9);   /* column 1 of last row exists: 9 is col 1, col 2 does not -> clamp */
    CHECK(mui_aa_palette_move(-1, 10, 4, 1, 0) == -1); /* no active cell */

    if (fails == 0) {
        printf("mui_palette_test: all passed\n");
    }
    return fails != 0;
}
