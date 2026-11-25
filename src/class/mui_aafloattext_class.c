/* UP-Floattext (UP_CLS_FLOATTEXT): MUI 3.8 MUIC_Floattext in the up-mcc theme. Area subclass, dispatcher pattern of
 * mui_aatext_class.c, line model and scroller pairing of UP-TextArea and UP-List. The MUI tags themselves are the interface:
 * MUIA_Floattext_Text (ISG, copied), _Justify (ISG), _SkipChars and _TabSize (IS, applied when the text is next set, so give them
 * before or with the text). UPA_Floattext_First is the top line (ISG, notifies) and UPA_Floattext_Scroller the UP-Scroller kept
 * in step. All wrap and spacing maths is mui_text_lines.c; this file is glue to the aa_* draw calls and MUI events.
 * shortcut: MUI text escapes (ESC c, ESC b, ...) are drawn as bytes, not interpreted; upgrade if a caller needs styled text. */
#include "mui_aafloattext_class.h"
#include "mui_aa_class.h"

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

#include "mui_aa_draw.h"
#include "mui_aascroller_class.h"
#include "mui_text_lines.h"

extern struct Library* MUIMasterBase;

#define FT_WHEEL_UP 0x7A /* as aalist */
#define FT_WHEEL_DOWN 0x7B
#define FT_WHEEL_STEP 3
#define FT_PAD 2
#define FT_LINE_BUF 256 /* shortcut: a wrapped line over 255 bytes is measured and drawn cut, as UP-TextArea */

typedef struct ft_data {
    char* text; /* AllocVec, prepared (skip chars gone, TABs expanded), NUL terminated */
    int len, first, justify, tabsize;
    const char* skip; /* MUIA_Floattext_SkipChars, caller owned */
    MtlLine* lines;
    int nlines, line_cap, wrap_w, dirty;
    Object* self;
    Object* scroller;
    aa_ctx aa;
    int shown; /* AA_SHOWN_CASES; outside aa_ctx, whose ABI 2 layout is frozen */
    struct MUI_EventHandlerNode ehn;
} ft_data;

static struct MUI_CustomClass* g_class;

/* MtlMeasure: ink width of the first len bytes in the object's font. */
static int ft_width(void* ctx, const char* s, int len) {
    ft_data* d = (ft_data*)ctx;
    char b[FT_LINE_BUF];
    if (len >= FT_LINE_BUF) {
        len = FT_LINE_BUF - 1;
    }
    memcpy(b, s, (size_t)len);
    b[len] = '\0';
    return aa_text_width(&d->aa, d->self, b);
}

static int ft_textw(Object* obj) {
    int w = _mwidth(obj) - 2 * FT_PAD;
    return w < 1 ? 1 : w;
}

static int ft_lh(ft_data* d, Object* obj) {
    int h = aa_text_height(&d->aa, obj);
    return h < 1 ? 1 : h;
}

static int ft_visible(ft_data* d, Object* obj) {
    int v = (_mheight(obj) - 2 * FT_PAD) / ft_lh(d, obj);
    return v < 1 ? 1 : v;
}

/* Wrap again when the text or the width changed (UP-TextArea's ta_layout); on a failed grow the tail is not shown. */
static void ft_layout(Object* obj, ft_data* d) {
    int w = ft_textw(obj), n;
    if (!d->dirty && d->wrap_w == w) {
        return;
    }
    n = mtl_wrap(d->text, d->len, w, ft_width, d, d->lines, d->line_cap);
    if (n > d->line_cap) {
        MtlLine* nl = (MtlLine*)AllocVec((ULONG)(n + 16) * sizeof(MtlLine), MEMF_ANY);
        if (nl != NULL) {
            FreeVec(d->lines);
            d->lines = nl;
            d->line_cap = n + 16;
            mtl_wrap(d->text, d->len, w, ft_width, d, d->lines, d->line_cap);
        } else {
            n = d->line_cap;
        }
    }
    d->nlines = n;
    d->wrap_w = w;
    d->dirty = 0;
}

