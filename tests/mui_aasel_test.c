/* Host test of the UP-Checkmark and UP-Radio plans (src/map/mui_aasel.c). */
#include "map/mui_aasel.h"

#include <stdio.h>

#define CHECK(c)                                                 \
    do {                                                         \
        if (!(c)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            return 1;                                            \
        }                                                        \
    } while (0)

int main(void) {
    uint32_t tok[SEL_GUI_TOKEN_COUNT], tick;
    up_metrics_t m;
    mui_aa_check_op_t ops[MUI_AASEL_OPS];
    mui_aa_tick_t t[64];
    int n, i, size, prev;
    for (i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        tok[i] = 0x102030u + (uint32_t)i * 0x010101u;
    }
    up_metrics(13, &m);
    /* checkmark: unselected is the AACheck outline plan, selected is one accent box */
    n = mui_aa_checkmark_plan(tok, &m, 0, MUI_AA_NORMAL, ops, &tick);
    CHECK(n == mui_aa_check_plan(tok, &m, MUI_AA_NORMAL, ops));
    n = mui_aa_checkmark_plan(tok, &m, 1, MUI_AA_NORMAL, ops, &tick);
    CHECK(n == 2 && ops[1].rgb == tok[SEL_GUI_TOKEN_ACCENT] && tick == tok[SEL_GUI_TOKEN_ACCENT_TEXT]);
    mui_aa_checkmark_plan(tok, &m, 1, MUI_AA_HOVER, ops, &tick);
    CHECK(ops[1].rgb != tok[SEL_GUI_TOKEN_ACCENT]);
    mui_aa_checkmark_plan(tok, &m, 1, MUI_AA_DISABLED, ops, &tick);
    CHECK(ops[1].rgb == tok[SEL_GUI_TOKEN_TEXT_DISABLED] && tick == tok[SEL_GUI_TOKEN_SURFACE]);
    /* tick: inside the box, contiguous columns, for box sizes of both tiers and a tall-pixel row count */
    for (size = 8; size <= 24; size += 4) {
        int h = size == 8 ? 4 : size, thick = size >= 16 ? 2 : 1;
        n = mui_aa_tick_cols(size, h, thick, t);
        CHECK(n > size / 2 && n <= size);
        prev = t[0].x - 1;
        for (i = 0; i < n; i++) {
            CHECK(t[i].x == prev + 1 && t[i].x < size && t[i].y >= 0 && t[i].y + thick <= h);
            prev = t[i].x;
        }
    }
    CHECK(mui_aa_tick_cols(0, 8, 1, t) == 0 && mui_aa_tick_cols(8, 1, 2, t) == 0);
    /* radio: dot only when selected, ring accent when selected, disabled greys the ring */
    n = mui_aa_radio_plan(tok, &m, 0, MUI_AA_NORMAL, ops);
    CHECK(n == 3 && ops[1].rgb == tok[SEL_GUI_TOKEN_TEXT_MUTED] && ops[1].radius == m.check_px / 2);
    n = mui_aa_radio_plan(tok, &m, 1, MUI_AA_NORMAL, ops);
    CHECK(n == 4 && ops[1].rgb == tok[SEL_GUI_TOKEN_ACCENT] && ops[3].rgb == tok[SEL_GUI_TOKEN_ACCENT] && ops[3].inset > ops[2].inset);
    mui_aa_radio_plan(tok, &m, 0, MUI_AA_HOVER, ops);
    CHECK(ops[1].rgb == tok[SEL_GUI_TOKEN_TEXT] && ops[2].rgb == tok[SEL_GUI_TOKEN_SURFACE_HOVER]);
    mui_aa_radio_plan(tok, &m, 1, MUI_AA_DISABLED, ops);
    CHECK(ops[1].rgb == tok[SEL_GUI_TOKEN_TEXT_DISABLED] && ops[3].rgb == tok[SEL_GUI_TOKEN_TEXT_DISABLED]);
    /* hit test */
    CHECK(mui_aa_radio_row_h(14, 16, 4) == 20 && mui_aa_radio_row_h(20, 16, 4) == 24);
    CHECK(mui_aa_radio_hit(0, 20, 3) == 0 && mui_aa_radio_hit(19, 20, 3) == 0 && mui_aa_radio_hit(20, 20, 3) == 1 && mui_aa_radio_hit(59, 20, 3) == 2);
    CHECK(mui_aa_radio_hit(60, 20, 3) == -1 && mui_aa_radio_hit(-1, 20, 3) == -1 && mui_aa_radio_hit(5, 20, 0) == -1);
    CHECK(mui_aasel_calls() > 0);
    return 0;
}
