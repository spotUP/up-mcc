/* I1491a2 (design 2026-11-22-i1491a2-draw-behind-library.md section 6): lib/classlib_draw_shims.c, what a class library links instead of
 * the draw layer. The UP_Draw and UP_Tip entries are fakes that record; each shim must reach the right entry with its arguments packed
 * into struct up_shape / up_text, and a refused UP_DrawSetup must leave a zeroed (unlinked) context. */
#include <stdio.h>
#include <string.h>

#include "mui_aa_draw.h"
#include "mui_aatip.h"
#include <mcc/up.h>

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { g_fail = 1; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); putchar('\n'); } } while (0)

static struct { const char* fn; int calls; const void *ctx, *obj, *rp; LONG v0, v1; struct up_shape sh; struct up_text tx; ULONG nsize; } g;
static int g_setup_result = 1;

LONG UP_DrawSetup(void* ctx, void* obj, const ULONG* rgb, ULONG nsize) { g.fn = "setup"; g.calls++; g.ctx = ctx; g.obj = obj; g.v0 = (LONG)rgb[0]; g.nsize = nsize; return g_setup_result; }
void UP_DrawCleanup(void* ctx) { g.fn = "cleanup"; g.calls++; g.ctx = ctx; }
void UP_DrawFont(void* ctx, void* obj, LONG px, LONG flags) { g.fn = "font"; g.calls++; g.ctx = ctx; g.obj = obj; g.v0 = px; g.v1 = flags; }
LONG UP_DrawPen(void* ctx, ULONG rgb, LONG hold) { g.fn = "pen"; g.calls++; g.ctx = ctx; g.v0 = (LONG)rgb; g.v1 = hold; return 9; }
void UP_DrawShape(void* ctx, void* rp, void* obj, const void* shape) { g.fn = "shape"; g.calls++; g.ctx = ctx; g.rp = rp; g.obj = obj; g.sh = *(const struct up_shape*)shape; }
LONG UP_DrawTextSize(void* ctx, void* obj, STRPTR str) { g.fn = "size"; g.calls++; g.ctx = ctx; g.obj = obj; g.v0 = str != NULL; return 21; }
void UP_DrawText(void* ctx, void* obj, void* rp, const void* text) { g.fn = "text"; g.calls++; g.ctx = ctx; g.obj = obj; g.rp = rp; g.tx = *(const struct up_text*)text; }
LONG UP_DrawLive(void* obj) { g.fn = "live"; g.calls++; g.obj = obj; return 1; }
ULONG UP_TipFire(void* obj, void* ctx, void* slot) { g.fn = "tipfire"; g.calls++; g.obj = obj; g.ctx = ctx; g.rp = slot; return 4; }
ULONG UP_TipSuperSet(void* cl, void* obj, void* msg, ULONG off) { g.fn = "tipsuper"; g.calls++; g.ctx = cl; g.obj = obj; g.rp = msg; g.v0 = (LONG)off; return 3; }
LONG UP_TipGet(void* msg, void* slot) { g.fn = "tipget"; g.calls++; g.ctx = msg; g.rp = slot; return 1; }
void UP_TipEvent(void* obj, void* slot, void* imsg, LONG in) { g.fn = "tipevent"; g.calls++; g.obj = obj; g.ctx = slot; g.rp = imsg; g.v0 = in; }
void UP_TipDrop(void* obj) { g.fn = "tipdrop"; g.calls++; g.obj = obj; }

#define IS(name) (g.calls == 1 && strcmp(g.fn, name) == 0)
#define NEXT() memset(&g, 0, sizeof g)

