/* Host test of the UP-Cycle / UP-Radio pure helpers (map/mui_aa.c) with no entries: a bare MUI_NewObject("UP-Cycle.mcc") has n == 0. */
#include <stdio.h>
#include "map/mui_aa.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    /* empty cycle: no modulo by zero, active stays 0, no index is valid */
    CHECK(mui_aa_cycle_step(0, 0, 1) == 0 && mui_aa_cycle_step(0, 0, -1) == 0 && mui_aa_cycle_step(5, -1, 1) == 0);
    CHECK(!mui_aa_cycle_valid(0, 0) && !mui_aa_cycle_valid(-1, 0) && !mui_aa_cycle_valid(0, -1));
    /* normal wrap */
    CHECK(mui_aa_cycle_step(2, 3, 1) == 0 && mui_aa_cycle_step(0, 3, -1) == 2 && mui_aa_cycle_step(1, 1, 1) == 0);
    CHECK(mui_aa_cycle_valid(0, 1) && mui_aa_cycle_valid(2, 3) && !mui_aa_cycle_valid(3, 3));
    /* empty width: min width of an empty cycle is padding + gap + chevron only */
    {
        mui_aa_cycle_t m;
        mui_aa_cycle_metrics(0, 256, &m);
        CHECK(mui_aa_cycle_min_w(&m, 0) == 2 * m.pad_x + m.gap + m.chev_w);
    }
    if (fails == 0) {
        printf("mui_cycle_test: all passed\n");
    }
    return fails != 0;
}
