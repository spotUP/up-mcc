/* UP-Checkmark. Dispatcher pattern of mui_aacycle_class.c; the paint plan and the tick are pure (map/mui_aasel.c). The Area's toggle
 * input mode owns MUIA_Selected, so notification, keyboard (space on the active object) and the label click come from MUI. */
#include "mui_aacheckmark_class.h"
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

#include "map/mui_aasel.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

typedef struct aacheckmark_data {
    const char* label;
    int hover;
    mui_aa_metrics_t m;
    aa_ctx aa;
    struct MUI_EventHandlerNode ehn;
} aacheckmark_data;

static struct MUI_CustomClass* g_check_class;
static uint32_t g_check_made;

static int aacheckmark_state(Object* obj, const aacheckmark_data* d) {
    ULONG dis = 0;
    GetAttr(MUIA_Disabled, obj, &dis);
    return dis ? MUI_AA_DISABLED : d->hover ? MUI_AA_HOVER : MUI_AA_NORMAL;
}

static ULONG aacheckmark_setup(struct IClass* cl, Object* obj, Msg msg) {
    aacheckmark_data* d = INST_DATA(cl, obj);
    mui_aa_check_op_t ops[MUI_AASEL_OPS];
    uint32_t tick;
    int st, sel, k, n;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_metrics_aspect(d->aa.font_px, d->aa.aspect_q8, &d->m);
    for (sel = 0; sel < 2; sel++) {
        for (st = MUI_AA_NORMAL; st <= MUI_AA_DISABLED; st++) {
            n = mui_aa_checkmark_plan(d->aa.tok, &d->m, sel, st, ops, &tick);
            for (k = 0; k < n; k++) {
                aa_hold(&d->aa, ops[k].rgb);
            }
            aa_hold(&d->aa, tick);
            aa_hold(&d->aa, mui_aa_text_rgb(d->aa.tok, d->aa.tok[SEL_GUI_TOKEN_TEXT], st));
        }
    }
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
    d->hover = 0;
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aacheckmark_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aacheckmark_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    d->hover = 0;
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aacheckmark_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aacheckmark_data* d = INST_DATA(cl, obj);
    int w = d->m.check_px, h = d->m.check_h, th;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (d->label != NULL && d->label[0] != '\0') {
        w += d->m.icon_gap + aa_text_width(&d->aa, obj, d->label);
        th = aa_text_height(&d->aa, obj);
        h = th > h ? th : h;
    }
    msg->MinMaxInfo->MinWidth += w;
    msg->MinMaxInfo->DefWidth += w;
    msg->MinMaxInfo->MaxWidth += w;
    msg->MinMaxInfo->MinHeight += h;
    msg->MinMaxInfo->DefHeight += h;
    msg->MinMaxInfo->MaxHeight += h;
    return 0;
}

static ULONG aacheckmark_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aacheckmark_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    mui_aa_check_op_t ops[MUI_AASEL_OPS];
    mui_aa_tick_t cols[64];
    ULONG sel = 0, active = 0;
    uint32_t tick;
    int l, t, h, bx, by, cp = d->m.check_px, ch = d->m.check_h, asp = d->m.aspect_q8, n, k, state, th, thick;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    h = _mheight(obj);
    GetAttr(MUIA_Selected, obj, &sel);
    state = aacheckmark_state(obj, d);
    bx = l;
    by = t + (h - ch) / 2;
    n = mui_aa_checkmark_plan(d->aa.tok, &d->m, sel ? 1 : 0, state, ops, &tick);
    for (k = 0; k < n; k++) {
        int iv = ops[k].inset > 0 ? mui_aa_rows(ops[k].inset, asp) : 0; /* the inset is a physical length, fewer rows on tall pixels */
        aa_round_fill(&d->aa, rp, bx + ops[k].inset, by + iv, cp - 2 * ops[k].inset, ch - 2 * iv, ops[k].radius, ops[k].rgb);
    }
    if (sel) {
        thick = cp >= 16 ? 2 : 1;
        n = mui_aa_tick_cols(cp - 4, ch - 2 * mui_aa_rows(2, asp), mui_aa_rows(thick, asp), cols);
        for (k = 0; k < n; k++) {
            aa_rect(&d->aa, rp, bx + 2 + cols[k].x, by + mui_aa_rows(2, asp) + cols[k].y, bx + 2 + cols[k].x,
                    by + mui_aa_rows(2, asp) + cols[k].y + mui_aa_rows(thick, asp) - 1, tick);
        }
    }
    if (d->label != NULL && d->label[0] != '\0') {
        th = aa_text_height(&d->aa, obj);
        aa_text(&d->aa, obj, rp, bx + cp + d->m.icon_gap, t + (h - th) / 2, d->label, mui_aa_text_rgb(d->aa.tok, d->aa.tok[SEL_GUI_TOKEN_TEXT], state), d->aa.bg_rgb);
    }
    if (aa_live(obj)) {
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
        if ((Object*)active == obj) {
            aa_round_frame(&d->aa, rp, bx, by, cp, ch, ops[1].radius, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
        }
    }
    return 0;
}

