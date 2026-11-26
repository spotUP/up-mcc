/* Host test of the UP-Menu check state (map/mui_aa.c): Checkit, Toggle, Exclude; and the UP-List row tone of a disabled item. */
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
    if (fails == 0) {
        printf("mui_menu_check_test OK\n");
    }
    return fails != 0;
}
