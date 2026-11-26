/* UP-Knob and UP-Levelmeter: MUIC_Knob and MUIC_Levelmeter equivalents. Both are Numeric subclasses, as UP-Slider is
 * (mui_aaslider_class.c): MUIA_Numeric_Value/Min/Max/Default/Format, MUIM_Numeric_* and the value notifications are the stock
 * Numeric class's; these classes only draw and (the knob) take input. Angles and points come from map/mui_aa.c (mui_aa_dial_*,
 * host test mui-dial). The knob: a round face with an ACCENT marker, a ring of dots lit up to the value; a drag (right or up adds,
 * two widths sweep the range) or the cursor keys (shift: a tenth of the range) change the value. The levelmeter: a half dial of
 * dots lit up to the value, a needle and MUIA_Levelmeter_Label under it. Dispatcher pattern of mui_aanumeric_class.c. */
#include "mui_aadial_class.h"
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

#include "map/mui_aa.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

#define DIAL_TICKS 11 /* dots on the ring of both dials */

typedef struct aaknob_data {
    long start_val;
    int hover, press, press_x, press_y;
    up_metrics_t m;
    aa_ctx aa;
    int shown; /* MUIM_Show seen, MUIM_Hide not yet (AA_SHOWN_CASES); outside aa_ctx, whose ABI 2 layout is frozen */
    struct MUI_EventHandlerNode ehn;
} aaknob_data;

typedef struct aalevel_data {
    const char* label;
    up_metrics_t m;
    aa_ctx aa;
    int shown;
} aalevel_data;

static struct MUI_CustomClass *g_knob_class, *g_level_class;

static void dial_range(Object* obj, long* lo, long* hi, long* val) {
    ULONG v = 0;
    GetAttr(MUIA_Numeric_Min, obj, &v);
    *lo = (LONG)v;
    GetAttr(MUIA_Numeric_Max, obj, &v);
    *hi = (LONG)v;
    GetAttr(MUIA_Numeric_Value, obj, &v);
    *val = (LONG)v;
}

static int dial_disabled(Object* obj) {
    ULONG dis = 0;
    GetAttr(MUIA_Disabled, obj, &dis);
    return dis != 0;
}

static void dial_dot(aa_ctx* c, struct RastPort* rp, int x, int y, int r, uint32_t rgb) {
    aa_round_fill(c, rp, x - r, y - r, 2 * r + 1, 2 * r + 1, r, rgb);
}

/* The ring of DIAL_TICKS dots at radius r round (cx, cy): fill up to the value's angle, track beyond it. */
static void dial_ring(aa_ctx* c, struct RastPort* rp, int cx, int cy, int r, int dot, int start, int sweep, int vdeg, uint32_t track, uint32_t fill) {
    int i, x, y, deg;
    for (i = 0; i < DIAL_TICKS; i++) {
        deg = start - sweep * i / (DIAL_TICKS - 1);
        mui_aa_dial_point(deg, r, &x, &y);
        dial_dot(c, rp, cx + x, cy + y, dot, deg >= vdeg ? fill : track);
    }
}

static void dial_hold_all(aa_ctx* c) {
    uint32_t a, b;
    int st;
    for (st = MUI_AA_NORMAL; st <= MUI_AA_DISABLED; st++) {
        mui_aa_slider_rgb(c->tok, st, &a, &b);
        aa_hold(c, a);
        aa_hold(c, b);
        mui_aa_button_rgb(c->tok, MUI_AA_ROLE_NORMAL, st, &a, &b);
        aa_hold(c, a);
        aa_hold(c, b);
    }
    aa_hold(c, c->tok[SEL_GUI_TOKEN_FOCUS_RING]);
}

/* ---- UP-Knob ---- */

static int aaknob_state(Object* obj, const aaknob_data* d) {
    return dial_disabled(obj) ? MUI_AA_DISABLED : d->press ? MUI_AA_PRESSED : d->hover ? MUI_AA_HOVER : MUI_AA_NORMAL;
}

