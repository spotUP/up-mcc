/* UP-Pendisplay and UP-Poppen (MUI gap D). Swatch drawing of mui_aacolorfield_class.c; popup window and state machine of
 * mui_aanumeric_class.c (created on the first open, reused, disposed in cleanup). One source, two class descriptions, as Slider/Progress.
 *   UP-Pendisplay: MUIC_Pendisplay. Shows the colour of a struct MUI_PenSpec (map/up_penspec.h): an RGB spec in its exact colour, a
 *                  colormap or MUI pen spec in the colour that pen has on the screen.
 *   UP-Poppen:     MUIC_Poppen. A UP-Pendisplay that is a RelVerify button; a click opens a UP-Penadjust under it, whose changes set the spec. */
#include "mui_aa_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include <mcc/up.h>
#include "map/mui_aa.h"
#include "map/up_penspec.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

typedef struct aapendisplay_data {
    struct MUI_PenSpec spec;
    struct MUI_RGBcolor rgbc; /* storage of the MUIA_Pendisplay_RGBcolor answer */
    uint32_t rgb;             /* 0xRRGGBB shown */
    long pen;                 /* the screen pen of a colormap or MUI pen spec, -1 for RGB */
    Object* ref;
    Object *popup, *adjust; /* UP-Poppen only */
    mui_aa_pop_t pop;
    int deact;
    aa_ctx aa;
} aapendisplay_data;

static struct MUI_CustomClass *g_pendisplay_class, *g_poppen_class;

/* The colour of the spec: exact for RGB; for a pen the screen's colour of that pen (needs a live object, black before setup). */
static void pd_resolve(Object* obj, aapendisplay_data* d) {
    up_penspec_t p;
    ULONG c[3];
    up_penspec_parse(d->spec.buf, &p);
    d->pen = -1;
    if (p.kind == UP_PEN_RGB) {
        d->rgb = up_penspec_rgb24(p.r, p.g, p.b);
        return;
    }
    d->rgb = 0;
    if (p.kind == UP_PEN_NONE || !aa_live(obj)) {
        return;
    }
    d->pen = p.kind == UP_PEN_MUI ? (p.n < MPEN_COUNT ? (long)MUIPEN(_pens(obj)[p.n]) : 0) : p.n;
    GetRGB32(_screen(obj)->ViewPort.ColorMap, (ULONG)d->pen, 1, c);
    d->rgb = up_penspec_rgb24(c[0], c[1], c[2]);
}

static void pd_spec(Object* obj, aapendisplay_data* d, const struct MUI_PenSpec* s) {
    if (s == NULL) {
        return;
    }
    memcpy(d->spec.buf, s->buf, sizeof d->spec.buf);
    d->spec.buf[sizeof d->spec.buf - 1] = 0;
    pd_resolve(obj, d);
    if (aa_live(obj)) {
        aa_hold(&d->aa, d->rgb); /* a new colour on a palette screen needs its pen before the redraw */
        aa_redraw(obj, MADF_DRAWOBJECT);
    }
}

/* SetRGB / SetColormap / SetMUIPen: build the spec and set it as an attribute, so MUIA_Pendisplay_Spec notifies. */
static ULONG pd_set_spec(Object* obj, int kind, ULONG r, ULONG g, ULONG b, long n) {
    up_penspec_t p;
    struct MUI_PenSpec s;
    p.kind = kind, p.r = r, p.g = g, p.b = b, p.n = n;
    up_penspec_format(&p, s.buf);
    SetAttrs(obj, MUIA_Pendisplay_Spec, (ULONG)&s, TAG_DONE);
    return 0;
}

static ULONG pd_setup(struct IClass* cl, Object* obj, Msg msg) {
    aapendisplay_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    pd_resolve(obj, d);
    aa_hold(&d->aa, d->rgb);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_SURFACE_PRESSED]);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_FOCUS_RING]);
    return TRUE;
}

