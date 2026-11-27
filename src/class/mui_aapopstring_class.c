/* UP-Popstring, UP-Poplist, UP-Popobject, UP-Popasl and UP-Popscreen. One Group subclass, five dispatchers (Poplist = the same data with
 * a UP-List popup built by the class; Popobject = Popstring under the MUI 3.8 Popobject names; Popasl = no popup window, the button runs
 * an ASL requester; Popscreen = Poplist whose entries are the public screens, read again before each open).
 * The popup is a borderless window opened under the object, copied from UP-Cycle (mui_aacycle_class.c). A caller's popup (Popstring,
 * Popobject) is created once and REUSED for every later open (its position is set again) and only disposed in cleanup. A class-built
 * list popup (Poplist, Popscreen) is dropped and rebuilt at each open, as the cycle does (see aapop_open). Open / close decisions are
 * the pure mui_aa_pop_* state machine. */
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
#include <libraries/asl.h>
#include <proto/asl.h>

#include "map/mui_aa.h"
#include "map/mui_theme.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

typedef struct aapop_data {
    Object *str, *btn, *popup, *root, *list;
    int is_list, n, active, deact;
    int ok;   /* MUIM_Popstring_Close TRUE: the next close runs the ObjStrHook */
    int asl_active;
    ULONG compat; /* bits of aapop_compat: Follow, Light, Volatile (stored only) */
    ULONG asl_type;
    struct TagItem* asl_tags; /* clone of the creation tags: the ASL tags among them go to the requester */
    void* scr;                /* Popscreen: aaps_names, allocated at the first open */
    struct Hook *objstr_hook, *win_hook, *start_hook, *stop_hook;
    int no_activate; /* UPA_Popstring_NoActivate: the popup opens without taking the keyboard from the string */
    const char* const* entries;
    struct Hook *open_hook, *close_hook;
    struct Hook row_hook; /* UPA_List_RowHook of the Poplist popup: the entry text (h_Data is this data) */
    struct MUI_EventHandlerNode ehn; /* owner window IDCMP_INACTIVEWINDOW: closes a NoActivate popup, which never gets its own Deactivated */
    struct MUI_EventHandlerNode phn; /* popup window IDCMP_MOUSEBUTTONS: the right button cancels any popup like Esc (a list popup's UP-List also does) */
    mui_aa_pop_t pop;
} aapop_data;

enum { AAPOP_K_STRING, AAPOP_K_LIST, AAPOP_K_OBJECT, AAPOP_K_ASL, AAPOP_K_SCREEN };
static struct MUI_CustomClass *g_pop_class, *g_poplist_class, *g_popobject_class, *g_popasl_class, *g_popscreen_class;
/* Each UP-Pop* attribute answers to its UP id and its MUI 3.8 id. Hooks first, then the stored-only Popobject flags. */
static const ULONG aapop_hook_ids[6][2] = {{UPA_Popstring_OpenHook, MUIA_Popstring_OpenHook}, {UPA_Popstring_CloseHook, MUIA_Popstring_CloseHook},
                                           {UPA_Popobject_StrObjHook, MUIA_Popobject_StrObjHook}, {UPA_Popobject_ObjStrHook, MUIA_Popobject_ObjStrHook},
                                           {UPA_Popobject_WindowHook, MUIA_Popobject_WindowHook}, {UPA_Popasl_StartHook, MUIA_Popasl_StartHook}};
static const ULONG aapop_compat_ids[3][2] = {{UPA_Popobject_Follow, MUIA_Popobject_Follow}, {UPA_Popobject_Light, MUIA_Popobject_Light},
                                             {UPA_Popobject_Volatile, MUIA_Popobject_Volatile}};

static struct TagItem* aapop_find(const ULONG* ids, struct TagItem* tags) {
    struct TagItem* t = FindTagItem(ids[0], tags);
    return t != NULL ? t : FindTagItem(ids[1], tags);
}

