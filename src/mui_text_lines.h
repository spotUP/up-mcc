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

/* New top line so caret_line is inside [top, top+visible), moving as little as possible, with top kept in [0, nlines-visible]. */
int mtl_scroll_top(int caret_line, int visible, int top, int nlines);

#endif
