/* UP-Numericbutton. An Area subclass drawn as a button with the value as text. Press and release without moving opens a popup with a
 * UP-Slider under the button (popup window and state machine copied from UP-Popstring, mui_aapopstring_class.c: created on the first
 * open, reused afterwards, disposed in cleanup); moving the mouse more than MUI_AA_NUM_DRAG_SLOP pixels while pressed drags the value
 * (mui_aa_numeric_drag). The slider and the button keep each other in step through UPA_Numeric_Value only. */
#include "mui_aanumeric_class.h"
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

#include "map/mui_aa.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

typedef struct aanumeric_data {
    long lo, hi, val, start_val;
    int hover, press, dragged, press_x, deact;
    Object *popup, *slider;
    up_metrics_t m;
    mui_aa_pop_t pop;
    aa_ctx aa;
    int shown; /* MUIM_Show seen, MUIM_Hide not yet (AA_SHOWN_CASES); outside aa_ctx, whose ABI 2 layout is frozen */
    struct MUI_EventHandlerNode ehn;
} aanumeric_data;

static struct MUI_CustomClass* g_numeric_class;

static uint32_t aanum_now(void) {
    ULONG s = 0, m = 0;
    CurrentTime(&s, &m);
    return (uint32_t)(s * 1000UL + m / 1000UL);
}

static int aanum_state(Object* obj, const aanumeric_data* d) {
    ULONG dis = 0;
    GetAttr(MUIA_Disabled, obj, &dis);
    return dis ? MUI_AA_DISABLED : d->press ? MUI_AA_PRESSED : d->hover ? MUI_AA_HOVER : MUI_AA_NORMAL;
}

/* Same face rule as the UP-Button (aabtn_face): a NORMAL face equal to the parent card is lifted so the button shows without hover. */
static void aanum_face(const aanumeric_data* d, int st, uint32_t* face, uint32_t* text) {
    mui_aa_button_rgb(d->aa.tok, MUI_AA_ROLE_NORMAL, st, face, text);
    if (d->aa.tc && *face == d->aa.bg_rgb) {
        *face = d->aa.tok[st == MUI_AA_NORMAL ? SEL_GUI_TOKEN_SURFACE_HOVER : SEL_GUI_TOKEN_SURFACE_PRESSED];
    }
}

static ULONG aanum_setup(struct IClass* cl, Object* obj, Msg msg) {
    aanumeric_data* d = INST_DATA(cl, obj);
    uint32_t face, text;
    int st;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    up_metrics_aspect(d->aa.font_px, d->aa.aspect_q8, &d->m);
    for (st = MUI_AA_NORMAL; st <= MUI_AA_DISABLED; st++) {
        aanum_face(d, st, &face, &text);
        aa_hold(&d->aa, face);
        aa_hold(&d->aa, text);
    }
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
    d->hover = d->press = 0;
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

/* Close and dispose the popup window (and the slider with it). Only from cleanup: never from the window's own notification. */
static void aanum_drop(Object* obj, aanumeric_data* d) {
    Object* app = NULL;
    if (d->popup == NULL) {
        return;
    }
    if (aa_live(obj)) {
        GetAttr(MUIA_ApplicationObject, _win(obj), (ULONG*)&app);
    }
    set(d->popup, MUIA_Window_Open, FALSE);
    if (app != NULL) {
        DoMethod(app, OM_REMMEMBER, (ULONG)d->popup);
    }
    MUI_DisposeObject(d->popup);
    d->popup = d->slider = NULL;
    memset(&d->pop, 0, sizeof d->pop);
}

static ULONG aanum_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aanumeric_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    d->hover = d->press = 0;
    aanum_drop(obj, d);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aanum_dispose(struct IClass* cl, Object* obj, Msg msg) {
    aanumeric_data* d = INST_DATA(cl, obj);
    if (d->popup == NULL && d->slider != NULL) {
        MUI_DisposeObject(d->slider); /* built but never put in a window */
    }
    return DoSuperMethodA(cl, obj, msg);
}

static int aanum_text_w(const aanumeric_data* d, Object* obj) {
    char a[16], b[16];
    int wa, wb;
    mui_aa_numeric_text(d->lo, a, sizeof a);
    mui_aa_numeric_text(d->hi, b, sizeof b);
    wa = aa_text_width((aa_ctx*)&d->aa, obj, a);
    wb = aa_text_width((aa_ctx*)&d->aa, obj, b);
    return wa > wb ? wa : wb;
}

static ULONG aanum_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aanumeric_data* d = INST_DATA(cl, obj);
    int w = aanum_text_w(d, obj) + 2 * d->m.pad_x;
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += w;
    msg->MinMaxInfo->DefWidth += w;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += d->m.height;
    msg->MinMaxInfo->DefHeight += d->m.height;
    msg->MinMaxInfo->MaxHeight += d->m.height;
    return 0;
}

