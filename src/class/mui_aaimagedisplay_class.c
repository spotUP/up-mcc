/* MUI gap E: UP-Imagedisplay and UP-Framedisplay (MUIC_Imagedisplay, MUIC_Framedisplay), and UP-Popimage and UP-Popframe (MUIC_Popimage,
 * MUIC_Popframe). One .mcc holds one class; the linker drops the other three.
 * The displays are Area classes that draw a preview of an image spec (map/up_specs.h) or a frame spec in the theme.
 * shortcut: the image preview shows the kind (colour fill, raised pattern/preset fill, or the file/class name), not the real image;
 * upgrade when a caller needs the real brush or picture drawn.
 * The pops are Groups around a UP-Popstring: its string is a display, its popup a UP-Imageadjust / UP-Frameadjust. The adjust's spec
 * notify runs UPM_Pop*_Changed, which shows the spec in the display and sets it on the pop, so it notifies. The popstring disposes the
 * popup root with the window cleanup; MUIM_Setup makes a new one. */
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

#include <mcc/up.h>
#include "map/up_specs.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

typedef struct aaspecdisp_data {
    char spec[UP_IMSPEC_LEN]; /* image spec, or frame spec in the first UP_FRSPEC_LEN bytes */
    int frame, shown;
    aa_ctx aa;
} aaspecdisp_data;

static struct MUI_CustomClass *g_imagedisplay_class, *g_framedisplay_class, *g_popimage_class, *g_popframe_class;

static ULONG sd_tag(int frame) {
    return frame ? MUIA_Framedisplay_Spec : MUIA_Imagedisplay_Spec;
}

static void sd_store(aaspecdisp_data* d, const char* s) {
    size_t n = d->frame ? UP_FRSPEC_LEN : UP_IMSPEC_LEN;
    strncpy(d->spec, s != NULL ? s : "", n - 1);
    d->spec[n - 1] = '\0';
}

/* A numeric value <= 0xFFFF is an MUII_ preset number (MUI 3.8 takes both); the rest is a spec string. */
static void sd_store_tag(aaspecdisp_data* d, ULONG v) {
    char b[8];
    if (!d->frame && v <= 0xFFFFu) {
        int i = 7;
        b[i] = '\0';
        do {
            b[--i] = (char)('0' + v % 10);
            v /= 10;
        } while (v != 0 && i > 2);
        b[--i] = ':', b[--i] = '6';
        sd_store(d, b + i);
    } else {
        sd_store(d, (const char*)v);
    }
}

static void sd_draw(Object* obj, aaspecdisp_data* d) {
    struct RastPort* rp = _rp(obj);
    const uint32_t* tok = d->aa.tok;
    int l = _mleft(obj), t = _mtop(obj), w = _mwidth(obj), h = _mheight(obj), r = (int)tok[SEL_GUI_TOKEN_RADIUS_SMALL];
    aa_erase(&d->aa, obj, l, t, w, h);
    if (d->frame) {
        int v[6];
        up_frspec_parse(d->spec, v);
        if (v[0] != 0) {
            aa_round_fill(&d->aa, rp, l, t, w, h, r, tok[v[1] ? SEL_GUI_TOKEN_SURFACE_PRESSED : SEL_GUI_TOKEN_SURFACE_RAISED]);
            aa_round_frame(&d->aa, rp, l, t, w, h, r, tok[SEL_GUI_TOKEN_DIVIDER]);
        }
        if (w > 2 + v[2] + v[3] && h > 2 + v[4] + v[5]) {
            aa_rect(&d->aa, rp, l + 1 + v[2], t + 1 + v[4], l + w - 2 - v[3], t + h - 2 - v[5], tok[SEL_GUI_TOKEN_ACCENT]);
        }
    } else {
        const char* tail;
        uint32_t rgb = tok[SEL_GUI_TOKEN_ACCENT];
        int has, k = up_imspec_parse(d->spec, &tail, &rgb, &has);
        uint32_t bg = k == 2 ? rgb : k == 0 || k == 6 ? tok[SEL_GUI_TOKEN_SURFACE_RAISED] : tok[SEL_GUI_TOKEN_SURFACE];
        aa_round_fill(&d->aa, rp, l, t, w, h, r, bg);
        aa_round_frame(&d->aa, rp, l, t, w, h, r, tok[SEL_GUI_TOKEN_DIVIDER]);
        if (k == 1 || k == 3 || k == 4 || k == 5) {
            const char* f = strrchr(tail, '/');
            f = f != NULL ? f + 1 : strrchr(tail, ':') != NULL ? strrchr(tail, ':') + 1 : tail;
            aa_text(&d->aa, obj, rp, l + 2, t + (h - aa_text_height(&d->aa, obj)) / 2, f, tok[SEL_GUI_TOKEN_TEXT_MUTED], bg);
        }
    }
}

