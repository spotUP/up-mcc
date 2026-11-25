/* UP-TextArea: a minimal multi-line text editor. Area subclass; the look is UP-Field's (face, outline, placeholder, tooltip from
 * map/mui_aa.c and mui_aatip.c). All line, caret and edit maths is mui_text_lines.c (mtl_*): this file is glue between those helpers,
 * the aa_* draw calls and the MUI events. Keys: the arrows, Home, End and the wheel come as RAWKEY; characters, Backspace, Delete and
 * Return come as IDCMP_VANILLAKEY, so no keymap library is needed. Selection is anchor..caret (empty when equal); the clipboard is
 * clipboard.device unit 0 holding FORM FTXT (mui_ftxt.c does the IFF bytes). */
#include "mui_aatextarea_class.h"
#include "mui_aa_class.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "map/mui_aa.h"
#include "mui_aa_draw.h"
#include "mui_aatip.h"
#include "mui_aaedit.h"
#include "mui_text_lines.h"

extern struct Library* MUIMasterBase;

#define TA_WHEEL_UP 0x7A /* as aalist */
#define TA_WHEEL_DOWN 0x7B
#define TA_WHEEL_STEP 3
#define TA_DEF_MAXLEN 1024
#define TA_LINE_BUF 256 /* shortcut: a wrapped line over 255 bytes is measured and drawn cut, upgrade when a font that small is used */

typedef struct ta_data {
    char* buf; /* cap bytes, NUL terminated, caller-visible through UPA_TextArea_Text */
    int cap, len, caret, anchor, pref_x, top;
    int dragging; /* left button held after a click inside: 1 mouse moves extend the selection, 2 they drag the scroll bar thumb */
    int sb_grab;  /* thumb drag: pointer offset below the thumb top */
    int sbw; /* width of the UP-Scroller bar the class draws on its right (aa_bar) */
    MtlLine* lines;
    int nlines, line_cap, wrap_w;
    int dirty;  /* text changed: wrap again */
    int reveal; /* caret moved: the next draw scrolls it into view (the wheel does not set it) */
    const char* placeholder;
    mui_aa_field_t m;
    aa_ctx aa;
    int shown; /* MUIM_Show seen, MUIM_Hide not yet (AA_SHOWN_CASES); outside aa_ctx, whose ABI 2 layout is frozen */
    struct MUI_EventHandlerNode ehn;
    aatip_slot tip;
    int error; /* UPA_TextArea_Error: ERROR outline until the next edit */
    int own_font; /* MUIA_Font given at OM_NEW: the text draws in that bitmap font (the PLAIN backend), not the TTF */
} ta_data;

typedef struct ta_geom {
    int x, y, w, lh, vis; /* text origin, text width, line height, whole lines that fit */
    int bx, bh;           /* scroll bar left edge and height (its top is y) */
} ta_geom;

typedef struct ta_fit {
    ta_data* d;
    Object* obj;
} ta_fit;

static struct MUI_CustomClass* g_textarea_class;

/* MtlMeasure / mui_aa_width_fn: ink width of the first len bytes in the gadget's font. */
static int ta_width(void* ctx, const char* s, int len) {
    ta_fit* f = (ta_fit*)ctx;
    char b[TA_LINE_BUF];
    if (len >= TA_LINE_BUF) {
        len = TA_LINE_BUF - 1;
    }
    memcpy(b, s, (size_t)len);
    b[len] = '\0';
    return aa_text_width(&f->d->aa, f->obj, b);
}

static int ta_focus(Object* obj) {
    ULONG act = 0;
    if (!aa_live(obj)) {
        return 0;
    }
    GetAttr(MUIA_Window_ActiveObject, _win(obj), &act);
    return (Object*)act == obj;
}

static void ta_redraw(Object* obj) {
    if (aa_live(obj)) {
        aa_redraw(obj, MADF_DRAWOBJECT);
    }
}