static ULONG aaknob_setup(struct IClass* cl, Object* obj, Msg msg) {
    aaknob_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    up_metrics_aspect(d->aa.font_px, d->aa.aspect_q8, &d->m);
    dial_hold_all(&d->aa);
    d->hover = d->press = 0;
    d->ehn.ehn_Priority = 1; /* ahead of any handler of the Numeric super, so a key it eats never steps twice */
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aaknob_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aaknob_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    d->hover = d->press = 0;
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

/* shortcut: a square box in pixels, so the dial is an ellipse on a tall-pixel screen; scale the height by up_rows when that matters */
static ULONG aaknob_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aaknob_data* d = INST_DATA(cl, obj);
    int s = 2 * d->m.height;
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += s;
    msg->MinMaxInfo->DefWidth += s;
    msg->MinMaxInfo->MaxWidth += s;
    msg->MinMaxInfo->MinHeight += s;
    msg->MinMaxInfo->DefHeight += s;
    msg->MinMaxInfo->MaxHeight += s;
    return 0;
}

static ULONG aaknob_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aaknob_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    ULONG active = 0;
    long lo, hi, val;
    uint32_t track, fill, face, text;
    int l, t, w, h, cx, cy, r, dot, rf, vdeg, x, y, st;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    w = _mwidth(obj);
    h = _mheight(obj);
    st = aaknob_state(obj, d);
    dial_range(obj, &lo, &hi, &val);
    mui_aa_slider_rgb(d->aa.tok, st, &track, &fill);
    mui_aa_button_rgb(d->aa.tok, MUI_AA_ROLE_NORMAL, st, &face, &text);
    if (d->aa.tc && face == d->aa.bg_rgb) {
        face = d->aa.tok[st == MUI_AA_NORMAL ? SEL_GUI_TOKEN_SURFACE_HOVER : SEL_GUI_TOKEN_SURFACE_PRESSED]; /* aanum_face rule */
    }
    aa_erase(&d->aa, obj, l, t, w, h); /* a value update repaints the marker: the old one goes with the box */
    dot = d->m.font_px >= 12 ? 2 : 1;
    r = (w < h ? w : h) / 2 - dot - 1;
    cx = l + w / 2;
    cy = t + h / 2;
    vdeg = mui_aa_dial_deg(val, lo, hi, MUI_AA_KNOB_START, MUI_AA_KNOB_SWEEP);
    dial_ring(&d->aa, rp, cx, cy, r, dot, MUI_AA_KNOB_START, MUI_AA_KNOB_SWEEP, vdeg, track, fill);
    rf = r - 2 * dot - 2;
    if (rf < 3) {
        return 0;
    }
    aa_round_fill(&d->aa, rp, cx - rf, cy - rf, 2 * rf + 1, 2 * rf + 1, rf, face);
    mui_aa_dial_point(vdeg, rf * 3 / 5, &x, &y);
    dial_dot(&d->aa, rp, cx + x, cy + y, rf / 5 > 1 ? rf / 5 : 1, fill);
    if (aa_live(obj)) {
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
        if ((Object*)active == obj) {
            aa_round_frame(&d->aa, rp, cx - rf, cy - rf, 2 * rf + 1, 2 * rf + 1, rf, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
        }
    }
    return 0;
}

static ULONG aaknob_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aaknob_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    ULONG active = 0;
    long lo, hi, val, nv;
    int in, used = 0, dis;
    if (im == NULL) {
        return DoSuperMethodA(cl, obj, (Msg)msg);
    }
    dis = dial_disabled(obj);
    dial_range(obj, &lo, &hi, &val);
    if (im->Class == IDCMP_RAWKEY) {
        if (dis || (im->Code & IECODE_UP_PREFIX) || !aa_live(obj)) {
            return 0;
        }
        GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
        if ((Object*)active != obj) {
            return 0;
        }
        nv = mui_aa_knob_key(val, lo, hi, im->Code, (im->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0, &used);
        if (used && nv != val) {
            set(obj, MUIA_Numeric_Value, (ULONG)nv);
        }
        return used ? MUI_EventHandlerRC_Eat : 0;
    }
    if (im->Class == IDCMP_MOUSEMOVE) {
        in = aa_inside(d->shown, obj, im);
        if (d->press && !dis) {
            /* right or up adds: two knob widths of travel sweep the range, as the UP-Numericbutton drag */
            nv = mui_aa_numeric_drag(d->start_val, (im->MouseX - d->press_x) - (im->MouseY - d->press_y), _mwidth(obj), lo, hi);
            if (nv != val) {
                set(obj, MUIA_Numeric_Value, (ULONG)nv);
            }
        }
        if (in != d->hover) {
            d->hover = in;
            aa_redraw(obj, MADF_DRAWOBJECT);
        }
        return 0;
    }
    if (im->Class != IDCMP_MOUSEBUTTONS) {
        return 0;
    }
    if (im->Code == SELECTDOWN && !dis && aa_inside(d->shown, obj, im)) {
        d->press = 1;
        d->press_x = im->MouseX;
        d->press_y = im->MouseY;
        d->start_val = val;
        aa_redraw(obj, MADF_DRAWOBJECT);
        return MUI_EventHandlerRC_Eat;
    }
    if (im->Code == SELECTUP && d->press) {
        d->press = 0;
        aa_redraw(obj, MADF_DRAWOBJECT);
        return MUI_EventHandlerRC_Eat;
    }
    return 0;
}

