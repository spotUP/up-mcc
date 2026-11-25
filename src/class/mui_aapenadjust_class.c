/* UP-Penadjust (MUI gap D): MUIC_Penadjust, and MUIC_Coloradjust with UPA_Penadjust_Coloradjust TRUE (one .mcc holds one class).
 * A Group of sibling classes, as UP-Scrollgroup: an optional UP-Cycle for the kind (RGB / Colormap / MUI pen), a UP-Colorfield preview and
 * a two-column group of UP-Text labels with UP-Sliders (Red, Green, Blue 0..255; Pen 0..255 colormap or 0..7 MUI pen). A child change
 * runs UPM_Penadjust_Changed, which builds the spec and sets MUIA_Pendisplay_Spec and the MUIA_Coloradjust_* attributes, so both notify.
 * A set of the spec goes back to the children with nnset (MUIA_NoNotify), so no notify loop. */
#include "mui_aa_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include <mcc/up.h>
#include "map/up_penspec.h"

extern struct Library* MUIMasterBase;

#define PA_ROWS 4 /* red, green, blue, pen */

typedef struct aapenadjust_data {
    struct MUI_PenSpec spec;
    ULONG rgb[3]; /* 32-bit guns: MUIA_Coloradjust_RGB points here */
    ULONG modeid;
    Object *cycle, *field, *label[PA_ROWS], *slider[PA_ROWS];
    int kind; /* UP_PEN_RGB / CMAP / MUI shown */
} aapenadjust_data;

static struct MUI_CustomClass* g_penadjust_class;
static const char* g_kinds[] = {"RGB", "Colormap", "MUI pen", NULL};
static const char* g_kinds_psi[] = {"RGB", "Colormap", NULL}; /* MUIA_Penadjust_PSIMode: no MUI pens */
static const char* g_rows[PA_ROWS] = {"Red", "Green", "Blue", "Pen"};

static ULONG pa_slider_value(Object* s) {
    ULONG v = 0;
    GetAttr(MUIA_Numeric_Value, s, &v);
    return v;
}

/* Show the spec in the children without notifying (the child notifies would come back as UPM_Penadjust_Changed). */
static void pa_show(Object* obj, aapenadjust_data* d) {
    up_penspec_t p;
    ULONG c[3];
    int i, rgb;
    up_penspec_parse(d->spec.buf, &p);
    if (p.kind == UP_PEN_NONE || (d->cycle == NULL && p.kind != UP_PEN_RGB)) {
        p.kind = UP_PEN_RGB; /* Coloradjust mode knows only RGB */
    }
    if (p.kind == UP_PEN_RGB) {
        d->rgb[0] = p.r, d->rgb[1] = p.g, d->rgb[2] = p.b;
    } else if (_screen(obj) != NULL && muiRenderInfo(obj) != NULL) {
        /* the preview of a pen: its colour on the screen; before setup the guns stay as they were */
        GetRGB32(_screen(obj)->ViewPort.ColorMap, (ULONG)(p.kind == UP_PEN_MUI && p.n < MPEN_COUNT ? MUIPEN(_pens(obj)[p.n]) : p.n), 1, c);
        d->rgb[0] = c[0], d->rgb[1] = c[1], d->rgb[2] = c[2];
    }
    d->kind = p.kind;
    rgb = p.kind == UP_PEN_RGB;
    for (i = 0; i < PA_ROWS; i++) {
        nnset(d->label[i], MUIA_ShowMe, (ULONG)((i < 3) == rgb));
        nnset(d->slider[i], MUIA_ShowMe, (ULONG)((i < 3) == rgb));
        if (i < 3) {
            nnset(d->slider[i], MUIA_Numeric_Value, d->rgb[i] >> 24);
        }
    }
    nnset(d->slider[3], MUIA_Numeric_Max, (ULONG)(p.kind == UP_PEN_MUI ? MPEN_COUNT - 1 : 255));
    if (!rgb) {
        nnset(d->slider[3], MUIA_Numeric_Value, (ULONG)p.n);
    }
    if (d->cycle != NULL) {
        nnset(d->cycle, UPA_Cycle_Active, (ULONG)(p.kind - UP_PEN_RGB));
    }
    nnset(d->field, UPA_Colorfield_RGB, up_penspec_rgb24(d->rgb[0], d->rgb[1], d->rgb[2]));
}

static void pa_store(Object* obj, aapenadjust_data* d, const char* buf) {
    memcpy(d->spec.buf, buf, sizeof d->spec.buf);
    d->spec.buf[sizeof d->spec.buf - 1] = 0;
    pa_show(obj, d);
}

