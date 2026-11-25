/* MUI gap E: UP-Imageadjust and UP-Frameadjust (MUIC_Imageadjust, MUIC_Frameadjust). One .mcc holds one class.
 * Groups of sibling classes, as UP-Penadjust:
 * Imageadjust: a UP-Cycle of the spec kinds MUIA_Imageadjust_Type allows, a UP-Imagedisplay preview and a UP-Field for the value after "<d>:".
 * Frameadjust: a UP-Framedisplay preview, a UP-Checkmark for recessed and UP-Text / UP-Slider rows for the type and the four spacings.
 * A child change runs UPM_*adjust_Changed, which builds the spec (map/up_specs.h) and sets it on the object, so it notifies. A set of the
 * spec goes back to the children with nnset (MUIA_NoNotify), so no notify loop. */
#include "mui_aa_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include <mcc/up.h>
#include "map/up_specs.h"

extern struct Library* MUIMasterBase;

#define FA_ROWS 5 /* type, left, right, top, bottom */
#define IA_KINDS 7

typedef struct aaimageadjust_data {
    char spec[UP_IMSPEC_LEN];
    const char* digits; /* the kind digits the cycle shows, by MUIA_Imageadjust_Type */
    Object *cycle, *preview, *field;
} aaimageadjust_data;

typedef struct aaframeadjust_data {
    char spec[UP_FRSPEC_LEN];
    Object *preview, *recessed, *slider[FA_ROWS];
} aaframeadjust_data;

static struct MUI_CustomClass *g_imageadjust_class, *g_frameadjust_class;
static const char* const g_kind_names[IA_KINDS] = {"Pattern", "Vector", "Colour", "Class", "Brush", "Picture", "Preset"};
static const char* const g_type_digits[4] = {"0123456", "13456", "025", "2"}; /* MUIV_Imageadjust_Type_All, _Image, _Background, _Pen */
static const char* const g_frame_rows[FA_ROWS] = {"Type", "Left", "Right", "Top", "Bottom"};

static ULONG aj_get(Object* o, ULONG tag) {
    ULONG v = 0;
    GetAttr(tag, o, &v);
    return v;
}

static void aj_dispose(Object** o, int n) {
    while (n-- > 0) {
        if (o[n] != NULL) {
            MUI_DisposeObject(o[n]);
        }
    }
}

/* kids[0..n-1] as MUIA_Group_Child tags in t; 0, and every kid disposed, when one kid is missing. */
static int aj_children(Object** kids, int n, struct TagItem* t) {
    int i;
    for (i = 0; i < n; i++) {
        if (kids[i] == NULL) {
            aj_dispose(kids, n);
            return 0;
        }
        t[i].ti_Tag = MUIA_Group_Child, t[i].ti_Data = (ULONG)kids[i];
    }
    return n;
}

/* ---- UP-Imageadjust ---- */

static void ia_show(aaimageadjust_data* d) {
    const char *tail, *k;
    uint32_t rgb;
    int has, kind = up_imspec_parse(d->spec, &tail, &rgb, &has);
    k = kind >= 0 ? strchr(d->digits, '0' + kind) : NULL;
    nnset(d->cycle, UPA_Cycle_Active, (ULONG)(k != NULL ? k - d->digits : 0));
    nnset(d->field, MUIA_String_Contents, (ULONG)(uintptr_t)tail);
    nnset(d->preview, MUIA_Imagedisplay_Spec, (ULONG)d->spec);
}

