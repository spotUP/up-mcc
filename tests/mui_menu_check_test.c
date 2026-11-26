/* Host test of the UP-Menu check state (map/mui_aa.c): Checkit, Toggle, Exclude; and of the Amiga+key shortcut lookup. */
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
    {
        static const UP_MenuItem ent[] = {{"Open", "o", 0, 0}, {"Save", "S", 0, 0}, {"Help", "F1", 0, 0}, {"Quit", "Q", UPV_MenuItem_CommandString, 0},
                                          {"Wrap", "W", 0, 0}, {"-", NULL, 0, 0}, {"Zoom", "\xE4", 0, 0}, {NULL, NULL, 0, 0}};
        CHECK(mui_aa_menu_shortcut(ent, 7, 0, 'O') == 0);        /* case-blind both ways */
        CHECK(mui_aa_menu_shortcut(ent, 7, 0, 's') == 1);
        CHECK(mui_aa_menu_shortcut(ent, 7, 0, 'F') == -1);       /* a longer shortcut text is no key */
        CHECK(mui_aa_menu_shortcut(ent, 7, 0, 'q') == -1);       /* CommandString is shown only */
        CHECK(mui_aa_menu_shortcut(ent, 7, 1UL << 4, 'w') == -1); /* a disabled item has no shortcut */
        CHECK(mui_aa_menu_shortcut(ent, 7, 0, 'w') == 4);
        CHECK(mui_aa_menu_shortcut(ent, 7, 0, 0xC4) == 6);       /* Latin-1 upper case */
        CHECK(mui_aa_menu_shortcut(ent, 4, 0, 'w') == -1);       /* beyond the count */
        CHECK(mui_aa_menu_shortcut(NULL, 7, 0, 'o') == -1);      /* a menu without Entries (UP-Menustrip asks every menu) */
    }
    if (fails == 0) {
        printf("mui_menu_check_test OK\n");
    }
    return fails != 0;
}