/* Top line kept in [0, nlines - visible]; before layout only the lower bound is known. */
static int ft_clamp(Object* obj, ft_data* d, int first) {
    if (aa_live(obj)) {
        int top;
        ft_layout(obj, d);
        top = d->nlines - ft_visible(d, obj);
        if (first > top) {
            first = top;
        }
    }
    return first < 0 ? 0 : first;
}

static void ft_sync_scroller(Object* obj, ft_data* d) {
    if (d->scroller != NULL && aa_live(obj)) {
        SetAttrs(d->scroller, MUIA_NoNotify, TRUE, UPA_Scroller_Visible, (ULONG)ft_visible(d, obj), UPA_Scroller_Entries, (ULONG)d->nlines,
                 UPA_Scroller_First, (ULONG)d->first, TAG_DONE);
    }
}

/* Copy src through mtl_float_prep. Returns 0 when out of memory (the old text stays). */
static int ft_set_text(ft_data* d, const char* src) {
    int n = mtl_float_prep(src, d->skip, d->tabsize, NULL, 0);
    char* t = (char*)AllocVec((ULONG)n + 1, MEMF_ANY);
    if (t == NULL) {
        return 0;
    }
    mtl_float_prep(src, d->skip, d->tabsize, t, n + 1);
    FreeVec(d->text);
    d->text = t;
    d->len = n;
    d->first = 0;
    d->dirty = 1;
    return 1;
}

static ULONG ft_setup(struct IClass* cl, Object* obj, Msg msg) {
    ft_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    d->self = obj;
    aa_setup(obj, &d->aa, NULL, 0);
    if (!d->aa.tc) {
        d->aa.bg_rgb = d->aa.tok[SEL_GUI_TOKEN_PANEL];
    }
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_TEXT]);
    aa_hold(&d->aa, d->aa.bg_rgb);
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_RAWKEY;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    d->dirty = 1; /* the font may differ from the last setup */
    return TRUE;
}

static ULONG ft_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    ft_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

/* MUIC_Floattext sizes: a few characters wide and one line high at least, grows without limit both ways. */
static ULONG ft_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    ft_data* d = INST_DATA(cl, obj);
    int lh = ft_lh(d, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += 4 * lh + 2 * FT_PAD;
    msg->MinMaxInfo->DefWidth += 16 * lh + 2 * FT_PAD;
    msg->MinMaxInfo->MinHeight += lh + 2 * FT_PAD;
    msg->MinMaxInfo->DefHeight += 5 * lh + 2 * FT_PAD;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;
    return 0;
}

/* One line. Justified: a soft-wrapped line (the next line starts right after it, so no LF ends it) has its trailing blanks dropped
 * and the spare width spread over its spaces, word by word. Every other line is drawn as it is. */
static void ft_draw_line(Object* obj, ft_data* d, int i, int x, int y, int w) {
    const MtlLine* ln = &d->lines[i];
    const char* s = d->text + ln->start;
    char b[FT_LINE_BUF];
    uint32_t fg = d->aa.tok[SEL_GUI_TOKEN_TEXT], bg = d->aa.bg_rgb;
    int n = ln->len < FT_LINE_BUF ? ln->len : FT_LINE_BUF - 1;
    int soft = i + 1 < d->nlines && d->lines[i + 1].start == ln->start + ln->len;
    if (d->justify && soft) {
        int gaps = 0, k, ws, g = 0, add = 0, extra;
        while (n > 0 && s[n - 1] == ' ') {
            n--;
        }
        for (k = 0; k < n; k++) {
            gaps += s[k] == ' ';
        }
        extra = w - ft_width(d, s, n);
        if (gaps > 0 && extra > 0) {
            for (ws = 0, k = 0; k <= n; k++) {
                if (k == n || s[k] == ' ') {
                    if (k > ws) {
                        memcpy(b, s + ws, (size_t)(k - ws));
                        b[k - ws] = '\0';
                        aa_text(&d->aa, obj, _rp(obj), x + ft_width(d, s, ws) + add, y, b, fg, bg);
                    }
                    if (k < n) {
                        add += mtl_justify_gap(extra, gaps, g++);
                    }
                    ws = k + 1;
                }
            }
            return;
        }
    }
    memcpy(b, s, (size_t)n);
    b[n] = '\0';
    if (n > 0) {
        aa_text(&d->aa, obj, _rp(obj), x, y, b, fg, bg);
    }
}

