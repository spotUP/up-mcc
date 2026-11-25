/* I1491a2 (up-mcc.library version 2): the entry points of the draw layer. Each one unpacks its arguments and runs the existing aa_*
 * or aatip_* function of src/mui_aa_draw.c (the library copy, compiled with UP_DRAW_LIBRARY) and src/mui_aatip.c, so the pixels do not
 * change. The class libraries call these through lib/classlib_draw_shims.c. Design 2026-11-22-i1491a2-draw-behind-library.md section 2. */
#include <stdint.h>

#include "mui_aa_draw.h"
#include "mui_aatip.h"
#include <mcc/up.h>

#include "map/mui_log.h"

#define CTX(p) ((aa_ctx*)(p))
/* A context the library never set up (UP_DrawSetup refused it, or the class never called it) draws nothing. */
#define CTX_OK(c) ((c) != NULL && (c)->linked)

#if defined(__m68k__)
/* The ABI of the class libraries' instance data (up_lib.h): a change is a version bump of up-mcc.library. */
_Static_assert(sizeof(aatip_slot) == 12, "aatip_slot ABI 2 changed");
_Static_assert(sizeof(struct up_bar) == 28, "up_bar ABI changed");
#endif

LONG UP_DrawSetup(void* ctx, void* obj, const ULONG* rgb, ULONG nsize) {
    static int logged;
    if ((nsize >> 16) != sizeof(aa_ctx) || ((nsize >> 8) & 0xFF) != sizeof(aatip_slot)) {
        if (!logged) {
            logged = 1;
            aa_log("up-mcc: draw ABI mismatch (class ctx %lu slot %lu, library ctx %lu slot %lu)\n", (unsigned long)(nsize >> 16),
                   (unsigned long)((nsize >> 8) & 0xFF), (unsigned long)sizeof(aa_ctx), (unsigned long)sizeof(aatip_slot));
        }
        return 0;
    }
    aa_setup((Object*)obj, CTX(ctx), (const uint32_t*)rgb, (int)(nsize & 0xFF));
    return 1;
}

void UP_DrawCleanup(void* ctx) {
    if (CTX_OK(CTX(ctx))) {
        aa_cleanup(CTX(ctx));
    }
}

void UP_DrawFont(void* ctx, void* obj, LONG px, LONG flags) {
    if (!CTX_OK(CTX(ctx))) {
        return;
    }
    if ((flags & UP_FONT_PLAIN) != 0) {
        aa_plain_font(CTX(ctx), (Object*)obj, (int)px, (flags & UP_FONT_BOLD) != 0);
    } else {
        aa_set_font_px(CTX(ctx), (int)px);
    }
}

LONG UP_DrawPen(void* ctx, ULONG rgb, LONG hold) {
    if (!CTX_OK(CTX(ctx))) {
        return 0;
    }
    if (hold != 0) {
        aa_hold(CTX(ctx), rgb);
        return 0;
    }
    return aa_nearest_pen(CTX(ctx), rgb);
}

void UP_DrawShape(void* ctx, void* rp, void* obj, const void* shape) {
    const struct up_shape* s = (const struct up_shape*)shape;
    aa_ctx* c = CTX(ctx);
    struct RastPort* r = (struct RastPort*)rp;
    if (!CTX_OK(c) || s == NULL) {
        return;
    }
    switch (s->kind) {
    case UP_SHAPE_RECT: aa_rect(c, r, s->x, s->y, s->w, s->h, s->rgb); break;
    case UP_SHAPE_ROUND_FILL: aa_round_fill(c, r, s->x, s->y, s->w, s->h, s->radius, s->rgb); break;
    case UP_SHAPE_ROUND_FRAME: aa_round_frame(c, r, s->x, s->y, s->w, s->h, s->radius, s->rgb); break;
    case UP_SHAPE_ROUND_CORNERS: aa_round_corners(c, r, s->x, s->y, s->w, s->h, s->radius, s->rgb); break;
    case UP_SHAPE_ROUND_FILL_HOLE: {
        const int* h = (const int*)s->p0;
        aa_round_fill_hole(c, r, s->x, s->y, s->w, s->h, s->radius, s->rgb, h[0], h[1], h[2], h[3]);
        break;
    }
    case UP_SHAPE_ROUND_FILL_HOLES:
        aa_round_fill_holes(c, r, s->x, s->y, s->w, s->h, s->radius, s->rgb, (const int*)s->p0, s->n);
        break;
    case UP_SHAPE_LOW_OUTLINE: aa_low_outline(c, r, s->x, s->y, s->w, s->h, s->radius); break;
    case UP_SHAPE_RUNS: aa_runs(c, r, s->x, s->y, (const unsigned char*)s->p0, s->n, (const uint32_t*)s->p1); break;
    case UP_SHAPE_ERASE: aa_erase(c, (Object*)obj, s->x, s->y, s->w, s->h); break;
    default: break;
    }
}

LONG UP_DrawTextSize(void* ctx, void* obj, STRPTR str) {
    if (!CTX_OK(CTX(ctx))) {
        return 0;
    }
    return str == NULL ? aa_text_height(CTX(ctx), (Object*)obj) : aa_text_width(CTX(ctx), (Object*)obj, (const char*)str);
}

void UP_DrawText(void* ctx, void* obj, void* rp, const void* text) {
    const struct up_text* t = (const struct up_text*)text;
    if (CTX_OK(CTX(ctx)) && t != NULL) {
        aa_text(CTX(ctx), (Object*)obj, (struct RastPort*)rp, t->x, t->y, (const char*)t->str, t->fg, t->bg);
    }
}

LONG UP_DrawLive(void* obj) {
    return aa_live((Object*)obj);
}

ULONG UP_TipFire(void* obj, void* ctx, void* slot) {
    return CTX_OK(CTX(ctx)) ? aatip_fire((Object*)obj, CTX(ctx), (const aatip_slot*)slot) : 0;
}

ULONG UP_TipSuperSet(void* cl, void* obj, void* msg, ULONG slotoff) {
    return aatip_super_set((struct IClass*)cl, (Object*)obj, (struct opSet*)msg, slotoff);
}

LONG UP_TipGet(void* msg, void* slot) {
    return aatip_get((struct opGet*)msg, (const aatip_slot*)slot);
}

void UP_TipEvent(void* obj, void* slot, void* imsg, LONG in) {
    aatip_event((Object*)obj, (aatip_slot*)slot, (const struct IntuiMessage*)imsg, (int)in);
}

void UP_TipDrop(void* obj) {
    aatip_drop((Object*)obj);
}

LONG UP_DrawBar(void* ctx, void* rp, void* bar) {
    struct up_bar* b = (struct up_bar*)bar;
    if (ctx == NULL || b == NULL || (!CTX(ctx)->linked && b->op <= UP_BAR_PAINT)) {
        return 0;
    }
    return aa_bar(CTX(ctx), (struct RastPort*)rp, b);
}