static void ta_geom_of(Object* obj, ta_data* d, ta_geom* g) {
    int inner;
    g->x = _mleft(obj) + d->m.pad_x;
    g->y = _mtop(obj) + d->m.pad_y + d->m.outline;
    g->w = _mwidth(obj) - 2 * d->m.pad_x - d->sbw; /* the bar strip is always reserved, so the wrap width never flips (as UP-List keeps its bar) */
    g->bx = g->x + g->w + d->m.pad_x / 2;
    g->lh = aa_text_height(&d->aa, obj);
    inner = _mheight(obj) - 2 * (d->m.pad_y + d->m.outline);
    g->bh = inner;
    if (g->w < 1) {
        g->w = 1;
    }
    if (g->lh < 1) {
        g->lh = 1;
    }
    g->vis = inner / g->lh < 1 ? 1 : inner / g->lh;
}

/* Wrap again when the text or the width changed; the line array grows to the line count (on a failed grow the tail is not shown). */
static void ta_layout(Object* obj, ta_data* d, const ta_geom* g) {
    ta_fit f;
    int n;
    if (!d->dirty && d->wrap_w == g->w) {
        return;
    }
    f.d = d;
    f.obj = obj;
    n = mtl_wrap(d->buf, d->len, g->w, ta_width, &f, d->lines, d->line_cap);
    if (n > d->line_cap) {
        MtlLine* nl = (MtlLine*)AllocVec((ULONG)(n + 16) * sizeof(MtlLine), MEMF_ANY);
        if (nl != NULL) {
            FreeVec(d->lines);
            d->lines = nl;
            d->line_cap = n + 16;
            mtl_wrap(d->buf, d->len, g->w, ta_width, &f, d->lines, d->line_cap);
        } else {
            n = d->line_cap;
        }
    }
    d->nlines = n;
    d->wrap_w = g->w;
    d->dirty = 0;
}

static void ta_ensure(Object* obj, ta_data* d, ta_geom* g) {
    ta_geom_of(obj, d, g);
    ta_layout(obj, d, g);
}

/* Copy s into the buffer (cut to MaxLen). Returns 1 when the text changed; s == the buffer itself (an edit announcing itself) is no change. */
static int ta_set_text(ta_data* d, const char* s) {
    int n;
    if (s == d->buf) {
        return 0;
    }
    if (s == NULL) {
        s = "";
    }
    n = (int)strlen(s);
    if (n > d->cap - 1) {
        n = d->cap - 1;
    }
    memmove(d->buf, s, (size_t)n);
    d->buf[n] = '\0';
    d->len = d->caret = d->anchor = n;
    d->pref_x = -1;
    d->top = 0;
    d->dirty = d->reveal = 1;
    return 1;
}

static void ta_moved(Object* obj, ta_data* d, int off, int keep_pref, int extend) {
    d->caret = off;
    if (!extend) {
        d->anchor = off;
    }
    if (!keep_pref) {
        d->pref_x = -1;
    }
    d->reveal = 1;
    ta_redraw(obj);
}

/* After an edit of the buffer: wrap again, show the caret, and announce the text (set notifies; the pointer is the buffer, so no copy). */
static void ta_edited(Object* obj, ta_data* d) {
    d->error = 0; /* the edit repaints, so the outline goes back to focus */
    d->anchor = d->caret;
    d->dirty = d->reveal = 1;
    d->pref_x = -1;
    ta_redraw(obj);
    set(obj, UPA_TextArea_Text, (ULONG)(uintptr_t)d->buf);
}

static int ta_selection(const ta_data* d, int* lo, int* hi) {
    return mtl_sel_range(d->anchor, d->caret, d->len, lo, hi);
}

/* Replace the selection (or insert at the caret) by s. Returns 1 when the text changed. */
static int ta_put(ta_data* d, const char* s, int slen) {
    int lo, hi, removed, put;
    if (!ta_selection(d, &lo, &hi)) {
        lo = hi = d->caret;
    }
    removed = hi - lo;
    put = mtl_replace(d->buf, d->cap, &d->len, lo, hi, s, slen);
    d->caret = lo + put;
    return removed > 0 || put > 0;
}