static ULONG ft_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    ft_data* d = INST_DATA(cl, obj);
    int lh, vis, i, x, y;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE)) || !d->shown) {
        return 0;
    }
    if (d->aa.tc) {
        aa_erase(&d->aa, obj, _left(obj), _top(obj), _width(obj), _height(obj));
    }
    ft_layout(obj, d);
    d->first = ft_clamp(obj, d, d->first);
    ft_sync_scroller(obj, d);
    lh = ft_lh(d, obj);
    vis = ft_visible(d, obj);
    x = _mleft(obj) + FT_PAD;
    y = _mtop(obj) + FT_PAD;
    for (i = d->first; i < d->nlines && i < d->first + vis; i++, y += lh) {
        ft_draw_line(obj, d, i, x, y, ft_textw(obj));
    }
    return 0;
}

static ULONG ft_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    ft_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    if (im == NULL || im->Class != IDCMP_RAWKEY || (im->Code != FT_WHEEL_UP && im->Code != FT_WHEEL_DOWN) || !aa_inside(d->shown, obj, im)) {
        return 0;
    }
    set(obj, UPA_Floattext_First, (ULONG)(d->first + (im->Code == FT_WHEEL_UP ? -FT_WHEEL_STEP : FT_WHEEL_STEP)));
    return MUI_EventHandlerRC_Eat;
}

static ULONG ft_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    ft_data* d = INST_DATA(cl, obj);
    struct TagItem* tag;
    int redraw = 0;
    if ((tag = FindTagItem(MUIA_Floattext_SkipChars, msg->ops_AttrList)) != NULL) {
        d->skip = (const char*)tag->ti_Data;
    }
    if ((tag = FindTagItem(MUIA_Floattext_TabSize, msg->ops_AttrList)) != NULL) {
        d->tabsize = (int)tag->ti_Data;
    }
    if ((tag = FindTagItem(MUIA_Floattext_Text, msg->ops_AttrList)) != NULL) {
        redraw |= ft_set_text(d, (const char*)tag->ti_Data);
    }
    if ((tag = FindTagItem(MUIA_Floattext_Justify, msg->ops_AttrList)) != NULL && d->justify != (tag->ti_Data != 0)) {
        d->justify = tag->ti_Data != 0;
        redraw = 1;
    }
    if ((tag = FindTagItem(UPA_Floattext_First, msg->ops_AttrList)) != NULL) {
        int f = ft_clamp(obj, d, (int)(LONG)tag->ti_Data);
        tag->ti_Data = (ULONG)f; /* the notify carries the clamped line */
        if (f != d->first) {
            d->first = f;
            redraw = 1;
        }
    }
    if (redraw && aa_live(obj)) {
        aa_redraw(obj, MADF_DRAWOBJECT);
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG ft_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    ft_data* d = INST_DATA(cl, obj);
    switch (msg->opg_AttrID) {
    case MUIA_Floattext_Text: *msg->opg_Storage = (ULONG)(uintptr_t)d->text; return TRUE;
    case MUIA_Floattext_Justify: *msg->opg_Storage = (ULONG)d->justify; return TRUE;
    case UPA_Floattext_First: *msg->opg_Storage = (ULONG)d->first; return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG ft_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    ft_data* d;
    struct TagItem* tags = msg->ops_AttrList;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)msg);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj); /* BOOPSI instance data starts cleared */
    d->skip = (const char*)GetTagData(MUIA_Floattext_SkipChars, 0, tags);
    d->tabsize = (int)GetTagData(MUIA_Floattext_TabSize, 8, tags);
    d->justify = GetTagData(MUIA_Floattext_Justify, FALSE, tags) != 0;
    d->scroller = (Object*)(uintptr_t)GetTagData(UPA_Floattext_Scroller, 0, tags);
    d->line_cap = 32;
    d->lines = (MtlLine*)AllocVec((ULONG)d->line_cap * sizeof(MtlLine), MEMF_ANY);
    if (d->lines == NULL || !ft_set_text(d, (const char*)GetTagData(MUIA_Floattext_Text, 0, tags))) {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }
    d->first = (int)GetTagData(UPA_Floattext_First, 0, tags);
    return (ULONG)obj;
}