int main(void) {
    static int m[8];
    aa_ctx c;
    aatip_slot slot;
    static const unsigned char runs[2] = {1, 2};
    static const uint32_t rgbs[3] = {5, 6, 7};
    static const int holes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint32_t rgb[2] = {0x112233, 0x445566};
    memset(&c, 0xAB, sizeof c);
    c.obj = (Object*)&m[1];

    NEXT(); aa_setup((Object*)&m[0], &c, rgb, 2);
    CHECK(IS("setup") && g.ctx == &c && g.obj == &m[0] && g.v0 == 0x112233 && g.nsize == UP_DRAW_NSIZE(2, sizeof(aa_ctx), sizeof(aatip_slot)), "setup packs n and both sizes");
    CHECK(((aa_ctx*)&c)->backend == (int)0xABABABAB, "an accepted setup leaves the ctx to the library");
    g_setup_result = 0; NEXT(); aa_setup((Object*)&m[0], &c, rgb, 2); g_setup_result = 1;
    { aa_ctx z; memset(&z, 0, sizeof z); CHECK(memcmp(&c, &z, sizeof c) == 0, "a refused setup zeroes the ctx"); }
    c.obj = (Object*)&m[1];
    NEXT(); aa_cleanup(&c); CHECK(IS("cleanup") && g.ctx == &c, "cleanup");
    NEXT(); aa_set_font_px(&c, 18); CHECK(IS("font") && g.v0 == 18 && g.v1 == 0 && g.obj == &m[1], "font px uses the ctx object");
    NEXT(); aa_plain_font(&c, (Object*)&m[2], 14, 1); CHECK(IS("font") && g.v1 == (UP_FONT_PLAIN | UP_FONT_BOLD) && g.obj == &m[2], "plain bold");
    NEXT(); aa_plain_font(&c, (Object*)&m[2], 14, 0); CHECK(IS("font") && g.v1 == UP_FONT_PLAIN, "plain regular");
    NEXT(); aa_free(); CHECK(g.calls == 0, "aa_free is a no-op");
    NEXT(); CHECK(aa_live((Object*)&m[3]) == 1 && IS("live") && g.obj == &m[3], "live");
    NEXT(); aa_hold(&c, 0x010203); CHECK(IS("pen") && g.v0 == 0x010203 && g.v1 == 1, "hold");
    NEXT(); CHECK(aa_nearest_pen(&c, 0x010203) == 9 && IS("pen") && g.v1 == 0, "nearest pen");
    NEXT(); aa_rect(&c, (struct RastPort*)&m[4], 1, 2, 3, 4, 0xAA);
    CHECK(IS("shape") && g.sh.kind == UP_SHAPE_RECT && g.sh.x == 1 && g.sh.y == 2 && g.sh.w == 3 && g.sh.h == 4 && g.sh.rgb == 0xAA && g.rp == &m[4], "rect");
    NEXT(); aa_round_fill(&c, (struct RastPort*)&m[4], 1, 2, 3, 4, 5, 0xBB);
    CHECK(IS("shape") && g.sh.kind == UP_SHAPE_ROUND_FILL && g.sh.radius == 5 && g.sh.rgb == 0xBB && g.sh.w == 3, "round fill");
    NEXT(); aa_round_frame(&c, NULL, 1, 2, 3, 4, 5, 0xBB); CHECK(IS("shape") && g.sh.kind == UP_SHAPE_ROUND_FRAME, "round frame");
    NEXT(); aa_round_corners(&c, NULL, 1, 2, 3, 4, 5, 0xBB); CHECK(IS("shape") && g.sh.kind == UP_SHAPE_ROUND_CORNERS, "round corners");
    NEXT(); aa_round_fill_hole(&c, NULL, 1, 2, 3, 4, 5, 0xBB, 6, 7, 8, 9);
    CHECK(IS("shape") && g.sh.kind == UP_SHAPE_ROUND_FILL_HOLE && g.sh.p0 != NULL, "round fill hole");
    NEXT(); aa_round_fill_holes(&c, NULL, 1, 2, 3, 4, 5, 0xBB, holes, 2);
    CHECK(IS("shape") && g.sh.kind == UP_SHAPE_ROUND_FILL_HOLES && g.sh.p0 == holes && g.sh.n == 2, "holes");
    NEXT(); aa_low_outline(&c, NULL, 1, 2, 3, 4, 5); CHECK(IS("shape") && g.sh.kind == UP_SHAPE_LOW_OUTLINE && g.sh.radius == 5, "low outline");
    NEXT(); aa_runs(&c, NULL, 8, 9, runs, 2, rgbs);
    CHECK(IS("shape") && g.sh.kind == UP_SHAPE_RUNS && g.sh.x == 8 && g.sh.y == 9 && g.sh.p0 == runs && g.sh.p1 == rgbs && g.sh.n == 2, "runs");
    NEXT(); aa_erase(&c, (Object*)&m[5], 1, 2, 3, 4);
    CHECK(IS("shape") && g.sh.kind == UP_SHAPE_ERASE && g.obj == &m[5] && g.sh.h == 4, "erase");
    NEXT(); CHECK(aa_text_width(&c, (Object*)&m[5], "ab") == 21 && IS("size") && g.v0 == 1, "text width");
    NEXT(); CHECK(aa_text_height(&c, (Object*)&m[5]) == 21 && IS("size") && g.v0 == 0, "text height");
    NEXT(); aa_text(&c, (Object*)&m[5], (struct RastPort*)&m[4], 3, 4, "hi", 0x1, 0x2);
    CHECK(IS("text") && g.tx.x == 3 && g.tx.y == 4 && strcmp(g.tx.str, "hi") == 0 && g.tx.fg == 1 && g.tx.bg == 2 && g.rp == &m[4], "text");
    NEXT(); CHECK(aatip_super_set((struct IClass*)&m[1], (Object*)&m[2], (struct opSet*)&m[3], 40) == 3 && IS("tipsuper") && g.v0 == 40, "tip super set");
    NEXT(); CHECK(aatip_get((struct opGet*)&m[1], &slot) == 1 && IS("tipget") && g.rp == &slot, "tip get");
    NEXT(); aatip_event((Object*)&m[1], &slot, (const struct IntuiMessage*)&m[2], 1); CHECK(IS("tipevent") && g.ctx == &slot && g.v0 == 1, "tip event");
    NEXT(); CHECK(aatip_fire((Object*)&m[1], &c, &slot) == 4 && IS("tipfire") && g.ctx == &c && g.rp == &slot, "tip fire");
    NEXT(); aatip_drop((Object*)&m[1]); CHECK(IS("tipdrop") && g.obj == &m[1], "tip drop");
    puts(g_fail ? "FAIL" : "OK");
    return g_fail;
}