/* Ctrl or Amiga + C/X/V/A. Returns 1 when the key was a clipboard or select-all command (handled). */
static int ta_clipboard_key(Object* obj, ta_data* d, int code, int qual) {
    int lo, hi, edited = 0;
    int key = mtl_clip_key(code, qual & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND));
    if (key == 'a') {
        d->anchor = 0;
        ta_moved(obj, d, d->len, 0, 1);
        return 1;
    }
    if (key == 'c' || key == 'x') {
        if (ta_selection(d, &lo, &hi)) {
            aaedit_clip_put(d->buf + lo, hi - lo);
            if (key == 'x') {
                edited = ta_put(d, "", 0);
            }
        }
    } else if (key == 'v') {
        char* t = (char*)AllocVec((ULONG)d->cap, MEMF_ANY);
        if (t != NULL) {
            int n = aaedit_clip_get(t, d->cap);
            if (n > 0) {
                edited = ta_put(d, t, n);
            }
            FreeVec(t);
        }
    } else {
        return 0;
    }
    if (edited) {
        ta_edited(obj, d);
    }
    return 1;
}

static ULONG ta_setup(struct IClass* cl, Object* obj, Msg msg) {
    ta_data* d = INST_DATA(cl, obj);
    mui_aa_check_op_t ops[MUI_AA_FIELD_OPS];
    int k, n;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    if (d->own_font) {
        d->aa.backend = AA_PLAIN; /* aa_text and its measures then use _font(obj), the MUIA_Font (a fixed font for NFO art) */
    }
    mui_aa_field_metrics(d->aa.font_px, d->aa.aspect_q8, &d->m);
    {
        struct up_bar b = {UP_BAR_SETUP};
        d->sbw = (WORD)aa_bar(&d->aa, NULL, &b);
    }
    for (k = 0; k <= MUI_AA_FIELD_ERROR; k++) {
        n = mui_aa_field_plan(d->aa.tok, &d->m, k, ops);
        while (n-- > 0) {
            aa_hold(&d->aa, ops[n].rgb);
        }
    }
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_ACCENT]);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_ACCENT_TEXT]);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_TEXT]);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_TEXT_MUTED]);
    DoMethod(_win(obj), MUIM_Notify, MUIA_Window_ActiveObject, MUIV_EveryTime, (ULONG)obj, 1, UPM_TextArea_Redraw);
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_VANILLAKEY;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    d->dirty = d->reveal = 1;
    return TRUE;
}

static ULONG ta_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    ta_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_KillNotifyObj, MUIA_Window_ActiveObject, (ULONG)obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    d->tip.in = 0;
    aatip_drop(obj);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG ta_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    ta_data* d = INST_DATA(cl, obj);
    int lh = aa_text_height(&d->aa, obj), px = d->aa.font_px;
    int ph = 2 * (d->m.pad_y + d->m.outline), pw = 2 * d->m.pad_x + d->sbw;
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += pw + 8 * px;
    msg->MinMaxInfo->DefWidth += pw + 24 * px;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += ph + 2 * lh;
    msg->MinMaxInfo->DefHeight += ph + 4 * lh;
    msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;
    return 0;
}