static ULONG aanum_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aanumeric_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    char txt[16];
    uint32_t face, text;
    ULONG active = 0;
    int l, t, w, h, st, tw;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    w = _mwidth(obj);
    h = _mheight(obj);
    st = aanum_state(obj, d);
    aanum_face(d, st, &face, &text);
    if (d->aa.tc) {
        aa_round_corners(&d->aa, rp, l, t, w, h, d->m.radius, d->aa.bg_rgb); /* the super painted the wedges with the snapped parent pen */
    }
    aa_round_fill(&d->aa, rp, l, t, w, h, d->m.radius, face);
    mui_aa_numeric_text(d->val, txt, sizeof txt);
    tw = aa_text_width(&d->aa, obj, txt);
    aa_text(&d->aa, obj, rp, l + (w - tw) / 2, t + (h - aa_text_height(&d->aa, obj)) / 2, txt, text, face);
    if (aa_live(obj)) {
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
        if ((Object*)active == obj) {
            aa_round_frame(&d->aa, rp, l, t, w, h, d->m.radius, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
        }
    }
    return 0;
}

/* The popup root: a Raised card around a UP-Slider over lo..hi, wired to set UPA_Numeric_Value on the button. */
static Object* aanum_build(aanumeric_data* d) {
    Object *slider, *card;
    struct TagItem ts[5];
    ts[0].ti_Tag = MUIA_Numeric_Min, ts[0].ti_Data = (ULONG)d->lo;
    ts[1].ti_Tag = MUIA_Numeric_Max, ts[1].ti_Data = (ULONG)d->hi;
    ts[2].ti_Tag = MUIA_Numeric_Value, ts[2].ti_Data = (ULONG)d->val;
    ts[3].ti_Tag = MUIA_CycleChain, ts[3].ti_Data = (ULONG)FALSE;
    ts[4].ti_Tag = TAG_DONE, ts[4].ti_Data = 0;
#ifdef UP_MCC_LIBRARY
    slider = MUI_NewObjectA((char*)"UP-Slider.mcc", ts); /* this library holds only its own class slot: siblings are made by name */
#else
    slider = UP_NewSlider(ts);
#endif
    if (slider == NULL) {
        return NULL;
    }
    card = MUI_NewObject((char*)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Popup, UPA_Panel_Popup, (ULONG)TRUE, MUIA_Group_Child, (ULONG)slider, TAG_DONE);
    d->slider = card != NULL ? slider : NULL;
    if (card == NULL) {
        MUI_DisposeObject(slider);
    }
    return card;
}

static void aanum_place(Object* obj, aanumeric_data* d) {
    struct Window* win = _window(obj);
    set(d->popup, MUIA_Window_LeftEdge, (ULONG)(win->LeftEdge + _left(obj)));
    set(d->popup, MUIA_Window_TopEdge, (ULONG)(win->TopEdge + _bottom(obj) + 2));
}