/* Close and dispose the popup window (and the adjust object with it). Only from cleanup: never from the window's own notification. */
static void pp_drop(Object* obj, aapendisplay_data* d) {
    Object* app = NULL;
    if (d->popup == NULL) {
        return;
    }
    GetAttr(MUIA_ApplicationObject, _win(obj), (ULONG*)&app);
    set(d->popup, MUIA_Window_Open, FALSE);
    if (app != NULL) {
        DoMethod(app, OM_REMMEMBER, (ULONG)d->popup);
    }
    MUI_DisposeObject(d->popup);
    d->popup = d->adjust = NULL;
    memset(&d->pop, 0, sizeof d->pop);
}

static ULONG pd_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aapendisplay_data* d = INST_DATA(cl, obj);
    pp_drop(obj, d);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG pd_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aapendisplay_data* d = INST_DATA(cl, obj);
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

static ULONG pd_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aapendisplay_data* d = INST_DATA(cl, obj);
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

static ULONG pd_new(struct IClass* cl, Object* obj, struct opSet* msg, ULONG input) {
    aapendisplay_data* d;
    struct opSet nm;
    struct TagItem def[4], *ti;
    def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
    def[1].ti_Tag = MUIA_InputMode, def[1].ti_Data = input;
    def[2].ti_Tag = MUIA_CycleChain, def[2].ti_Data = (ULONG)(input != MUIV_InputMode_None);
    def[3].ti_Tag = TAG_MORE, def[3].ti_Data = (ULONG)(uintptr_t)msg->ops_AttrList;
    nm = *msg;
    nm.ops_AttrList = def;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->pen = -1;
    strcpy(d->spec.buf, "m5"); /* MUI's default: the text pen */
    if ((ti = FindTagItem(MUIA_Pendisplay_Spec, msg->ops_AttrList)) != NULL) {
        pd_spec(obj, d, (const struct MUI_PenSpec*)ti->ti_Data);
    }
    pd_resolve(obj, d);
    return (ULONG)obj;
}

