/* Host test of the UP-List multi-selection bitset and column hit (src/map/mui_aalistsel.c). */
#include "map/mui_aa.h"
#include "map/mui_aalistsel.h"

#include <stdio.h>

#define CHECK(c)                                                 \
    do {                                                         \
        if (!(c)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            return 1;                                            \
        }                                                        \
    } while (0)

int main(void) {
    uint32_t s[UP_SEL_WORDS(70)] = {0}, t[UP_SEL_WORDS(40)];
    up_list_t m = {16, 4, 2, 3, 12, 4};
    int cw[3] = {50, 0, 30};
    CHECK(UP_SEL_WORDS(0) == 0 && UP_SEL_WORDS(32) == 1 && UP_SEL_WORDS(33) == 2);
    CHECK(up_sel_next(s, 70, -1) == -1);
    CHECK(up_sel_op(s, 3, 1) == 1 && up_sel_op(s, 3, 3) == 1);
    CHECK(up_sel_op(s, 33, 2) == 1 && up_sel_op(s, 69, 1) == 1);
    CHECK(up_sel_op(s, 3, 2) == 0 && up_sel_op(s, 3, 3) == 0); /* toggle off */
    CHECK(up_sel_op(s, 3, 1) == 1 && up_sel_op(s, 3, 0) == 0);
    CHECK(up_sel_next(s, 70, -1) == 33 && up_sel_next(s, 70, 33) == 69 && up_sel_next(s, 70, 69) == -1);
    CHECK(up_sel_next(s, 50, 33) == -1); /* bounded by count */
    up_sel_copy(t, 40, s, 70); /* shrink: row 69 dropped, 33 kept */
    CHECK(up_sel_next(t, 40, -1) == 33 && up_sel_next(t, 40, 33) == -1);
    t[1] = 0xFFFFFFFFUL;
    up_sel_copy(t, 40, s, 20); /* rows past the source are cleared */
    CHECK(up_sel_next(t, 40, -1) == -1);
    /* columns: x of col 0 = 4, col 1 absent, col 2 = 4 + 50 + 7 = 61; col 2 starts at its divider (61 - gap) */
    CHECK(mui_aa_list_col_at(&m, 0, 3, cw) == 0 && mui_aa_list_col_at(&m, 57, 3, cw) == 0);
    CHECK(mui_aa_list_col_at(&m, 58, 3, cw) == 2 && mui_aa_list_col_at(&m, 500, 3, cw) == 2);
    CHECK(mui_aa_list_col_at(&m, 500, 1, cw) == 0);
    /* content width: pads 2 * 4, 50 + 30, one divider 7 (col 1 absent takes none); widths of that face give the columns back uncut */
    CHECK(mui_aa_list_content_w(&m, 3, cw) == 95);
    {
        int wt[3] = {0, 1, 0}, fx[3] = {12, 0, 40}, need[3] = {12, 90, 40}, out[3];
        mui_aa_list_col_widths(&m, mui_aa_list_content_w(&m, 3, need), 3, wt, fx, out);
        CHECK(out[0] == 12 && out[1] >= 90 && out[2] == 40);
    }
    return 0;
}