static void aanum_open(Object* obj, aanumeric_data* d, int act) {
    Object *app = NULL, *root;
    struct Window* win = _window(obj);
    ULONG dis = 0;
    GetAttr(MUIA_Disabled, obj, &dis);
    if (win == NULL || dis || d->pop.open || !aa_live(obj)) {
        return;
    }
    if (act == MUI_AA_POP_OPEN_NEW) {
        GetAttr(MUIA_ApplicationObject, _win(obj), (ULONG*)&app);
        root = app != NULL ? aanum_build(d) : NULL;
        if (root == NULL) {
            return;
        }
        d->popup = MUI_NewObject((char*)MUIC_Window, MUIA_Window_Screen, (ULONG)_screen(obj), MUIA_Window_Borderless, TRUE, MUIA_Window_DragBar, FALSE,
                                 MUIA_Window_CloseGadget, FALSE, MUIA_Window_SizeGadget, FALSE, MUIA_Window_DepthGadget, FALSE,
                                 MUIA_Window_LeftEdge, (ULONG)(win->LeftEdge + _left(obj)), MUIA_Window_TopEdge, (ULONG)(win->TopEdge + _bottom(obj) + 2),
                                 MUIA_Window_Width, (ULONG)(_width(obj) * 3), MUIA_Window_RootObject, (ULONG)root, TAG_DONE);
        if (d->popup == NULL) {
            MUI_DisposeObject(root); /* the root is still ours: the window never took it */
            d->slider = NULL;
            return;
        }
        DoMethod(app, OM_ADDMEMBER, (ULONG)d->popup);
        DoMethod(d->slider, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime, obj, 3, MUIM_Set, UPA_Numeric_Value, MUIV_TriggerValue);
        DoMethod(d->popup, MUIM_Notify, MUIA_Window_Activate, FALSE, obj, 1, UPM_Numeric_Deactivated);
        DoMethod(d->popup, MUIM_Notify, MUIA_Window_Open, FALSE, obj, 1, UPM_Numeric_Closed);
    } else {
        aanum_place(obj, d);
    }
    set(d->slider, MUIA_Numeric_Value, (ULONG)d->val);
    mui_aa_pop_opened(&d->pop);
    set(d->popup, MUIA_Window_Open, TRUE);
    aa_pop_fit(obj, d->popup);
}

static void aanum_close(aanumeric_data* d) {
    if (d->popup != NULL && d->pop.open) {
        set(d->popup, MUIA_Window_Open, FALSE); /* the Open notify runs UPM_Numeric_Closed */
    }
}

static void aanum_click(Object* obj, aanumeric_data* d) {
    int act = mui_aa_pop_click(&d->pop, aanum_now());
    if (act == MUI_AA_POP_CLOSE) {
        aanum_close(d);
    } else if (act != MUI_AA_POP_NONE) {
        aanum_open(obj, d, act);
    }
}

static int aanum_inside(const aanumeric_data* d, Object* obj, const struct IntuiMessage* im) {
    return aa_inside(d->shown, obj, im);
}

static ULONG aanum_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aanumeric_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    ULONG dis = 0;
    int in, dx;
    if (im == NULL || (im->Class != IDCMP_MOUSEMOVE && im->Class != IDCMP_MOUSEBUTTONS)) {
        return DoSuperMethodA(cl, obj, (Msg)msg);
    }
    GetAttr(MUIA_Disabled, obj, &dis);
    if (im->Class == IDCMP_MOUSEMOVE) {
        in = aanum_inside(d, obj, im);
        if (d->press && !dis) {
            dx = im->MouseX - d->press_x;
            if (d->dragged || dx >= MUI_AA_NUM_DRAG_SLOP || -dx >= MUI_AA_NUM_DRAG_SLOP) {
                long nv = mui_aa_numeric_drag(d->start_val, dx, _width(obj), d->lo, d->hi);
                d->dragged = 1;
                if (nv != d->val) {
                    set(obj, UPA_Numeric_Value, (ULONG)nv);
                }
            }
        }
        if (in != d->hover) {
            d->hover = in;
            aa_redraw(obj, MADF_DRAWOBJECT);
        }
        return 0;
    }
    if (im->Code == SELECTDOWN && !dis && aanum_inside(d, obj, im)) {
        d->press = 1;
        d->dragged = 0;
        d->press_x = im->MouseX;
        d->start_val = d->val;
        aa_redraw(obj, MADF_DRAWOBJECT);
        return MUI_EventHandlerRC_Eat;
    }
    if (im->Code == SELECTUP && d->press) {
        d->press = 0;
        aa_redraw(obj, MADF_DRAWOBJECT);
        if (!d->dragged && aanum_inside(d, obj, im)) {
            aanum_click(obj, d);
        }
        d->dragged = 0;
        return MUI_EventHandlerRC_Eat;
    }
    return 0;
}