static ULONG aacheckmark_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aacheckmark_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    int in;
    if (im == NULL || im->Class != IDCMP_MOUSEMOVE) {
        return DoSuperMethodA(cl, obj, (Msg)msg);
    }
    in = aa_inside(&d->aa, obj, im);
    if (in != d->hover) {
        d->hover = in;
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }
    return 0;
}

/* OM_NEW: the one place UPA_Checkmark_* are parsed (UP_NewCheckmark and MUI_NewObject("UP-Checkmark.mcc") build the same object). */
static ULONG aacheckmark_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aacheckmark_data* d;
    struct opSet nm;
    struct TagItem def[5];
    const char* label = (const char*)(uintptr_t)GetTagData(UPA_Checkmark_Label, 0, msg->ops_AttrList);
    def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
    def[1].ti_Tag = MUIA_InputMode, def[1].ti_Data = (ULONG)MUIV_InputMode_Toggle;
    def[2].ti_Tag = MUIA_ShowSelState, def[2].ti_Data = (ULONG)FALSE;
    def[3].ti_Tag = MUIA_CycleChain, def[3].ti_Data = (ULONG)TRUE;
    def[4].ti_Tag = TAG_MORE, def[4].ti_Data = (ULONG)(uintptr_t)msg->ops_AttrList;
    nm = *msg;
    nm.ops_AttrList = def;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->label = label;
    return (ULONG)obj;
}

static ULONG __attribute__((used)) aacheckmark_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aacheckmark_dispatch(void);
const struct aa_class_desc aacheckmark_class_desc = {MUIC_Area, sizeof(aacheckmark_data), aacheckmark_dispatch, &g_check_class};
__asm__(".text\n.even\n.globl _aacheckmark_dispatch\n_aacheckmark_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aacheckmark_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aacheckmark_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aacheckmark_new(cl, obj, (struct opSet*)msg);
    AA_SHOWN_CASES(aacheckmark_data)
    case MUIM_Setup:
        return aacheckmark_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aacheckmark_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aacheckmark_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aacheckmark_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_HandleEvent:
        return aacheckmark_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* UP_NewCheckmark(const struct TagItem* tags) {
    Object* o;
    struct TagItem def[1];
    if (g_check_class == NULL) {
        g_check_class = AA_CLASS_CREATE(&aacheckmark_class_desc);
    }
    if (g_check_class == NULL) {
        return NULL;
    }
    def[0].ti_Tag = TAG_MORE, def[0].ti_Data = (ULONG)(uintptr_t)tags;
    o = NewObjectA(g_check_class->mcc_Class, NULL, def);
    if (o != NULL) {
        g_check_made++;
    }
    return o;
}

uint32_t mui_aacheckmark_made(void) {
    return g_check_made;
}

void mui_aacheckmark_class_free(void) {
    if (g_check_class != NULL) {
        MUI_DeleteCustomClass(g_check_class);
        g_check_class = NULL;
    }
}
