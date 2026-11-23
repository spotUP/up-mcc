/* UP-Virtgroup: MUIC_Virtgroup equivalent. Group subclass; the contents live in one inner plain group (as in mui_aapanel_class.c) that a
 * layout hook sizes to max(view, own minimum) and places at (-left, -top); the draw clips to the view. Offsets are clamped by
 * mui_aa_scr_clamp at layout, when the view size is known. */
#include "mui_aavirtgroup_class.h"
#include "mui_aa_class.h"

#include <stdint.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "map/mui_aascr.h"

typedef struct aavirtgroup_data {
    Object* inner;
    long left, top;
    long cw, ch; /* the contents' own minimum size */
    int live;    /* between MUIM_Setup and MUIM_Cleanup */
} aavirtgroup_data;

static struct MUI_CustomClass* g_class;
static uint32_t g_made;
static struct Hook g_layout_hook;

static ULONG aavirtgroup_layout(struct Hook* hook, Object* obj, struct MUI_LayoutMsg* lm) {
    aavirtgroup_data* d = INST_DATA(g_class->mcc_Class, obj);
    Object* in = d->inner;
    (void)hook;
    if (in == NULL) {
        return (ULONG)MUILM_UNKNOWN;
    }
    switch (lm->lm_Type) {
    case MUILM_MINMAX: /* the view may be smaller than the contents: the minimum is a stub, the default is the contents */
        d->cw = _minwidth(in);
        d->ch = _minheight(in);
        lm->lm_MinMax.MinWidth = 8;
        lm->lm_MinMax.MinHeight = 8;
        lm->lm_MinMax.DefWidth = _defwidth(in);
        lm->lm_MinMax.DefHeight = _defheight(in);
        lm->lm_MinMax.MaxWidth = MUI_MAXMAX;
        lm->lm_MinMax.MaxHeight = MUI_MAXMAX;
        return 0;
    case MUILM_LAYOUT: {
        int w, h;
        mui_aa_scr_inner(lm->lm_Layout.Width, lm->lm_Layout.Height, (int)d->cw, (int)d->ch, &w, &h);
        d->left = mui_aa_scr_clamp(d->left, w, lm->lm_Layout.Width);
        d->top = mui_aa_scr_clamp(d->top, h, lm->lm_Layout.Height);
        MUI_Layout(in, (int)-d->left, (int)-d->top, w, h, 0);
        return TRUE;
    }
    }
    return (ULONG)MUILM_UNKNOWN;
}

static ULONG aavirtgroup_setup(struct IClass* cl, Object* obj, Msg msg) {
    aavirtgroup_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    d->live = 1;
    return TRUE;
}

static ULONG aavirtgroup_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aavirtgroup_data* d = INST_DATA(cl, obj);
    d->live = 0;
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aavirtgroup_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    APTR clip;
    ULONG r;
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return DoSuperMethodA(cl, obj, (Msg)msg);
    }
    clip = MUI_AddClipping(muiRenderInfo(obj), _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj));
    r = DoSuperMethodA(cl, obj, (Msg)msg);
    MUI_RemoveClipping(muiRenderInfo(obj), clip);
    return r;
}