static ULONG __attribute__((used)) aaknob_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aaknob_dispatch(void);
const struct aa_class_desc aaknob_class_desc = {MUIC_Numeric, sizeof(aaknob_data), aaknob_dispatch, &g_knob_class};
__asm__(".section .text._aaknob_dispatch,\"ax\"\n.even\n.globl _aaknob_dispatch\n_aaknob_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aaknob_dispatch_c\n\tlea 12(sp),sp\n\trts\n.text\n");

static ULONG __attribute__((used)) aaknob_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    AA_SHOWN_CASES(aaknob_data)
    case MUIM_Setup:
        return aaknob_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aaknob_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aaknob_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aaknob_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_HandleEvent:
        return aaknob_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* ---- UP-Levelmeter ---- */

static ULONG aalevel_setup(struct IClass* cl, Object* obj, Msg msg) {
    aalevel_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    up_metrics_aspect(d->aa.font_px, d->aa.aspect_q8, &d->m);
    dial_hold_all(&d->aa);
    return TRUE;
}

static ULONG aalevel_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aalevel_data* d = INST_DATA(cl, obj);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

/* Width: four button heights or the label, the wider; height: the half dial plus a text line. */
static ULONG aalevel_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aalevel_data* d = INST_DATA(cl, obj);
    int w = 4 * d->m.height, lw = d->label != NULL ? aa_text_width(&d->aa, obj, d->label) + 2 * d->m.pad_x : 0;
    int h;
    if (lw > w) {
        w = lw;
    }
    h = w / 2 + aa_text_height(&d->aa, obj) + 2 * d->m.pad_y;
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += w;
    msg->MinMaxInfo->DefWidth += w;
    msg->MinMaxInfo->MaxWidth += w;
    msg->MinMaxInfo->MinHeight += h;
    msg->MinMaxInfo->DefHeight += h;
    msg->MinMaxInfo->MaxHeight += h;
    return 0;
}

static ULONG aalevel_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aalevel_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    long lo, hi, val;
    uint32_t track, fill, face, text;
    int l, t, w, h, th, cx, cy, r, dot, vdeg, x, y, k, st;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj);
    t = _mtop(obj);
    w = _mwidth(obj);
    h = _mheight(obj);
    th = aa_text_height(&d->aa, obj);
    st = dial_disabled(obj) ? MUI_AA_DISABLED : MUI_AA_NORMAL;
    dial_range(obj, &lo, &hi, &val);
    mui_aa_slider_rgb(d->aa.tok, st, &track, &fill);
    mui_aa_button_rgb(d->aa.tok, MUI_AA_ROLE_NORMAL, st, &face, &text);
    if (d->aa.tc) {
        aa_round_corners(&d->aa, rp, l, t, w, h, d->m.radius, d->aa.bg_rgb);
    }
    aa_round_fill(&d->aa, rp, l, t, w, h, d->m.radius, face); /* the whole face: a value update repaints the needle over it */
    dot = d->m.font_px >= 12 ? 2 : 1;
    cx = l + w / 2;
    cy = t + h - th - d->m.pad_y - dot;
    r = h - th - 2 * d->m.pad_y - 2 * dot;
    if (r > w / 2 - d->m.pad_x) {
        r = w / 2 - d->m.pad_x;
    }
    if (r < 4) {
        return 0;
    }
    vdeg = mui_aa_dial_deg(val, lo, hi, MUI_AA_METER_START, MUI_AA_METER_SWEEP);
    dial_ring(&d->aa, rp, cx, cy, r, dot, MUI_AA_METER_START, MUI_AA_METER_SWEEP, vdeg, track, fill);
    for (k = 2 * dot; k < r - 2 * dot - 1; k += dot + 1) { /* the needle, a row of dots from the hub */
        mui_aa_dial_point(vdeg, k, &x, &y);
        aa_rect(&d->aa, rp, cx + x - dot / 2, cy + y - dot / 2, cx + x + dot / 2, cy + y + dot / 2, text);
    }
    dial_dot(&d->aa, rp, cx, cy, dot + 1, text);
    if (d->label != NULL) {
        aa_text(&d->aa, obj, rp, l + (w - aa_text_width(&d->aa, obj, d->label)) / 2, t + h - th - d->m.pad_y / 2, d->label, text, face);
    }
    return 0;
}