static ULONG ta_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    ta_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    mui_aa_check_op_t ops[MUI_AA_FIELD_OPS];
    ta_geom g;
    ta_fit f;
    int focus, n, i, maxtop;
    int l, t, w, h, has_sel, sel_lo, sel_hi;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    w = _mwidth(obj);
    h = _mheight(obj);
    focus = d->error ? MUI_AA_FIELD_ERROR : ta_focus(obj);
    n = mui_aa_field_plan(d->aa.tok, &d->m, focus, ops);
    aaedit_frame(&d->aa, rp, l, t, w, h, ops, n);

    ta_ensure(obj, d, &g);
    f.d = d;
    f.obj = obj;
    if (d->reveal) {
        d->top = mtl_scroll_top(mtl_line_of(d->lines, d->nlines, d->caret), g.vis, d->top, d->nlines);
        d->reveal = 0;
    }
    maxtop = d->nlines > g.vis ? d->nlines - g.vis : 0;
    d->top = d->top < 0 ? 0 : d->top > maxtop ? maxtop : d->top;
    {
        struct up_bar b = {UP_BAR_PAINT, d->dragging == 2 ? MUI_AA_PRESSED : MUI_AA_NORMAL, g.bx, g.y, d->sbw, g.bh, 0, 0, d->top, g.vis, d->nlines};
        aa_bar(&d->aa, rp, &b);
    }

    if (d->placeholder != NULL) {
        uint32_t rgb;
        if (mui_aa_field_placeholder(d->aa.tok, d->buf, focus, &rgb)) {
            char fit[160];
            mui_aa_text_fit(d->placeholder, g.w, ta_width, &f, fit, (int)sizeof fit);
            aa_text(&d->aa, obj, rp, g.x, g.y, fit, rgb, d->aa.tok[MUI_AA_FIELD_FACE]);
        }
    }
    has_sel = ta_selection(d, &sel_lo, &sel_hi);
    for (i = d->top; i < d->nlines && i < d->top + g.vis; i++) {
        char ln[TA_LINE_BUF];
        int len = d->lines[i].len < TA_LINE_BUF - 1 ? d->lines[i].len : TA_LINE_BUF - 1;
        int ly = g.y + (i - d->top) * g.lh;
        if (len > 0) {
            memcpy(ln, d->buf + d->lines[i].start, (size_t)len);
            ln[len] = '\0';
            aa_text(&d->aa, obj, rp, g.x, ly, ln, d->aa.tok[SEL_GUI_TOKEN_TEXT], d->aa.tok[MUI_AA_FIELD_FACE]);
        }
        if (has_sel) {
            /* selected bytes of this line; a selected newline also covers one blank cell */
            int ls = d->lines[i].start, a = sel_lo > ls ? sel_lo - ls : 0, b = sel_hi < ls + len ? sel_hi - ls : len;
            int past = i + 1 < d->nlines && d->lines[i + 1].start > ls + d->lines[i].len && sel_lo <= ls + d->lines[i].len && sel_hi > ls + d->lines[i].len;
            if (b > a || past) {
                int x0 = g.x + mtl_x_of(d->buf, d->lines, i, a, ta_width, &f);
                int x1 = g.x + mtl_x_of(d->buf, d->lines, i, b, ta_width, &f);
                if (past) {
                    x1 += d->aa.font_px / 2 > 2 ? d->aa.font_px / 2 : 2;
                }
                if (x1 > g.x + g.w) {
                    x1 = g.x + g.w;
                }
                if (x1 > x0) {
                    aa_rect(&d->aa, rp, x0, ly, x1 - 1, ly + g.lh - 1, d->aa.tok[SEL_GUI_TOKEN_ACCENT]);
                    if (b > a) {
                        char seg[TA_LINE_BUF];
                        memcpy(seg, ln + a, (size_t)(b - a));
                        seg[b - a] = '\0';
                        aa_text(&d->aa, obj, rp, x0, ly, seg, d->aa.tok[SEL_GUI_TOKEN_ACCENT_TEXT], d->aa.tok[SEL_GUI_TOKEN_ACCENT]);
                    }
                }
            }
        }
    }
    if (focus) {
        int line = mtl_line_of(d->lines, d->nlines, d->caret);
        if (line >= d->top && line < d->top + g.vis) {
            int cw = d->aa.font_px >= 16 ? 2 : 1;
            int cx = g.x + mtl_x_of(d->buf, d->lines, line, mtl_column_of(d->lines, d->nlines, d->caret), ta_width, &f);
            int cy = g.y + (line - d->top) * g.lh;
            if (cx > g.x + g.w - cw) {
                cx = g.x + g.w - cw;
            }
            aa_rect(&d->aa, rp, cx, cy, cx + cw - 1, cy + g.lh - 1, d->aa.tok[SEL_GUI_TOKEN_TEXT]);
        }
    }
    return 0;
}