static ULONG sd_dispatch(struct IClass* cl, Object* obj, Msg msg, int frame) {
    aaspecdisp_data* d;
    struct TagItem* ti;
    switch (msg->MethodID) {
    case OM_NEW: {
        struct TagItem def[2];
        struct opSet nm = *(struct opSet*)msg;
        def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
        def[1].ti_Tag = TAG_MORE, def[1].ti_Data = (ULONG)(uintptr_t)nm.ops_AttrList;
        nm.ops_AttrList = def;
        obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm);
        if (obj == NULL) {
            return 0;
        }
        d = INST_DATA(cl, obj);
        memset(d, 0, sizeof *d);
        d->frame = frame;
        ti = FindTagItem(sd_tag(frame), ((struct opSet*)msg)->ops_AttrList);
        sd_store_tag(d, ti != NULL ? ti->ti_Data : (ULONG)(uintptr_t)(frame ? "000000" : "0:128"));
        return (ULONG)obj;
    }
    case OM_SET:
        d = INST_DATA(cl, obj);
        if ((ti = FindTagItem(sd_tag(frame), ((struct opSet*)msg)->ops_AttrList)) != NULL && ti->ti_Data != (ULONG)d->spec) {
            sd_store_tag(d, ti->ti_Data);
            if (aa_live(obj)) {
                const char* tail;
                uint32_t rgb = 0;
                int has;
                up_imspec_parse(d->spec, &tail, &rgb, &has);
                if (has) {
                    aa_hold(&d->aa, rgb);
                }
                aa_redraw(obj, MADF_DRAWOBJECT);
            }
        }
        break;
    case OM_GET:
        if (((struct opGet*)msg)->opg_AttrID == sd_tag(frame)) {
            *((struct opGet*)msg)->opg_Storage = (ULONG)((aaspecdisp_data*)INST_DATA(cl, obj))->spec;
            return TRUE;
        }
        break;
    case MUIM_Setup: {
        const char* tail;
        uint32_t rgb = 0;
        int has, i;
        static const int held[] = {SEL_GUI_TOKEN_SURFACE, SEL_GUI_TOKEN_SURFACE_RAISED, SEL_GUI_TOKEN_SURFACE_PRESSED, SEL_GUI_TOKEN_DIVIDER,
                                   SEL_GUI_TOKEN_ACCENT, SEL_GUI_TOKEN_TEXT_MUTED};
        d = INST_DATA(cl, obj);
        if (!DoSuperMethodA(cl, obj, msg)) {
            return FALSE;
        }
        aa_setup(obj, &d->aa, NULL, 0);
        for (i = 0; i < (int)(sizeof held / sizeof held[0]); i++) {
            aa_hold(&d->aa, d->aa.tok[held[i]]);
        }
        if (!frame && up_imspec_parse(d->spec, &tail, &rgb, &has) == 2 && has) {
            aa_hold(&d->aa, rgb);
        }
        return TRUE;
    }
    case MUIM_Cleanup:
        aa_cleanup(&((aaspecdisp_data*)INST_DATA(cl, obj))->aa);
        break;
    case MUIM_AskMinMax: {
        struct MUI_MinMax* mm = ((struct MUIP_AskMinMax*)msg)->MinMaxInfo;
        int w, h;
        d = INST_DATA(cl, obj);
        DoSuperMethodA(cl, obj, msg);
        w = 3 * d->aa.font_px, h = up_rows(d->aa.font_px + d->aa.font_px / 2, d->aa.aspect_q8);
        mm->MinWidth += w, mm->DefWidth += 2 * w, mm->MaxWidth = MUI_MAXMAX;
        mm->MinHeight += h, mm->DefHeight += h, mm->MaxHeight = MUI_MAXMAX;
        return 0;
    }
    case MUIM_Draw:
        d = INST_DATA(cl, obj);
        DoSuperMethodA(cl, obj, msg);
        if (d->shown && (((struct MUIP_Draw*)msg)->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
            sd_draw(obj, d);
        }
        return 0;
    AA_SHOWN_CASES(aaspecdisp_data)
    }
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG __attribute__((used)) aaimagedisplay_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return sd_dispatch(cl, obj, msg, 0);
}
static ULONG __attribute__((used)) aaframedisplay_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return sd_dispatch(cl, obj, msg, 1);
}

