/* aa_scroller_paint: the flat thumb-only bar of UP-Scroller, shared by every class that draws one (UP-Scroller, UP-TextArea). Colours and
 * geometry come from map/mui_aa.c (mui_aa_scroller_rgb, mui_aa_scroller_geom). */
#include "mui_aa_draw.h"
#include "map/mui_aa.h"

void aa_scroller_paint(aa_ctx* c, struct RastPort* rp, const up_scroller_t* m, int state, int x, int y, int w, int h, long first, long visible, long entries) {
    int ty, th;
    uint32_t track, thumb;
    mui_aa_scroller_rgb(c->tok, state, &track, &thumb);
    aa_round_fill(c, rp, x, y, w, h, 0, track);
    if (mui_aa_scroller_geom(m, h, first, visible, entries, &ty, &th)) {
        aa_round_fill(c, rp, x, y + ty, w, th, m->radius, thumb);
    }
}

/* Holds the track and thumb pens of every state, for MUIM_Setup. */
void aa_scroller_hold(aa_ctx* c) {
    int st;
    uint32_t track, thumb;
    for (st = MUI_AA_NORMAL; st <= MUI_AA_PRESSED; st++) {
        mui_aa_scroller_rgb(c->tok, st, &track, &thumb);
        aa_hold(c, track);
        aa_hold(c, thumb);
    }
}
