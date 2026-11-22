/* I1491a2 (design 2026-11-22-i1491a2-draw-behind-library.md section 3): what the class libraries call instead of linking the draw layer
 * (src/mui_aa_draw.c, src/mui_aatip.c). Each name is also defined by the static Selecta build; a library gets its draw code from
 * up-mcc.library, so no AA text, pen, fill or tooltip code lands in a .mcc. The archive members are pulled only for undefined names,
 * and these definitions come first. The shims pack their arguments into struct up_shape / up_text on the stack and call one entry. */
#include <string.h>

#include <exec/types.h>
#include "mui_aa_draw.h"
#include "mui_aatip.h"

/* After the draw headers: they pull up_lib.h, whose prototypes must come before the inline stubs. */
#include <proto/up_mcc.h>

void aa_setup(Object* obj, aa_ctx* c, const uint32_t* rgb, int n) {
    if (!UP_DrawSetup(c, obj, (const ULONG*)rgb, UP_DRAW_NSIZE(n, sizeof(aa_ctx), sizeof(aatip_slot)))) {
        memset(c, 0, sizeof *c); /* refused (size mismatch or no library): a plain, unlinked context every later call tolerates */
    }
}

void aa_cleanup(aa_ctx* c) {
    UP_DrawCleanup(c);
}

void aa_set_font_px(aa_ctx* c, int px) {
    UP_DrawFont(c, c->obj, px, 0);
}

void aa_plain_font(aa_ctx* c, Object* obj, int px, int bold) {
    UP_DrawFont(c, obj, px, UP_FONT_PLAIN | (bold ? UP_FONT_BOLD : 0));
}

/* The library closes ttengine and the RTG libraries itself at its own expunge. */
void aa_free(void) {}

int aa_live(Object* obj) {
    return (int)UP_DrawLive(obj);
}

void aa_hold(aa_ctx* c, uint32_t rgb) {
    UP_DrawPen(c, rgb, 1);
}

LONG aa_nearest_pen(aa_ctx* c, uint32_t rgb) {
    return UP_DrawPen(c, rgb, 0);
}

static void shape(aa_ctx* c, struct RastPort* rp, Object* obj, int kind, int x, int y, int w, int h, int radius, uint32_t rgb, const void* p0,
                  const void* p1, int n) {
    struct up_shape s;
    s.kind = (UWORD)kind;
    s.x = (WORD)x;
    s.y = (WORD)y;
    s.w = (WORD)w;
    s.h = (WORD)h;
    s.radius = (WORD)radius;
    s.rgb = rgb;
    s.p0 = p0;
    s.p1 = p1;
    s.n = (WORD)n;
    UP_DrawShape(c, rp, obj, &s);
}

void aa_rect(aa_ctx* c, struct RastPort* rp, int x0, int y0, int x1, int y1, uint32_t rgb) {
    shape(c, rp, NULL, UP_SHAPE_RECT, x0, y0, x1, y1, 0, rgb, NULL, NULL, 0);
}

void aa_round_fill(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb) {
    shape(c, rp, NULL, UP_SHAPE_ROUND_FILL, x, y, w, h, radius, rgb, NULL, NULL, 0);
}

void aa_round_frame(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb) {
    shape(c, rp, NULL, UP_SHAPE_ROUND_FRAME, x, y, w, h, radius, rgb, NULL, NULL, 0);
}

void aa_round_corners(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb) {
    shape(c, rp, NULL, UP_SHAPE_ROUND_CORNERS, x, y, w, h, radius, rgb, NULL, NULL, 0);
}

void aa_round_fill_hole(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb, int hx, int hy, int hw, int hh) {
    int hole[4];
    hole[0] = hx;
    hole[1] = hy;
    hole[2] = hw;
    hole[3] = hh;
    shape(c, rp, NULL, UP_SHAPE_ROUND_FILL_HOLE, x, y, w, h, radius, rgb, hole, NULL, 0);
}

void aa_round_fill_holes(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb, const int* holes, int n) {
    shape(c, rp, NULL, UP_SHAPE_ROUND_FILL_HOLES, x, y, w, h, radius, rgb, holes, NULL, n);
}

void aa_low_outline(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius) {
    shape(c, rp, NULL, UP_SHAPE_LOW_OUTLINE, x, y, w, h, radius, 0, NULL, NULL, 0);
}

void aa_runs(aa_ctx* c, struct RastPort* rp, int x, int y, const unsigned char* slot, int n, const uint32_t* slot_rgb) {
    shape(c, rp, NULL, UP_SHAPE_RUNS, x, y, 0, 0, 0, 0, slot, slot_rgb, n);
}

void aa_erase(aa_ctx* c, Object* obj, int x, int y, int w, int h) {
    shape(c, NULL, obj, UP_SHAPE_ERASE, x, y, w, h, 0, 0, NULL, NULL, 0);
}

int aa_text_width(aa_ctx* c, Object* obj, const char* str) {
    return (int)UP_DrawTextSize(c, obj, (STRPTR)str);
}

int aa_text_height(aa_ctx* c, Object* obj) {
    return (int)UP_DrawTextSize(c, obj, NULL);
}

void aa_text(aa_ctx* c, Object* obj, struct RastPort* rp, int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    struct up_text t;
    t.x = (WORD)x;
    t.y = (WORD)y;
    t.str = (STRPTR)str;
    t.fg = fg;
    t.bg = bg;
    UP_DrawText(c, obj, rp, &t);
}

ULONG aatip_super_set(struct IClass* cl, Object* obj, struct opSet* msg, ULONG slot_off) {
    return UP_TipSuperSet(cl, obj, msg, slot_off);
}

int aatip_get(struct opGet* msg, const aatip_slot* t) {
    return (int)UP_TipGet(msg, (APTR)t);
}

void aatip_event(Object* obj, aatip_slot* t, const struct IntuiMessage* im, int in) {
    UP_TipEvent(obj, t, (APTR)im, in);
}

ULONG aatip_fire(Object* obj, aa_ctx* aa, const aatip_slot* t) {
    return UP_TipFire(obj, aa, (APTR)t);
}

void aatip_drop(Object* obj) {
    UP_TipDrop(obj);
}
