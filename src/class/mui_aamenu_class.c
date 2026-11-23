/* UP-Menu. One Group subclass holding a UP-Button (the title) and, once opened, a popup window with a UP-List of the items. The open /
 * close / reuse logic is mui_aapopstring_class.c with the string removed: same pure state machine (mui_aa_pop_*), one popup window created
 * at the first open and reused, disposed only in cleanup. A pick sets UPA_Menu_Chosen (notifies) and closes the popup. */
#include "mui_aamenu_class.h"
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

typedef struct aamenu_data {
    Object *btn, *popup, *root, *list;
    int n, chosen, deact;
    const char* const* items;
    struct Hook row_hook; /* UPA_List_RowHook: the item text (h_Data is this data) */
    mui_aa_pop_t pop;
} aamenu_data;

static struct MUI_CustomClass* g_menu_class;

static uint32_t aamenu_now(void) {
    ULONG s = 0, m = 0;
    CurrentTime(&s, &m);
    return (uint32_t)(s * 1000UL + m / 1000UL);
}

static int aamenu_is_sep(const aamenu_data* d, int i) {
    return i >= 0 && i < d->n && d->items[i][0] == '-' && d->items[i][1] == '\0';
}

/* Only from cleanup: never from the window's own notification. */
static void aamenu_drop(Object* obj, aamenu_data* d) {
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

static const char* aamenu_row(struct Hook* hook, Object* obj, UP_ListCell* cell) {
    aamenu_data* d = (aamenu_data*)hook->h_Data;
    (void)obj;
    if (cell->row < 0 || cell->row >= d->n) {
        return "";
    }
    return aamenu_is_sep(d, cell->row) ? "--------------" : d->items[cell->row]; /* shortcut: a separator is a dashed row, not a thin rule */
}

/* The popup root: a Raised card around a UP-List of the items (same build as UP-Poplist). */
static int aamenu_build(aamenu_data* d) {
    Object *list = NULL, *group, *card;
    struct TagItem tl[7];
    d->row_hook.h_Entry = (__typeof__(d->row_hook.h_Entry))(void (*)(void))HookEntry;
    d->row_hook.h_SubEntry = (__typeof__(d->row_hook.h_SubEntry))(void (*)(void))aamenu_row;
    d->row_hook.h_Data = d;
    tl[0].ti_Tag = UPA_List_Count, tl[0].ti_Data = (ULONG)d->n;
    tl[1].ti_Tag = UPA_List_Active, tl[1].ti_Data = (ULONG)-1;
    tl[2].ti_Tag = UPA_List_RowHook, tl[2].ti_Data = (ULONG)(uintptr_t)&d->row_hook;
    tl[3].ti_Tag = UPA_List_MaxRows, tl[3].ti_Data = (ULONG)(d->n < 16 ? d->n : 16);
    tl[4].ti_Tag = UPA_List_KeysUnfocused, tl[4].ti_Data = (ULONG)TRUE;
    tl[5].ti_Tag = UPA_List_Raised, tl[5].ti_Data = (ULONG)TRUE;
    tl[6].ti_Tag = TAG_DONE, tl[6].ti_Data = 0;
#ifdef UP_MCC_LIBRARY
    group = list = MUI_NewObjectA((char*)"UP-List.mcc", tl); /* .mcc build: sibling classes by name (a by-name list has no scroller) */
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

static void aamenu_place(Object* obj, aamenu_data* d) {
    struct Window* win = _window(obj);
    set(d->popup, MUIA_Window_LeftEdge, (ULONG)(win->LeftEdge + _left(obj)));
    set(d->popup, MUIA_Window_TopEdge, (ULONG)(win->TopEdge + _bottom(obj) + 2));
}

static void aamenu_open(Object* obj, aamenu_data* d, int act) {
    Object* app = NULL;
    struct Window* win = _window(obj);
    ULONG dis = 0;
    GetAttr(MUIA_Disabled, obj, &dis);
    if (win == NULL || dis || d->pop.open || !aa_live(obj)) {
        return;
    }
    if (act == MUI_AA_POP_OPEN_NEW) {
        GetAttr(MUIA_ApplicationObject, _win(obj), (ULONG*)&app);
        if (app == NULL || d->n <= 0 || !aamenu_build(d)) {
            return;
        }
        d->popup = MUI_NewObject((char*)MUIC_Window, MUIA_Window_Screen, (ULONG)_screen(obj), MUIA_Window_Borderless, TRUE, MUIA_Window_DragBar, FALSE, MUIA_Window_CloseGadget, FALSE,
                                 MUIA_Window_SizeGadget, FALSE, MUIA_Window_DepthGadget, FALSE, MUIA_Window_LeftEdge, (ULONG)(win->LeftEdge + _left(obj)),
                                 MUIA_Window_TopEdge, (ULONG)(win->TopEdge + _bottom(obj) + 2), MUIA_Window_RootObject, (ULONG)d->root, TAG_DONE);
        if (d->popup == NULL) {
            MUI_DisposeObject(d->root); /* the window never took it */
            d->root = d->list = NULL;
            return;
        }
        DoMethod(app, OM_ADDMEMBER, (ULONG)d->popup);
        DoMethod(d->list, MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, obj, 3, MUIM_Set, UPA_Menu_Chosen, MUIV_TriggerValue);
        DoMethod(d->list, MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, d->popup, 3, MUIM_Set, MUIA_Window_Open, FALSE);
        DoMethod(d->list, MUIM_Notify, UPA_List_Cancelled, TRUE, d->popup, 3, MUIM_Set, MUIA_Window_Open, FALSE);
        DoMethod(d->popup, MUIM_Notify, MUIA_Window_Activate, FALSE, obj, 1, UPM_Menu_Deactivated);
        DoMethod(d->popup, MUIM_Notify, MUIA_Window_Open, FALSE, obj, 1, UPM_Menu_Closed);
    } else {
        aamenu_place(obj, d);
        set(d->list, UPA_List_Active, (ULONG)-1);
    }
    mui_aa_pop_opened(&d->pop);
    set(d->popup, MUIA_Window_Open, TRUE);
    set(obj, UPA_Menu_Open, TRUE);
}

static void aamenu_close(aamenu_data* d) {
    if (d->popup != NULL && d->pop.open) {
        set(d->popup, MUIA_Window_Open, FALSE); /* the Open notify runs UPM_Menu_Closed */
    }
}

static void aamenu_closed(Object* obj, aamenu_data* d) {
    int was = d->pop.open;
    mui_aa_pop_closed(&d->pop, d->deact, aamenu_now());
    d->deact = 0;
    if (was) {
        set(obj, UPA_Menu_Open, FALSE);
    }
}

static ULONG aamenu_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aamenu_drop(obj, INST_DATA(cl, obj));
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aamenu_dispose(struct IClass* cl, Object* obj, Msg msg) {
    aamenu_data* d = INST_DATA(cl, obj);
    if (d->popup == NULL && d->root != NULL) {
        MUI_DisposeObject(d->root);
    }
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aamenu_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aamenu_data* d;
    struct opSet nm;
    struct TagItem def[5];
    struct TagItem* tags = msg->ops_AttrList;
    const char* title = (const char*)GetTagData(UPA_Menu_Title, 0, tags);
    const char* const* items = (const char* const*)GetTagData(UPA_Menu_Items, 0, tags);
    int n = 0;
    Object* btn;
    while (items != NULL && items[n] != NULL) {
        n++;
    }
    btn = MUI_NewObject((char*)"UP-Button.mcc", UPA_Button_Caption, (ULONG)(title != NULL ? title : ""), TAG_DONE);
    if (btn == NULL) {
        return 0;
    }
    def[0].ti_Tag = MUIA_Group_Horiz, def[0].ti_Data = (ULONG)TRUE;
    def[1].ti_Tag = MUIA_Frame, def[1].ti_Data = (ULONG)MUIV_Frame_None;
    def[2].ti_Tag = MUIA_Group_Child, def[2].ti_Data = (ULONG)btn;
    def[3].ti_Tag = TAG_MORE, def[3].ti_Data = (ULONG)(uintptr_t)tags;
    nm = *msg;
    nm.ops_AttrList = def;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm);
    if (obj == NULL) {
        return 0; /* the group disposed the button */
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->btn = btn;
    d->items = items;
    d->n = n;
    d->chosen = -1;
    DoMethod(btn, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, UPM_Menu_Click);
    return (ULONG)obj;
}

static ULONG aamenu_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aamenu_data* d = INST_DATA(cl, obj);
    struct TagItem* ti;
    if ((ti = FindTagItem(UPA_Menu_Chosen, msg->ops_AttrList)) != NULL) {
        int v = (int)ti->ti_Data;
        if (v < -1 || v >= d->n || aamenu_is_sep(d, v)) {
            ti->ti_Tag = TAG_IGNORE; /* a separator or an out of range pick chooses nothing and does not notify */
        } else {
            d->chosen = v;
        }
    }
    if ((ti = FindTagItem(UPA_Menu_Open, msg->ops_AttrList)) != NULL) {
        /* the notify of our own set(UPA_Menu_Open) comes back here: act only when the wish differs from the state */
        if (ti->ti_Data && !d->pop.open) {
            aamenu_open(obj, d, d->popup != NULL ? MUI_AA_POP_OPEN_REUSE : MUI_AA_POP_OPEN_NEW);
        } else if (!ti->ti_Data && d->pop.open) {
            aamenu_close(d);
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aamenu_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aamenu_data* d = INST_DATA(cl, obj);
    if (msg->opg_AttrID == UPA_Menu_Chosen) {
        *msg->opg_Storage = (ULONG)d->chosen;
        return TRUE;
    }
    if (msg->opg_AttrID == UPA_Menu_Open) {
        *msg->opg_Storage = (ULONG)(d->pop.open != 0);
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aamenu_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aamenu_dispatch(void);
const struct aa_class_desc aamenu_class_desc = {MUIC_Group, sizeof(aamenu_data), aamenu_dispatch, &g_menu_class};
__asm__(".text\n.even\n.globl _aamenu_dispatch\n_aamenu_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aamenu_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aamenu_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    aamenu_data* d;
    switch (msg->MethodID) {
    case OM_NEW:
        return aamenu_new(cl, obj, (struct opSet*)msg);
    case OM_DISPOSE:
        return aamenu_dispose(cl, obj, msg);
    case OM_SET:
        return aamenu_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aamenu_get(cl, obj, (struct opGet*)msg);
    case MUIM_Cleanup:
        return aamenu_cleanup(cl, obj, msg);
    case UPM_Menu_Click:
        d = INST_DATA(cl, obj);
        {
            int act = mui_aa_pop_click(&d->pop, aamenu_now());
            if (act == MUI_AA_POP_CLOSE) {
                aamenu_close(d);
            } else if (act != MUI_AA_POP_NONE) {
                aamenu_open(obj, d, act);
            }
        }
        return 0;
    case UPM_Menu_Deactivated:
        d = INST_DATA(cl, obj);
        d->deact = 1;
        aamenu_close(d);
        return 0;
    case UPM_Menu_Closed:
        aamenu_closed(obj, INST_DATA(cl, obj));
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* UP_NewMenu(const struct TagItem* tags) {
    if (g_menu_class == NULL) {
        g_menu_class = AA_CLASS_CREATE(&aamenu_class_desc);
    }
    if (g_menu_class == NULL) {
        return NULL;
    }
    return NewObjectA(g_menu_class->mcc_Class, NULL, (struct TagItem*)tags);
}

void mui_aamenu_class_free(void) {
    if (g_menu_class != NULL) {
        MUI_DeleteCustomClass(g_menu_class);
        g_menu_class = NULL;
    }
}
