/* UP-TextArea: a minimal multi-line text editor. Area subclass; the look is UP-Field's (face, outline, placeholder, tooltip from
 * map/mui_aa.c and mui_aatip.c). All line, caret and edit maths is mui_text_lines.c (mtl_*): this file is glue between those helpers,
 * the aa_* draw calls and the MUI events. Keys: the arrows, Home, End and the wheel come as RAWKEY; characters, Backspace, Delete and
 * Return come as IDCMP_VANILLAKEY, so no keymap library is needed. */
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
#include "mui_text_lines.h"

extern struct Library* MUIMasterBase;

#define TA_WHEEL_UP 0x7A /* as aalist */
#define TA_WHEEL_DOWN 0x7B
#define TA_WHEEL_STEP 3
#define TA_DEF_MAXLEN 1024
#define TA_LINE_BUF 256 /* shortcut: a wrapped line over 255 bytes is measured and drawn cut, upgrade when a font that small is used */

typedef struct ta_data {
    char* buf; /* cap bytes, NUL terminated, caller-visible through UPA_TextArea_Text */
    int cap, len, caret, pref_x, top;
    MtlLine* lines;
    int nlines, line_cap, wrap_w;
    int dirty;  /* text changed: wrap again */
    int reveal; /* caret moved: the next draw scrolls it into view (the wheel does not set it) */
    const char* placeholder;
    mui_aa_field_t m;
    aa_ctx aa;
    struct MUI_EventHandlerNode ehn;
    aatip_slot tip;
} ta_data;

typedef struct ta_geom {
    int x, y, w, lh, vis; /* text origin, text width, line height, whole lines that fit */
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
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }
}

static void ta_geom_of(Object* obj, ta_data* d, ta_geom* g) {
    int inner;
    g->x = _mleft(obj) + d->m.pad_x;
    g->y = _mtop(obj) + d->m.pad_y + d->m.outline;
    g->w = _mwidth(obj) - 2 * d->m.pad_x;
    g->lh = aa_text_height(&d->aa, obj);
    inner = _mheight(obj) - 2 * (d->m.pad_y + d->m.outline);
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
    d->len = d->caret = n;
    d->pref_x = -1;
    d->top = 0;
    d->dirty = d->reveal = 1;
    return 1;
}

static void ta_moved(Object* obj, ta_data* d, int off, int keep_pref) {
    d->caret = off;
    if (!keep_pref) {
        d->pref_x = -1;
    }
    d->reveal = 1;
    ta_redraw(obj);
}

/* After an edit of the buffer: wrap again, show the caret, and announce the text (set notifies; the pointer is the buffer, so no copy). */
static void ta_edited(Object* obj, ta_data* d) {
    d->dirty = d->reveal = 1;
    d->pref_x = -1;
    ta_redraw(obj);
    set(obj, UPA_TextArea_Text, (ULONG)(uintptr_t)d->buf);
}