static ULONG aavirtgroup_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aavirtgroup_data* d = INST_DATA(cl, obj);
    struct TagItem *tl = FindTagItem(UPA_Virtgroup_Left, msg->ops_AttrList), *tt = FindTagItem(UPA_Virtgroup_Top, msg->ops_AttrList);
    long nl = tl != NULL ? (long)tl->ti_Data : d->left, nt = tt != NULL ? (long)tt->ti_Data : d->top;
    nl = nl < 0 ? 0 : nl;
    nt = nt < 0 ? 0 : nt;
    if (nl != d->left || nt != d->top) {
        d->left = nl;
        d->top = nt;
        if (d->live) { /* the group lays the contents out again (the hook clamps) and redraws */
            DoMethod(obj, MUIM_Group_InitChange);
            DoMethod(obj, MUIM_Group_ExitChange);
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aavirtgroup_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aavirtgroup_data* d = INST_DATA(cl, obj);
    switch (msg->opg_AttrID) {
    case UPA_Virtgroup_Left:
        *msg->opg_Storage = (ULONG)d->left;
        return TRUE;
    case UPA_Virtgroup_Top:
        *msg->opg_Storage = (ULONG)d->top;
        return TRUE;
    case UPA_Virtgroup_Width:
        *msg->opg_Storage = (ULONG)d->cw;
        return TRUE;
    case UPA_Virtgroup_Height:
        *msg->opg_Storage = (ULONG)d->ch;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

/* OM_NEW: the one place the tags are parsed (UP_NewVirtgroup and MUI_NewObject("UP-Virtgroup.mcc") build the same object): the
 * program's MUIA_Group_Child objects go into the inner group, the layout hook places it. */
static ULONG aavirtgroup_new(struct IClass* cl, Object* obj, struct opSet* ops) {
    struct TagItem *tag, *state = ops->ops_AttrList, *copy, *t;
    struct opSet nm = *ops;
    Object* inner;
    ULONG made, horiz = GetTagData(MUIA_Group_Horiz, FALSE, ops->ops_AttrList);
    int count = 0;
    while (NextTagItem(&state) != NULL) {
        count++;
    }
    copy = (struct TagItem*)AllocVec((ULONG)(count + 8) * sizeof(struct TagItem), MEMF_ANY);
    inner = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, horiz, MUIA_FillArea, (ULONG)FALSE, TAG_DONE);
    if (copy == NULL || inner == NULL) {
        if (inner != NULL) {
            MUI_DisposeObject(inner);
        }
        if (copy != NULL) {
            FreeVec(copy);
        }
        return 0;
    }
    if (g_layout_hook.h_Entry == NULL) {
        g_layout_hook.h_Entry = (__typeof__(g_layout_hook.h_Entry))(void (*)(void))HookEntry;
        g_layout_hook.h_SubEntry = (__typeof__(g_layout_hook.h_SubEntry))(void (*)(void))aavirtgroup_layout;
    }
    t = copy;
    t->ti_Tag = MUIA_Frame, (t++)->ti_Data = (ULONG)MUIV_Frame_None;
    t->ti_Tag = MUIA_Group_Spacing, (t++)->ti_Data = 0;
    t->ti_Tag = MUIA_Group_LayoutHook, (t++)->ti_Data = (ULONG)(uintptr_t)&g_layout_hook;
    t->ti_Tag = MUIA_Group_Child, (t++)->ti_Data = (ULONG)(uintptr_t)inner;
    state = ops->ops_AttrList;
    while ((tag = NextTagItem(&state)) != NULL) {
        if (tag->ti_Tag == MUIA_Group_Child) {
            if (tag->ti_Data != 0) {
                DoMethod(inner, OM_ADDMEMBER, tag->ti_Data);
            }
        } else if (tag->ti_Tag != MUIA_Group_Horiz && tag->ti_Tag != MUIA_Group_LayoutHook && tag->ti_Tag != MUIA_Frame) {
            *t++ = *tag;
        }
    }
    t->ti_Tag = TAG_DONE;
    nm.ops_AttrList = copy;
    made = DoSuperMethodA(cl, obj, (Msg)&nm); /* on failure the Group super disposes its children, the inner group with them */
    FreeVec(copy);
    if (made != 0) {
        aavirtgroup_data* d = INST_DATA(cl, (Object*)made);
        d->inner = inner;
        d->left = (long)GetTagData(UPA_Virtgroup_Left, 0, ops->ops_AttrList);
        d->top = (long)GetTagData(UPA_Virtgroup_Top, 0, ops->ops_AttrList);
        g_made++;
    }
    return made;
}

static ULONG __attribute__((used)) aavirtgroup_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aavirtgroup_dispatch(void);
const struct aa_class_desc aavirtgroup_class_desc = {MUIC_Group, sizeof(aavirtgroup_data), aavirtgroup_dispatch, &g_class};
__asm__(".text\n.even\n.globl _aavirtgroup_dispatch\n_aavirtgroup_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aavirtgroup_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aavirtgroup_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aavirtgroup_new(cl, obj, (struct opSet*)msg);
    case OM_SET:
        return aavirtgroup_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aavirtgroup_get(cl, obj, (struct opGet*)msg);
    case MUIM_Setup:
        return aavirtgroup_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aavirtgroup_cleanup(cl, obj, msg);
    case MUIM_Draw:
        return aavirtgroup_draw(cl, obj, (struct MUIP_Draw*)msg);
    case OM_ADDMEMBER:
    case OM_REMMEMBER: {
        aavirtgroup_data* d = INST_DATA(cl, obj);
        if (d->inner != NULL) { /* callers add to the virtgroup, the children live in the inner group */
            return DoMethodA(d->inner, msg);
        }
        break;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* UP_NewVirtgroup(const struct TagItem* tags) {
    Object* o;
    if (g_class == NULL) {
        g_class = AA_CLASS_CREATE(&aavirtgroup_class_desc);
    }
    if (g_class == NULL) {
        return NULL;
    }
    o = NewObjectA(g_class->mcc_Class, NULL, (struct TagItem*)tags);
    return o;
}

uint32_t mui_aavirtgroup_made(void) {
    return g_made;
}

void mui_aavirtgroup_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
}
