/* UP-Palette. Dispatcher pattern of mui_aaradio_class.c (click and cursor keys pick one entry). The entries are 0xRRGGBB values in a
 * caller array that is copied at construction; cells are square, a fixed UPA_Palette_Columns wide. All grid maths is pure
 * (map/mui_aa.c mui_aa_palette_*). The active cell has an accent outline, the hovered one a lighter one. */
#include "mui_aapalette_class.h"
#include "mui_aa_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

#define PAL_MAX 256 /* shortcut: entries past 256 are ignored, raise when a screen of more swatches is needed */
#define PAL_GAP 2
#define PAL_DEF_COLS 8

typedef struct aapalette_data {
    uint32_t rgb[PAL_MAX];
    int n, cols, active, hover;
    int cw, ch;
    aa_ctx aa;
    struct MUI_EventHandlerNode ehn;
} aapalette_data;

static struct MUI_CustomClass* g_palette_class;

static ULONG aapalette_setup(struct IClass* cl, Object* obj, Msg msg) {
    aapalette_data* d = INST_DATA(cl, obj);
    int k;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    for (k = 0; k < d->n; k++) {
        aa_hold(&d->aa, d->rgb[k]);
    }
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_SURFACE_PRESSED]);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_TEXT_MUTED]);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_ACCENT]);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
    d->cw = d->aa.font_px + d->aa.font_px / 2;
    d->ch = mui_aa_rows(d->cw, d->aa.aspect_q8); /* square on screen, fewer rows on tall pixels */
    d->hover = -1;
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aapalette_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aapalette_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    d->hover = -1;
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aapalette_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aapalette_data* d = INST_DATA(cl, obj);
    int rows = mui_aa_palette_rows(d->n, d->cols);
    int w = d->cols > 0 ? d->cols * (d->cw + PAL_GAP) - PAL_GAP : 0;
    int h = rows > 0 ? rows * (d->ch + PAL_GAP) - PAL_GAP : 0;
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += w;
    msg->MinMaxInfo->DefWidth += w;
    msg->MinMaxInfo->MaxWidth += w;
    msg->MinMaxInfo->MinHeight += h;
    msg->MinMaxInfo->DefHeight += h;
    msg->MinMaxInfo->MaxHeight += h;
    return 0;
}

static ULONG aapalette_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aapalette_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    ULONG focus = 0;
    int l, t, k, x, y;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    aa_erase(&d->aa, obj, l, t, _mwidth(obj), _mheight(obj));
    if (aa_live(obj)) {
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &focus);
    }
    for (k = 0; k < d->n; k++) {
        uint32_t edge = d->aa.tok[k == d->hover ? SEL_GUI_TOKEN_TEXT_MUTED : SEL_GUI_TOKEN_SURFACE_PRESSED];
        mui_aa_palette_cell(k, d->cols, d->cw, d->ch, PAL_GAP, &x, &y);
        x += l, y += t;
        aa_rect(&d->aa, rp, x, y, x + d->cw - 1, y + d->ch - 1, edge);
        aa_rect(&d->aa, rp, x + 1, y + 1, x + d->cw - 2, y + d->ch - 2, d->rgb[k]);
        if (k == d->active) {
            uint32_t ring = d->aa.tok[(Object*)focus == obj ? SEL_GUI_TOKEN_FOCUS_RING : SEL_GUI_TOKEN_ACCENT];
            aa_round_frame(&d->aa, rp, x, y, d->cw, d->ch, 0, ring);
            aa_round_frame(&d->aa, rp, x + 1, y + 1, d->cw - 2, d->ch - 2, 0, ring);
        }
    }
    return 0;
}

static void aapalette_pick(Object* obj, aapalette_data* d, int i) {
    if (i >= 0 && i < d->n && i != d->active) {
        set(obj, UPA_Palette_Active, (ULONG)i);
    }
}