static ULONG aapop_tag(ULONG upa, ULONG muia, ULONG def, struct TagItem* tags) {
    const ULONG ids[2] = {upa, muia};
    struct TagItem* t = aapop_find(ids, tags);
    return t != NULL ? t->ti_Data : def;
}

/* The hook slot for hook attribute i of aapop_hook_ids; StrObjHook is the open hook (same call: popup object, string). */
static struct Hook** aapop_hook_slot(struct aapop_data* d, int i) {
    struct Hook** slots[6] = {&d->open_hook, &d->close_hook, &d->open_hook, &d->objstr_hook, &d->win_hook, &d->start_hook};
    return slots[i];
}

/* Hooks and stored flags from a tag list (OM_NEW and OM_SET). The StopHook shares the MUI id table of no other class. */
static void aapop_read_tags(struct aapop_data* d, struct TagItem* tags) {
    struct TagItem* t;
    int i;
    for (i = 0; i < 6; i++) {
        if ((t = aapop_find(aapop_hook_ids[i], tags)) != NULL) {
            *aapop_hook_slot(d, i) = (struct Hook*)t->ti_Data;
        }
    }
    d->stop_hook = (struct Hook*)aapop_tag(UPA_Popasl_StopHook, MUIA_Popasl_StopHook, (ULONG)d->stop_hook, tags);
    for (i = 0; i < 3; i++) {
        if ((t = aapop_find(aapop_compat_ids[i], tags)) != NULL) {
            d->compat = t->ti_Data ? d->compat | (1UL << i) : d->compat & ~(1UL << i);
        }
    }
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
    DoMethod(d->popup, MUIM_Window_RemEventHandler, (ULONG)&d->phn); /* before the close, as UP-Menustrip's bar handler */
    set(d->popup, MUIA_Window_Open, FALSE);
    aa_pop_unpush(obj, d->popup); /* a queued close must not reach the disposed popup */
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

/* act: MUI_AA_POP_OPEN_NEW or _REUSE. The popup window is created here (first open) and kept, except a list popup (below). */
static void aapop_open(Object* obj, aapop_data* d, int act) {
    Object* app = NULL;
    struct Window* win = _window(obj);
    ULONG dis = 0;
    GetAttr(MUIA_Disabled, obj, &dis);
    if (win == NULL || dis || d->pop.open || !aa_live(obj)) {
        return;
    }
    if (act == MUI_AA_POP_OPEN_REUSE && d->list != NULL) {
        /* A list popup is rebuilt at every open, as UP-Cycle does. A kept one came back with the picked list as the window's
         * remembered active object (the list's click sets MUIA_Window_ActiveObject, OpenMUI restores it at the reopen), and the
         * outside click on that reopened, active popup hung UP-Demo (rig 5, 871acb6). A first open closes cleanly. */
        aapop_drop(obj, d);
        act = MUI_AA_POP_OPEN_NEW;
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
                                 MUIA_Window_SizeGadget, FALSE, MUIA_Window_DepthGadget, FALSE, MUIA_Window_NoMenus, TRUE, /* RMBTRAP: MENUDOWN reaches phn */
                                 MUIA_Window_LeftEdge, (ULONG)(win->LeftEdge + _left(obj)),
                                 MUIA_Window_TopEdge, (ULONG)(win->TopEdge + _bottom(obj) + 2), MUIA_Window_Width, (ULONG)_width(obj),
                                 MUIA_Window_Activate, (ULONG)!d->no_activate, MUIA_Window_RootObject, (ULONG)d->root,
                                 MUIA_UserData, (ULONG)_win(obj), TAG_DONE); /* the owner window: aa_pop_holds_active */
        if (d->popup == NULL) {
            MUI_DisposeObject(d->root); /* the root is still ours: the window never took it */
            d->root = d->list = NULL;
            return;
        }
        DoMethod(app, OM_ADDMEMBER, (ULONG)d->popup);
        d->phn.ehn_Priority = 0, d->phn.ehn_Flags = 0, d->phn.ehn_Object = obj, d->phn.ehn_Class = d->ehn.ehn_Class;
        d->phn.ehn_Events = IDCMP_MOUSEBUTTONS;
        DoMethod(d->popup, MUIM_Window_AddEventHandler, (ULONG)&d->phn);
        if (d->list != NULL) {
            DoMethod(d->list, MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, obj, 3, MUIM_Set, UPA_Poplist_Active, MUIV_TriggerValue);
            DoMethod(d->list, MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, d->popup, 3, MUIM_Set, MUIA_Window_Open, FALSE);
            DoMethod(d->list, MUIM_Notify, UPA_List_Cancelled, TRUE, d->popup, 3, MUIM_Set, MUIA_Window_Open, FALSE);
        }
        DoMethod(d->popup, MUIM_Notify, MUIA_Window_Activate, FALSE, obj, 1, UPM_Popstring_Deactivated);
        DoMethod(d->popup, MUIM_Notify, MUIA_Window_Open, FALSE, obj, 1, UPM_Popstring_Closed);
    } else {
        aa_pop_reopen(obj, d->popup, !d->no_activate); /* drop a queued close; active again like the first open */
        aapop_place(obj, d);
    }
    aapop_hook(d->win_hook, d->root, d->popup);
    mui_aa_pop_opened(&d->pop);
    set(d->popup, MUIA_Window_Open, TRUE);
    aa_pop_fit(obj, d->popup);
    set(obj, UPA_Popstring_Open, TRUE);
}