static ULONG ia_dispatch(struct IClass* cl, Object* obj, Msg msg) {
    aaimageadjust_data* d;
    struct TagItem* ti;
    switch (msg->MethodID) {
    case OM_NEW: {
        static const char* entries[4][IA_KINDS + 1];
        struct opSet nm = *(struct opSet*)msg;
        struct TagItem tc[2], none[] = {{TAG_DONE, 0}}, def[5];
        Object* kids[3];
        ULONG type = GetTagData(MUIA_Imageadjust_Type, MUIV_Imageadjust_Type_All, nm.ops_AttrList);
        const char* digits = g_type_digits[type < 4 ? type : 0];
        int i;
        for (i = 0; digits[i] != '\0'; i++) {
            entries[type < 4 ? type : 0][i] = g_kind_names[digits[i] - '0'];
        }
        tc[0].ti_Tag = UPA_Cycle_Entries, tc[0].ti_Data = (ULONG)(uintptr_t)entries[type < 4 ? type : 0];
        tc[1].ti_Tag = TAG_DONE, tc[1].ti_Data = 0;
        kids[0] = AA_SIBLING(UP_NewCycle, "UP-Cycle.mcc", tc);
        kids[1] = AA_SIBLING(UP_NewImagedisplay, "UP-Imagedisplay.mcc", none);
        kids[2] = AA_SIBLING(UP_NewField, "UP-Field.mcc", none);
        def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
        if (!aj_children(kids, 3, def + 1)) {
            return 0;
        }
        def[4].ti_Tag = TAG_MORE, def[4].ti_Data = (ULONG)(uintptr_t)nm.ops_AttrList;
        nm.ops_AttrList = def;
        obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm); /* MUIC_Group disposes its children on failure */
        if (obj == NULL) {
            return 0;
        }
        d = INST_DATA(cl, obj);
        d->digits = digits, d->cycle = kids[0], d->preview = kids[1], d->field = kids[2];
        ti = FindTagItem(MUIA_Imagedisplay_Spec, ((struct opSet*)msg)->ops_AttrList);
        up_imspec_format(digits[0] - '0', "", d->spec);
        if (ti != NULL && ti->ti_Data > 0xFFFFu) {
            strncpy(d->spec, (const char*)ti->ti_Data, sizeof d->spec - 1);
        }
        ia_show(d);
        DoMethod(kids[0], MUIM_Notify, UPA_Cycle_Active, MUIV_EveryTime, (ULONG)obj, 1, UPM_Imageadjust_Changed);
        DoMethod(kids[2], MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, (ULONG)obj, 1, UPM_Imageadjust_Changed);
        return (ULONG)obj;
    }
    case OM_SET:
        d = INST_DATA(cl, obj);
        if ((ti = FindTagItem(MUIA_Imagedisplay_Spec, ((struct opSet*)msg)->ops_AttrList)) != NULL && ti->ti_Data > 0xFFFFu &&
            ti->ti_Data != (ULONG)d->spec) {
            strncpy(d->spec, (const char*)ti->ti_Data, sizeof d->spec - 1);
            ia_show(d);
        }
        break;
    case OM_GET:
        if (((struct opGet*)msg)->opg_AttrID == MUIA_Imagedisplay_Spec) {
            *((struct opGet*)msg)->opg_Storage = (ULONG)((aaimageadjust_data*)INST_DATA(cl, obj))->spec;
            return TRUE;
        }
        break;
    case UPM_Imageadjust_Changed: {
        const char* v;
        d = INST_DATA(cl, obj);
        v = (const char*)aj_get(d->field, MUIA_String_Contents);
        up_imspec_format(d->digits[aj_get(d->cycle, UPA_Cycle_Active) % strlen(d->digits)] - '0', v != NULL ? v : "", d->spec);
        nnset(d->preview, MUIA_Imagedisplay_Spec, (ULONG)d->spec);
        SetAttrs(obj, MUIA_Imagedisplay_Spec, (ULONG)d->spec, TAG_DONE);
        return 0;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* ---- UP-Frameadjust ---- */

static void fa_show(aaframeadjust_data* d) {
    int v[6], i;
    up_frspec_parse(d->spec, v);
    nnset(d->recessed, MUIA_Selected, (ULONG)v[1]);
    for (i = 0; i < FA_ROWS; i++) {
        nnset(d->slider[i], MUIA_Numeric_Value, (ULONG)v[i == 0 ? 0 : i + 1]);
    }
    nnset(d->preview, MUIA_Framedisplay_Spec, (ULONG)d->spec);
}

static ULONG fa_dispatch(struct IClass* cl, Object* obj, Msg msg) {
    aaframeadjust_data* d;
    struct TagItem* ti;
    int i;
    switch (msg->MethodID) {
    case OM_NEW: {
        struct opSet nm = *(struct opSet*)msg;
        struct TagItem tt[2], ts[4], tk[2], none[] = {{TAG_DONE, 0}}, def[5], grid[2 * FA_ROWS + 2];
        Object *rows[2 * FA_ROWS], *kids[3];
        for (i = 0; i < FA_ROWS; i++) {
            tt[0].ti_Tag = UPA_Text_Contents, tt[0].ti_Data = (ULONG)(uintptr_t)g_frame_rows[i];
            tt[1].ti_Tag = TAG_DONE, tt[1].ti_Data = 0;
            ts[0].ti_Tag = MUIA_Numeric_Min, ts[0].ti_Data = 0;
            ts[1].ti_Tag = MUIA_Numeric_Max, ts[1].ti_Data = 15;
            ts[2].ti_Tag = MUIA_Numeric_Value, ts[2].ti_Data = 0;
            ts[3].ti_Tag = TAG_DONE, ts[3].ti_Data = 0;
            rows[2 * i] = AA_SIBLING(UP_NewText, "UP-Text.mcc", tt);
            rows[2 * i + 1] = AA_SIBLING(UP_NewSlider, "UP-Slider.mcc", ts);
        }
        tk[0].ti_Tag = MUIA_Selected, tk[0].ti_Data = FALSE;
        tk[1].ti_Tag = TAG_DONE, tk[1].ti_Data = 0;
        kids[0] = AA_SIBLING(UP_NewFramedisplay, "UP-Framedisplay.mcc", none);
        kids[1] = AA_SIBLING(UP_NewCheckmark, "UP-Checkmark.mcc", tk);
        kids[2] = NULL;
        if (aj_children(rows, 2 * FA_ROWS, grid)) {
            grid[2 * FA_ROWS].ti_Tag = MUIA_Group_Columns, grid[2 * FA_ROWS].ti_Data = 2;
            grid[2 * FA_ROWS + 1].ti_Tag = TAG_DONE, grid[2 * FA_ROWS + 1].ti_Data = 0;
            kids[2] = MUI_NewObjectA((char*)MUIC_Group, grid);
        }
        def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
        if (!aj_children(kids, 3, def + 1)) {
            return 0;
        }
        def[4].ti_Tag = TAG_MORE, def[4].ti_Data = (ULONG)(uintptr_t)nm.ops_AttrList;
        nm.ops_AttrList = def;
        obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm); /* MUIC_Group disposes its children on failure */
        if (obj == NULL) {
            return 0;
        }
        d = INST_DATA(cl, obj);
        d->preview = kids[0], d->recessed = kids[1];
        for (i = 0; i < FA_ROWS; i++) {
            d->slider[i] = rows[2 * i + 1];
            DoMethod(d->slider[i], MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime, (ULONG)obj, 1, UPM_Frameadjust_Changed);
        }
        DoMethod(d->recessed, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, (ULONG)obj, 1, UPM_Frameadjust_Changed);
        strncpy(d->spec, (const char*)GetTagData(MUIA_Framedisplay_Spec, (ULONG)(uintptr_t) "000000", ((struct opSet*)msg)->ops_AttrList), sizeof d->spec - 1);
        fa_show(d);
        return (ULONG)obj;
    }
    case OM_SET:
        d = INST_DATA(cl, obj);
        if ((ti = FindTagItem(MUIA_Framedisplay_Spec, ((struct opSet*)msg)->ops_AttrList)) != NULL && ti->ti_Data != 0 && ti->ti_Data != (ULONG)d->spec) {
            strncpy(d->spec, (const char*)ti->ti_Data, sizeof d->spec - 1);
            fa_show(d);
        }
        break;
    case OM_GET:
        if (((struct opGet*)msg)->opg_AttrID == MUIA_Framedisplay_Spec) {
            *((struct opGet*)msg)->opg_Storage = (ULONG)((aaframeadjust_data*)INST_DATA(cl, obj))->spec;
            return TRUE;
        }
        break;
    case UPM_Frameadjust_Changed: {
        int v[6];
        d = INST_DATA(cl, obj);
        v[1] = aj_get(d->recessed, MUIA_Selected) != 0;
        for (i = 0; i < FA_ROWS; i++) {
            v[i == 0 ? 0 : i + 1] = (int)aj_get(d->slider[i], MUIA_Numeric_Value);
        }
        up_frspec_format(v, d->spec);
        nnset(d->preview, MUIA_Framedisplay_Spec, (ULONG)d->spec);
        SetAttrs(obj, MUIA_Framedisplay_Spec, (ULONG)d->spec, TAG_DONE);
        return 0;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG __attribute__((used)) aaimageadjust_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return ia_dispatch(cl, obj, msg);
}
static ULONG __attribute__((used)) aaframeadjust_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return fa_dispatch(cl, obj, msg);
}

#define ADJ_CLASS(name, data, gcls, Ctor) \
    extern ULONG name##_dispatch(void); \
    const struct aa_class_desc name##_class_desc = {MUIC_Group, sizeof(data), name##_dispatch, &gcls}; \
    __asm__(".text\n.even\n.globl _" #name "_dispatch\n_" #name "_dispatch:\n" \
            "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n" \
            "\tjbsr _" #name "_dispatch_c\n\tlea 12(sp),sp\n\trts\n"); \
    Object* Ctor(const struct TagItem* tags) { \
        if (gcls == NULL) { \
            gcls = AA_CLASS_CREATE(&name##_class_desc); \
        } \
        return gcls != NULL ? NewObjectA(gcls->mcc_Class, NULL, (struct TagItem*)tags) : NULL; \
    }

ADJ_CLASS(aaimageadjust, aaimageadjust_data, g_imageadjust_class, UP_NewImageadjust)
ADJ_CLASS(aaframeadjust, aaframeadjust_data, g_frameadjust_class, UP_NewFrameadjust)
