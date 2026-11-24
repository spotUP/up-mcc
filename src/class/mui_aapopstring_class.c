/* UP-Popstring and UP-Poplist. One Group subclass, two dispatchers (Poplist = the same data with a UP-List popup built by the class).
 * The popup is a borderless window opened under the object, copied from UP-Cycle (mui_aacycle_class.c). Unlike the cycle, the window is
 * created once and REUSED for every later open (its position is set again), and it is only disposed in cleanup: the cycle kept a closed
 * popup half alive and ignored or crashed on the second click. Open / close decisions are the pure mui_aa_pop_* state machine. */
#include "mui_aapopstring_class.h"
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
#include "map/mui_theme.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

typedef struct aapop_data {
    Object *str, *btn, *popup, *root, *list;
    int is_list, n, active, deact;
    int no_activate; /* UPA_Popstring_NoActivate: the popup opens without taking the keyboard from the string */
    const char* const* entries;
    struct Hook *open_hook, *close_hook;
    struct Hook row_hook; /* UPA_List_RowHook of the Poplist popup: the entry text (h_Data is this data) */
    mui_aa_pop_t pop;
} aapop_data;

static struct MUI_CustomClass *g_pop_class, *g_poplist_class;

static uint32_t aapop_now(void) {
    ULONG s = 0, m = 0;
    CurrentTime(&s, &m);
    return (uint32_t)(s * 1000UL + m / 1000UL);
}

static ULONG aapop_hook(struct Hook* h, Object* obj, Object* msg) {
    return h != NULL ? CallHookPkt(h, obj, msg) : TRUE;
}

/* Close and dispose the popup window (and with it the root). Only from cleanup: never from the window's own notification. */
static void aapop_drop(Object* obj, aapop_data* d) {
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
    d->popup = d->root = d->list = NULL;
    memset(&d->pop, 0, sizeof d->pop);
}

static const char* aapop_row(struct Hook* hook, Object* obj, UP_ListCell* cell) {
    aapop_data* d = (aapop_data*)hook->h_Data;
    (void)obj;
    return cell->row >= 0 && cell->row < d->n ? d->entries[cell->row] : "";
}

/* Poplist: the popup root = a Raised card around a UP-List of the entries (same build as UP-Cycle). */
static int aapop_build_list(Object* obj, aapop_data* d) {
    Object *list = NULL, *group, *card;
    struct TagItem tl[7];
    d->row_hook.h_Entry = (__typeof__(d->row_hook.h_Entry))(void (*)(void))HookEntry;
    d->row_hook.h_SubEntry = (__typeof__(d->row_hook.h_SubEntry))(void (*)(void))aapop_row;
    d->row_hook.h_Data = d;
    tl[0].ti_Tag = UPA_List_Count, tl[0].ti_Data = (ULONG)d->n;
    tl[1].ti_Tag = UPA_List_Active, tl[1].ti_Data = (ULONG)d->active;
    tl[2].ti_Tag = UPA_List_RowHook, tl[2].ti_Data = (ULONG)(uintptr_t)&d->row_hook;
    tl[3].ti_Tag = UPA_List_MaxRows, tl[3].ti_Data = (ULONG)(d->n < 12 ? d->n : 12);
    tl[4].ti_Tag = UPA_List_KeysUnfocused, tl[4].ti_Data = (ULONG)TRUE;
    tl[5].ti_Tag = UPA_List_Raised, tl[5].ti_Data = (ULONG)TRUE;
    tl[6].ti_Tag = TAG_DONE, tl[6].ti_Data = 0;
#ifdef UP_MCC_LIBRARY
    /* .mcc build: this library holds only its own class slot, so the sibling classes are made by name (a by-name list has no scroller) */
    group = list = MUI_NewObjectA((char*)"UP-List.mcc", tl);
#else
    group = UP_NewList(tl, &list);
#endif
    if (group == NULL) {
        return 0;
    }
    card = MUI_NewObject((char*)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Popup, UPA_Panel_Popup, (ULONG)TRUE, MUIA_Group_Child, (ULONG)group, TAG_DONE);
    d->root = card != NULL ? card : group;
    d->list = list;
    return 1;
}

static void aapop_place(Object* obj, aapop_data* d) {
    struct Window* win = _window(obj);
    set(d->popup, MUIA_Window_LeftEdge, (ULONG)(win->LeftEdge + _left(obj)));
    set(d->popup, MUIA_Window_TopEdge, (ULONG)(win->TopEdge + _bottom(obj) + 2));
}

