/* Pure-C helpers for a multi-line text editor class: word-wrap, caret math, buffer edits, vertical scroll.
 * No Amiga/MUI headers, no allocation: the caller owns every buffer. Plain 8-bit bytes (ASCII/Latin-1), LF separates paragraphs.
 *
 * Line model: lines are contiguous slices of the text. A soft wrap keeps the space it broke at on the line it ends, so the next
 * line starts exactly at start+len. A hard LF is NOT part of any line: the next line starts at start+len+1. Text that is empty or
 * ends in LF has a final empty line, so there is always at least one line.
 * An offset equal to a soft-wrap boundary belongs to the NEXT line (column 0); an offset on a LF belongs to the line it ends. */
#ifndef MUI_TEXT_LINES_H
#define MUI_TEXT_LINES_H

typedef struct {
    int start; /* offset of the first byte */
    int len;   /* bytes in the line, LF excluded */
} MtlLine;

/* Width in pixels of s[0..len). */
typedef int (*MtlMeasure)(void *ctx, const char *s, int len);

/* Wrap text[0..n) to width pixels. Breaks after a space, hard-breaks a word wider than width (at least one byte per line).
 * Fills at most max entries of out (out may be NULL with max 0 to just count). Returns the TOTAL line count (>= 1), which can
 * exceed max; then the caller grows its array and calls again. */
int mtl_wrap(const char *text, int n, int width, MtlMeasure measure, void *ctx, MtlLine *out, int max);

/* Line index containing offset off (clamped). */
int mtl_line_of(const MtlLine *lines, int nlines, int off);
/* Offset -> column in its line. */
int mtl_column_of(const MtlLine *lines, int nlines, int off);
/* Largest caret column of a line: len, except a soft-wrapped line stops before its last byte so the caret stays on that line. */
int mtl_max_column(const char *text, int n, const MtlLine *lines, int nlines, int line);
/* Pixel x of a caret at column col of line. */
int mtl_x_of(const char *text, const MtlLine *lines, int line, int col, MtlMeasure measure, void *ctx);
/* (line, x pixel) -> offset: the caret position whose x is nearest to x. line is clamped. */
int mtl_offset_at(const char *text, int n, const MtlLine *lines, int nlines, int line, int x, MtlMeasure measure, void *ctx);
/* Move the caret one visual line up (delta < 0) or down (delta > 0) keeping the preferred x. *pref_x < 0 means "take it from the
 * current caret"; it is then stored, and the caller resets it to -1 on any horizontal move or edit. Up on the first line goes to
 * offset 0, down on the last line to the end of text. */
int mtl_move_vertical(const char *text, int n, const MtlLine *lines, int nlines, int off, int delta, int *pref_x, MtlMeasure measure, void *ctx);
/* Home / End of the visual line holding off. */
int mtl_line_home(const MtlLine *lines, int nlines, int off);
int mtl_line_end(const char *text, int n, const MtlLine *lines, int nlines, int off);

/* Edit ops. buf has cap bytes in all; text length is kept <= cap-1 and buf stays NUL terminated. *len is the text length.
 * off is clamped to [0, *len]. */
/* Insert s[0..slen) at off; inserts only what fits (possibly 0 bytes). Returns the number of bytes inserted. */
int mtl_insert(char *buf, int cap, int *len, int off, const char *s, int slen);
/* Delete the byte before off. Returns the new caret offset (unchanged, 0, when off is 0). */
int mtl_backspace(char *buf, int *len, int off);
/* Delete the byte at off. Returns 1 if a byte was deleted, 0 at the end of text. */
int mtl_delete(char *buf, int *len, int off);

/* Selection: anchor and caret are offsets; the selection is the bytes between them (empty when equal). */
/* Normalised range lo <= hi, clamped to [0, n]. Returns 1 when non-empty. */
int mtl_sel_range(int anchor, int caret, int n, int *lo, int *hi);
/* Clipboard/selection shortcut of a VANILLAKEY code: with the Amiga key held (cmd) a letter with or without Shift, without it the
 * Ctrl control codes. Returns 'c', 'x', 'v' or 'a' (select all), 0 for any other key. */
int mtl_clip_key(int code, int cmd);
/* Double-click word: the run of bytes around off that share its class (word: alnum, '_' or >= 0x80; blank; other), off clamped
 * to [0, n]; at the end of the text the byte before off counts. Sets [lo, hi); empty text gives lo == hi == 0. */
void mtl_word_range(const char *text, int n, int off, int *lo, int *hi);
/* Delete the bytes [lo, hi) (clamped). Returns the number of bytes removed. */
int mtl_delete_range(char *buf, int *len, int lo, int hi);
/* Replace [lo, hi) by s[0..slen): the range goes first, then as much of s as fits in cap. Returns the number of bytes inserted. */
int mtl_replace(char *buf, int cap, int *len, int lo, int hi, const char *s, int slen);

/* New top line so caret_line is inside [top, top+visible), moving as little as possible, with top kept in [0, nlines-visible]. */
int mtl_scroll_top(int caret_line, int visible, int top, int nlines);

/* Single-line edit (UP-Field): the text is pixel-scrolled left by scroll. */
/* New scroll so a caret at text x cx, cw wide, shows inside a view w wide, moving as little as possible; never past the text end
 * (text width tw plus the caret), never below 0. */
int mtl_scroll_x(int cx, int cw, int w, int scroll, int tw);
/* Bytes [*c0, *c1) of text[0..n) that show whole in a view w wide at scroll: c0 is the first byte starting at or right of scroll, c1
 * ends at the last byte whose right edge is inside scroll+w. They draw at view x measure(c0) - scroll. */
void mtl_visible_cols(const char *text, int n, int scroll, int w, MtlMeasure measure, void *ctx, int *c0, int *c1);

/* Read-only text (UP-Floattext, MUIA_Floattext_*). */
/* Copy src to dst dropping every byte found in skip (NULL or "" = none) and expanding each TAB with spaces to the next multiple of
 * tabsize columns (tabsize < 1 = 8); the column restarts after a LF. Writes at most cap-1 bytes plus a NUL; dst may be NULL (cap 0)
 * to count. Returns the full length the result needs, without the NUL. */
int mtl_float_prep(const char *src, const char *skip, int tabsize, char *dst, int cap);
/* Justify: pixels to add after space number i (0-based) of a line with gaps spaces and extra spare pixels, spread so the sum over
 * all gaps is exactly extra and no two gaps differ by more than one. 0 when gaps < 1. */
int mtl_justify_gap(int extra, int gaps, int i);

#endif