static ULONG pd_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aapendisplay_data* d = INST_DATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList, *ti;
    const struct MUI_RGBcolor* c;
    while ((ti = NextTagItem(&tags)) != NULL) {
        switch (ti->ti_Tag) {
        case MUIA_Pendisplay_Spec:
            if (ti->ti_Data != 0 && strncmp(((const struct MUI_PenSpec*)ti->ti_Data)->buf, d->spec.buf, sizeof d->spec.buf) != 0) {
                pd_spec(obj, d, (const struct MUI_PenSpec*)ti->ti_Data);
            }
            break;
        case MUIA_Pendisplay_RGBcolor:
            if ((c = (const struct MUI_RGBcolor*)ti->ti_Data) != NULL) {
                pd_set_spec(obj, UP_PEN_RGB, c->red, c->green, c->blue, 0);
            }
            break;
        case MUIA_Pendisplay_Reference:
            /* shortcut: the notify on an earlier reference stays; set the reference once (MUI programs do) */
            if ((d->ref = (Object*)ti->ti_Data) != NULL) {
                struct MUI_PenSpec* s = NULL;
                GetAttr(MUIA_Pendisplay_Spec, d->ref, (ULONG*)&s);
                pd_spec(obj, d, s);
                DoMethod(d->ref, MUIM_Notify, MUIA_Pendisplay_Spec, MUIV_EveryTime, (ULONG)obj, 3, MUIM_Set, MUIA_Pendisplay_Spec, MUIV_TriggerValue);
            }
            break;
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG pd_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aapendisplay_data* d = INST_DATA(cl, obj);
    up_penspec_t p;
    switch (msg->opg_AttrID) {
    case MUIA_Pendisplay_Spec:
        *msg->opg_Storage = (ULONG)&d->spec;
        return TRUE;
    case MUIA_Pendisplay_Pen:
        *msg->opg_Storage = (ULONG)d->pen;
        return TRUE;
    case MUIA_Pendisplay_Reference:
        *msg->opg_Storage = (ULONG)d->ref;
        return TRUE;
    case MUIA_Pendisplay_RGBcolor:
        if (up_penspec_parse(d->spec.buf, &p) == UP_PEN_RGB) {
            d->rgbc.red = p.r, d->rgbc.green = p.g, d->rgbc.blue = p.b;
        } else {
            d->rgbc.red = up_penspec_gun32(d->rgb >> 16), d->rgbc.green = up_penspec_gun32(d->rgb >> 8), d->rgbc.blue = up_penspec_gun32(d->rgb);
        }
        *msg->opg_Storage = (ULONG)&d->rgbc;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aapendisplay_dispatch_c(struct IClass* cl, Object* obj, Msg msg);
static ULONG __attribute__((used)) aapoppen_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aapendisplay_dispatch(void);
extern ULONG aapoppen_dispatch(void);
const struct aa_class_desc aapendisplay_class_desc = {MUIC_Area, sizeof(aapendisplay_data), aapendisplay_dispatch, &g_pendisplay_class};
const struct aa_class_desc aapoppen_class_desc = {MUIC_Area, sizeof(aapendisplay_data), aapoppen_dispatch, &g_poppen_class};
__asm__(".pushsection .text._aapendisplay_dispatch,\"ax\"\n.even\n.globl _aapendisplay_dispatch\n_aapendisplay_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapendisplay_dispatch_c\n\tlea 12(sp),sp\n\trts\n.popsection\n");
__asm__(".pushsection .text._aapoppen_dispatch,\"ax\"\n.even\n.globl _aapoppen_dispatch\n_aapoppen_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapoppen_dispatch_c\n\tlea 12(sp),sp\n\trts\n.popsection\n");

/* Everything but OM_NEW, shared by both classes (UP-Poppen is a UP-Pendisplay with a popup). */
static ULONG pd_common(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_SET:
        return pd_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return pd_get(cl, obj, (struct opGet*)msg);
    case MUIM_Setup:
        return pd_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return pd_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return pd_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return pd_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_Pendisplay_SetRGB:
        return pd_set_spec(obj, UP_PEN_RGB, ((struct MUIP_Pendisplay_SetRGB*)msg)->red, ((struct MUIP_Pendisplay_SetRGB*)msg)->green,
                           ((struct MUIP_Pendisplay_SetRGB*)msg)->blue, 0);
    case MUIM_Pendisplay_SetColormap:
        return pd_set_spec(obj, UP_PEN_CMAP, 0, 0, 0, ((struct MUIP_Pendisplay_SetColormap*)msg)->colormap);
    case MUIM_Pendisplay_SetMUIPen:
        return pd_set_spec(obj, UP_PEN_MUI, 0, 0, 0, ((struct MUIP_Pendisplay_SetMUIPen*)msg)->muipen);
    }
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG __attribute__((used)) aapendisplay_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    if (msg->MethodID == OM_NEW) {
        return pd_new(cl, obj, (struct opSet*)msg, MUIV_InputMode_None);
    }
    return pd_common(cl, obj, msg);
}

static uint32_t pp_now(void) {
    ULONG s = 0, m = 0;
    CurrentTime(&s, &m);
    return (uint32_t)(s * 1000UL + m / 1000UL);
}

/* The popup root: a Popup card around a UP-Penadjust showing the current spec. */
static Object* pp_build(aapendisplay_data* d) {
    Object* card;
    struct TagItem ta[2];
    ta[0].ti_Tag = MUIA_Pendisplay_Spec, ta[0].ti_Data = (ULONG)&d->spec;
    ta[1].ti_Tag = TAG_DONE, ta[1].ti_Data = 0;
    d->adjust = AA_SIBLING(UP_NewPenadjust, MUIC_UP_Penadjust, ta);
    if (d->adjust == NULL) {
        return NULL;
    }
    card = MUI_NewObject((char*)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Popup, UPA_Panel_Popup, (ULONG)TRUE, MUIA_Group_Child, (ULONG)d->adjust, TAG_DONE);
    if (card == NULL) {
        MUI_DisposeObject(d->adjust);
        d->adjust = NULL;
    }
    return card;
}

static void pp_open(Object* obj, aapendisplay_data* d, int act) {
    Object *app = NULL, *root;
    struct Window* win = _window(obj);
    if (win == NULL || !aa_live(obj)) {
        return;
    }
    if (act == MUI_AA_POP_OPEN_NEW) {
        GetAttr(MUIA_ApplicationObject, _win(obj), (ULONG*)&app);
        root = app != NULL ? pp_build(d) : NULL;
        if (root == NULL) {
            return;
        }
        d->popup = MUI_NewObject((char*)MUIC_Window, MUIA_Window_Screen, (ULONG)_screen(obj), MUIA_Window_Borderless, TRUE, MUIA_Window_DragBar, FALSE,
                                 MUIA_Window_CloseGadget, FALSE, MUIA_Window_SizeGadget, FALSE, MUIA_Window_DepthGadget, FALSE,
                                 MUIA_Window_LeftEdge, (ULONG)(win->LeftEdge + _left(obj)), MUIA_Window_TopEdge, (ULONG)(win->TopEdge + _bottom(obj) + 2),
                                 MUIA_Window_RootObject, (ULONG)root, TAG_DONE);
        if (d->popup == NULL) {
            MUI_DisposeObject(root); /* the root is still ours: the window never took it */
            d->adjust = NULL;
            return;
        }
        DoMethod(app, OM_ADDMEMBER, (ULONG)d->popup);
        DoMethod(d->adjust, MUIM_Notify, MUIA_Pendisplay_Spec, MUIV_EveryTime, (ULONG)obj, 3, MUIM_Set, MUIA_Pendisplay_Spec, MUIV_TriggerValue);
        DoMethod(d->popup, MUIM_Notify, MUIA_Window_Activate, FALSE, (ULONG)obj, 1, UPM_Poppen_Deactivated);
        DoMethod(d->popup, MUIM_Notify, MUIA_Window_Open, FALSE, (ULONG)obj, 1, UPM_Poppen_Closed);
    } else {
        set(d->popup, MUIA_Window_LeftEdge, (ULONG)(win->LeftEdge + _left(obj)));
        set(d->popup, MUIA_Window_TopEdge, (ULONG)(win->TopEdge + _bottom(obj) + 2));
    }
    nnset(d->adjust, MUIA_Pendisplay_Spec, (ULONG)&d->spec);
    mui_aa_pop_opened(&d->pop);
    set(d->popup, MUIA_Window_Open, TRUE);
}

static void pp_close(aapendisplay_data* d) {
    if (d->popup != NULL && d->pop.open) {
        set(d->popup, MUIA_Window_Open, FALSE); /* the Open notify runs UPM_Poppen_Closed */
    }
}

static ULONG __attribute__((used)) aapoppen_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    aapendisplay_data* d;
    int act;
    switch (msg->MethodID) {
    case OM_NEW:
        obj = (Object*)pd_new(cl, obj, (struct opSet*)msg, MUIV_InputMode_RelVerify);
        if (obj != NULL) {
            DoMethod(obj, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)obj, 1, UPM_Poppen_Click);
        }
        return (ULONG)obj;
    case OM_DISPOSE:
        d = INST_DATA(cl, obj);
        if (d->popup == NULL && d->adjust != NULL) {
            MUI_DisposeObject(d->adjust); /* built but never put in a window */
        }
        break;
    case UPM_Poppen_Click:
        d = INST_DATA(cl, obj);
        act = mui_aa_pop_click(&d->pop, pp_now());
        if (act == MUI_AA_POP_CLOSE) {
            pp_close(d);
        } else if (act != MUI_AA_POP_NONE) {
            pp_open(obj, d, act);
        }
        return 0;
    case UPM_Poppen_Deactivated:
        d = INST_DATA(cl, obj);
        d->deact = 1;
        pp_close(d);
        return 0;
    case UPM_Poppen_Closed:
        d = INST_DATA(cl, obj);
        mui_aa_pop_closed(&d->pop, d->deact, pp_now());
        d->deact = 0;
        return 0;
    }
    return pd_common(cl, obj, msg);
}

static Object* pd_make(struct MUI_CustomClass** slot, const struct aa_class_desc* desc, const struct TagItem* tags) {
    if (*slot == NULL) {
        *slot = AA_CLASS_CREATE(desc);
    }
    return *slot != NULL ? NewObjectA((*slot)->mcc_Class, NULL, (struct TagItem*)tags) : NULL;
}

Object* UP_NewPendisplay(const struct TagItem* tags) {
    return pd_make(&g_pendisplay_class, &aapendisplay_class_desc, tags);
}

Object* UP_NewPoppen(const struct TagItem* tags) {
    return pd_make(&g_poppen_class, &aapoppen_class_desc, tags);
}
