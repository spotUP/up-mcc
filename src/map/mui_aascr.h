/* UP-Virtgroup and UP-Scrollgroup geometry (pure, host tested in tests/mui_aascr_test.c): offset clamp, when the bar shows, and the
 * size the contents are laid out at inside a view. */
#ifndef MUI_AASCR_H
#define MUI_AASCR_H

#include <stdint.h>

/* off limited to 0 .. content - view (0 when the contents fit). */
long mui_aa_scr_clamp(long off, long content, long view);
/* 1 when the contents are taller (or wider) than the view: the bar shows. */
int mui_aa_scr_need(long content, long view);
/* Size the contents are laid out at: at least the view, at least their own minimum. */
void mui_aa_scr_inner(int view_w, int view_h, int min_w, int min_h, int* w, int* h);
/* Scrollgroup split of its width: the bar takes bar_w when content_h > h, the contents get the rest. */
void mui_aa_scr_split(int w, int h, int content_h, int bar_w, int* need_bar, int* view_w);
/* Size grip: one window edge after a drag of delta px from start, kept in min .. max and in room (the screen past the window's
 * left or top edge). min wins when room is smaller: MUI cannot lay the window out under its minimum. */
int mui_aa_grip_size(int start, int delta, int min, int max, int room);
/* Calls of the functions above: the sentinel that the classes (and not a copy) run them. */
uint32_t mui_aascr_calls(void);

#endif
