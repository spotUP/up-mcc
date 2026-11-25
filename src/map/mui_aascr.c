#include "mui_aascr.h"

static uint32_t g_calls;

long mui_aa_scr_clamp(long off, long content, long view) {
    long max = content - view;
    g_calls++;
    if (max < 0) {
        max = 0;
    }
    return off < 0 ? 0 : off > max ? max : off;
}

int mui_aa_scr_need(long content, long view) {
    g_calls++;
    return content > view;
}

void mui_aa_scr_inner(int view_w, int view_h, int min_w, int min_h, int* w, int* h) {
    g_calls++;
    *w = view_w > min_w ? view_w : min_w;
    *h = view_h > min_h ? view_h : min_h;
}

void mui_aa_scr_split(int w, int h, int content_h, int bar_w, int* need_bar, int* view_w) {
    g_calls++;
    *need_bar = content_h > h && bar_w > 0 && w > bar_w;
    *view_w = *need_bar ? w - bar_w : w;
}

uint32_t mui_aascr_calls(void) {
    return g_calls;
}
