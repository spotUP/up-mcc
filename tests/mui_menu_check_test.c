/* Host test of the UP-Menu check state (map/mui_aa.c): Checkit, Toggle, Exclude. */
#include <stdio.h>
#include "map/mui_aa.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    CHECK(mui_aa_menu_pick(0, 2, 0, 0, 0) == 0);          /* plain item: no state */
    CHECK(mui_aa_menu_pick(0, 2, 1, 0, 0) == 4);          /* checkit: checked */
    CHECK(mui_aa_menu_pick(4, 2, 1, 0, 0) == 4);          /* checked again without toggle: stays */
    CHECK(mui_aa_menu_pick(4, 2, 1, 1, 0) == 0);          /* toggle unchecks */
    CHECK(mui_aa_menu_pick(0x3, 2, 1, 0, 0x7) == 0x4);    /* radio group 0..2: excludes the others, never itself */
    CHECK(mui_aa_menu_pick(0x8, 2, 1, 0, 0x3) == 0xC);    /* items outside the exclude mask stay */
    CHECK(mui_aa_menu_pick(0, 31, 1, 0, 0) == 0x80000000UL);
    CHECK(mui_aa_menu_pick(0, 32, 1, 0, 0) == 0);         /* beyond the mask: no state */
    if (fails == 0) {
        printf("mui_menu_check_test OK\n");
    }
    return fails != 0;
}
