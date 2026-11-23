/* UP-Scrollgroup: MUIC_Scrollgroup equivalent. Group subclass of one UP-Virtgroup (the contents) and one UP-Scroller. A layout hook
 * gives the bar the right edge while the contents are taller than the view and zero width otherwise (mui_aa_scr_split); after layout
 * it pushes first/visible/entries to the bar. The bar drives UPA_Virtgroup_Top by notify. Vertical bar only. */
#include "mui_aascrollgroup_class.h"
#include "mui_aa_class.h"
#include "mui_aascroller_class.h"
#include "mui_aavirtgroup_class.h"

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

#define SCRGRP_BAR_W 12 /* width of the bar when shown */

typedef struct aascrollgroup_data {
    Object *virt, *bar;
    long s_first, s_vis, s_ent; /* last values pushed to the bar */
} aascrollgroup_data;

static struct MUI_CustomClass* g_class;
static uint32_t g_made;
static struct Hook g_layout_hook;

static ULONG aascrollgroup_layout(struct Hook* hook, Object* obj, struct MUI_LayoutMsg* lm) {
    aascrollgroup_data* d = INST_DATA(g_class->mcc_Class, obj);
    (void)hook;
    if (d->virt == NULL || d->bar == NULL) {
        return (ULONG)MUILM_UNKNOWN;
    }
    switch (lm->lm_Type) {
    case MUILM_MINMAX:
        lm->lm_MinMax.MinWidth = (WORD)(_minwidth(d->virt) + SCRGRP_BAR_W);
        lm->lm_MinMax.MinHeight = (WORD)_minheight(d->virt);
        lm->lm_MinMax.DefWidth = (WORD)(_defwidth(d->virt) + SCRGRP_BAR_W);
        lm->lm_MinMax.DefHeight = (WORD)_defheight(d->virt);
        lm->lm_MinMax.MaxWidth = MUI_MAXMAX;
        lm->lm_MinMax.MaxHeight = MUI_MAXMAX;
        return 0;
    case MUILM_LAYOUT: {
        int w = lm->lm_Layout.Width, h = lm->lm_Layout.Height, need, vw;
        ULONG ch = 0, top = 0;
        GetAttr(UPA_Virtgroup_Height, d->virt, &ch);
        mui_aa_scr_split(w, h, (int)ch, SCRGRP_BAR_W, &need, &vw);
        MUI_Layout(d->virt, 0, 0, vw, h, 0);
        if (need) {
            MUI_Layout(d->bar, vw, 0, w - vw, h, 0);
        } else {
            MUI_Layout(d->bar, w, 0, 0, 0, 0); /* hidden: no width */
        }
        GetAttr(UPA_Virtgroup_Top, d->virt, &top); /* the virtgroup clamped it during its own layout */
        if ((long)top != d->s_first || h != d->s_vis || (long)ch != d->s_ent) {
            d->s_first = (long)top;
            d->s_vis = h;
            d->s_ent = (long)ch;
            SetAttrs(d->bar, MUIA_NoNotify, (ULONG)TRUE, UPA_Scroller_Entries, (ULONG)ch, UPA_Scroller_Visible, (ULONG)h, UPA_Scroller_First,
                     (ULONG)top, TAG_DONE);
        }
        return TRUE;
    }
    }
    return (ULONG)MUILM_UNKNOWN;
}