/* A child changed: read the kind and the sliders, build the spec, set it on the object (so it notifies). */
static ULONG pa_changed(Object* obj, aapenadjust_data* d) {
    up_penspec_t p;
    struct MUI_PenSpec s;
    ULONG kind = 0;
    if (d->cycle != NULL) {
        GetAttr(UPA_Cycle_Active, d->cycle, &kind);
    }
    memset(&p, 0, sizeof p);
    p.kind = UP_PEN_RGB + (int)kind;
    if (p.kind == UP_PEN_RGB) {
        p.r = up_penspec_gun32(pa_slider_value(d->slider[0]));
        p.g = up_penspec_gun32(pa_slider_value(d->slider[1]));
        p.b = up_penspec_gun32(pa_slider_value(d->slider[2]));
    } else if (p.kind == d->kind) {
        p.n = (long)pa_slider_value(d->slider[3]);
    } /* a new kind starts at pen 0 */
    up_penspec_format(&p, s.buf);
    pa_store(obj, d, s.buf);
    SetAttrs(obj, MUIA_Pendisplay_Spec, (ULONG)&d->spec, MUIA_Coloradjust_Red, d->rgb[0], MUIA_Coloradjust_Green, d->rgb[1],
             MUIA_Coloradjust_Blue, d->rgb[2], MUIA_Coloradjust_RGB, (ULONG)d->rgb, TAG_DONE);
    return 0;
}

/* One gun changed by Coloradjust_Red/Green/Blue/RGB: the spec becomes RGB with the new guns. */
static void pa_guns(Object* obj, aapenadjust_data* d) {
    up_penspec_t p;
    char buf[UP_PENSPEC_LEN];
    p.kind = UP_PEN_RGB, p.r = d->rgb[0], p.g = d->rgb[1], p.b = d->rgb[2], p.n = 0;
    up_penspec_format(&p, buf);
    pa_store(obj, d, buf);
}

static void pa_dispose_all(Object** o, int n) {
    while (n-- > 0) {
        if (o[n] != NULL) {
            MUI_DisposeObject(o[n]);
        }
    }
}

static ULONG pa_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aapenadjust_data* d;
    struct TagItem tt[2], ts[4], tc[3], none[] = {{TAG_DONE, 0}}, def[6], *ti;
    Object *kids[2 + 2 * PA_ROWS], *grid, *cycle = NULL;
    struct MUI_PenSpec* spec;
    int i, n = 0, coloradjust = (int)GetTagData(UPA_Penadjust_Coloradjust, FALSE, msg->ops_AttrList);
    struct opSet nm = *msg;
    memset(kids, 0, sizeof kids);
    if (!coloradjust) {
        tc[0].ti_Tag = UPA_Cycle_Entries, tc[0].ti_Data = (ULONG)(uintptr_t)(GetTagData(MUIA_Penadjust_PSIMode, FALSE, msg->ops_AttrList) ? g_kinds_psi : g_kinds);
        tc[1].ti_Tag = TAG_DONE, tc[1].ti_Data = 0;
        cycle = AA_SIBLING(UP_NewCycle, "UP-Cycle.mcc", tc);
    }
    kids[n++] = AA_SIBLING(UP_NewColorfield, "UP-Colorfield.mcc", none);
    for (i = 0; i < PA_ROWS; i++) {
        tt[0].ti_Tag = UPA_Text_Contents, tt[0].ti_Data = (ULONG)(uintptr_t)g_rows[i];
        tt[1].ti_Tag = TAG_DONE, tt[1].ti_Data = 0;
        ts[0].ti_Tag = MUIA_Numeric_Min, ts[0].ti_Data = 0;
        ts[1].ti_Tag = MUIA_Numeric_Max, ts[1].ti_Data = 255;
        ts[2].ti_Tag = MUIA_Numeric_Value, ts[2].ti_Data = 0;
        ts[3].ti_Tag = TAG_DONE, ts[3].ti_Data = 0;
        kids[n++] = AA_SIBLING(UP_NewText, "UP-Text.mcc", tt);
        kids[n++] = AA_SIBLING(UP_NewSlider, "UP-Slider.mcc", ts);
    }
    for (i = 0; i < n; i++) {
        if (kids[i] == NULL) {
            break;
        }
    }
    grid = i == n && (coloradjust || cycle != NULL)
               ? MUI_NewObject((char*)MUIC_Group, MUIA_Group_Columns, 2, MUIA_Group_Child, (ULONG)kids[1], MUIA_Group_Child, (ULONG)kids[2], MUIA_Group_Child, (ULONG)kids[3],
                               MUIA_Group_Child, (ULONG)kids[4], MUIA_Group_Child, (ULONG)kids[5], MUIA_Group_Child, (ULONG)kids[6], MUIA_Group_Child, (ULONG)kids[7],
                               MUIA_Group_Child, (ULONG)kids[8], TAG_DONE)
               : NULL;
    if (grid == NULL) {
        pa_dispose_all(kids, n);
        pa_dispose_all(&cycle, 1);
        return 0;
    }
    i = 0;
    def[i].ti_Tag = MUIA_Frame, def[i++].ti_Data = (ULONG)MUIV_Frame_None;
    if (cycle != NULL) {
        def[i].ti_Tag = MUIA_Group_Child, def[i++].ti_Data = (ULONG)cycle;
    }
    def[i].ti_Tag = MUIA_Group_Child, def[i++].ti_Data = (ULONG)kids[0];
    def[i].ti_Tag = MUIA_Group_Child, def[i++].ti_Data = (ULONG)grid;
    def[i].ti_Tag = TAG_MORE, def[i].ti_Data = (ULONG)(uintptr_t)msg->ops_AttrList;
    nm.ops_AttrList = def;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm); /* MUIC_Group disposes its children on failure */
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->cycle = cycle, d->field = kids[0];
    for (i = 0; i < PA_ROWS; i++) {
        d->label[i] = kids[1 + 2 * i];
        d->slider[i] = kids[2 + 2 * i];
        DoMethod(d->slider[i], MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime, (ULONG)obj, 1, UPM_Penadjust_Changed);
    }
    if (cycle != NULL) {
        DoMethod(cycle, MUIM_Notify, UPA_Cycle_Active, MUIV_EveryTime, (ULONG)obj, 1, UPM_Penadjust_Changed);
    }
    d->modeid = GetTagData(MUIA_Coloradjust_ModeID, 0, msg->ops_AttrList);
    spec = (struct MUI_PenSpec*)GetTagData(MUIA_Pendisplay_Spec, 0, msg->ops_AttrList);
    pa_store(obj, d, spec != NULL ? spec->buf : "r00000000,00000000,00000000");
    if ((ti = FindTagItem(MUIA_Coloradjust_RGB, msg->ops_AttrList)) != NULL && ti->ti_Data != 0) {
        memcpy(d->rgb, (const ULONG*)ti->ti_Data, sizeof d->rgb);
        pa_guns(obj, d);
    }
    return (ULONG)obj;
}

