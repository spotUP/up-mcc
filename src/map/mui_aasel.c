/* UP-Checkmark and UP-Radio paint plans; see mui_aasel.h. */
#include "mui_aasel.h"

static uint32_t g_calls;

int mui_aa_checkmark_plan(const uint32_t* token, const mui_aa_metrics_t* m, int selected, int state, mui_aa_check_op_t* out, uint32_t* tick) {
    int n = 0;
    g_calls++;
    if (!selected) {
        *tick = token[SEL_GUI_TOKEN_ACCENT_TEXT];
        return mui_aa_check_plan(token, m, state == MUI_AA_SELECTED ? MUI_AA_NORMAL : state, out);
    }
    out[n].inset = 0, out[n].radius = 0, out[n++].rgb = token[SEL_GUI_TOKEN_SURFACE];
    out[n].inset = 0;
    out[n].radius = (int)token[SEL_GUI_TOKEN_RADIUS_SMALL] / (m->check_px < 16 ? 2 : 1);
    out[n++].rgb = state == MUI_AA_DISABLED ? token[SEL_GUI_TOKEN_TEXT_DISABLED]
                   : state == MUI_AA_HOVER || state == MUI_AA_PRESSED ? mui_aa_pressed_rgb(token[SEL_GUI_TOKEN_ACCENT]) : token[SEL_GUI_TOKEN_ACCENT];
    *tick = state == MUI_AA_DISABLED ? token[SEL_GUI_TOKEN_SURFACE] : token[SEL_GUI_TOKEN_ACCENT_TEXT];
    return n;
}

int mui_aa_tick_cols(int w, int h, int thick, mui_aa_tick_t* out) {
    /* Corner points in permille of the box: (220,520) -> (420,740) -> (800,280). */
    static const int px[3] = {220, 420, 800}, py[3] = {520, 740, 280};
    int leg, x, n = 0, lo, hi, y, maxy = h - thick;
    g_calls++;
    if (w <= 0 || h <= 0 || thick <= 0 || maxy < 0) {
        return 0;
    }
    for (leg = 0; leg < 2; leg++) {
        lo = px[leg] * w / 1000;
        hi = px[leg + 1] * w / 1000;
        for (x = lo + leg; x <= hi && n < w; x++) { /* the second leg starts one column on: the corner column is not doubled */
            y = (py[leg] + (py[leg + 1] - py[leg]) * (hi > lo ? (x - lo) * 1000 / (hi - lo) : 0) / 1000) * h / 1000 - thick / 2;
            out[n].x = x < 0 ? 0 : x >= w ? w - 1 : x;
            out[n++].y = y < 0 ? 0 : y > maxy ? maxy : y;
        }
    }
    return n;
}

int mui_aa_radio_plan(const uint32_t* token, const mui_aa_metrics_t* m, int selected, int state, mui_aa_check_op_t* out) {
    int cp = m->check_px, line = cp >= 16 ? 2 : 1, dot = line + (cp >= 16 ? 3 : 3), n = 0;
    uint32_t ring = selected ? token[SEL_GUI_TOKEN_ACCENT] : state == MUI_AA_HOVER || state == MUI_AA_PRESSED ? token[SEL_GUI_TOKEN_TEXT] : token[SEL_GUI_TOKEN_TEXT_MUTED];
    g_calls++;
    if (state == MUI_AA_DISABLED) {
        ring = token[SEL_GUI_TOKEN_TEXT_DISABLED];
    }
    out[n].inset = 0, out[n].radius = 0, out[n++].rgb = token[SEL_GUI_TOKEN_SURFACE];
    out[n].inset = 0, out[n].radius = cp / 2, out[n++].rgb = ring;
    out[n].inset = line, out[n].radius = (cp - 2 * line) / 2;
    out[n++].rgb = mui_aa_state_rgb(token, token[SEL_GUI_TOKEN_SURFACE], state == MUI_AA_SELECTED ? MUI_AA_NORMAL : state);
    if (selected) {
        out[n].inset = dot, out[n].radius = (cp - 2 * dot) / 2;
        out[n++].rgb = state == MUI_AA_DISABLED ? token[SEL_GUI_TOKEN_TEXT_DISABLED] : token[SEL_GUI_TOKEN_ACCENT];
    }
    return n;
}

int mui_aa_radio_row_h(int text_h, int dot_h, int gap) {
    g_calls++;
    return (text_h > dot_h ? text_h : dot_h) + gap;
}

int mui_aa_radio_hit(int y, int row_h, int n) {
    g_calls++;
    return row_h > 0 && y >= 0 && y / row_h < n ? y / row_h : -1;
}

uint32_t mui_aasel_calls(void) {
    return g_calls;
}
