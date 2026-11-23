/* UP-Scale. Dispatcher pattern of mui_aacheckmark_class.c. Draws on the parent background: ticks in TEXT_MUTED (majors longer) and the
 * value of every major tick under it. All positions come from mui_aa_scale_plan / mui_aa_scale_metrics (map/mui_aa.c). */
#include "mui_aascale_class.h"
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

typedef struct aascale_data {
    long lo, hi;
    int ticks;
    aa_ctx aa;
} aascale_data;

static struct MUI_CustomClass* g_scale_class;

static ULONG aascale_setup(struct IClass* cl, Object* obj, Msg msg) {
    aascale_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_TEXT_MUTED]);
    return TRUE;
}

static ULONG aascale_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aascale_data* d = INST_DATA(cl, obj);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aascale_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aascale_data* d = INST_DATA(cl, obj);
    int minor, major, gap, h;
    mui_aa_scale_metrics(d->aa.font_px, &minor, &major, &gap);
    h = major + gap + aa_text_height(&d->aa, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += 2 * d->ticks;
    msg->MinMaxInfo->DefWidth += 2 * d->ticks;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += h;
    msg->MinMaxInfo->DefHeight += h;
    msg->MinMaxInfo->MaxHeight += h;
    return 0;
}

static ULONG aascale_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aascale_data* d = INST_DATA(cl, obj);
    mui_aa_scale_tick_t tk[MUI_AA_SCALE_MAX];
    struct RastPort* rp;
    char txt[16];
    uint32_t col = d->aa.tok[SEL_GUI_TOKEN_TEXT_MUTED];
    int l, t, w, h, n, i, minor, major, gap, tw;
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
    mui_aa_scale_metrics(d->aa.font_px, &minor, &major, &gap);
    n = mui_aa_scale_plan(w, d->lo, d->hi, d->ticks, tk);
    for (i = 0; i < n; i++) {
        aa_rect(&d->aa, rp, l + tk[i].x, t, l + tk[i].x, t + (tk[i].major ? major : minor) - 1, col);
    }
    for (i = 0; i < n; i++) {
        if (!tk[i].major) {
            continue;
        }
        mui_aa_numeric_text(tk[i].value, txt, sizeof txt);
        tw = aa_text_width(&d->aa, obj, txt);
        /* first label starts at the left end, last ends at the right end, the middle one is centred under its tick */
        aa_text(&d->aa, obj, rp, l + (i == 0 ? 0 : i == n - 1 ? w - tw : tk[i].x - tw / 2), t + major + gap, txt, col, d->aa.bg_rgb);
    }
    return 0;
}

static ULONG aascale_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aascale_data* d;
    struct opSet nm;
    struct TagItem def[2];
    def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
    def[1].ti_Tag = TAG_MORE, def[1].ti_Data = (ULONG)(uintptr_t)msg->ops_AttrList;
    nm = *msg;
    nm.ops_AttrList = def;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->lo = (long)GetTagData(UPA_Scale_Min, 0, msg->ops_AttrList);
    d->hi = (long)GetTagData(UPA_Scale_Max, 100, msg->ops_AttrList);
    d->ticks = (int)GetTagData(UPA_Scale_Ticks, 11, msg->ops_AttrList);
    return (ULONG)obj;
}

static ULONG aascale_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aascale_data* d = INST_DATA(cl, obj);
    struct TagItem* ti;
    int changed = 0;
    if ((ti = FindTagItem(UPA_Scale_Min, msg->ops_AttrList)) != NULL && (long)ti->ti_Data != d->lo) {
        d->lo = (long)ti->ti_Data, changed = 1;
    }
    if ((ti = FindTagItem(UPA_Scale_Max, msg->ops_AttrList)) != NULL && (long)ti->ti_Data != d->hi) {
        d->hi = (long)ti->ti_Data, changed = 1;
    }
    if ((ti = FindTagItem(UPA_Scale_Ticks, msg->ops_AttrList)) != NULL && (int)ti->ti_Data != d->ticks) {
        d->ticks = (int)ti->ti_Data, changed = 1;
    }
    if (changed && aa_live(obj)) {
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aascale_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aascale_data* d = INST_DATA(cl, obj);
    switch (msg->opg_AttrID) {
    case UPA_Scale_Min:
        *msg->opg_Storage = (ULONG)d->lo;
        return TRUE;
    case UPA_Scale_Max:
        *msg->opg_Storage = (ULONG)d->hi;
        return TRUE;
    case UPA_Scale_Ticks:
        *msg->opg_Storage = (ULONG)d->ticks;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aascale_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aascale_dispatch(void);
const struct aa_class_desc aascale_class_desc = {MUIC_Area, sizeof(aascale_data), aascale_dispatch, &g_scale_class};
__asm__(".text\n.even\n.globl _aascale_dispatch\n_aascale_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aascale_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aascale_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aascale_new(cl, obj, (struct opSet*)msg);
    case OM_SET:
        return aascale_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aascale_get(cl, obj, (struct opGet*)msg);
    case MUIM_Setup:
        return aascale_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aascale_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aascale_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aascale_draw(cl, obj, (struct MUIP_Draw*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* UP_NewScale(const struct TagItem* tags) {
    if (g_scale_class == NULL) {
        g_scale_class = AA_CLASS_CREATE(&aascale_class_desc);
    }
    if (g_scale_class == NULL) {
        return NULL;
    }
    return NewObjectA(g_scale_class->mcc_Class, NULL, (struct TagItem*)tags);
}

void mui_aascale_class_free(void) {
    if (g_scale_class != NULL) {
        MUI_DeleteCustomClass(g_scale_class);
        g_scale_class = NULL;
    }
}