static ULONG pa_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aapenadjust_data* d = INST_DATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList, *ti;
    while ((ti = NextTagItem(&tags)) != NULL) {
        switch (ti->ti_Tag) {
        case MUIA_Pendisplay_Spec:
            if (ti->ti_Data != 0 && ti->ti_Data != (ULONG)&d->spec) {
                pa_store(obj, d, ((const struct MUI_PenSpec*)ti->ti_Data)->buf);
            }
            break;
        case MUIA_Coloradjust_Red:
        case MUIA_Coloradjust_Green:
        case MUIA_Coloradjust_Blue:
            if (d->rgb[ti->ti_Tag == MUIA_Coloradjust_Red ? 0 : ti->ti_Tag == MUIA_Coloradjust_Green ? 1 : 2] != ti->ti_Data) {
                d->rgb[ti->ti_Tag == MUIA_Coloradjust_Red ? 0 : ti->ti_Tag == MUIA_Coloradjust_Green ? 1 : 2] = ti->ti_Data;
                pa_guns(obj, d);
            }
            break;
        case MUIA_Coloradjust_RGB:
            if (ti->ti_Data != 0 && ti->ti_Data != (ULONG)d->rgb) {
                memcpy(d->rgb, (const ULONG*)ti->ti_Data, sizeof d->rgb);
                pa_guns(obj, d);
            }
            break;
        case MUIA_Coloradjust_ModeID:
            d->modeid = ti->ti_Data; /* stored only: the sliders work on any screen mode */
            break;
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG pa_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aapenadjust_data* d = INST_DATA(cl, obj);
    switch (msg->opg_AttrID) {
    case MUIA_Pendisplay_Spec:
        *msg->opg_Storage = (ULONG)&d->spec;
        return TRUE;
    case MUIA_Coloradjust_Red:
        *msg->opg_Storage = d->rgb[0];
        return TRUE;
    case MUIA_Coloradjust_Green:
        *msg->opg_Storage = d->rgb[1];
        return TRUE;
    case MUIA_Coloradjust_Blue:
        *msg->opg_Storage = d->rgb[2];
        return TRUE;
    case MUIA_Coloradjust_RGB:
        *msg->opg_Storage = (ULONG)d->rgb;
        return TRUE;
    case MUIA_Coloradjust_ModeID:
        *msg->opg_Storage = d->modeid;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aapenadjust_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return pa_new(cl, obj, (struct opSet*)msg);
    case OM_SET:
        return pa_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return pa_get(cl, obj, (struct opGet*)msg);
    case MUIM_Setup:
        if (!DoSuperMethodA(cl, obj, msg)) {
            return FALSE;
        }
        pa_show(obj, INST_DATA(cl, obj)); /* a pen spec's preview needs the screen */
        return TRUE;
    case UPM_Penadjust_Changed:
        return pa_changed(obj, INST_DATA(cl, obj));
    }
    return DoSuperMethodA(cl, obj, msg);
}

extern ULONG aapenadjust_dispatch(void);
const struct aa_class_desc aapenadjust_class_desc = {MUIC_Group, sizeof(aapenadjust_data), aapenadjust_dispatch, &g_penadjust_class};
__asm__(".text\n.even\n.globl _aapenadjust_dispatch\n_aapenadjust_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapenadjust_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

Object* UP_NewPenadjust(const struct TagItem* tags) {
    if (g_penadjust_class == NULL) {
        g_penadjust_class = AA_CLASS_CREATE(&aapenadjust_class_desc);
    }
    return g_penadjust_class != NULL ? NewObjectA(g_penadjust_class->mcc_Class, NULL, (struct TagItem*)tags) : NULL;
}
