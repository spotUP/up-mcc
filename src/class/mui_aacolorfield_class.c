/* UP-Colorfield. Dispatcher pattern of mui_aascale_class.c. One swatch in the exact RGB (truecolour fill, nearest pen on a palette
 * screen) inside a 1 pixel outline that stays visible on a dark card; it is a RelVerify area, so a click notifies MUIA_Pressed as a button does. */
#include "mui_aacolorfield_class.h"
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

typedef struct aacolorfield_data {
    uint32_t rgb;
    aa_ctx aa;
} aacolorfield_data;

static struct MUI_CustomClass* g_colorfield_class;

static ULONG aacolorfield_setup(struct IClass* cl, Object* obj, Msg msg) {
    aacolorfield_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    aa_hold(&d->aa, d->rgb);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_SURFACE_PRESSED]);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
    return TRUE;
}

static ULONG aacolorfield_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aacolorfield_data* d = INST_DATA(cl, obj);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aacolorfield_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aacolorfield_data* d = INST_DATA(cl, obj);
    int w = 2 * d->aa.font_px, h = up_rows(d->aa.font_px + d->aa.font_px / 2, d->aa.aspect_q8);
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += w;
    msg->MinMaxInfo->DefWidth += w;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += h;
    msg->MinMaxInfo->DefHeight += h;
    msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;
    return 0;
}

static ULONG aacolorfield_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aacolorfield_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    ULONG active = 0;
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
    aa_erase(&d->aa, obj, l, t, w, h);
    aa_rect(&d->aa, rp, l, t, l + w - 1, t + h - 1, d->aa.tok[SEL_GUI_TOKEN_SURFACE_PRESSED]);
    aa_rect(&d->aa, rp, l + 1, t + 1, l + w - 2, t + h - 2, d->rgb);
    if (aa_live(obj)) {
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
        if ((Object*)active == obj) {
            aa_round_frame(&d->aa, rp, l, t, w, h, 0, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
        }
    }
    return 0;
}

static ULONG aacolorfield_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aacolorfield_data* d;
    struct opSet nm;
    struct TagItem def[4];
    def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
    def[1].ti_Tag = MUIA_InputMode, def[1].ti_Data = (ULONG)MUIV_InputMode_RelVerify;
    def[2].ti_Tag = MUIA_CycleChain, def[2].ti_Data = (ULONG)TRUE;
    def[3].ti_Tag = TAG_MORE, def[3].ti_Data = (ULONG)(uintptr_t)msg->ops_AttrList;
    nm = *msg;
    nm.ops_AttrList = def;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->rgb = GetTagData(UPA_Colorfield_RGB, 0, msg->ops_AttrList) & 0xFFFFFFu;
    return (ULONG)obj;
}

static ULONG aacolorfield_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aacolorfield_data* d = INST_DATA(cl, obj);
    struct TagItem* ti = FindTagItem(UPA_Colorfield_RGB, msg->ops_AttrList);
    if (ti != NULL && (ti->ti_Data & 0xFFFFFFu) != d->rgb) {
        d->rgb = ti->ti_Data & 0xFFFFFFu;
        if (aa_live(obj)) {
            aa_hold(&d->aa, d->rgb); /* a new colour on a palette screen needs its pen before the redraw */
            aa_redraw(obj, MADF_DRAWOBJECT);
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aacolorfield_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aacolorfield_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == UPA_Colorfield_RGB) {
        *msg->opg_Storage = d->rgb;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aacolorfield_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aacolorfield_dispatch(void);
const struct aa_class_desc aacolorfield_class_desc = {MUIC_Area, sizeof(aacolorfield_data), aacolorfield_dispatch, &g_colorfield_class};
__asm__(".text\n.even\n.globl _aacolorfield_dispatch\n_aacolorfield_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aacolorfield_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aacolorfield_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aacolorfield_new(cl, obj, (struct opSet*)msg);
    case OM_SET:
        return aacolorfield_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aacolorfield_get(cl, obj, (struct opGet*)msg);
    case MUIM_Setup:
        return aacolorfield_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aacolorfield_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aacolorfield_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aacolorfield_draw(cl, obj, (struct MUIP_Draw*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* UP_NewColorfield(const struct TagItem* tags) {
    if (g_colorfield_class == NULL) {
        g_colorfield_class = AA_CLASS_CREATE(&aacolorfield_class_desc);
    }
    if (g_colorfield_class == NULL) {
        return NULL;
    }
    return NewObjectA(g_colorfield_class->mcc_Class, NULL, (struct TagItem*)tags);
}

void mui_aacolorfield_class_free(void) {
    if (g_colorfield_class != NULL) {
        MUI_DeleteCustomClass(g_colorfield_class);
        g_colorfield_class = NULL;
    }
}
