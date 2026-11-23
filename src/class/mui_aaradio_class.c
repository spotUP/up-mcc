/* UP-Radio. Dispatcher pattern of mui_aacycle_class.c; the dot plan and the row maths are pure (map/mui_aasel.c), the step is
 * mui_aa_cycle_step. The group of dots is one object: the chosen index is UPA_Radio_Active and there is no per-row object. */
#include "mui_aaradio_class.h"
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

typedef struct aaradio_data {
    const char* const* entries;
    int n, active, hover; /* hover: row under the pointer, -1 none */
    int row_h;
    mui_aa_metrics_t m;
    aa_ctx aa;
    struct MUI_EventHandlerNode ehn;
} aaradio_data;

static struct MUI_CustomClass* g_radio_class;
static uint32_t g_radio_made;

static ULONG aaradio_setup(struct IClass* cl, Object* obj, Msg msg) {
    aaradio_data* d = INST_DATA(cl, obj);
    mui_aa_check_op_t ops[MUI_AASEL_OPS];
    int st, sel, k, n;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_metrics_aspect(d->aa.font_px, d->aa.aspect_q8, &d->m);
    for (sel = 0; sel < 2; sel++) {
        for (st = MUI_AA_NORMAL; st <= MUI_AA_DISABLED; st++) {
            n = mui_aa_radio_plan(d->aa.tok, &d->m, sel, st, ops);
            for (k = 0; k < n; k++) {
                aa_hold(&d->aa, ops[k].rgb);
            }
            aa_hold(&d->aa, mui_aa_text_rgb(d->aa.tok, d->aa.tok[SEL_GUI_TOKEN_TEXT], st));
        }
    }
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
    d->row_h = mui_aa_radio_row_h(aa_text_height(&d->aa, obj), d->m.check_h, d->m.pad_y);
    d->hover = -1;
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aaradio_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aaradio_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    d->hover = -1;
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aaradio_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aaradio_data* d = INST_DATA(cl, obj);
    int k, tw, widest = 0, w, h = d->n * d->row_h;
    DoSuperMethodA(cl, obj, (Msg)msg);
    for (k = 0; k < d->n; k++) {
        tw = aa_text_width(&d->aa, obj, d->entries[k]);
        widest = tw > widest ? tw : widest;
    }
    w = d->m.check_px + d->m.icon_gap + widest;
    msg->MinMaxInfo->MinWidth += w;
    msg->MinMaxInfo->DefWidth += w;
    msg->MinMaxInfo->MaxWidth += w;
    msg->MinMaxInfo->MinHeight += h;
    msg->MinMaxInfo->DefHeight += h;
    msg->MinMaxInfo->MaxHeight += h;
    return 0;
}

static ULONG aaradio_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aaradio_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    mui_aa_check_op_t ops[MUI_AASEL_OPS];
    ULONG dis = 0, active = 0;
    int l, t, k, j, n, state, by, th, cp = d->m.check_px, ch = d->m.check_h, asp = d->m.aspect_q8, iv, rh;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    th = aa_text_height(&d->aa, obj);
    GetAttr(MUIA_Disabled, obj, &dis);
    for (k = 0; k < d->n; k++) {
        state = dis ? MUI_AA_DISABLED : k == d->hover ? MUI_AA_HOVER : MUI_AA_NORMAL;
        by = t + k * d->row_h + (d->row_h - ch) / 2;
        n = mui_aa_radio_plan(d->aa.tok, &d->m, k == d->active, state, ops);
        for (j = 0; j < n; j++) {
            iv = ops[j].inset > 0 ? mui_aa_rows(ops[j].inset, asp) : 0;
            rh = ch - 2 * iv;
            aa_round_fill(&d->aa, rp, l + ops[j].inset, by + iv, cp - 2 * ops[j].inset, rh, ops[j].radius < rh / 2 ? ops[j].radius : rh / 2, ops[j].rgb);
        }
        aa_text(&d->aa, obj, rp, l + cp + d->m.icon_gap, t + k * d->row_h + (d->row_h - th) / 2, d->entries[k],
                mui_aa_text_rgb(d->aa.tok, d->aa.tok[SEL_GUI_TOKEN_TEXT], state), d->aa.bg_rgb);
        if (k == d->active && aa_live(obj)) {
            GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
            if ((Object*)active == obj) {
                aa_round_frame(&d->aa, rp, l, by, cp, ch, ch / 2, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
            }
        }
    }
    return 0;
}