static ULONG ta_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    ta_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    ta_geom g;
    ta_fit f;
    int in, off, code, shift, has_sel, lo, hi;
    if (im == NULL) {
        return 0;
    }
    in = aa_inside(d->shown, obj, im);
    aatip_event(obj, &d->tip, im, in);
    f.d = d;
    f.obj = obj;
    if (im->Class == IDCMP_MOUSEBUTTONS) {
        if (im->Code == SELECTDOWN && in) {
            int line;
            if (aa_live(obj)) {
                set(_win(obj), MUIA_Window_ActiveObject, (ULONG)obj);
            }
            ta_ensure(obj, d, &g);
            if (im->MouseX >= g.bx) { /* the scroll bar: page on the track, drag the thumb */
                struct up_bar b = {UP_BAR_PRESS, 0, 0, 0, 0, g.bh, im->MouseY - g.y, 0, d->top, g.vis, d->nlines};
                d->top = (int)aa_bar(&d->aa, NULL, &b);
                d->sb_grab = b.grab;
                d->dragging = d->sb_grab >= 0 ? 2 : 0;
                ta_redraw(obj);
                return MUI_EventHandlerRC_Eat;
            }
            line = d->top + (im->MouseY - g.y) / g.lh;
            line = line < d->top ? d->top : line;
            off = mtl_offset_at(d->buf, d->len, d->lines, d->nlines, line, im->MouseX - g.x, ta_width, &f);
            ta_moved(obj, d, off, 0, im->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT));
            d->reveal = 0; /* a click inside the view must not scroll it */
            d->dragging = 1;
            return MUI_EventHandlerRC_Eat;
        }
        if (im->Code == (SELECTDOWN | IECODE_UP_PREFIX) && d->dragging) {
            if (d->dragging == 2) {
                ta_redraw(obj); /* the thumb leaves its pressed colour */
            }
            d->dragging = 0;
        }
        return 0;
    }
    if (im->Class == IDCMP_MOUSEMOVE) {
        if (d->dragging == 2) {
            ta_ensure(obj, d, &g);
            struct up_bar b = {UP_BAR_DRAG, 0, 0, 0, 0, g.bh, im->MouseY - g.y, (WORD)d->sb_grab, 0, g.vis, d->nlines};
            d->top = (int)aa_bar(&d->aa, NULL, &b);
            ta_redraw(obj);
        } else if (d->dragging) {
            int line;
            ta_ensure(obj, d, &g);
            line = d->top + (im->MouseY - g.y) / g.lh;
            line = im->MouseY < g.y ? d->top - 1 : line; /* above the view: one line up per move, the draw scrolls it into view */
            line = line < 0 ? 0 : line;
            off = mtl_offset_at(d->buf, d->len, d->lines, d->nlines, line, im->MouseX - g.x, ta_width, &f);
            ta_moved(obj, d, off, 0, 1);
        }
        return 0;
    }
    code = im->Code;
    if (im->Class == IDCMP_RAWKEY) {
        if (code & IECODE_UP_PREFIX) {
            return 0;
        }
        if (code == TA_WHEEL_UP || code == TA_WHEEL_DOWN) {
            if (!in) {
                return 0;
            }
            ta_ensure(obj, d, &g);
            d->top += code == TA_WHEEL_UP ? -TA_WHEEL_STEP : TA_WHEEL_STEP; /* draw clamps it */
            ta_redraw(obj);
            return MUI_EventHandlerRC_Eat;
        }
        if (!ta_focus(obj)) {
            return 0;
        }
        ta_ensure(obj, d, &g);
        shift = im->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT);
        has_sel = ta_selection(d, &lo, &hi);
        switch (code) {
        case 0x4C: /* up */
        case 0x4D: /* down */
            off = mtl_move_vertical(d->buf, d->len, d->lines, d->nlines, d->caret, code == 0x4C ? -1 : 1, &d->pref_x, ta_width, &f);
            ta_moved(obj, d, off, 1, shift);
            return MUI_EventHandlerRC_Eat;
        case 0x4E: /* right: without Shift a selection collapses to its end */
            ta_moved(obj, d, !shift && has_sel ? hi : d->caret < d->len ? d->caret + 1 : d->caret, 0, shift);
            return MUI_EventHandlerRC_Eat;
        case 0x4F: /* left: ... or to its start */
            ta_moved(obj, d, !shift && has_sel ? lo : d->caret > 0 ? d->caret - 1 : 0, 0, shift);
            return MUI_EventHandlerRC_Eat;
        case 0x70: /* Home, as aalist */
            ta_moved(obj, d, mtl_line_home(d->lines, d->nlines, d->caret), 0, shift);
            return MUI_EventHandlerRC_Eat;
        case 0x71: /* End */
            ta_moved(obj, d, mtl_line_end(d->buf, d->len, d->lines, d->nlines, d->caret), 0, shift);
            return MUI_EventHandlerRC_Eat;
        }
        return 0;
    }
    if (im->Class == IDCMP_VANILLAKEY && ta_focus(obj)) {
        char c = (char)code;
        if (ta_clipboard_key(obj, d, code, im->Qualifier)) {
            return MUI_EventHandlerRC_Eat;
        }
        if (im->Qualifier & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)) {
            return 0; /* other Amiga-key shortcuts stay with the window */
        }
        has_sel = ta_selection(d, &lo, &hi);
        if (code == 0x08 || code == 0x7F) { /* Backspace, Delete: a selection goes whole, else one byte */
            if (has_sel) {
                ta_put(d, "", 0);
                ta_edited(obj, d);
            } else if (code == 0x08 && d->caret > 0) {
                d->caret = mtl_backspace(d->buf, &d->len, d->caret);
                ta_edited(obj, d);
            } else if (code == 0x7F && mtl_delete(d->buf, &d->len, d->caret)) {
                ta_edited(obj, d);
            }
        } else if (code == 0x0D || (code >= 0x20 && code <= 0x7E) || (code >= 0xA0 && code <= 0xFF)) {
            if (code == 0x0D) {
                c = '\n';
            }
            if (ta_put(d, &c, 1)) {
                ta_edited(obj, d);
            }
        } else if (code == 0x09) {
            /* Asking for VANILLAKEY makes Intuition deliver Tab only as vanilla 0x09 (no RAWKEY 0x42), so MUI's own forward
             * cycle never sees it; Shift+Tab has no vanilla code and still arrives as RAWKEY. Cycle here; never insert a TAB. */
            set(_win(obj), MUIA_Window_ActiveObject, (ULONG)MUIV_Window_ActiveObject_Next);
        } else {
            return 0; /* Esc and the rest stay with the window */
        }
        return MUI_EventHandlerRC_Eat;
    }
    return 0;
}

