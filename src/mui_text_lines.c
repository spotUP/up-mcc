/* Pure-C text helpers for a multi-line editor class. See mui_text_lines.h for the line model. */
#include "mui_text_lines.h"

#include <string.h>

int mtl_wrap(const char *text, int n, int width, MtlMeasure measure, void *ctx, MtlLine *out, int max) {
    int count = 0, ps = 0;
    for (;;) {
        int pe = ps, pos = ps;
        while (pe < n && text[pe] != '\n') pe++;
        for (;;) {
            int e = pos, soft = -1;
            while (e < pe && measure(ctx, text + pos, e + 1 - pos) <= width) {
                e++;
                if (text[e - 1] == ' ') soft = e;
            }
            if (e < pe) { /* overflow at byte e */
                if (text[e] == ' ') e++; /* a trailing space may hang past the width */
                else if (soft > pos) e = soft;
                else if (e == pos) e = pos + 1; /* not even one byte fits: still progress */
            }
            if (count < max) { out[count].start = pos; out[count].len = e - pos; }
            count++;
            pos = e;
            if (pos >= pe) break;
        }
        if (pe >= n) break;
        ps = pe + 1;
    }
    return count;
}

#ifdef __GNUC__
__attribute__((noinline)) /* -O2 would copy the loop into every caller; the class libraries have a text size limit */
#endif
int mtl_line_of(const MtlLine *lines, int nlines, int off) {
    int i = 0;
    while (i + 1 < nlines && lines[i + 1].start <= off) i++;
    return i;
}

int mtl_column_of(const MtlLine *lines, int nlines, int off) {
    int l = mtl_line_of(lines, nlines, off), c = off - lines[l].start;
    return c < 0 ? 0 : c;
}

int mtl_max_column(const char *text, int n, const MtlLine *lines, int nlines, int line) {
    int end = lines[line].start + lines[line].len;
    (void)nlines;
    if (end < n && text[end] != '\n' && lines[line].len > 0) return lines[line].len - 1;
    return lines[line].len;
}

int mtl_x_of(const char *text, const MtlLine *lines, int line, int col, MtlMeasure measure, void *ctx) {
    return col <= 0 ? 0 : measure(ctx, text + lines[line].start, col);
}

int mtl_offset_at(const char *text, int n, const MtlLine *lines, int nlines, int line, int x, MtlMeasure measure, void *ctx) {
    int col, max, prev = 0;
    if (line < 0) line = 0;
    if (line >= nlines) line = nlines - 1;
    max = mtl_max_column(text, n, lines, nlines, line);
    for (col = 0; col < max; col++) {
        int w = measure(ctx, text + lines[line].start, col + 1);
        if (w > x) { if (x - prev >= w - x) col++; break; } /* nearer to the right edge of this byte */
        prev = w;
    }
    return lines[line].start + col;
}

int mtl_move_vertical(const char *text, int n, const MtlLine *lines, int nlines, int off, int delta, int *pref_x, MtlMeasure measure, void *ctx) {
    int l = mtl_line_of(lines, nlines, off), col = off - lines[l].start;
    if (*pref_x < 0) *pref_x = mtl_x_of(text, lines, l, col, measure, ctx);
    if (delta < 0) {
        if (l == 0) return 0;
        return mtl_offset_at(text, n, lines, nlines, l - 1, *pref_x, measure, ctx);
    }
    if (l + 1 >= nlines) return n;
    return mtl_offset_at(text, n, lines, nlines, l + 1, *pref_x, measure, ctx);
}

int mtl_line_home(const MtlLine *lines, int nlines, int off) {
    return lines[mtl_line_of(lines, nlines, off)].start;
}