static ULONG ta_setup(struct IClass* cl, Object* obj, Msg msg) {
    ta_data* d = INST_DATA(cl, obj);
    mui_aa_check_op_t ops[MUI_AA_FIELD_OPS];
    int k, n;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_field_metrics(d->aa.font_px, d->aa.aspect_q8, &d->m);
    for (k = 0; k < 2; k++) {
        n = mui_aa_field_plan(d->aa.tok, &d->m, k, ops);
        while (n-- > 0) {
            aa_hold(&d->aa, ops[n].rgb);
        }
    }
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
    int ph = 2 * (d->m.pad_y + d->m.outline), pw = 2 * d->m.pad_x;
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
    int focus, n, k, i, maxtop;
    int l, t, w, h;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    w = _mwidth(obj);
    h = _mheight(obj);
    focus = ta_focus(obj);
    n = mui_aa_field_plan(d->aa.tok, &d->m, focus, ops);
    aa_erase(&d->aa, obj, l, t, w, h);
    for (k = 0; k < n; k++) {
        aa_round_fill(&d->aa, rp, l + ops[k].inset, t + ops[k].inset, w - 2 * ops[k].inset, h - 2 * ops[k].inset, ops[k].radius, ops[k].rgb);
    }
    aa_low_outline(&d->aa, rp, l, t, w, h, n > 0 ? ops[0].radius : 0);

    ta_ensure(obj, d, &g);
    f.d = d;
    f.obj = obj;
    if (d->reveal) {
        d->top = mtl_scroll_top(mtl_line_of(d->lines, d->nlines, d->caret), g.vis, d->top, d->nlines);
        d->reveal = 0;
    }
    maxtop = d->nlines > g.vis ? d->nlines - g.vis : 0;
    d->top = d->top < 0 ? 0 : d->top > maxtop ? maxtop : d->top;

    if (d->placeholder != NULL) {
        uint32_t rgb;
        if (mui_aa_field_placeholder(d->aa.tok, d->buf, focus, &rgb)) {
            char fit[160];
            mui_aa_text_fit(d->placeholder, g.w, ta_width, &f, fit, (int)sizeof fit);
            aa_text(&d->aa, obj, rp, g.x, g.y, fit, rgb, d->aa.tok[MUI_AA_FIELD_FACE]);
        }
    }
    for (i = d->top; i < d->nlines && i < d->top + g.vis; i++) {
        char ln[TA_LINE_BUF];
        int len = d->lines[i].len < TA_LINE_BUF - 1 ? d->lines[i].len : TA_LINE_BUF - 1;
        if (len > 0) {
            memcpy(ln, d->buf + d->lines[i].start, (size_t)len);
            ln[len] = '\0';
            aa_text(&d->aa, obj, rp, g.x, g.y + (i - d->top) * g.lh, ln, d->aa.tok[SEL_GUI_TOKEN_TEXT], d->aa.tok[MUI_AA_FIELD_FACE]);
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
    int in, off, code;
    if (im == NULL) {
        return 0;
    }
    in = aa_inside(&d->aa, obj, im);
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
            line = d->top + (im->MouseY - g.y) / g.lh;
            line = line < d->top ? d->top : line;
            off = mtl_offset_at(d->buf, d->len, d->lines, d->nlines, line, im->MouseX - g.x, ta_width, &f);
            ta_moved(obj, d, off, 0);
            d->reveal = 0; /* a click inside the view must not scroll it */
            return MUI_EventHandlerRC_Eat;
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
        switch (code) {
        case 0x4C: /* up */
        case 0x4D: /* down */
            off = mtl_move_vertical(d->buf, d->len, d->lines, d->nlines, d->caret, code == 0x4C ? -1 : 1, &d->pref_x, ta_width, &f);
            ta_moved(obj, d, off, 1);
            return MUI_EventHandlerRC_Eat;
        case 0x4E: /* right */
            ta_moved(obj, d, d->caret < d->len ? d->caret + 1 : d->caret, 0);
            return MUI_EventHandlerRC_Eat;
        case 0x4F: /* left */
            ta_moved(obj, d, d->caret > 0 ? d->caret - 1 : 0, 0);
            return MUI_EventHandlerRC_Eat;
        case 0x70: /* Home, as aalist */
            ta_moved(obj, d, mtl_line_home(d->lines, d->nlines, d->caret), 0);
            return MUI_EventHandlerRC_Eat;
        case 0x71: /* End */
            ta_moved(obj, d, mtl_line_end(d->buf, d->len, d->lines, d->nlines, d->caret), 0);
            return MUI_EventHandlerRC_Eat;
        }
        return 0;
    }
    if (im->Class == IDCMP_VANILLAKEY && ta_focus(obj)) {
        char c = (char)code;
        if (code == 0x08) { /* Backspace */
            if (d->caret > 0) {
                d->caret = mtl_backspace(d->buf, &d->len, d->caret);
                ta_edited(obj, d);
            }
        } else if (code == 0x7F) { /* Delete */
            if (mtl_delete(d->buf, &d->len, d->caret)) {
                ta_edited(obj, d);
            }
        } else if (code == 0x0D || (code >= 0x20 && code <= 0x7E) || (code >= 0xA0 && code <= 0xFF)) {
            if (code == 0x0D) {
                c = '\n';
            }
            off = mtl_insert(d->buf, d->cap, &d->len, d->caret, &c, 1);
            if (off > 0) {
                d->caret += off;
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
    def[2].ti_Tag = TAG_MORE, def[2].ti_Data = (ULONG)(uintptr_t)tags;
    nm = *msg;
    nm.ops_AttrList = def;
    obj = (Object*)aatip_super_set(cl, obj, &nm, offsetof(ta_data, tip));
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj); /* BOOPSI instance data starts cleared; the tooltip slot above is already filled, so no memset */
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
    return aatip_super_set(cl, obj, msg, offsetof(ta_data, tip));
}

static ULONG ta_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    ta_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == UPA_TextArea_Text) {
        *msg->opg_Storage = (ULONG)(uintptr_t)d->buf;
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
        ((ta_data*)INST_DATA(cl, obj))->aa.shown = 1;
        break;
    case MUIM_Hide:
        ((ta_data*)INST_DATA(cl, obj))->aa.shown = 0;
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
        MUI_Redraw(obj, MADF_DRAWOBJECT);
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