static ULONG ta_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    ta_data* d;
    struct opSet nm;
    struct TagItem def[4];
    struct TagItem* tags = msg->ops_AttrList;
    LONG maxlen = (LONG)GetTagData(UPA_TextArea_MaxLen, TA_DEF_MAXLEN, tags);
    def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
    def[1].ti_Tag = MUIA_CycleChain, def[1].ti_Data = (ULONG)TRUE;
    def[2].ti_Tag = MUIA_FillArea, def[2].ti_Data = (ULONG)FALSE; /* the draw paints every pixel; a MUI fill first would flash */
    def[3].ti_Tag = TAG_MORE, def[3].ti_Data = (ULONG)(uintptr_t)tags;
    nm = *msg;
    nm.ops_AttrList = def;
    obj = (Object*)aatip_super_set(cl, obj, &nm, offsetof(ta_data, tip));
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj); /* BOOPSI instance data starts cleared; the tooltip slot above is already filled, so no memset */
    d->own_font = FindTagItem(MUIA_Font, tags) != NULL;
    d->cap = (maxlen < 1 ? TA_DEF_MAXLEN : maxlen) + 1;
    d->line_cap = 32;
    d->pref_x = -1;
    d->buf = (char*)AllocVec((ULONG)d->cap, MEMF_CLEAR);
    d->lines = (MtlLine*)AllocVec((ULONG)d->line_cap * sizeof(MtlLine), MEMF_ANY);
    if (d->buf == NULL || d->lines == NULL) {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }
    d->placeholder = (const char*)GetTagData(UPA_TextArea_Placeholder, 0, tags);
    ta_set_text(d, (const char*)GetTagData(UPA_TextArea_Text, 0, tags));
    return (ULONG)obj;
}