/* act: MUI_AA_POP_OPEN_NEW or _REUSE. The popup window is created here (first open) and kept. */
static void aapop_open(Object* obj, aapop_data* d, int act) {
    Object* app = NULL;
    struct Window* win = _window(obj);
    ULONG dis = 0;
    GetAttr(MUIA_Disabled, obj, &dis);
    if (win == NULL || dis || d->pop.open || !aa_live(obj)) {
        return;
    }
    if (act == MUI_AA_POP_OPEN_NEW) {
        GetAttr(MUIA_ApplicationObject, _win(obj), (ULONG*)&app);
        if (app == NULL || (d->is_list && d->root == NULL && (d->n <= 0 || !aapop_build_list(obj, d))) || d->root == NULL) {
            return;
        }
    }
    if (!aapop_hook(d->open_hook, d->root, d->str)) {
        return;
    }
    if (act == MUI_AA_POP_OPEN_NEW) {
        d->popup = MUI_NewObject((char*)MUIC_Window, MUIA_Window_Screen, (ULONG)_screen(obj), MUIA_Window_Borderless, TRUE, MUIA_Window_DragBar, FALSE, MUIA_Window_CloseGadget, FALSE,
                                 MUIA_Window_SizeGadget, FALSE, MUIA_Window_DepthGadget, FALSE, MUIA_Window_LeftEdge, (ULONG)(win->LeftEdge + _left(obj)),
                                 MUIA_Window_TopEdge, (ULONG)(win->TopEdge + _bottom(obj) + 2), MUIA_Window_Width, (ULONG)_width(obj),
                                 MUIA_Window_Activate, (ULONG)!d->no_activate, MUIA_Window_RootObject, (ULONG)d->root, TAG_DONE);
        if (d->popup == NULL) {
            MUI_DisposeObject(d->root); /* the root is still ours: the window never took it */
            d->root = d->list = NULL;
            return;
        }
        DoMethod(app, OM_ADDMEMBER, (ULONG)d->popup);
        if (d->list != NULL) {
            DoMethod(d->list, MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, obj, 3, MUIM_Set, UPA_Poplist_Active, MUIV_TriggerValue);
            DoMethod(d->list, MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, d->popup, 3, MUIM_Set, MUIA_Window_Open, FALSE);
            DoMethod(d->list, MUIM_Notify, UPA_List_Cancelled, TRUE, d->popup, 3, MUIM_Set, MUIA_Window_Open, FALSE);
        }
        DoMethod(d->popup, MUIM_Notify, MUIA_Window_Activate, FALSE, obj, 1, UPM_Popstring_Deactivated);
        DoMethod(d->popup, MUIM_Notify, MUIA_Window_Open, FALSE, obj, 1, UPM_Popstring_Closed);
    } else {
        aapop_place(obj, d);
        if (d->no_activate) {
            SetAttrs(d->popup, MUIA_NoNotify, TRUE, MUIA_Window_Activate, FALSE, TAG_DONE); /* no Deactivated notify: the window is not open yet */
        }
        if (d->list != NULL) {
            set(d->list, UPA_List_Active, (ULONG)d->active);
        }
    }
    mui_aa_pop_opened(&d->pop);
    set(d->popup, MUIA_Window_Open, TRUE);
    set(obj, UPA_Popstring_Open, TRUE);
}

static void aapop_close(aapop_data* d) {
    if (d->popup != NULL && d->pop.open) {
        set(d->popup, MUIA_Window_Open, FALSE); /* the Open notify runs UPM_Popstring_Closed */
    }
}

static void aapop_closed(Object* obj, aapop_data* d) {
    int was = d->pop.open;
    mui_aa_pop_closed(&d->pop, d->deact, aapop_now());
    d->deact = 0;
    if (was) {
        aapop_hook(d->close_hook, d->root, d->str);
        set(obj, UPA_Popstring_Open, FALSE);
    }
}

static void aapop_set_active(Object* obj, aapop_data* d, int v) {
    if (v < -1 || v >= d->n || v == d->active) {
        return;
    }
    d->active = v;
    set(d->str, MUIA_String_Contents, (ULONG)(v >= 0 ? d->entries[v] : ""));
}

