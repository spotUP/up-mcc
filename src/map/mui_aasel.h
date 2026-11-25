/* UP-Checkmark and UP-Radio paint plans (pure, host tested in tests/mui_aasel_test.c). Colours come from the token array. Both
 * reuse mui_aa_check_op_t of mui_aa.h: ops painted in order into the check_px square, each a rounded fill inset from the box edge. */
#ifndef MUI_AASEL_H
#define MUI_AASEL_H

#include <stdint.h>

#include "mui_aa.h"

#define MUI_AASEL_OPS 5

/* Checkmark box: unselected = the AACheck outline plan (mui_aa_check_plan, state NORMAL/HOVER/PRESSED/DISABLED); selected = parent
 * corners then one ACCENT box (hover and press show the pressed accent, disabled TEXT_DISABLED). *tick = the tick colour:
 * ACCENT_TEXT (SURFACE when disabled). Returns the op count. */
int mui_aa_checkmark_plan(const uint32_t* token, const up_metrics_t* m, int selected, int state, mui_aa_check_op_t* out, uint32_t* tick);

/* The tick as pixel columns of a w x h box: column x, top row y, each thick rows tall; a short rising leg then a long one.
 * Every column lies inside the box. Returns the count (at most w). */
typedef struct mui_aa_tick_t {
    int x, y;
} mui_aa_tick_t;
int mui_aa_tick_cols(int w, int h, int thick, mui_aa_tick_t* out);

/* Radio dot: parent corners, ring, face, and (selected) the centre dot. ring = ACCENT when selected else TEXT_MUTED (TEXT on hover,
 * TEXT_DISABLED when disabled); face = SURFACE through the state tokens; dot ACCENT (TEXT_DISABLED when disabled). Radius is the
 * circle radius for a square of check_px; the class clamps it to the row count on tall pixels. Returns the op count. */
int mui_aa_radio_plan(const uint32_t* token, const up_metrics_t* m, int selected, int state, mui_aa_check_op_t* out);

/* Height in rows of one radio row: the larger of the text and the dot, plus the gap between rows. */
int mui_aa_radio_row_h(int text_h, int dot_h, int gap);
/* Row under y (relative to the object top) of n rows of row_h; -1 outside. */
int mui_aa_radio_hit(int y, int row_h, int n);
/* Calls of the functions above: the sentinel that the classes (and not a copy) run them. */
uint32_t mui_aasel_calls(void);

#endif
