/* Read-only text helpers (UP-Floattext). Own object file so UP-Textarea.mcc, which links mui_text_lines.o, does not carry them. */
#include "mui_text_lines.h"
#include <string.h>

int mtl_float_prep(const char *src, const char *skip, int tabsize, char *dst, int cap) {
    int n = 0, col = 0, k;
    if (tabsize < 1) {
        tabsize = 8;
    }
    for (; src != 0 && *src != '\0'; src++) {
        int reps = 1;
        char c = *src;
        if (skip != 0 && strchr(skip, c) != 0) {
            continue;
        }
        if (c == '\t') {
            reps = tabsize - col % tabsize;
            c = ' ';
        }
        for (k = 0; k < reps; k++, n++) {
            if (dst != 0 && n < cap - 1) {
                dst[n] = c;
            }
        }
        col = c == '\n' ? 0 : col + reps;
    }
    if (dst != 0 && cap > 0) {
        dst[n < cap - 1 ? n : cap - 1] = '\0';
    }
    return n;
}

int mtl_justify_gap(int extra, int gaps, int i) {
    if (gaps < 1 || i < 0 || i >= gaps) {
        return 0;
    }
    return extra * (i + 1) / gaps - extra * i / gaps;
}