static void aapop_close(aapop_data* d) {
    if (d->popup != NULL && d->pop.open) {
        set(d->popup, MUIA_Window_Open, FALSE); /* the Open notify runs UPM_Popstring_Closed */
    }
}

static void aapop_closed(Object* obj, aapop_data* d) {
    int was = d->pop.open;
    mui_aa_pop_closed(&d->pop, d->deact, aa_pop_now());
    d->deact = 0;
    if (was) {
        if (d->ok) {
            aapop_hook(d->objstr_hook, d->root, d->str);
        }
        aapop_hook(d->close_hook, d->root, d->str);
        d->ok = 0;
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

static ULONG aapop_setup(struct IClass* cl, Object* obj, Msg msg) {
    aapop_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_INACTIVEWINDOW;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aapop_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aapop_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    aapop_drop(obj, d);
    return DoSuperMethodA(cl, obj, msg);
}

/* The owner window went inactive. A NoActivate popup closes unless the popup itself took the activation (a click into it):
 * Intuition sets WFLG_WINDOWACTIVE before either window's message is read, so the flag is already right here. */
static ULONG aapop_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aapop_data* d = INST_DATA(cl, obj);
    if (msg->imsg != NULL && msg->imsg->Class == IDCMP_MOUSEBUTTONS) { /* only phn asks for buttons: a press in the popup window */
        if (msg->imsg->Code == MENUDOWN && d->pop.open) {
            aapop_close(d); /* a cancel: d->ok stays 0, no ObjStrHook */
            return MUI_EventHandlerRC_Eat;
        }
        return 0;
    }
    if (msg->imsg == NULL || msg->imsg->Class != IDCMP_INACTIVEWINDOW || !d->no_activate || !d->pop.open || d->popup == NULL) {
        return 0;
    }
    if (aa_pop_holds_active(obj, d->popup)) { /* the popup or a popup opened from inside it took the activation */
        return 0;
    }
    d->deact = 1;
    aapop_close(d);
    return 0; /* not eaten: other handlers of the owner window still see the deactivation */
}

static ULONG aapop_dispose(struct IClass* cl, Object* obj, Msg msg) {
    aapop_data* d = INST_DATA(cl, obj);
    if (d->popup == NULL && d->root != NULL) {
        MUI_DisposeObject(d->root); /* never opened: the root is not in a window */
    }
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aapop_new(struct IClass* cl, Object* obj, struct opSet* msg, int kind) {
    int is_list = kind == AAPOP_K_LIST || kind == AAPOP_K_SCREEN;
    aapop_data* d;
    struct opSet nm;
    struct TagItem def[7], tf[3];
    struct TagItem* tags = msg->ops_AttrList;
    Object* str = (Object*)aapop_tag(UPA_Popstring_String, MUIA_Popstring_String, 0, tags);
    Object* btn = (Object*)aapop_tag(UPA_Popstring_Button, MUIA_Popstring_Button, 0, tags);
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
    d->root = is_list ? NULL : (Object*)aapop_tag(UPA_Popstring_Popup, MUIA_Popobject_Object, 0, tags);
    d->compat = 5; /* MUI defaults: Follow TRUE, Light FALSE, Volatile TRUE */
    aapop_read_tags(d, tags);
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
    aapop_read_tags(d, tags);
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
    ULONG id = msg->opg_AttrID;
    int i;
    for (i = 0; i < 6; i++) {
        if (id == aapop_hook_ids[i][0] || id == aapop_hook_ids[i][1]) {
            *msg->opg_Storage = (ULONG)*aapop_hook_slot(d, i);
            return TRUE;
        }
    }
    for (i = 0; i < 3; i++) {
        if (id == aapop_compat_ids[i][0] || id == aapop_compat_ids[i][1]) {
            *msg->opg_Storage = (d->compat >> i) & 1;
            return TRUE;
        }
    }
    switch (id) {
    case UPA_Popasl_StopHook:
    case MUIA_Popasl_StopHook:
        *msg->opg_Storage = (ULONG)d->stop_hook;
        return TRUE;
    case MUIA_Popstring_Button:
        *msg->opg_Storage = (ULONG)d->btn;
        return TRUE;
    case MUIA_Popstring_String:
    case UPA_Popstring_String:
        *msg->opg_Storage = (ULONG)d->str;
        return TRUE;
    case MUIA_Popobject_Object:
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

static ULONG aapop_dispatch_c(struct IClass* cl, Object* obj, Msg msg, int kind) {
    aapop_data* d;
    switch (msg->MethodID) {
    case OM_NEW:
        return aapop_new(cl, obj, (struct opSet*)msg, kind);
    case OM_DISPOSE:
        return aapop_dispose(cl, obj, msg);
    case OM_SET:
        return aapop_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aapop_get(cl, obj, (struct opGet*)msg);
    case MUIM_Setup:
        return aapop_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aapop_cleanup(cl, obj, msg);
    case MUIM_HandleEvent:
        return aapop_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    case MUIM_Hide:
        /* a hidden popstring (page switch, ShowMe FALSE, window close) never leaves its popup floating over other gadgets */
        aapop_close(INST_DATA(cl, obj));
        break;
    case UPM_Popstring_Click:
        d = INST_DATA(cl, obj);
        {
            int act = mui_aa_pop_click(&d->pop, aa_pop_now());
            if (act == MUI_AA_POP_CLOSE) {
                aapop_close(d);
            } else if (act != MUI_AA_POP_NONE) {
                aapop_open(obj, d, act);
            }
        }
        return 0;
    case MUIM_Popstring_Open:
        d = INST_DATA(cl, obj);
        if (!d->pop.open) {
            aapop_open(obj, d, d->popup != NULL ? MUI_AA_POP_OPEN_REUSE : MUI_AA_POP_OPEN_NEW);
        }
        return (ULONG)(d->pop.open != 0);
    case MUIM_Popstring_Close:
        d = INST_DATA(cl, obj);
        if (d->pop.open) {
            d->ok = ((struct MUIP_Popstring_Close*)msg)->result != 0; /* TRUE: aapop_closed runs the ObjStrHook */
            aapop_close(d);
        }
        return 0;
    case UPM_Popstring_Deactivated:
        d = INST_DATA(cl, obj);
        if (aa_pop_holds_active(obj, d->popup)) { /* a nested popup (a Cycle in the Popimage popup) took it: still inside */
            return 0;
        }
        d->deact = 1;
        if (d->pop.open) { /* queued: never closed inside the popup's own Activate notify (aa_pop_push_close) */
            aa_pop_push_close(obj, d->popup);
        }
        return 0;
    case UPM_Popstring_Closed:
        d = INST_DATA(cl, obj);
        aa_pop_child_closed(obj, d->deact); /* before aapop_closed clears deact */
        aapop_closed(obj, d);
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG __attribute__((used)) aapopstring_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return aapop_dispatch_c(cl, obj, msg, AAPOP_K_STRING);
}
static ULONG __attribute__((used)) aapoplist_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return aapop_dispatch_c(cl, obj, msg, AAPOP_K_LIST);
}
/* shortcut: Follow, Light and Volatile are stored and read back only (the popup is placed at each open, never dragged along, and
 * hides with the object); upgrade when a client needs a popup that follows a moving window. */
static ULONG __attribute__((used)) aapopobject_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return aapop_dispatch_c(cl, obj, msg, AAPOP_K_OBJECT);
}

/* Popasl: the ASL requester runs synchronously from the button (or MUIM_Popstring_Open), seeded from and written back to the string.
 * asl.library is opened per run into a local AslBase (the inline calls bind to that name), so only this dispatcher pulls ASL in. */
static void aapasl_run(Object* obj, aapop_data* d) {
    struct Library* AslBase;
    struct TagItem t[5];
    char a[256], b[108], out[300];
    STRPTR cur = NULL;
    long size = 0;
    APTR req;
    ULONG dis = 0;
    GetAttr(MUIA_Disabled, obj, &dis);
    if (d->asl_active || dis || !aa_live(obj) || (AslBase = OpenLibrary((STRPTR) "asl.library", 37)) == NULL) {
        return;
    }
    GetAttr(MUIA_String_Contents, d->str, (ULONG*)&cur);
    cur = cur != NULL ? cur : (STRPTR) "";
    out[0] = 0;
    t[0].ti_Tag = ASLFR_Window, t[0].ti_Data = (ULONG)_window(obj); /* the same tag id for all three requester types */
    t[1].ti_Tag = t[2].ti_Tag = TAG_IGNORE;
    if (d->asl_type == ASL_FileRequest) {
        mui_aa_path_split(cur, a, sizeof a, b, sizeof b);
        t[1].ti_Tag = ASLFR_InitialDrawer, t[1].ti_Data = (ULONG)a;
        t[2].ti_Tag = ASLFR_InitialFile, t[2].ti_Data = (ULONG)b;
    } else if (d->asl_type == ASL_FontRequest && mui_aa_font_parse(cur, a, sizeof a, &size) && a[0] != 0) {
        t[1].ti_Tag = ASLFO_InitialName, t[1].ti_Data = (ULONG)a;
        t[2].ti_Tag = size > 0 ? ASLFO_InitialSize : TAG_IGNORE, t[2].ti_Data = (ULONG)size;
    } else if (d->asl_type == ASL_ScreenModeRequest && cur[0] != 0) {
        t[1].ti_Tag = ASLSM_InitialDisplayID, t[1].ti_Data = mui_aa_hex_parse(cur);
    }
    t[3].ti_Tag = TAG_MORE, t[3].ti_Data = (ULONG)(uintptr_t)d->asl_tags; /* the caller's ASL tags from the creation list */
    t[4].ti_Tag = TAG_DONE;
    d->asl_active = 1;
    set(obj, UPA_Popasl_Active, TRUE);
    set(_app(obj), MUIA_Application_Sleep, TRUE);
    if (aapop_hook(d->start_hook, obj, (Object*)t) && (req = AllocAslRequest(d->asl_type, t)) != NULL) {
        if (AslRequest(req, NULL)) {
            if (d->stop_hook != NULL) {
                aapop_hook(d->stop_hook, obj, (Object*)req);
            } else if (d->asl_type == ASL_FileRequest) {
                mui_aa_path_join(((struct FileRequester*)req)->fr_Drawer, ((struct FileRequester*)req)->fr_File, out, sizeof out);
            } else if (d->asl_type == ASL_FontRequest) {
                mui_aa_font_spec((const char*)((struct FontRequester*)req)->fo_Attr.ta_Name, ((struct FontRequester*)req)->fo_Attr.ta_YSize, out, sizeof out);
            } else {
                mui_aa_hex_text(((struct ScreenModeRequester*)req)->sm_DisplayID, out, sizeof out);
            }
            if (out[0] != 0) {
                set(d->str, MUIA_String_Contents, (ULONG)out);
            }
        }
        FreeAslRequest(req);
    }
    set(_app(obj), MUIA_Application_Sleep, FALSE);
    CloseLibrary(AslBase);
    d->asl_active = 0;
    set(obj, UPA_Popasl_Active, FALSE);
}

static ULONG __attribute__((used)) aapopasl_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    aapop_data* d;
    struct TagItem* tags;
    switch (msg->MethodID) {
    case OM_NEW:
        tags = ((struct opSet*)msg)->ops_AttrList;
        obj = (Object*)aapop_dispatch_c(cl, obj, msg, AAPOP_K_ASL);
        if (obj != NULL) {
            d = INST_DATA(cl, obj);
            d->asl_type = aapop_tag(UPA_Popasl_Type, MUIA_Popasl_Type, ASL_FileRequest, tags);
            d->asl_tags = CloneTagItems(tags); /* NULL on no memory: then only our own tags reach the requester */
        }
        return (ULONG)obj;
    case OM_DISPOSE:
        FreeTagItems(((aapop_data*)INST_DATA(cl, obj))->asl_tags);
        break;
    case UPM_Popstring_Click:
    case MUIM_Popstring_Open:
        aapasl_run(obj, INST_DATA(cl, obj));
        return 0;
    case OM_GET:
        d = INST_DATA(cl, obj);
        switch (((struct opGet*)msg)->opg_AttrID) {
        case UPA_Popasl_Type:
        case MUIA_Popasl_Type:
            *((struct opGet*)msg)->opg_Storage = d->asl_type;
            return TRUE;
        case UPA_Popasl_Active:
        case MUIA_Popasl_Active:
            *((struct opGet*)msg)->opg_Storage = (ULONG)d->asl_active;
            return TRUE;
        }
        break;
    }
    return aapop_dispatch_c(cl, obj, msg, AAPOP_K_ASL);
}

/* Popscreen: the entries are the public screen names, copied under LockPubScreenList before each open into one AllocVec block
 * (pointer table, then the name buffers). shortcut: at most AAPS_MAX screens, and the list height is fixed at the first open;
 * upgrade when a system runs more public screens. */
#define AAPS_MAX 32
static void aaps_refresh(aapop_data* d) {
    const char** ptr;
    char* name;
    struct List* l;
    struct Node* nd;
    int n = 0, i;
    if (d->scr == NULL) {
        d->scr = AllocVec((AAPS_MAX + 1) * sizeof(char*) + AAPS_MAX * (MAXPUBSCREENNAME + 1), MEMF_ANY);
    }
    if (d->scr == NULL) {
        return;
    }
    ptr = (const char**)d->scr;
    name = (char*)(ptr + AAPS_MAX + 1);
    l = LockPubScreenList();
    for (nd = l->lh_Head; nd->ln_Succ != NULL && n < AAPS_MAX; nd = nd->ln_Succ, n++) {
        for (i = 0; i < MAXPUBSCREENNAME && nd->ln_Name[i] != 0; i++) {
            name[i] = nd->ln_Name[i];
        }
        name[i] = 0;
        ptr[n] = name;
        name += MAXPUBSCREENNAME + 1;
    }
    UnlockPubScreenList();
    ptr[n] = NULL;
    d->entries = ptr;
    d->n = n;
    d->active = -1;
    if (d->list != NULL) {
        SetAttrs(d->list, UPA_List_Count, (ULONG)n, UPA_List_Active, (ULONG)-1, TAG_DONE);
    }
}

static ULONG __attribute__((used)) aapopscreen_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    aapop_data* d;
    struct TagItem* ti;
    switch (msg->MethodID) {
    case OM_DISPOSE:
        d = INST_DATA(cl, obj);
        if (d->scr != NULL) {
            FreeVec(d->scr);
        }
        break;
    case OM_SET:
        ti = FindTagItem(UPA_Popstring_Open, ((struct opSet*)msg)->ops_AttrList);
        if (ti == NULL || !ti->ti_Data) {
            break;
        }
        /* fall through: an open wish refreshes the screens like a click */
    case UPM_Popstring_Click:
    case MUIM_Popstring_Open:
        d = INST_DATA(cl, obj);
        if (!d->pop.open) {
            aaps_refresh(d);
        }
        break;
    }
    return aapop_dispatch_c(cl, obj, msg, AAPOP_K_SCREEN);
}

extern ULONG aapopstring_dispatch(void);
extern ULONG aapoplist_dispatch(void);
extern ULONG aapopobject_dispatch(void);
extern ULONG aapopasl_dispatch(void);
extern ULONG aapopscreen_dispatch(void);
const struct aa_class_desc aapopstring_class_desc = {MUIC_Group, sizeof(aapop_data), aapopstring_dispatch, &g_pop_class};
const struct aa_class_desc aapoplist_class_desc = {MUIC_Group, sizeof(aapop_data), aapoplist_dispatch, &g_poplist_class};
const struct aa_class_desc aapopobject_class_desc = {MUIC_Group, sizeof(aapop_data), aapopobject_dispatch, &g_popobject_class};
const struct aa_class_desc aapopasl_class_desc = {MUIC_Group, sizeof(aapop_data), aapopasl_dispatch, &g_popasl_class};
const struct aa_class_desc aapopscreen_class_desc = {MUIC_Group, sizeof(aapop_data), aapopscreen_dispatch, &g_popscreen_class};
/* One section per stub: in one shared .text, --gc-sections kept all five dispatchers in every UP-Pop*.mcc. */
__asm__(".section .text._aapopstring_dispatch,\"ax\"\n.even\n.globl _aapopstring_dispatch\n_aapopstring_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapopstring_dispatch_c\n\tlea 12(sp),sp\n\trts\n.text\n"
        ".section .text._aapoplist_dispatch,\"ax\"\n.even\n.globl _aapoplist_dispatch\n_aapoplist_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapoplist_dispatch_c\n\tlea 12(sp),sp\n\trts\n.text\n"
        ".section .text._aapopobject_dispatch,\"ax\"\n.even\n.globl _aapopobject_dispatch\n_aapopobject_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapopobject_dispatch_c\n\tlea 12(sp),sp\n\trts\n.text\n"
        ".section .text._aapopasl_dispatch,\"ax\"\n.even\n.globl _aapopasl_dispatch\n_aapopasl_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapopasl_dispatch_c\n\tlea 12(sp),sp\n\trts\n.text\n"
        ".section .text._aapopscreen_dispatch,\"ax\"\n.even\n.globl _aapopscreen_dispatch\n_aapopscreen_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapopscreen_dispatch_c\n\tlea 12(sp),sp\n\trts\n.text\n");

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

Object* UP_NewPopobject(const struct TagItem* tags) {
    return aapop_make(&g_popobject_class, &aapopobject_class_desc, tags);
}

Object* UP_NewPopasl(const struct TagItem* tags) {
    return aapop_make(&g_popasl_class, &aapopasl_class_desc, tags);
}

Object* UP_NewPopscreen(const struct TagItem* tags) {
    return aapop_make(&g_popscreen_class, &aapopscreen_class_desc, tags);
}

void mui_aapopstring_class_free(void) {
    struct MUI_CustomClass** slot[5] = {&g_pop_class, &g_poplist_class, &g_popobject_class, &g_popasl_class, &g_popscreen_class};
    int i;
    for (i = 0; i < 5; i++) {
        if (*slot[i] != NULL) {
            MUI_DeleteCustomClass(*slot[i]);
            *slot[i] = NULL;
        }
    }
}