static ULONG aaradio_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aaradio_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    ULONG dis = 0, act = 0;
    int in, row;
    if (im == NULL) {
        return DoSuperMethodA(cl, obj, (Msg)msg);
    }
    GetAttr(MUIA_Disabled, obj, &dis);
    in = aa_inside(&d->aa, obj, im);
    row = in ? mui_aa_radio_hit(im->MouseY - _top(obj), d->row_h, d->n) : -1;
    if (im->Class == IDCMP_MOUSEMOVE) {
        if (row != d->hover) {
            d->hover = row;
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        }
        return 0;
    }
    if (im->Class == IDCMP_MOUSEBUTTONS) {
        if (im->Code == SELECTDOWN && row >= 0 && !dis) {
            set(obj, UPA_Radio_Active, (ULONG)row);
            return MUI_EventHandlerRC_Eat;
        }
        return 0;
    }
    if (im->Class == IDCMP_RAWKEY && !dis && aa_live(obj)) {
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &act);
        if ((Object*)act == obj && (im->Code == 0x4C || im->Code == 0x4D)) { /* cursor up, cursor down */
            set(obj, UPA_Radio_Active, (ULONG)mui_aa_cycle_step(d->active, d->n, im->Code == 0x4C ? -1 : 1));
            return MUI_EventHandlerRC_Eat;
        }
    }
    return 0;
}

static ULONG aaradio_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aaradio_data* d;
    struct opSet nm;
    struct TagItem def[3];
    struct TagItem *te = FindTagItem(UPA_Radio_Entries, msg->ops_AttrList), *ta = FindTagItem(UPA_Radio_Active, msg->ops_AttrList);
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
    d->entries = te != NULL ? (const char* const*)(uintptr_t)te->ti_Data : NULL;
    while (d->entries != NULL && d->entries[d->n] != NULL) {
        d->n++;
    }
    d->active = ta != NULL && mui_aa_cycle_valid((int)ta->ti_Data, d->n) ? (int)ta->ti_Data : 0;
    d->hover = -1;
    return (ULONG)obj;
}

static ULONG aaradio_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aaradio_data* d = INST_DATA(cl, obj);
    struct TagItem* ta = FindTagItem(UPA_Radio_Active, msg->ops_AttrList);
    if (ta != NULL && mui_aa_cycle_valid((int)ta->ti_Data, d->n) && (int)ta->ti_Data != d->active) {
        d->active = (int)ta->ti_Data;
        if (aa_live(obj)) {
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aaradio_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aaradio_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == UPA_Radio_Active) {
        *msg->opg_Storage = (ULONG)d->active;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aaradio_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aaradio_dispatch(void);
const struct aa_class_desc aaradio_class_desc = {MUIC_Area, sizeof(aaradio_data), aaradio_dispatch, &g_radio_class};
__asm__(".text\n.even\n.globl _aaradio_dispatch\n_aaradio_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aaradio_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aaradio_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aaradio_new(cl, obj, (struct opSet*)msg);
    case OM_SET:
        return aaradio_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aaradio_get(cl, obj, (struct opGet*)msg);
    AA_SHOWN_CASES(aaradio_data)
    case MUIM_Setup:
        return aaradio_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aaradio_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aaradio_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aaradio_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_HandleEvent:
        return aaradio_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* UP_NewRadio(const struct TagItem* tags) {
    Object* o;
    struct TagItem def[1];
    if (g_radio_class == NULL) {
        g_radio_class = AA_CLASS_CREATE(&aaradio_class_desc);
    }
    if (g_radio_class == NULL) {
        return NULL;
    }
    def[0].ti_Tag = TAG_MORE, def[0].ti_Data = (ULONG)(uintptr_t)tags;
    o = NewObjectA(g_radio_class->mcc_Class, NULL, def);
    if (o != NULL) {
        g_radio_made++;
    }
    return o;
}

uint32_t mui_aaradio_made(void) {
    return g_radio_made;
}

void mui_aaradio_class_free(void) {
    if (g_radio_class != NULL) {
        MUI_DeleteCustomClass(g_radio_class);
        g_radio_class = NULL;
    }
}