static ULONG aalevel_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aalevel_data* d;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)msg);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->label = (const char*)(uintptr_t)GetTagData(MUIA_Levelmeter_Label, 0, msg->ops_AttrList);
    return (ULONG)obj;
}

static ULONG aalevel_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aalevel_data* d = INST_DATA(cl, obj);
    struct TagItem* ti = FindTagItem(MUIA_Levelmeter_Label, msg->ops_AttrList);
    if (ti != NULL) {
        d->label = (const char*)(uintptr_t)ti->ti_Data;
        if (d->shown && aa_live(obj)) {
            aa_redraw(obj, MADF_DRAWOBJECT);
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aalevelmeter_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aalevelmeter_dispatch(void);
const struct aa_class_desc aalevelmeter_class_desc = {MUIC_Numeric, sizeof(aalevel_data), aalevelmeter_dispatch, &g_level_class};
__asm__(".section .text._aalevelmeter_dispatch,\"ax\"\n.even\n.globl _aalevelmeter_dispatch\n_aalevelmeter_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aalevelmeter_dispatch_c\n\tlea 12(sp),sp\n\trts\n.text\n");

static ULONG __attribute__((used)) aalevelmeter_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aalevel_new(cl, obj, (struct opSet*)msg);
    case OM_SET:
        return aalevel_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        if (((struct opGet*)msg)->opg_AttrID == MUIA_Levelmeter_Label) {
            *((struct opGet*)msg)->opg_Storage = (ULONG)(uintptr_t)((aalevel_data*)INST_DATA(cl, obj))->label;
            return TRUE;
        }
        break;
    AA_SHOWN_CASES(aalevel_data)
    case MUIM_Setup:
        return aalevel_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aalevel_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aalevel_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aalevel_draw(cl, obj, (struct MUIP_Draw*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* Tags: the Numeric class's own pass through. Defaults: no frame; the knob is on the cycle chain. */
static Object* dial_new(struct MUI_CustomClass** slot, const struct aa_class_desc* desc, int chain, const struct TagItem* tags) {
    struct TagItem def[3];
    if (*slot == NULL) {
        *slot = AA_CLASS_CREATE(desc);
    }
    if (*slot == NULL) {
        return NULL;
    }
    def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
    def[1].ti_Tag = MUIA_CycleChain, def[1].ti_Data = (ULONG)chain;
    def[2].ti_Tag = TAG_MORE, def[2].ti_Data = (ULONG)(uintptr_t)tags;
    return NewObjectA((*slot)->mcc_Class, NULL, def);
}

Object* UP_NewKnob(const struct TagItem* tags) {
    return dial_new(&g_knob_class, &aaknob_class_desc, TRUE, tags);
}

Object* UP_NewLevelmeter(const struct TagItem* tags) {
    return dial_new(&g_level_class, &aalevelmeter_class_desc, FALSE, tags);
}

void mui_aadial_class_free(void) {
    if (g_knob_class != NULL) {
        MUI_DeleteCustomClass(g_knob_class);
        g_knob_class = NULL;
    }
    if (g_level_class != NULL) {
        MUI_DeleteCustomClass(g_level_class);
        g_level_class = NULL;
    }
}
