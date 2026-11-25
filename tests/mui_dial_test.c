/* Host test of the UP-Knob / UP-Levelmeter dial maths (map/mui_aa.c): sine table, value to angle, angle to point, knob keys. */
#include <stdio.h>
#include "map/mui_aa.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    int x, y, used;

    /* sine in every quadrant and wrapped */
    CHECK(mui_aa_dial_sin_q14(0) == 0 && mui_aa_dial_sin_q14(90) == 16384 && mui_aa_dial_sin_q14(30) == 8192);
    CHECK(mui_aa_dial_sin_q14(150) == 8192 && mui_aa_dial_sin_q14(210) == -8192 && mui_aa_dial_sin_q14(330) == -8192);
    CHECK(mui_aa_dial_sin_q14(270) == -16384 && mui_aa_dial_sin_q14(-90) == -16384 && mui_aa_dial_sin_q14(450) == 16384);

    /* knob: lo at 225 (lower left), middle at 90 (top), hi at -45 (lower right); clamped; empty range stays at the start */
    CHECK(mui_aa_dial_deg(0, 0, 100, MUI_AA_KNOB_START, MUI_AA_KNOB_SWEEP) == 225);
    CHECK(mui_aa_dial_deg(50, 0, 100, MUI_AA_KNOB_START, MUI_AA_KNOB_SWEEP) == 90);
    CHECK(mui_aa_dial_deg(100, 0, 100, MUI_AA_KNOB_START, MUI_AA_KNOB_SWEEP) == -45);
    CHECK(mui_aa_dial_deg(500, 0, 100, MUI_AA_KNOB_START, MUI_AA_KNOB_SWEEP) == -45);
    CHECK(mui_aa_dial_deg(-5, -10, 10, MUI_AA_KNOB_START, MUI_AA_KNOB_SWEEP) == 225 - 67); /* 5/20 of 270 = 67.5, truncated */
    CHECK(mui_aa_dial_deg(3, 3, 3, MUI_AA_KNOB_START, MUI_AA_KNOB_SWEEP) == 225);
    /* meter: lo left, hi right */
    CHECK(mui_aa_dial_deg(0, 0, 64, MUI_AA_METER_START, MUI_AA_METER_SWEEP) == 180);
    CHECK(mui_aa_dial_deg(64, 0, 64, MUI_AA_METER_START, MUI_AA_METER_SWEEP) == 0);

    /* points, screen y down */
    mui_aa_dial_point(0, 10, &x, &y);
    CHECK(x == 10 && y == 0);
    mui_aa_dial_point(90, 10, &x, &y);
    CHECK(x == 0 && y == -10);
    mui_aa_dial_point(180, 10, &x, &y);
    CHECK(x == -10 && y == 0);
    mui_aa_dial_point(225, 20, &x, &y);
    CHECK(x == -14 && y == 14);
    mui_aa_dial_point(-45, 20, &x, &y);
    CHECK(x == 14 && y == 14);

    /* keys */
    CHECK(mui_aa_knob_key(5, 0, 100, 0x4E, 0, &used) == 6 && used);
    CHECK(mui_aa_knob_key(5, 0, 100, 0x4C, 0, &used) == 6 && used);
    CHECK(mui_aa_knob_key(5, 0, 100, 0x4F, 0, &used) == 4 && used);
    CHECK(mui_aa_knob_key(5, 0, 100, 0x4D, 1, &used) == 0 && used);   /* a tenth, clamped */
    CHECK(mui_aa_knob_key(95, 0, 100, 0x4E, 1, &used) == 100 && used);
    CHECK(mui_aa_knob_key(2, 0, 5, 0x4E, 1, &used) == 3 && used);     /* tenth of a tiny range is still one */
    CHECK(mui_aa_knob_key(5, 0, 100, 0x40, 0, &used) == 5 && !used);  /* space: not ours */

    printf(fails ? "mui_dial_test: %d FAILED\n" : "mui_dial_test: OK\n", fails);
    return fails != 0;
}
