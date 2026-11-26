/* Host test of the multi-line text helpers (src/mui_text_lines.c). Fixed-width measure: 10 pixels per byte. */
#include "mui_text_lines.h"

#undef NDEBUG /* the asserts hold the test calls; the release flags define NDEBUG */
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int mw(void *ctx, const char *s, int len) { (void)ctx; (void)s; return len * 10; }

static int wrap(const char *t, int width, MtlLine *l) { return mtl_wrap(t, (int)strlen(t), width, mw, 0, l, 16); }

static void check_line(const MtlLine *l, int i, int start, int len) {
    if (l[i].start != start || l[i].len != len) { fprintf(stderr, "line %d: got %d+%d want %d+%d\n", i, l[i].start, l[i].len, start, len); assert(0); }
}

int main(void) {
    MtlLine l[16];
    int n, pref, off, len, i;
    char buf[8];

    /* empty text: one empty line; also count-only mode */
    assert(wrap("", 100, l) == 1); check_line(l, 0, 0, 0);
    assert(mtl_wrap("abc", 3, 100, mw, 0, 0, 0) == 1);

    /* trailing LF gives a final empty line; LF is not in any line */
    n = wrap("ab\n", 100, l); assert(n == 2); check_line(l, 0, 0, 2); check_line(l, 1, 3, 0);
    n = wrap("ab\n\ncd", 100, l); assert(n == 3); check_line(l, 1, 3, 0); check_line(l, 2, 4, 2);

    /* soft wrap keeps the space on the line it ends (width 5 bytes) */
    n = wrap("aaa bbb ccc", 50, l); assert(n == 3);
    check_line(l, 0, 0, 4); check_line(l, 1, 4, 4); check_line(l, 2, 8, 3);
    /* exact fit, then trailing space hangs */
    n = wrap("abcde fg", 50, l); assert(n == 2); check_line(l, 0, 0, 6); check_line(l, 1, 6, 2);

    /* word longer than width hard-breaks; short word before it wraps first */
    n = wrap("abcdefghijkl", 50, l); assert(n == 3);
    check_line(l, 0, 0, 5); check_line(l, 1, 5, 5); check_line(l, 2, 10, 2);
    n = wrap("ab cdefghijk", 50, l); assert(n == 3); check_line(l, 0, 0, 3); check_line(l, 1, 3, 5); check_line(l, 2, 8, 4);
    /* width smaller than one byte still progresses */
    n = wrap("abc", 3, l); assert(n == 3);
    /* count exceeds max: fills max, returns total */
    assert(mtl_wrap("abcdefghij", 10, 10, mw, 0, l, 2) == 10);

    /* caret math on "aaa bbb ccc" */
    n = wrap("aaa bbb ccc", 50, l);
    assert(mtl_line_of(l, n, 0) == 0 && mtl_line_of(l, n, 3) == 0);
    assert(mtl_line_of(l, n, 4) == 1 && mtl_column_of(l, n, 4) == 0); /* boundary belongs to next line */
    assert(mtl_column_of(l, n, 6) == 2);
    assert(mtl_line_of(l, n, 11) == 2 && mtl_column_of(l, n, 11) == 3);
    assert(mtl_line_home(l, n, 6) == 4);
    assert(mtl_line_end("aaa bbb ccc", 11, l, n, 5) == 7);  /* soft line: before its trailing space */
    assert(mtl_line_end("aaa bbb ccc", 11, l, n, 9) == 11); /* last line: true end */
    { /* UP-Textarea triple click on wrapped line 1: the click offset stays in that line, home..end is the line without its space */
        int off = mtl_offset_at("aaa bbb ccc", 11, l, n, 1, 999, mw, 0);
        assert(mtl_line_home(l, n, off) == 4 && mtl_line_end("aaa bbb ccc", 11, l, n, off) == 7);
    }
    assert(mtl_offset_at("aaa bbb ccc", 11, l, n, 1, 0, mw, 0) == 4);
    assert(mtl_offset_at("aaa bbb ccc", 11, l, n, 1, 14, mw, 0) == 5);  /* 14 is nearer to x=10 than 20 */
    assert(mtl_offset_at("aaa bbb ccc", 11, l, n, 1, 16, mw, 0) == 6);
    assert(mtl_offset_at("aaa bbb ccc", 11, l, n, 1, 999, mw, 0) == 7);
    assert(mtl_offset_at("aaa bbb ccc", 11, l, n, 99, 0, mw, 0) == 8);   /* line clamped */
    assert(mtl_x_of("aaa bbb ccc", l, 2, 3, mw, 0) == 30);

    /* up/down keeps the preferred x across a short line */
    {
        const char *t = "abcdefgh\nxy\nabcdefgh";
        n = mtl_wrap(t, 20, 200, mw, 0, l, 16); assert(n == 3);
        pref = -1; off = 6; /* x = 60 */
        off = mtl_move_vertical(t, 20, l, n, off, 1, &pref, mw, 0); assert(off == 11 && pref == 60); /* end of "xy" */
        off = mtl_move_vertical(t, 20, l, n, off, 1, &pref, mw, 0); assert(off == 12 + 6);          /* back to x 60 */
        off = mtl_move_vertical(t, 20, l, n, off, 1, &pref, mw, 0); assert(off == 20);              /* last line down: end */
        off = 18; pref = 60;
        off = mtl_move_vertical(t, 20, l, n, off, -1, &pref, mw, 0); assert(off == 11);
        off = mtl_move_vertical(t, 20, l, n, off, -1, &pref, mw, 0); assert(off == 6);
        off = mtl_move_vertical(t, 20, l, n, off, -1, &pref, mw, 0); assert(off == 0);              /* first line up: 0 */
    }

    /* edits: cap 8 -> at most 7 bytes of text */
    len = 0; buf[0] = 0;
    assert(mtl_insert(buf, 8, &len, 0, "abcd", 4) == 4 && len == 4 && !strcmp(buf, "abcd"));
    assert(mtl_insert(buf, 8, &len, 2, "XYZW", 4) == 3 && len == 7 && !strcmp(buf, "abXYZcd")); /* truncated to fit */
    assert(mtl_insert(buf, 8, &len, 0, "q", 1) == 0 && len == 7 && !strcmp(buf, "abXYZcd"));    /* full: no change */
    assert(mtl_insert(buf, 8, &len, 99, "q", 1) == 0);
    assert(mtl_delete(buf, &len, 0) == 1 && !strcmp(buf, "bXYZcd"));
    assert(mtl_insert(buf, 8, &len, 99, "!", 1) == 1 && !strcmp(buf, "bXYZcd!")); /* offset clamped to the end */
    assert(mtl_insert(buf, 0, &len, 0, "q", 1) == 0);
    /* backspace at 0 is a no-op; in the middle moves the caret back */
    assert(mtl_backspace(buf, &len, 0) == 0 && !strcmp(buf, "bXYZcd!"));
    assert(mtl_backspace(buf, &len, 2) == 1 && !strcmp(buf, "bYZcd!") && len == 6);
    /* delete at end is a no-op */
    assert(mtl_delete(buf, &len, len) == 0 && len == 6);
    for (i = 6; i > 0; i--) mtl_backspace(buf, &len, i);
    assert(len == 0 && buf[0] == 0);

    /* selection */
    { int lo, hi;
      assert(mtl_sel_range(5, 2, 10, &lo, &hi) == 1 && lo == 2 && hi == 5);
      assert(mtl_sel_range(2, 5, 10, &lo, &hi) == 1 && lo == 2 && hi == 5);
      assert(mtl_sel_range(4, 4, 10, &lo, &hi) == 0);
      assert(mtl_sel_range(-3, 99, 10, &lo, &hi) == 1 && lo == 0 && hi == 10); /* clamped */
      len = 6; strcpy(buf, "abcdef");
      assert(mtl_delete_range(buf, &len, 1, 3) == 2 && len == 4 && !strcmp(buf, "adef"));
      assert(mtl_delete_range(buf, &len, 2, 99) == 2 && len == 2 && !strcmp(buf, "ad"));
      assert(mtl_delete_range(buf, &len, 1, 1) == 0 && !strcmp(buf, "ad"));
      len = 6; strcpy(buf, "abcdef"); /* cap 8: room for 7 bytes */
      assert(mtl_replace(buf, 8, &len, 1, 3, "XY", 2) == 2 && !strcmp(buf, "aXYdef") && len == 6);
    }
    len = 6; strcpy(buf, "abcdef");
    assert(mtl_replace(buf, 8, &len, 1, 3, "12345", 5) == 3 && len == 7 && !strcmp(buf, "a123def")); /* deleted 2, so room for 3 */
    len = 6; strcpy(buf, "abcdef");
    assert(mtl_replace(buf, 8, &len, 0, 0, "12345", 5) == 1 && len == 7);                          /* nothing deleted: only 1 fits */
    len = 6; strcpy(buf, "abcdef");
    assert(mtl_replace(buf, 8, &len, 0, 6, "", 0) == 0 && len == 0 && buf[0] == 0);

    /* scroll */
    assert(mtl_scroll_top(0, 5, 0, 20) == 0);
    assert(mtl_scroll_top(4, 5, 0, 20) == 0);    /* last visible line: no move */
    assert(mtl_scroll_top(5, 5, 0, 20) == 1);    /* just below: minimal move */
    assert(mtl_scroll_top(2, 5, 10, 20) == 2);   /* above: caret becomes top */
    assert(mtl_scroll_top(19, 5, 0, 20) == 15);
    assert(mtl_scroll_top(1, 5, 9, 3) == 0);     /* fewer lines than visible */
    assert(mtl_scroll_top(3, 0, 0, 10) == 3);    /* visible clamped to 1 */

    /* clipboard / select-all keys: Ctrl codes, or a letter with the Amiga key (either case) */
    assert(mtl_clip_key(0x03, 0) == 'c' && mtl_clip_key(0x18, 0) == 'x' && mtl_clip_key(0x16, 0) == 'v' && mtl_clip_key(0x01, 0) == 'a');
    assert(mtl_clip_key('a', 1) == 'a' && mtl_clip_key('A', 1) == 'a' && mtl_clip_key('C', 1) == 'c' && mtl_clip_key('v', 1) == 'v');
    assert(mtl_clip_key('z', 1) == 0 && mtl_clip_key('a', 0) == 0 && mtl_clip_key(0x08, 0) == 0);

    puts("mui_text_lines_test: ok");
    /* Floattext prep: skip chars, TAB to the next multiple of tabsize, column restarts after LF; count-only and a cut buffer */
    {
        char fb[32];
        assert(mtl_float_prep("a\tb", 0, 4, fb, sizeof fb) == 5 && strcmp(fb, "a   b") == 0);
        assert(mtl_float_prep("ab\n\tc", 0, 4, fb, sizeof fb) == 8 && strcmp(fb, "ab\n    c") == 0);
        assert(mtl_float_prep("x*y_z", "*_", 8, fb, sizeof fb) == 3 && strcmp(fb, "xyz") == 0);
        assert(mtl_float_prep("\t", 0, 0, 0, 0) == 8);
        assert(mtl_float_prep("abcdef", 0, 8, fb, 4) == 6 && strcmp(fb, "abc") == 0);
        assert(mtl_float_prep(0, 0, 8, fb, sizeof fb) == 0 && fb[0] == '\0');
    }
    /* Justify: gaps sum to extra, differ by at most one */
    {
        int sum = 0, g;
        for (g = 0; g < 3; g++) {
            int v = mtl_justify_gap(7, 3, g);
            assert(v == 2 || v == 3);
            sum += v;
        }
        assert(sum == 7);
        assert(mtl_justify_gap(5, 0, 0) == 0 && mtl_justify_gap(0, 4, 2) == 0);
    }
    return 0;
}