/* ---- UP-Popimage / UP-Popframe ---- */

typedef struct aaspecpop_data {
    Object *pop, *disp;
} aaspecpop_data;

static Object* sp_adjust(int frame, Object* obj, const char* spec) {
    struct TagItem t[2];
    Object* a;
    t[0].ti_Tag = sd_tag(frame), t[0].ti_Data = (ULONG)(uintptr_t)spec;
    t[1].ti_Tag = TAG_DONE, t[1].ti_Data = 0;
    a = frame ? AA_SIBLING(UP_NewFrameadjust, "UP-Frameadjust.mcc", t) : AA_SIBLING(UP_NewImageadjust, "UP-Imageadjust.mcc", t);
    if (a != NULL && obj != NULL) { /* OM_NEW adds the notify itself once the pop exists */
        DoMethod(a, MUIM_Notify, sd_tag(frame), MUIV_EveryTime, (ULONG)obj, 1, frame ? UPM_Popframe_Changed : UPM_Popimage_Changed);
    }
    return a;
}

static const char* sp_spec(int frame, Object* o) {
    ULONG s = 0;
    GetAttr(sd_tag(frame), o, &s);
    return (const char*)s;
}

static ULONG sp_dispatch(struct IClass* cl, Object* obj, Msg msg, int frame) {
    aaspecpop_data* d;
    ULONG popup = 0;
    struct TagItem *ti, t[4];
    switch (msg->MethodID) {
    case OM_NEW: {
        struct opSet nm = *(struct opSet*)msg;
        Object *disp, *adj, *pop = NULL;
        ti = FindTagItem(sd_tag(frame), nm.ops_AttrList);
        t[0].ti_Tag = sd_tag(frame), t[0].ti_Data = ti != NULL ? ti->ti_Data : (ULONG)(uintptr_t)(frame ? "000000" : "0:128");
        t[1].ti_Tag = TAG_DONE, t[1].ti_Data = 0;
        disp = frame ? AA_SIBLING(UP_NewFramedisplay, "UP-Framedisplay.mcc", t) : AA_SIBLING(UP_NewImagedisplay, "UP-Imagedisplay.mcc", t);
        adj = disp != NULL ? sp_adjust(frame, NULL, sp_spec(frame, disp)) : NULL;
        if (adj != NULL) {
            t[0].ti_Tag = UPA_Popstring_String, t[0].ti_Data = (ULONG)disp;
            t[1].ti_Tag = UPA_Popstring_Popup, t[1].ti_Data = (ULONG)adj;
            t[2].ti_Tag = TAG_DONE, t[2].ti_Data = 0;
            pop = AA_SIBLING(UP_NewPopstring, "UP-Popstring.mcc", t);
        }
        if (pop == NULL) {
            if (adj != NULL) MUI_DisposeObject(adj);
            if (disp != NULL) MUI_DisposeObject(disp);
            return 0;
        }
        t[0].ti_Tag = MUIA_Frame, t[0].ti_Data = (ULONG)MUIV_Frame_None;
        t[1].ti_Tag = MUIA_Group_Child, t[1].ti_Data = (ULONG)pop;
        t[2].ti_Tag = TAG_MORE, t[2].ti_Data = (ULONG)(uintptr_t)nm.ops_AttrList;
        nm.ops_AttrList = t;
        obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm); /* MUIC_Group disposes its children on failure */
        if (obj == NULL) {
            return 0;
        }
        d = INST_DATA(cl, obj);
        d->pop = pop, d->disp = disp;
        DoMethod(adj, MUIM_Notify, sd_tag(frame), MUIV_EveryTime, (ULONG)obj, 1, frame ? UPM_Popframe_Changed : UPM_Popimage_Changed);
        return (ULONG)obj;
    }
    case OM_SET:
        d = INST_DATA(cl, obj);
        if ((ti = FindTagItem(sd_tag(frame), ((struct opSet*)msg)->ops_AttrList)) != NULL && ti->ti_Data != (ULONG)sp_spec(frame, d->disp)) {
            nnset(d->disp, sd_tag(frame), ti->ti_Data);
            GetAttr(UPA_Popstring_Popup, d->pop, &popup);
            if (popup != 0) {
                nnset((Object*)popup, sd_tag(frame), (ULONG)sp_spec(frame, d->disp));
            }
        }
        break;
    case OM_GET:
        if (((struct opGet*)msg)->opg_AttrID == sd_tag(frame)) {
            *((struct opGet*)msg)->opg_Storage = (ULONG)sp_spec(frame, ((aaspecpop_data*)INST_DATA(cl, obj))->disp);
            return TRUE;
        }
        break;
    case MUIM_Setup:
        d = INST_DATA(cl, obj);
        GetAttr(UPA_Popstring_Popup, d->pop, &popup);
        if (popup == 0 && (popup = (ULONG)sp_adjust(frame, obj, sp_spec(frame, d->disp))) != 0) {
            SetAttrs(d->pop, UPA_Popstring_Popup, popup, TAG_DONE);
        }
        break;
    case UPM_Popimage_Changed:
    case UPM_Popframe_Changed:
        d = INST_DATA(cl, obj);
        GetAttr(UPA_Popstring_Popup, d->pop, &popup);
        if (popup != 0) {
            nnset(d->disp, sd_tag(frame), (ULONG)sp_spec(frame, (Object*)popup));
            SetAttrs(obj, sd_tag(frame), (ULONG)sp_spec(frame, d->disp), TAG_DONE);
        }
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG __attribute__((used)) aapopimage_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return sp_dispatch(cl, obj, msg, 0);
}
static ULONG __attribute__((used)) aapopframe_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return sp_dispatch(cl, obj, msg, 1);
}