static ULONG ft_dispose(struct IClass* cl, Object* obj, Msg msg) {
    ft_data* d = INST_DATA(cl, obj);
    FreeVec(d->text);
    FreeVec(d->lines);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG __attribute__((used)) ft_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aafloattext_dispatch(void);
const struct aa_class_desc aafloattext_class_desc = {MUIC_Area, sizeof(ft_data), aafloattext_dispatch, &g_class};
__asm__(".text\n.even\n.globl _aafloattext_dispatch\n_aafloattext_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _ft_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) ft_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return ft_new(cl, obj, (struct opSet*)msg);
    case OM_DISPOSE:
        return ft_dispose(cl, obj, msg);
    case OM_SET:
        return ft_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return ft_get(cl, obj, (struct opGet*)msg);
    case MUIM_HandleEvent:
        return ft_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    case MUIM_Setup:
        return ft_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return ft_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return ft_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return ft_draw(cl, obj, (struct MUIP_Draw*)msg);
    AA_SHOWN_CASES(ft_data)
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* The text and a UP-Scroller in a horizontal group, as UP_NewList; the scroller drives UPA_Floattext_First. Returns the group;
 * *ft_out the text object. */
Object* UP_NewFloattext(const struct TagItem* tags, Object** ft_out) {
    Object *ft, *scroller, *group;
    if (g_class == NULL) {
        g_class = AA_CLASS_CREATE(&aafloattext_class_desc);
    }
    if (g_class == NULL) {
        return NULL;
    }
    {
        struct TagItem sc[] = {{MUIA_Frame, (ULONG)MUIV_Frame_None}, {TAG_DONE, 0}};
        scroller = AA_SIBLING(UP_NewScroller, "UP-Scroller.mcc", sc);
    }
    {
        struct TagItem ft_tags[] = {{UPA_Floattext_Scroller, (ULONG)(uintptr_t)scroller}, {TAG_MORE, (ULONG)(uintptr_t)tags}};
        ft = scroller != NULL ? NewObjectA(g_class->mcc_Class, NULL, ft_tags) : NULL;
    }
    group = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 0, MUIA_Group_HorizSpacing, 0, Child, (ULONG)ft, Child, (ULONG)scroller,
                          TAG_DONE);
    if (ft == NULL || scroller == NULL || group == NULL) {
        if (group != NULL) {
            MUI_DisposeObject(group);
        } else {
            if (ft != NULL) {
                MUI_DisposeObject(ft);
            }
            if (scroller != NULL) {
                MUI_DisposeObject(scroller);
            }
        }
        return NULL;
    }
    DoMethod(scroller, MUIM_Notify, UPA_Scroller_First, MUIV_EveryTime, ft, 3, MUIM_Set, UPA_Floattext_First, MUIV_TriggerValue);
    if (ft_out != NULL) {
        *ft_out = ft;
    }
    return group;
}

void mui_aafloattext_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
    mui_aascroller_class_free();
}