static ULONG aapalette_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aapalette_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    ULONG dis = 0, act = 0;
    int in, hit, dx = 0, dy = 0;
    if (im == NULL) {
        return DoSuperMethodA(cl, obj, (Msg)msg);
    }
    GetAttr(MUIA_Disabled, obj, &dis);
    in = aa_inside(&d->aa, obj, im);
    hit = in ? mui_aa_palette_hit(im->MouseX - _left(obj), im->MouseY - _top(obj), d->cols, d->n, d->cw, d->ch, PAL_GAP) : -1;
    if (im->Class == IDCMP_MOUSEMOVE) {
        if (hit != d->hover) {
            d->hover = hit;
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        }
        return 0;
    }
    if (im->Class == IDCMP_MOUSEBUTTONS) {
        if (im->Code == SELECTDOWN && hit >= 0 && !dis) {
            aapalette_pick(obj, d, hit);
            return MUI_EventHandlerRC_Eat;
        }
        return 0;
    }
    if (im->Class == IDCMP_RAWKEY && !dis && aa_live(obj)) {
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &act);
        if ((Object*)act == obj) {
            switch (im->Code) {
            case 0x4C: dy = -1; break; /* cursor up */
            case 0x4D: dy = 1; break;  /* cursor down */
            case 0x4E: dx = 1; break;  /* cursor right */
            case 0x4F: dx = -1; break; /* cursor left */
            default: return 0;
            }
            aapalette_pick(obj, d, mui_aa_palette_move(d->active, d->n, d->cols, dx, dy));
            return MUI_EventHandlerRC_Eat;
        }
    }
    return 0;
}

static ULONG aapalette_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aapalette_data* d;
    struct opSet nm;
    struct TagItem def[3];
    const uint32_t* src = (const uint32_t*)(uintptr_t)GetTagData(UPA_Palette_Entries, 0, msg->ops_AttrList);
    long n = (long)GetTagData(UPA_Palette_Count, 0, msg->ops_AttrList), cols = (long)GetTagData(UPA_Palette_Columns, 0, msg->ops_AttrList);
    long act = (long)GetTagData(UPA_Palette_Active, 0, msg->ops_AttrList);
    int k;
    def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
    def[1].ti_Tag = MUIA_CycleChain, def[1].ti_Data = (ULONG)TRUE;
    def[2].ti_Tag = TAG_MORE, def[2].ti_Data = (ULONG)(uintptr_t)msg->ops_AttrList;
    nm = *msg;
    nm.ops_AttrList = def;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->n = src == NULL || n < 0 ? 0 : n > PAL_MAX ? PAL_MAX : (int)n;
    for (k = 0; k < d->n; k++) {
        d->rgb[k] = src[k] & 0xFFFFFFu;
    }
    if (cols <= 0) {
        cols = PAL_DEF_COLS;
    }
    d->cols = cols < d->n ? (int)cols : d->n;
    d->active = act >= 0 && act < d->n ? (int)act : 0;
    d->hover = -1;
    return (ULONG)obj;
}

static ULONG aapalette_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aapalette_data* d = INST_DATA(cl, obj);
    struct TagItem* ta = FindTagItem(UPA_Palette_Active, msg->ops_AttrList);
    if (ta != NULL && (long)ta->ti_Data >= 0 && (long)ta->ti_Data < d->n && (int)ta->ti_Data != d->active) {
        d->active = (int)ta->ti_Data;
        if (aa_live(obj)) {
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aapalette_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aapalette_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == UPA_Palette_Active) {
        *msg->opg_Storage = (ULONG)d->active;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aapalette_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aapalette_dispatch(void);
const struct aa_class_desc aapalette_class_desc = {MUIC_Area, sizeof(aapalette_data), aapalette_dispatch, &g_palette_class};
__asm__(".text\n.even\n.globl _aapalette_dispatch\n_aapalette_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapalette_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aapalette_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aapalette_new(cl, obj, (struct opSet*)msg);
    case OM_SET:
        return aapalette_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aapalette_get(cl, obj, (struct opGet*)msg);
    AA_SHOWN_CASES(aapalette_data)
    case MUIM_Setup:
        return aapalette_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aapalette_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aapalette_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aapalette_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_HandleEvent:
        return aapalette_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* UP_NewPalette(const struct TagItem* tags) {
    if (g_palette_class == NULL) {
        g_palette_class = AA_CLASS_CREATE(&aapalette_class_desc);
    }
    if (g_palette_class == NULL) {
        return NULL;
    }
    return NewObjectA(g_palette_class->mcc_Class, NULL, (struct TagItem*)tags);
}

void mui_aapalette_class_free(void) {
    if (g_palette_class != NULL) {
        MUI_DeleteCustomClass(g_palette_class);
        g_palette_class = NULL;
    }
}