static ULONG ta_dispose(struct IClass* cl, Object* obj, Msg msg) {
    ta_data* d = INST_DATA(cl, obj);
    FreeVec(d->buf);
    FreeVec(d->lines);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG ta_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    ta_data* d = INST_DATA(cl, obj);
    struct TagItem* tag = FindTagItem(UPA_TextArea_Text, msg->ops_AttrList);
    if (tag != NULL) {
        if (ta_set_text(d, (const char*)tag->ti_Data)) {
            ta_redraw(obj);
        }
        tag->ti_Data = (ULONG)(uintptr_t)d->buf; /* the notify delivers the buffer, never the caller's temporary string */
    }
    tag = FindTagItem(UPA_TextArea_Error, msg->ops_AttrList);
    if (tag != NULL && d->error != (tag->ti_Data != 0)) {
        d->error = tag->ti_Data != 0;
        ta_redraw(obj);
    }
    return aatip_super_set(cl, obj, msg, offsetof(ta_data, tip));
}

static ULONG ta_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    ta_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == UPA_TextArea_Text) {
        *msg->opg_Storage = (ULONG)(uintptr_t)d->buf;
        return TRUE;
    }
    if (msg->opg_AttrID == UPA_TextArea_Error) {
        *msg->opg_Storage = (ULONG)d->error;
        return TRUE;
    }
    if (msg->opg_AttrID == UPA_TextArea_MaxLen) {
        *msg->opg_Storage = (ULONG)(d->cap - 1);
        return TRUE;
    }
    if (aatip_get(msg, &d->tip)) {
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) ta_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aatextarea_dispatch(void);
const struct aa_class_desc aatextarea_class_desc = {MUIC_Area, sizeof(ta_data), aatextarea_dispatch, &g_textarea_class};
__asm__(".text\n.even\n.globl _aatextarea_dispatch\n_aatextarea_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _ta_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) ta_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return ta_new(cl, obj, (struct opSet*)msg);
    case OM_DISPOSE:
        return ta_dispose(cl, obj, msg);
    case OM_SET:
        return ta_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return ta_get(cl, obj, (struct opGet*)msg);
    case UPM_Shared_Fire: {
        ta_data* d = INST_DATA(cl, obj);
        return aatip_fire(obj, &d->aa, &d->tip);
    }
    case MUIM_Show:
        ((ta_data*)INST_DATA(cl, obj))->shown = 1;
        break;
    case MUIM_Hide:
        ((ta_data*)INST_DATA(cl, obj))->shown = 0;
        aatip_drop(obj);
        break;
    case MUIM_HandleEvent:
        return ta_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    case MUIM_Setup:
        return ta_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return ta_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return ta_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return ta_draw(cl, obj, (struct MUIP_Draw*)msg);
    case UPM_TextArea_Redraw:
        aa_redraw(obj, MADF_DRAWOBJECT);
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* UP_NewTextArea(const struct TagItem* tags) {
    if (g_textarea_class == NULL) {
        g_textarea_class = AA_CLASS_CREATE(&aatextarea_class_desc);
    }
    if (g_textarea_class == NULL) {
        return NULL;
    }
    return NewObjectA(g_textarea_class->mcc_Class, NULL, (struct TagItem*)tags);
}

void mui_aatextarea_class_free(void) {
    if (g_textarea_class != NULL) {
        MUI_DeleteCustomClass(g_textarea_class);
        g_textarea_class = NULL;
    }
}
