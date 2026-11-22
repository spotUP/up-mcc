/* I1491a2 (design 2026-11-22-i1491a2-draw-behind-library.md section 6): the library side of the draw layer. lib/support_draw.c is
 * compiled against the real mui_aa_draw.h and mui_aatip.h (host stand-ins for the Amiga system headers in tests/fake) and a fake
 * aa_* / aatip_* layer that records what it was called with.
 * entry: every UP_Draw and UP_Tip entry runs the right aa function with the unpacked arguments; every shape kind has a case.
 * size:  UP_DrawSetup refuses a class built with another aa_ctx or aatip_slot size and logs once; later calls on that ctx draw nothing. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "mui_aa_draw.h"
#include "mui_aatip.h"
#include <mcc/up.h>

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { g_fail = 1; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); putchar('\n'); } } while (0)

enum { F_NONE, F_SETUP, F_CLEANUP, F_FONT_PX, F_PLAIN, F_HOLD, F_NEAREST, F_RECT, F_FILL, F_FRAME, F_CORNERS, F_HOLE, F_HOLES, F_LOW, F_RUNS,
       F_ERASE, F_TEXT_W, F_TEXT_H, F_TEXT, F_LIVE, F_TIP_FIRE, F_TIP_SUPER, F_TIP_GET, F_TIP_EVENT, F_TIP_DROP };
static struct {
    int fn, calls;
    const void *c, *rp, *obj;
    long a[9];
    const void *p0, *p1;
} g_rec;
static int g_logs;

static void rec(int fn, const void* c) {
    g_rec.fn = fn;
    g_rec.calls++;
    g_rec.c = c;
}
static void reset(void) {
    memset(&g_rec, 0, sizeof g_rec);
}

void aa_log(const char* fmt, ...) {
    (void)fmt;
    g_logs++;
}
void aa_setup(Object* obj, aa_ctx* c, const uint32_t* rgb, int n) {
    rec(F_SETUP, c);
    g_rec.obj = obj;
    g_rec.p0 = rgb;
    g_rec.a[0] = n;
    c->linked = 1;
}
void aa_cleanup(aa_ctx* c) { rec(F_CLEANUP, c); }
void aa_set_font_px(aa_ctx* c, int px) { rec(F_FONT_PX, c); g_rec.a[0] = px; }
void aa_plain_font(aa_ctx* c, Object* obj, int px, int bold) { rec(F_PLAIN, c); g_rec.obj = obj; g_rec.a[0] = px; g_rec.a[1] = bold; }
void aa_hold(aa_ctx* c, uint32_t rgb) { rec(F_HOLD, c); g_rec.a[0] = (long)rgb; }
LONG aa_nearest_pen(aa_ctx* c, uint32_t rgb) { rec(F_NEAREST, c); g_rec.a[0] = (long)rgb; return 77; }
void aa_rect(aa_ctx* c, struct RastPort* rp, int x0, int y0, int x1, int y1, uint32_t rgb) {
    rec(F_RECT, c); g_rec.rp = rp; g_rec.a[0] = x0; g_rec.a[1] = y0; g_rec.a[2] = x1; g_rec.a[3] = y1; g_rec.a[4] = (long)rgb;
}
#define SHAPE_FN(name, id) \
    void name(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb) { \
        rec(id, c); g_rec.rp = rp; g_rec.a[0] = x; g_rec.a[1] = y; g_rec.a[2] = w; g_rec.a[3] = h; g_rec.a[4] = radius; g_rec.a[5] = (long)rgb; }
SHAPE_FN(aa_round_fill, F_FILL)
SHAPE_FN(aa_round_frame, F_FRAME)
SHAPE_FN(aa_round_corners, F_CORNERS)
void aa_round_fill_hole(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb, int hx, int hy, int hw, int hh) {
    rec(F_HOLE, c); g_rec.rp = rp; g_rec.a[0] = x; g_rec.a[1] = y; g_rec.a[2] = w; g_rec.a[3] = h; g_rec.a[4] = radius; g_rec.a[5] = (long)rgb;
    g_rec.a[6] = hx; g_rec.a[7] = hy; g_rec.a[8] = hw * 1000 + hh;
}
void aa_round_fill_holes(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius, uint32_t rgb, const int* holes, int n) {
    rec(F_HOLES, c); g_rec.rp = rp; g_rec.a[0] = x; g_rec.a[1] = y; g_rec.a[2] = w; g_rec.a[3] = h; g_rec.a[4] = radius; g_rec.a[5] = (long)rgb;
    g_rec.p0 = holes; g_rec.a[6] = n;
}
void aa_low_outline(aa_ctx* c, struct RastPort* rp, int x, int y, int w, int h, int radius) {
    rec(F_LOW, c); g_rec.rp = rp; g_rec.a[0] = x; g_rec.a[1] = y; g_rec.a[2] = w; g_rec.a[3] = h; g_rec.a[4] = radius;
}
void aa_runs(aa_ctx* c, struct RastPort* rp, int x, int y, const unsigned char* slot, int n, const uint32_t* slot_rgb) {
    rec(F_RUNS, c); g_rec.rp = rp; g_rec.a[0] = x; g_rec.a[1] = y; g_rec.p0 = slot; g_rec.a[2] = n; g_rec.p1 = slot_rgb;
}
void aa_erase(aa_ctx* c, Object* obj, int x, int y, int w, int h) {
    rec(F_ERASE, c); g_rec.obj = obj; g_rec.a[0] = x; g_rec.a[1] = y; g_rec.a[2] = w; g_rec.a[3] = h;
}
int aa_text_width(aa_ctx* c, Object* obj, const char* str) { rec(F_TEXT_W, c); g_rec.obj = obj; g_rec.p0 = str; return 41; }
int aa_text_height(aa_ctx* c, Object* obj) { rec(F_TEXT_H, c); g_rec.obj = obj; return 13; }
void aa_text(aa_ctx* c, Object* obj, struct RastPort* rp, int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    rec(F_TEXT, c); g_rec.obj = obj; g_rec.rp = rp; g_rec.a[0] = x; g_rec.a[1] = y; g_rec.p0 = str; g_rec.a[2] = (long)fg; g_rec.a[3] = (long)bg;
}
int aa_live(Object* obj) { rec(F_LIVE, NULL); g_rec.obj = obj; return 1; }
ULONG aatip_fire(Object* obj, aa_ctx* aa, const aatip_slot* t) { rec(F_TIP_FIRE, aa); g_rec.obj = obj; g_rec.p0 = t; return 5; }
ULONG aatip_super_set(struct IClass* cl, Object* obj, struct opSet* msg, ULONG slot_off) {
    rec(F_TIP_SUPER, NULL); g_rec.obj = obj; g_rec.p0 = cl; g_rec.p1 = msg; g_rec.a[0] = (long)slot_off; return 6;
}
int aatip_get(struct opGet* msg, const aatip_slot* t) { rec(F_TIP_GET, NULL); g_rec.p0 = msg; g_rec.p1 = t; return 1; }
void aatip_event(Object* obj, aatip_slot* t, const struct IntuiMessage* im, int in) {
    rec(F_TIP_EVENT, NULL); g_rec.obj = obj; g_rec.p0 = t; g_rec.p1 = im; g_rec.a[0] = in;
}
void aatip_drop(Object* obj) { rec(F_TIP_DROP, NULL); g_rec.obj = obj; }

static aa_ctx g_ctx;
static aatip_slot g_slot;
static int g_marker[16];
#define OBJ ((void*)&g_marker[1])
#define RP ((void*)&g_marker[2])

static void shape_case(int kind, int expect_fn) {
    struct up_shape s;
    int hole[4] = {7, 8, 9, 10};
    memset(&s, 0, sizeof s);
    s.kind = (UWORD)kind;
    s.x = 11; s.y = 12; s.w = 13; s.h = 14; s.radius = 3; s.rgb = 0x123456;
    s.p0 = kind == UP_SHAPE_ROUND_FILL_HOLE ? (const void*)hole : (const void*)&g_marker[3];
    s.p1 = &g_marker[4];
    s.n = 5;
    reset();
    UP_DrawShape(&g_ctx, RP, OBJ, &s);
    CHECK(g_rec.calls == 1 && g_rec.fn == expect_fn, "shape kind %d: fn %d calls %d (want %d)", kind, g_rec.fn, g_rec.calls, expect_fn);
    CHECK(g_rec.c == &g_ctx, "shape kind %d: ctx", kind);
    if (kind == UP_SHAPE_ERASE) {
        CHECK(g_rec.obj == OBJ && g_rec.a[0] == 11 && g_rec.a[3] == 14, "erase args");
        return;
    }
    CHECK(g_rec.rp == RP && g_rec.a[0] == 11 && g_rec.a[1] == 12, "shape kind %d: rp, x, y", kind);
    if (kind == UP_SHAPE_RECT) {
        CHECK(g_rec.a[2] == 13 && g_rec.a[3] == 14 && g_rec.a[4] == 0x123456, "rect corner and colour");
    } else if (kind == UP_SHAPE_LOW_OUTLINE) {
        CHECK(g_rec.a[2] == 13 && g_rec.a[3] == 14 && g_rec.a[4] == 3, "low outline");
    } else if (kind == UP_SHAPE_RUNS) {
        CHECK(g_rec.p0 == &g_marker[3] && g_rec.p1 == &g_marker[4] && g_rec.a[2] == 5, "runs slot, rgb, n");
    } else {
        CHECK(g_rec.a[2] == 13 && g_rec.a[3] == 14 && g_rec.a[4] == 3 && g_rec.a[5] == 0x123456, "round shape %d: w h radius rgb", kind);
        if (kind == UP_SHAPE_ROUND_FILL_HOLE) {
            CHECK(g_rec.a[6] == 7 && g_rec.a[7] == 8 && g_rec.a[8] == 9010, "hole rectangle");
        } else if (kind == UP_SHAPE_ROUND_FILL_HOLES) {
            CHECK(g_rec.p0 == &g_marker[3] && g_rec.a[6] == 5, "holes pointer and count");
        }
    }
}

static void entry(void) {
    static const int expect[UP_SHAPE_COUNT] = {F_RECT, F_FILL, F_FRAME, F_CORNERS, F_HOLE, F_HOLES, F_LOW, F_RUNS, F_ERASE};
    ULONG rgb[3] = {1, 2, 3};
    struct up_text t = {4, 5, (STRPTR)"txt", 0xAABBCC, 0x112233};
    struct up_shape bad;
    int k;
    reset();
    CHECK(UP_DrawSetup(&g_ctx, OBJ, rgb, UP_DRAW_NSIZE(3, sizeof(aa_ctx), sizeof(aatip_slot))) == 1, "setup returns 1");
    CHECK(g_rec.fn == F_SETUP && g_rec.c == &g_ctx && g_rec.obj == OBJ && g_rec.p0 == rgb && g_rec.a[0] == 3, "setup args");
    for (k = 0; k < UP_SHAPE_COUNT; k++) {
        shape_case(k, expect[k]);
    }
    memset(&bad, 0, sizeof bad);
    bad.kind = UP_SHAPE_COUNT;
    reset();
    UP_DrawShape(&g_ctx, RP, OBJ, &bad);
    CHECK(g_rec.calls == 0, "an unknown kind draws nothing");
    reset(); UP_DrawFont(&g_ctx, OBJ, 18, 0);
    CHECK(g_rec.fn == F_FONT_PX && g_rec.a[0] == 18 && g_rec.calls == 1, "font px");
    reset(); UP_DrawFont(&g_ctx, OBJ, 14, UP_FONT_PLAIN | UP_FONT_BOLD);
    CHECK(g_rec.fn == F_PLAIN && g_rec.a[0] == 14 && g_rec.a[1] == 1 && g_rec.obj == OBJ, "plain bold font");
    reset(); UP_DrawFont(&g_ctx, OBJ, 14, UP_FONT_PLAIN);
    CHECK(g_rec.fn == F_PLAIN && g_rec.a[1] == 0, "plain regular font");
    reset(); CHECK(UP_DrawPen(&g_ctx, 0x010203, 1) == 0 && g_rec.fn == F_HOLD && g_rec.a[0] == 0x010203, "pen hold");
    reset(); CHECK(UP_DrawPen(&g_ctx, 0x010203, 0) == 77 && g_rec.fn == F_NEAREST && g_rec.a[0] == 0x010203, "nearest pen");
    reset(); CHECK(UP_DrawTextSize(&g_ctx, OBJ, NULL) == 13 && g_rec.fn == F_TEXT_H && g_rec.obj == OBJ, "text height");
    reset(); CHECK(UP_DrawTextSize(&g_ctx, OBJ, (STRPTR)"abc") == 41 && g_rec.fn == F_TEXT_W && g_rec.p0 != NULL, "text width");
    reset(); UP_DrawText(&g_ctx, OBJ, RP, &t);
    CHECK(g_rec.fn == F_TEXT && g_rec.rp == RP && g_rec.obj == OBJ && g_rec.a[0] == 4 && g_rec.a[1] == 5 && g_rec.p0 == t.str &&
              g_rec.a[2] == 0xAABBCC && g_rec.a[3] == 0x112233, "text args");
    reset(); CHECK(UP_DrawLive(OBJ) == 1 && g_rec.fn == F_LIVE && g_rec.obj == OBJ, "live");
    reset(); CHECK(UP_TipFire(OBJ, &g_ctx, &g_slot) == 5 && g_rec.fn == F_TIP_FIRE && g_rec.c == &g_ctx && g_rec.p0 == &g_slot, "tip fire");
    reset(); CHECK(UP_TipSuperSet(&g_marker[5], OBJ, &g_marker[6], 24) == 6 && g_rec.fn == F_TIP_SUPER && g_rec.p0 == &g_marker[5] &&
                       g_rec.p1 == &g_marker[6] && g_rec.a[0] == 24, "tip super set");
    reset(); CHECK(UP_TipGet(&g_marker[7], &g_slot) == 1 && g_rec.fn == F_TIP_GET && g_rec.p0 == &g_marker[7] && g_rec.p1 == &g_slot, "tip get");
    reset(); UP_TipEvent(OBJ, &g_slot, &g_marker[8], 1);
    CHECK(g_rec.fn == F_TIP_EVENT && g_rec.p0 == &g_slot && g_rec.p1 == &g_marker[8] && g_rec.a[0] == 1, "tip event");
    reset(); UP_TipDrop(OBJ); CHECK(g_rec.fn == F_TIP_DROP && g_rec.obj == OBJ, "tip drop");
    reset(); UP_DrawCleanup(&g_ctx); CHECK(g_rec.fn == F_CLEANUP && g_rec.c == &g_ctx, "cleanup");
}

static void size(void) {
    ULONG rgb[1] = {1};
    aa_ctx other;
    memset(&other, 0, sizeof other);
    reset(); g_logs = 0;
    CHECK(UP_DrawSetup(&other, OBJ, rgb, UP_DRAW_NSIZE(1, sizeof(aa_ctx) + 4, sizeof(aatip_slot))) == 0, "ctx size mismatch refused");
    CHECK(UP_DrawSetup(&other, OBJ, rgb, UP_DRAW_NSIZE(1, sizeof(aa_ctx), sizeof(aatip_slot) + 4)) == 0, "slot size mismatch refused");
    CHECK(g_rec.calls == 0 && g_logs == 1, "refused twice: aa_setup never ran, one log line (got %d)", g_logs);
    UP_DrawFont(&other, OBJ, 12, 0);
    UP_DrawText(&other, OBJ, RP, &(struct up_text){1, 2, (STRPTR)"x", 3, 4});
    CHECK(UP_DrawTextSize(&other, OBJ, NULL) == 0 && UP_DrawPen(&other, 1, 0) == 0 && g_rec.calls == 0, "an unset-up ctx draws and measures nothing");
    CHECK(UP_DrawSetup(&other, OBJ, rgb, UP_DRAW_NSIZE(1, sizeof(aa_ctx), sizeof(aatip_slot))) == 1, "the right sizes pass");
}

int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "size") == 0) {
        size();
    } else {
        entry();
    }
    puts(g_fail ? "FAIL" : "OK");
    return g_fail;
}