static ULONG aascrollgroup_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aascrollgroup_data* d = INST_DATA(cl, obj);
    struct TagItem* t = FindTagItem(UPA_Scrollgroup_Top, msg->ops_AttrList);
    if (t != NULL && d->virt != NULL) {
        SetAttrs(d->virt, UPA_Virtgroup_Top, t->ti_Data, TAG_DONE);
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aascrollgroup_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aascrollgroup_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == UPA_Scrollgroup_Top && d->virt != NULL) {
        return GetAttr(UPA_Virtgroup_Top, d->virt, msg->opg_Storage);
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

/* OM_NEW: the one place the tags are parsed (UP_NewScrollgroup and MUI_NewObject("UP-Scrollgroup.mcc") build the same object): the
 * program's MUIA_Group_Child objects go into the virtgroup, the bar and the virtgroup become the group's two children. */
static ULONG aascrollgroup_new(struct IClass* cl, Object* obj, struct opSet* ops) {
    struct TagItem *tag, *state = ops->ops_AttrList, *copy, *t;
    struct opSet nm = *ops;
    Object *virt, *bar;
    ULONG made;
    int count = 0;
    while (NextTagItem(&state) != NULL) {
        count++;
    }
    copy = (struct TagItem*)AllocVec((ULONG)(count + 8) * sizeof(struct TagItem), MEMF_ANY);
    virt = UP_NewVirtgroup(NULL);
    bar = UP_NewScroller(NULL);
    if (copy == NULL || virt == NULL || bar == NULL) {
        if (virt != NULL) {
            MUI_DisposeObject(virt);
        }
        if (bar != NULL) {
            MUI_DisposeObject(bar);
        }
        if (copy != NULL) {
            FreeVec(copy);
        }
        return 0;
    }
    if (g_layout_hook.h_Entry == NULL) {
        g_layout_hook.h_Entry = (__typeof__(g_layout_hook.h_Entry))(void (*)(void))HookEntry;
        g_layout_hook.h_SubEntry = (__typeof__(g_layout_hook.h_SubEntry))(void (*)(void))aascrollgroup_layout;
    }
    t = copy;
    t->ti_Tag = MUIA_Frame, (t++)->ti_Data = (ULONG)MUIV_Frame_None;
    t->ti_Tag = MUIA_Group_Spacing, (t++)->ti_Data = 0;
    t->ti_Tag = MUIA_Group_LayoutHook, (t++)->ti_Data = (ULONG)(uintptr_t)&g_layout_hook;
    t->ti_Tag = MUIA_Group_Child, (t++)->ti_Data = (ULONG)(uintptr_t)virt;
    t->ti_Tag = MUIA_Group_Child, (t++)->ti_Data = (ULONG)(uintptr_t)bar;
    state = ops->ops_AttrList;
    while ((tag = NextTagItem(&state)) != NULL) {
        if (tag->ti_Tag == MUIA_Group_Child) {
            if (tag->ti_Data != 0) {
                DoMethod(virt, OM_ADDMEMBER, tag->ti_Data);
            }
        } else if (tag->ti_Tag == MUIA_Group_Horiz) {
            SetAttrs(virt, MUIA_Group_Horiz, tag->ti_Data, TAG_DONE);
        } else if (tag->ti_Tag != MUIA_Group_LayoutHook && tag->ti_Tag != MUIA_Frame && tag->ti_Tag != UPA_Scrollgroup_Top) {
            *t++ = *tag;
        }
    }
    t->ti_Tag = TAG_DONE;
    nm.ops_AttrList = copy;
    made = DoSuperMethodA(cl, obj, (Msg)&nm); /* on failure the Group super disposes its children */
    FreeVec(copy);
    if (made != 0) {
        aascrollgroup_data* d = INST_DATA(cl, (Object*)made);
        d->virt = virt;
        d->bar = bar;
        d->s_first = d->s_vis = d->s_ent = -1;
        SetAttrs(virt, UPA_Virtgroup_Top, GetTagData(UPA_Scrollgroup_Top, 0, ops->ops_AttrList), TAG_DONE);
        DoMethod(bar, MUIM_Notify, UPA_Scroller_First, MUIV_EveryTime, virt, 3, MUIM_Set, UPA_Virtgroup_Top, MUIV_TriggerValue);
        g_made++;
    }
    return made;
}

static ULONG __attribute__((used)) aascrollgroup_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aascrollgroup_dispatch(void);
const struct aa_class_desc aascrollgroup_class_desc = {MUIC_Group, sizeof(aascrollgroup_data), aascrollgroup_dispatch, &g_class};
__asm__(".text\n.even\n.globl _aascrollgroup_dispatch\n_aascrollgroup_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aascrollgroup_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aascrollgroup_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aascrollgroup_new(cl, obj, (struct opSet*)msg);
    case OM_SET:
        return aascrollgroup_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aascrollgroup_get(cl, obj, (struct opGet*)msg);
    case OM_ADDMEMBER:
    case OM_REMMEMBER: {
        aascrollgroup_data* d = INST_DATA(cl, obj);
        if (d->virt != NULL) { /* callers add to the scrollgroup, the children live in the virtgroup */
            return DoMethodA(d->virt, msg);
        }
        break;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* UP_NewScrollgroup(const struct TagItem* tags) {
    if (g_class == NULL) {
        g_class = AA_CLASS_CREATE(&aascrollgroup_class_desc);
    }
    if (g_class == NULL) {
        return NULL;
    }
    return NewObjectA(g_class->mcc_Class, NULL, (struct TagItem*)tags);
}

uint32_t mui_aascrollgroup_made(void) {
    return g_made;
}

void mui_aascrollgroup_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
    mui_aavirtgroup_class_free();
    mui_aascroller_class_free();
}