#define SPEC_CLASS(name, super, data, gcls, Ctor) \
    extern ULONG name##_dispatch(void); \
    const struct aa_class_desc name##_class_desc = {super, sizeof(data), name##_dispatch, &gcls}; \
    __asm__(".pushsection .text._" #name "_dispatch,\"ax\"\n.even\n.globl _" #name "_dispatch\n_" #name "_dispatch:\n" \
            "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n" \
            "\tjbsr _" #name "_dispatch_c\n\tlea 12(sp),sp\n\trts\n.popsection\n"); \
    Object* Ctor(const struct TagItem* tags) { \
        if (gcls == NULL) { \
            gcls = AA_CLASS_CREATE(&name##_class_desc); \
        } \
        return gcls != NULL ? NewObjectA(gcls->mcc_Class, NULL, (struct TagItem*)tags) : NULL; \
    }

SPEC_CLASS(aaimagedisplay, MUIC_Area, aaspecdisp_data, g_imagedisplay_class, UP_NewImagedisplay)
SPEC_CLASS(aaframedisplay, MUIC_Area, aaspecdisp_data, g_framedisplay_class, UP_NewFramedisplay)
SPEC_CLASS(aapopimage, MUIC_Group, aaspecpop_data, g_popimage_class, UP_NewPopimage)
SPEC_CLASS(aapopframe, MUIC_Group, aaspecpop_data, g_popframe_class, UP_NewPopframe)