static ULONG aanum_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aanumeric_data* d;
    struct opSet nm;
    struct TagItem def[3];
    long lo = (long)GetTagData(UPA_Numeric_Min, 0, msg->ops_AttrList), hi = (long)GetTagData(UPA_Numeric_Max, 100, msg->ops_AttrList);
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
    d->lo = lo;
    d->hi = hi < lo ? lo : hi;
    d->val = mui_aa_numeric_clamp((long)GetTagData(UPA_Numeric_Value, (ULONG)lo, msg->ops_AttrList), d->lo, d->hi);
    return (ULONG)obj;
}

static ULONG aanum_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aanumeric_data* d = INST_DATA(cl, obj);
    struct TagItem *ti, *tags = msg->ops_AttrList;
    long nv = d->val;
    int changed = 0;
    if ((ti = FindTagItem(UPA_Numeric_Min, tags)) != NULL && (long)ti->ti_Data != d->lo) {
        d->lo = (long)ti->ti_Data;
        if (d->hi < d->lo) {
            d->hi = d->lo;
        }
        changed = 1;
    }
    if ((ti = FindTagItem(UPA_Numeric_Max, tags)) != NULL && (long)ti->ti_Data != d->hi) {
        d->hi = (long)ti->ti_Data < d->lo ? d->lo : (long)ti->ti_Data;
        changed = 1;
    }
    if ((ti = FindTagItem(UPA_Numeric_Value, tags)) != NULL) {
        nv = (long)ti->ti_Data;
    }
    nv = mui_aa_numeric_clamp(nv, d->lo, d->hi);
    if (ti != NULL) {
        ti->ti_Data = (ULONG)nv; /* notify and later readers see the clamped value */
    }
    if (nv != d->val) {
        d->val = nv;
        changed = 1;
    }
    if (changed) {
        if (d->slider != NULL) {
            set(d->slider, MUIA_Numeric_Min, (ULONG)d->lo);
            set(d->slider, MUIA_Numeric_Max, (ULONG)d->hi);
            set(d->slider, MUIA_Numeric_Value, (ULONG)d->val); /* the slider's own notify comes back with the same value: no change, no loop */
        }
        if (aa_live(obj)) {
            aa_redraw(obj, MADF_DRAWOBJECT);
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aanum_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aanumeric_data* d = INST_DATA(cl, obj);
    switch (msg->opg_AttrID) {
    case UPA_Numeric_Min:
        *msg->opg_Storage = (ULONG)d->lo;
        return TRUE;
    case UPA_Numeric_Max:
        *msg->opg_Storage = (ULONG)d->hi;
        return TRUE;
    case UPA_Numeric_Value:
        *msg->opg_Storage = (ULONG)d->val;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aanumeric_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aanumeric_dispatch(void);
const struct aa_class_desc aanumeric_class_desc = {MUIC_Area, sizeof(aanumeric_data), aanumeric_dispatch, &g_numeric_class};
__asm__(".text\n.even\n.globl _aanumeric_dispatch\n_aanumeric_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aanumeric_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aanumeric_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    aanumeric_data* d;
    switch (msg->MethodID) {
    case OM_NEW:
        return aanum_new(cl, obj, (struct opSet*)msg);
    case OM_DISPOSE:
        return aanum_dispose(cl, obj, msg);
    case OM_SET:
        return aanum_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aanum_get(cl, obj, (struct opGet*)msg);
    AA_SHOWN_CASES(aanumeric_data)
    case MUIM_Setup:
        return aanum_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aanum_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aanum_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aanum_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_HandleEvent:
        return aanum_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    case UPM_Numeric_Deactivated:
        d = INST_DATA(cl, obj);
        d->deact = 1;
        aanum_close(d);
        return 0;
    case UPM_Numeric_Closed:
        d = INST_DATA(cl, obj);
        mui_aa_pop_closed(&d->pop, d->deact, aanum_now());
        d->deact = 0;
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* UP_NewNumericbutton(const struct TagItem* tags) {
    if (g_numeric_class == NULL) {
        g_numeric_class = AA_CLASS_CREATE(&aanumeric_class_desc);
    }
    if (g_numeric_class == NULL) {
        return NULL;
    }
    return NewObjectA(g_numeric_class->mcc_Class, NULL, (struct TagItem*)tags);
}

void mui_aanumeric_class_free(void) {
    if (g_numeric_class != NULL) {
        MUI_DeleteCustomClass(g_numeric_class);
        g_numeric_class = NULL;
    }
}
