/* Host test of the UP-Menu check state (map/mui_aa.c): Checkit, Toggle, Exclude; the UP-List row tone of a disabled item; the Amiga+key shortcut lookup; the UP-Menustrip hold-drag-release state. */
#include <stdint.h>
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
    /* UPA_List_Disabled / UPA_Menu_Disabled: a disabled row wins over selection and hover; rows past bit 31 and -1 never are */
    CHECK(mui_aa_list_row_state(0x4, 2, 1, 2) == MUI_AA_DISABLED);
    CHECK(mui_aa_list_row_state(0x4, 1, 1, -1) == MUI_AA_SELECTED);
    CHECK(mui_aa_list_row_state(0x4, 3, 0, 3) == MUI_AA_HOVER);
    CHECK(mui_aa_list_row_state(0x4, 0, 0, 3) == MUI_AA_NORMAL);
    CHECK(mui_aa_list_row_state(0x80000000UL, 31, 0, -1) == MUI_AA_DISABLED);
    CHECK(mui_aa_list_row_state(0xFFFFFFFFUL, 32, 0, -1) == MUI_AA_NORMAL);
    CHECK(mui_aa_list_row_state(0xFFFFFFFFUL, -1, 0, 5) == MUI_AA_NORMAL);
    {
        uint32_t tok[SEL_GUI_TOKEN_COUNT], face = 0, text = 0;
        int k;
        for (k = 0; k < SEL_GUI_TOKEN_COUNT; k++) {
            tok[k] = (uint32_t)k + 1;
        }
        mui_aa_list_row_rgb(tok, MUI_AA_DISABLED, 1, &face, &text); /* grey text even on the playing row, the resting face */
        CHECK(text == tok[SEL_GUI_TOKEN_TEXT_DISABLED] && face == tok[SEL_GUI_TOKEN_SURFACE]);
    }
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
    {
        int cur = -2;
        CHECK(mui_aa_strip_track(&cur, 0, 0) == MUI_AA_STRIP_NONE && cur == -2); /* not held: moves and releases do nothing */
        CHECK(mui_aa_strip_track(&cur, -1, 1) == MUI_AA_STRIP_NONE);
        cur = -1;                                                                   /* the right button opened the bar */
        CHECK(mui_aa_strip_track(&cur, -1, 0) == MUI_AA_STRIP_NONE && cur == -1);
        CHECK(mui_aa_strip_track(&cur, -1, 1) == MUI_AA_STRIP_KEEP && cur == -2);   /* press and release in place: click style */
        cur = -1;
        CHECK(mui_aa_strip_track(&cur, 1, 0) == MUI_AA_STRIP_OPEN && cur == 1);     /* onto a title opens it */
        CHECK(mui_aa_strip_track(&cur, 1, 0) == MUI_AA_STRIP_NONE && cur == 1);
        CHECK(mui_aa_strip_track(&cur, -1, 0) == MUI_AA_STRIP_NONE && cur == 1);    /* off the titles (into the items) keeps it */
        CHECK(mui_aa_strip_track(&cur, 2, 0) == MUI_AA_STRIP_OPEN && cur == 2);     /* across titles switches */
        CHECK(mui_aa_strip_track(&cur, -1, 1) == MUI_AA_STRIP_PICK && cur == -2);   /* release after a drag: pick or close */
        CHECK(mui_aa_strip_track(&cur, 0, 0) == MUI_AA_STRIP_NONE && cur == -2);
    }
    /* item Family methods: Checked / Disabled bits follow the items */
    CHECK(mui_aa_mask_insert(0x5, 0, 0) == 0xA);                 /* AddHead: all move up */
    CHECK(mui_aa_mask_insert(0x5, 1, 1) == 0xB);                 /* Insert after item 0, new item checked */
    CHECK(mui_aa_mask_insert(0x5, 3, 0) == 0x5);                 /* AddTail of 3 items */
    CHECK(mui_aa_mask_insert(0x80000001UL, 1, 0) == 0x1);        /* bit 31 falls off */
    CHECK(mui_aa_mask_insert(0x5, 32, 1) == 0x5);                /* outside the mask */
    CHECK(mui_aa_mask_remove(0xB, 1) == 0x5);
    CHECK(mui_aa_mask_remove(0xA, 0) == 0x5);
    CHECK(mui_aa_mask_remove(0xFFFFFFFFUL, 31) == 0x7FFFFFFFUL);
    CHECK(mui_aa_mask_remove(0x80000000UL, 0) == 0x40000000UL);
    CHECK(mui_aa_mask_remove(0x5, -1) == 0x5);
    /* keyboard: Right Amiga opens the first enabled menu, Left / Right wrap and skip disabled menus */
    CHECK(mui_aa_strip_step(-1, 1, 4, 0) == 0);
    CHECK(mui_aa_strip_step(-1, 1, 4, 0x1) == 1);
    CHECK(mui_aa_strip_step(3, 1, 4, 0) == 0);
    CHECK(mui_aa_strip_step(0, -1, 4, 0) == 3);
    CHECK(mui_aa_strip_step(0, -1, 4, 0x8) == 2);
    CHECK(mui_aa_strip_step(1, 1, 4, 0xD) == 1);                 /* only itself enabled: stays */
    CHECK(mui_aa_strip_step(-1, 1, 3, 0x7) == -1);               /* all disabled */
    CHECK(mui_aa_strip_step(-1, 1, 0, 0) == -1);
    if (fails == 0) {
        printf("mui_menu_check_test OK\n");
    }
    return fails != 0;
}