int mtl_line_end(const char *text, int n, const MtlLine *lines, int nlines, int off) {
    int l = mtl_line_of(lines, nlines, off);
    return lines[l].start + mtl_max_column(text, n, lines, nlines, l);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int mtl_insert(char *buf, int cap, int *len, int off, const char *s, int slen) {
    int room;
    if (cap < 1 || slen <= 0) return 0;
    off = clampi(off, 0, *len);
    room = cap - 1 - *len;
    if (slen > room) slen = room;
    if (slen <= 0) return 0;
    memmove(buf + off + slen, buf + off, (size_t)(*len - off));
    memcpy(buf + off, s, (size_t)slen);
    *len += slen;
    buf[*len] = '\0';
    return slen;
}

int mtl_backspace(char *buf, int *len, int off) {
    off = clampi(off, 0, *len);
    if (off == 0) return 0;
    memmove(buf + off - 1, buf + off, (size_t)(*len - off));
    buf[--*len] = '\0';
    return off - 1;
}

int mtl_delete(char *buf, int *len, int off) {
    off = clampi(off, 0, *len);
    if (off >= *len) return 0;
    memmove(buf + off, buf + off + 1, (size_t)(*len - off - 1));
    buf[--*len] = '\0';
    return 1;
}

int mtl_scroll_top(int caret_line, int visible, int top, int nlines) {
    int maxtop;
    if (visible < 1) visible = 1;
    if (caret_line < top) top = caret_line;
    else if (caret_line >= top + visible) top = caret_line - visible + 1;
    maxtop = nlines - visible;
    return clampi(top, 0, maxtop < 0 ? 0 : maxtop);
}

int mtl_sel_range(int anchor, int caret, int n, int *lo, int *hi) {
    int a = anchor < 0 ? 0 : anchor > n ? n : anchor, c = caret < 0 ? 0 : caret > n ? n : caret;
    *lo = a < c ? a : c;
    *hi = a < c ? c : a;
    return *lo < *hi;
}

int mtl_clip_key(int code, int cmd) {
    int key = cmd ? (code | 0x20) : code == 0x03 ? 'c' : code == 0x18 ? 'x' : code == 0x16 ? 'v' : code == 0x01 ? 'a' : 0;
    return key == 'c' || key == 'x' || key == 'v' || key == 'a' ? key : 0;
}

static int word_class(unsigned char c) {
    if (c == '_' || c >= 0x80 || (c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z')) return 1;
    return c == ' ' || c == '\t' ? 2 : c == '\n' ? 0 : 3;
}

void mtl_word_range(const char *text, int n, int off, int *lo, int *hi) {
    int k, a, b;
    off = off > n ? n : off;
    off = off < 0 ? 0 : off;
    a = off;
    if (a >= n || text[a] == '\n') a--; /* at a line end: the run before it */
    if (a < 0 || text[a] == '\n') { /* an empty line: nothing, a LF is never part of a run */
        *lo = *hi = off;
        return;
    }
    k = word_class((unsigned char)text[a]);
    b = a + 1;
    while (a > 0 && word_class((unsigned char)text[a - 1]) == k) a--;
    while (b < n && word_class((unsigned char)text[b]) == k) b++;
    *lo = a;
    *hi = b;
}

int mtl_delete_range(char *buf, int *len, int lo, int hi) {
    int n = *len;
    lo = lo < 0 ? 0 : lo > n ? n : lo;
    hi = hi < lo ? lo : hi > n ? n : hi;
    memmove(buf + lo, buf + hi, (size_t)(n - hi + 1)); /* +1 keeps the NUL */
    *len = n - (hi - lo);
    return hi - lo;
}

int mtl_replace(char *buf, int cap, int *len, int lo, int hi, const char *s, int slen) {
    mtl_delete_range(buf, len, lo, hi);
    return mtl_insert(buf, cap, len, lo, s, slen);
}

int mtl_scroll_x(int cx, int cw, int w, int scroll, int tw) {
    int max;
    if (w < 1) w = 1;
    if (cx < scroll) scroll = cx;
    else if (cx + cw > scroll + w) scroll = cx + cw - w;
    max = tw + cw - w;
    return clampi(scroll, 0, max < 0 ? 0 : max);
}

void mtl_visible_cols(const char *text, int n, int scroll, int w, MtlMeasure measure, void *ctx, int *c0, int *c1) {
    int a = 0, b;
    while (a < n && measure(ctx, text, a) < scroll) a++;
    b = a;
    while (b < n && measure(ctx, text, b + 1) - scroll <= w) b++;
    *c0 = a;
    *c1 = b;
}
