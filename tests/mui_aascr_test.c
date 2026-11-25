/* Host test of the UP-Virtgroup and UP-Scrollgroup geometry (src/map/mui_aascr.c). */
#include "map/mui_aascr.h"

#include <stdio.h>

#define CHECK(c)                                                 \
    do {                                                         \
        if (!(c)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            return 1;                                            \
        }                                                        \
    } while (0)

int main(void) {
    int w, h, need, vw;
    CHECK(mui_aa_scr_clamp(-5, 100, 40) == 0);
    CHECK(mui_aa_scr_clamp(30, 100, 40) == 30);
    CHECK(mui_aa_scr_clamp(99, 100, 40) == 60);
    CHECK(mui_aa_scr_clamp(10, 30, 40) == 0); /* fits: never scrolls */
    CHECK(mui_aa_scr_need(100, 40) == 1);
    CHECK(mui_aa_scr_need(40, 40) == 0);
    mui_aa_scr_inner(200, 40, 120, 90, &w, &h);
    CHECK(w == 200 && h == 90);
    mui_aa_scr_inner(100, 100, 120, 90, &w, &h);
    CHECK(w == 120 && h == 100);
    mui_aa_scr_split(200, 40, 90, 12, &need, &vw);
    CHECK(need == 1 && vw == 188);
    mui_aa_scr_split(200, 100, 90, 12, &need, &vw);
    CHECK(need == 0 && vw == 200);
    mui_aa_scr_split(10, 40, 90, 12, &need, &vw); /* no room for a bar */
    CHECK(need == 0 && vw == 10);
    CHECK(mui_aascr_calls() > 0);
    return 0;
}
