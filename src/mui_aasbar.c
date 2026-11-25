/* aa_bar: the flat thumb-only bar of UP-Scroller, shared by every class that draws one (UP-Scroller, UP-TextArea). up-mcc.library runs
 * it behind UP_DrawBar; the static build calls it directly. Colours and geometry come from map/mui_aa.c (mui_aa_scroller_rgb,
 * mui_aa_scroller_geom, _press, _first_at), the size from up_scroller_metrics. */
#include "mui_aa_draw.h"
#include "map/mui_aa.h"
#include <mcc/up.h>
#include <mcc/up_metrics.h>

LONG aa_bar(aa_ctx* c, struct RastPort* rp, struct up_bar* b) {
    up_scroller_t m;
    int st, ty, th, grab;
    uint32_t track, thumb;
    long first;
    up_scroller_metrics(c->font_px, c->aspect_q8, &m);
    switch (b->op) {
    case UP_BAR_SETUP:
        for (st = MUI_AA_NORMAL; st <= MUI_AA_PRESSED; st++) {
            mui_aa_scroller_rgb(c->tok, st, &track, &thumb);
            aa_hold(c, track);
            aa_hold(c, thumb);
        }
        return (LONG)m.width | (LONG)m.min_h << 16;
    case UP_BAR_PAINT:
        mui_aa_scroller_rgb(c->tok, b->state, &track, &thumb);
        aa_round_fill(c, rp, b->x, b->y, b->w, b->h, 0, track);
        if (mui_aa_scroller_geom(&m, b->h, b->first, b->visible, b->entries, &ty, &th)) {
            aa_round_fill(c, rp, b->x, b->y + ty, b->w, th, m.radius, thumb);
        }
        return 0;
    case UP_BAR_PRESS:
        first = mui_aa_scroller_press(&m, b->h, b->py, b->first, b->visible, b->entries, &grab);
        b->grab = (WORD)grab;
        return first;
    case UP_BAR_DRAG: return mui_aa_scroller_first_at(&m, b->h, b->grab, b->py, b->visible, b->entries);
    default: return 0;
    }
}
