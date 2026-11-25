/* UP-List multi-selection bitset and column hit (pure, host tested in tests/mui_aalistsel_test.c). A selection is an array of uint32_t,
 * one bit per row (row r is bit r%32 of word r/32); the class allocates UP_SEL_WORDS(count) words and bounds every row by its count. */
#ifndef MUI_AALISTSEL_H
#define MUI_AALISTSEL_H

#include <stdint.h>

#include "mui_aa.h"

#define UP_SEL_WORDS(n) (((n) + 31) / 32)

/* One row, op as MUIV_List_Select_Off/On/Toggle/Ask (0..3): returns the row's state after the op (Ask leaves it). */
int up_sel_op(uint32_t* s, long r, int op);
/* First selected row after pos (pos -1 = from the start) below count; -1 when none is left. */
long up_sel_next(const uint32_t* s, long count, long pos);
/* Copies the bits of rows 0..n-1 from src (src_rows rows) into dst (UP_SEL_WORDS(n) words); rows past src_rows are cleared. */
void up_sel_copy(uint32_t* dst, long n, const uint32_t* src, long src_rows);
/* Column under x (pixels from the list's left edge) for the n column widths cw of the draw (mui_aa_list_col_widths): the last shown
 * column whose divider is left of x; 0 for a single-column list. */
int mui_aa_list_col_at(const up_list_t* m, int x, int n, const int* cw);

#endif