static ULONG aapop_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aapop_data* d = INST_DATA(cl, obj);
    aapop_drop(obj, d);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aapop_dispose(struct IClass* cl, Object* obj, Msg msg) {
    aapop_data* d = INST_DATA(cl, obj);
    if (d->popup == NULL && d->root != NULL) {
        MUI_DisposeObject(d->root); /* never opened: the root is not in a window */
    }
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aapop_new(struct IClass* cl, Object* obj, struct opSet* msg, int is_list) {
    aapop_data* d;
    struct opSet nm;
    struct TagItem def[7], tf[3];
    struct TagItem* tags = msg->ops_AttrList;
    Object* str = (Object*)GetTagData(UPA_Popstring_String, 0, tags);
    Object* btn = (Object*)GetTagData(UPA_Popstring_Button, 0, tags);
    const char* const* entries = (const char* const*)GetTagData(UPA_Poplist_Entries, 0, tags);
    int n = 0, active = (int)GetTagData(UPA_Poplist_Active, (ULONG)-1, tags);
    while (entries != NULL && entries[n] != NULL) {
        n++;
    }
    if (is_list && (active < -1 || active >= n)) {
        active = -1;
    }
    if (str == NULL) {
        tf[0].ti_Tag = UPA_Field_Tokens, tf[0].ti_Data = (ULONG)(uintptr_t)mui_theme_tokens();
        tf[1].ti_Tag = MUIA_String_MaxLen, tf[1].ti_Data = GetTagData(MUIA_String_MaxLen, 80, tags);
        tf[2].ti_Tag = TAG_DONE, tf[2].ti_Data = 0;
#ifdef UP_MCC_LIBRARY
        str = MUI_NewObject((char*)"UP-Field.mcc", MUIA_String_MaxLen, tf[1].ti_Data, TAG_DONE);
#else
        str = UP_NewField(tf);
#endif
    }
    if (btn == NULL) {
        btn = MUI_NewObject((char*)"UP-Button.mcc", UPA_Button_Icon, (ULONG) "chevron-down", UPA_Button_IconOnly, (ULONG)TRUE, TAG_DONE);
    }
    if (str == NULL || btn == NULL) {
        if (str != NULL) {
            MUI_DisposeObject(str);
        }
        if (btn != NULL) {
            MUI_DisposeObject(btn);
        }
        return 0;
    }
    if (is_list && active >= 0) {
        set(str, MUIA_String_Contents, (ULONG)entries[active]);
    }
    def[0].ti_Tag = MUIA_Group_Horiz, def[0].ti_Data = (ULONG)TRUE;
    def[1].ti_Tag = MUIA_Group_Spacing, def[1].ti_Data = 4;
    def[2].ti_Tag = MUIA_Frame, def[2].ti_Data = (ULONG)MUIV_Frame_None;
    def[3].ti_Tag = MUIA_Group_Child, def[3].ti_Data = (ULONG)str;
    def[4].ti_Tag = MUIA_Group_Child, def[4].ti_Data = (ULONG)btn;
    def[5].ti_Tag = TAG_MORE, def[5].ti_Data = (ULONG)(uintptr_t)tags;
    nm = *msg;
    nm.ops_AttrList = def;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm);
    if (obj == NULL) {
        return 0; /* the group disposed its children */
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->str = str;
    d->btn = btn;
    d->is_list = is_list;
    d->entries = entries;
    d->n = n;
    d->active = active;
    d->root = is_list ? NULL : (Object*)GetTagData(UPA_Popstring_Popup, 0, tags);
    d->open_hook = (struct Hook*)GetTagData(UPA_Popstring_OpenHook, 0, tags);
    d->close_hook = (struct Hook*)GetTagData(UPA_Popstring_CloseHook, 0, tags);
    d->no_activate = GetTagData(UPA_Popstring_NoActivate, FALSE, tags) != 0;
    DoMethod(btn, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, UPM_Popstring_Click);
    return (ULONG)obj;
}

static ULONG aapop_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aapop_data* d = INST_DATA(cl, obj);
    struct TagItem *ti, *tags = msg->ops_AttrList;
    if ((ti = FindTagItem(MUIA_String_Contents, tags)) != NULL) {
        set(d->str, MUIA_String_Contents, ti->ti_Data);
    }
    if ((ti = FindTagItem(UPA_Popstring_OpenHook, tags)) != NULL) {
        d->open_hook = (struct Hook*)ti->ti_Data;
    }
    if ((ti = FindTagItem(UPA_Popstring_CloseHook, tags)) != NULL) {
        d->close_hook = (struct Hook*)ti->ti_Data;
    }
    if ((ti = FindTagItem(UPA_Popstring_Popup, tags)) != NULL && !d->is_list && d->popup == NULL) {
        if (d->root != NULL && d->root != (Object*)ti->ti_Data) {
            MUI_DisposeObject(d->root);
        }
        d->root = (Object*)ti->ti_Data;
    }
    if (d->is_list && (ti = FindTagItem(UPA_Poplist_Active, tags)) != NULL) {
        aapop_set_active(obj, d, (int)ti->ti_Data);
    }
    if ((ti = FindTagItem(UPA_Popstring_Open, tags)) != NULL) {
        /* the notify of our own set(UPA_Popstring_Open) comes back here: act only when the wish differs from the state */
        if (ti->ti_Data && !d->pop.open) {
            aapop_open(obj, d, d->popup != NULL ? MUI_AA_POP_OPEN_REUSE : MUI_AA_POP_OPEN_NEW);
        } else if (!ti->ti_Data && d->pop.open) {
            aapop_close(d);
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aapop_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aapop_data* d = INST_DATA(cl, obj);
    switch (msg->opg_AttrID) {
    case UPA_Popstring_String:
        *msg->opg_Storage = (ULONG)d->str;
        return TRUE;
    case UPA_Popstring_Popup:
        *msg->opg_Storage = (ULONG)d->root; /* NULL once the owner window's cleanup disposed it */
        return TRUE;
    case UPA_Popstring_Open:
        *msg->opg_Storage = (ULONG)(d->pop.open != 0);
        return TRUE;
    case UPA_Poplist_Active:
        *msg->opg_Storage = (ULONG)d->active;
        return TRUE;
    case MUIA_String_Contents:
        return GetAttr(MUIA_String_Contents, d->str, msg->opg_Storage);
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aapop_dispatch_c(struct IClass* cl, Object* obj, Msg msg, int is_list) {
    aapop_data* d;
    switch (msg->MethodID) {
    case OM_NEW:
        return aapop_new(cl, obj, (struct opSet*)msg, is_list);
    case OM_DISPOSE:
        return aapop_dispose(cl, obj, msg);
    case OM_SET:
        return aapop_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aapop_get(cl, obj, (struct opGet*)msg);
    case MUIM_Cleanup:
        return aapop_cleanup(cl, obj, msg);
    case UPM_Popstring_Click:
        d = INST_DATA(cl, obj);
        {
            int act = mui_aa_pop_click(&d->pop, aapop_now());
            if (act == MUI_AA_POP_CLOSE) {
                aapop_close(d);
            } else if (act != MUI_AA_POP_NONE) {
                aapop_open(obj, d, act);
            }
        }
        return 0;
    case UPM_Popstring_Deactivated:
        d = INST_DATA(cl, obj);
        d->deact = 1;
        aapop_close(d);
        return 0;
    case UPM_Popstring_Closed:
        aapop_closed(obj, INST_DATA(cl, obj));
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG __attribute__((used)) aapopstring_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return aapop_dispatch_c(cl, obj, msg, 0);
}
static ULONG __attribute__((used)) aapoplist_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return aapop_dispatch_c(cl, obj, msg, 1);
}

extern ULONG aapopstring_dispatch(void);
extern ULONG aapoplist_dispatch(void);
const struct aa_class_desc aapopstring_class_desc = {MUIC_Group, sizeof(aapop_data), aapopstring_dispatch, &g_pop_class};
const struct aa_class_desc aapoplist_class_desc = {MUIC_Group, sizeof(aapop_data), aapoplist_dispatch, &g_poplist_class};
__asm__(".text\n.even\n.globl _aapopstring_dispatch\n_aapopstring_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapopstring_dispatch_c\n\tlea 12(sp),sp\n\trts\n"
        ".globl _aapoplist_dispatch\n_aapoplist_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapoplist_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static Object* aapop_make(struct MUI_CustomClass** slot, const struct aa_class_desc* desc, const struct TagItem* tags) {
    if (*slot == NULL) {
        *slot = AA_CLASS_CREATE(desc);
    }
    if (*slot == NULL) {
        return NULL;
    }
    return NewObjectA((*slot)->mcc_Class, NULL, (struct TagItem*)tags);
}

Object* UP_NewPopstring(const struct TagItem* tags) {
    return aapop_make(&g_pop_class, &aapopstring_class_desc, tags);
}

Object* UP_NewPoplist(const struct TagItem* tags) {
    return aapop_make(&g_poplist_class, &aapoplist_class_desc, tags);
}

void mui_aapopstring_class_free(void) {
    if (g_pop_class != NULL) {
        MUI_DeleteCustomClass(g_pop_class);
        g_pop_class = NULL;
    }
    if (g_poplist_class != NULL) {
        MUI_DeleteCustomClass(g_poplist_class);
        g_poplist_class = NULL;
    }
}
