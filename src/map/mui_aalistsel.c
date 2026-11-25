#include "mui_aalistsel.h"

int up_sel_op(uint32_t* s, long r, int op) {
    uint32_t b = 1UL << (r & 31);
    uint32_t* w = &s[r >> 5];
    if (op == 0) {
        *w &= ~b;
    } else if (op == 1) {
        *w |= b;
    } else if (op == 2) {
        *w ^= b;
    }
    return (*w & b) != 0;
}

long up_sel_next(const uint32_t* s, long count, long pos) {
    long r;
    for (r = pos < 0 ? 0 : pos + 1; r < count; r++) {
        if (s[r >> 5] & (1UL << (r & 31))) {
            return r;
        }
    }
    return -1;
}

void up_sel_copy(uint32_t* dst, long n, const uint32_t* src, long src_rows) {
    long r;
    for (r = 0; r < UP_SEL_WORDS(n); r++) {
        dst[r] = 0;
    }
    for (r = 0; r < n && r < src_rows; r++) {
        if (src[r >> 5] & (1UL << (r & 31))) {
            dst[r >> 5] |= 1UL << (r & 31);
        }
    }
}

int mui_aa_list_col_at(const up_list_t* m, int x, int n, const int* cw) {
    int c, hit = 0;
    for (c = 1; c < n; c++) {
        if (cw[c] > 0 && x >= mui_aa_list_col_x(m, c, cw) - m->gap) {
            hit = c;
        }
    }
    return hit;
}
