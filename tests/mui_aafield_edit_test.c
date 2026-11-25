/* UP-Field own edit text (2026-11-25): the single-line caret x, horizontal scroll and click-to-position maths the focused field
 * draws with (mui_text_lines.c mtl_x_of, mtl_scroll_x, mtl_visible_cols, mtl_offset_at), measured in a proportional font so it
 * cannot pass on fixed topaz widths by accident. */
#include <stdio.h>
#include <string.h>

#include "mui_text_lines.h"

#define CHECK(c)                                                 \
    do {                                                         \
        if (!(c)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            return 1;                                            \
        }                                                        \
    } while (0)

/* 'i' is 3 px, 'W' 12 px, every other byte 7 px */
static int prop(void* ctx, const char* s, int len) {
    int w = 0, k;
    (void)ctx;
    for (k = 0; k < len; k++) {
        w += s[k] == 'i' ? 3 : s[k] == 'W' ? 12 : 7;
    }
    return w;
}

int main(void) {
    const char* t = "WiWiabcdefgh"; /* x of each column: 0 12 15 27 30 37 44 51 58 65 72 79 86 */
    MtlLine line;
    int n = (int)strlen(t), tw, c0, c1, s;
    line.start = 0;
    line.len = n;
    tw = prop(NULL, t, n);
    CHECK(tw == 86);
    /* caret x comes from the AA widths, not col * 8 */
    CHECK(mtl_x_of(t, &line, 0, 3, prop, NULL) == 27);
    CHECK(mtl_x_of(t, &line, 0, 0, prop, NULL) == 0);
    /* scroll: a caret inside the view keeps it; past the right edge scrolls just enough; left of it scrolls back */
    CHECK(mtl_scroll_x(27, 1, 40, 0, tw) == 0);
    CHECK(mtl_scroll_x(86, 1, 40, 0, tw) == 47);  /* caret at the end, its 1 px inside the 40 px view */
    CHECK(mtl_scroll_x(12, 1, 40, 47, tw) == 12); /* Home-ward move */
    CHECK(mtl_scroll_x(0, 1, 40, 30, tw) == 0);
    /* text shorter than the view never scrolls; a shrunk text (delete) pulls the scroll back to its end */
    CHECK(mtl_scroll_x(30, 1, 200, 25, tw) == 0);
    CHECK(mtl_scroll_x(30, 1, 40, 47, 37) == 0);
    /* visible bytes: whole ones only */
    mtl_visible_cols(t, n, 0, 40, prop, NULL, &c0, &c1);
    CHECK(c0 == 0 && c1 == 5); /* "WiWia" ends at 37 */
    mtl_visible_cols(t, n, 13, 40, prop, NULL, &c0, &c1);
    CHECK(c0 == 2 && c1 == 7); /* starts at x 15, ends at 51 <= 53 */
    mtl_visible_cols(t, n, 47, 40, prop, NULL, &c0, &c1);
    CHECK(c0 == 7 && c1 == 12); /* byte 6 starts at 44 < 47: cut on the left, not drawn */
    mtl_visible_cols("", 0, 0, 40, prop, NULL, &c0, &c1);
    CHECK(c0 == 0 && c1 == 0);
    /* click: view x plus scroll, nearest caret edge in AA widths (topaz 8 px cells would give column 2 for x 16) */
    CHECK(mtl_offset_at(t, n, &line, 1, 0, 16 + 0, prop, NULL) == 2);
    CHECK(mtl_offset_at(t, n, &line, 1, 0, 20 + 0, prop, NULL) == 2); /* 20 is nearer 15 than 27 */
    CHECK(mtl_offset_at(t, n, &line, 1, 0, 22 + 0, prop, NULL) == 3);
    s = 47;
    CHECK(mtl_offset_at(t, n, &line, 1, 0, 5 + s, prop, NULL) == 7);  /* x 52 nearest 51 */
    CHECK(mtl_offset_at(t, n, &line, 1, 0, 39 + s, prop, NULL) == 12); /* past the end */
    CHECK(mtl_offset_at(t, n, &line, 1, 0, -5, prop, NULL) == 0);
    /* double click: the word (alnum, '_', 8-bit), the space run or the punctuation run under the offset */
    {
        const char* w = "ab_1  ,.x";
        int lo, hi;
        mtl_word_range(w, 9, 1, &lo, &hi);
        CHECK(lo == 0 && hi == 4);
        mtl_word_range(w, 9, 4, &lo, &hi); /* the space run */
        CHECK(lo == 4 && hi == 6);
        mtl_word_range(w, 9, 6, &lo, &hi); /* ",." */
        CHECK(lo == 6 && hi == 8);
        mtl_word_range(w, 9, 9, &lo, &hi); /* off == n: the word before the end */
        CHECK(lo == 8 && hi == 9);
        mtl_word_range(w, 9, -3, &lo, &hi); /* clamped to 0 */
        CHECK(lo == 0 && hi == 4);
        mtl_word_range(w, 9, 99, &lo, &hi); /* clamped to n */
        CHECK(lo == 8 && hi == 9);
        mtl_word_range("", 0, 0, &lo, &hi);
        CHECK(lo == 0 && hi == 0);
        mtl_word_range("2026-11-25", 10, 6, &lo, &hi); /* release date: one part */
        CHECK(lo == 5 && hi == 7);
        /* selection: ordered, clamped to the text, empty when anchor == caret */
        CHECK(mtl_sel_range(7, 2, 9, &lo, &hi) && lo == 2 && hi == 7);
        CHECK(mtl_sel_range(-4, 30, 9, &lo, &hi) && lo == 0 && hi == 9);
        CHECK(!mtl_sel_range(3, 3, 9, &lo, &hi));
    }
    puts("mui_aafield_edit_test: ok");
    return 0;
}
