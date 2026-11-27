/* UP-Menu. One Group subclass holding a UP-Button (the title) and, once opened, a popup window with a UP-List of the items. The open /
 * close / reuse logic is mui_aapopstring_class.c with the string removed: same pure state machine (mui_aa_pop_*), one popup window created
 * at the first open and reused, disposed only in cleanup. A pick sets UPA_Menu_Chosen (notifies) and closes the popup.
 * MUIC_Menuitem model (UPA_Menu_Entries): per item Checkit / Toggle / Exclude (mui_aa_menu_pick), Checked and Disabled bitmasks, and a
 * shortcut. The list then has three columns: "*" for a checked item, the title, the shortcut text. A one-character shortcut is
 * Amiga+key through an IDCMP_RAWKEY handler mapped with keymap.library (opened per use); it picks the item like a click.
 * The items are a copy of the Entries array, so MUIM_Family_AddHead / AddTail / Insert / Remove can change them (MUIC_Menuitem children);
 * the popup is built again at the next open, since UP-List takes its row count limit at creation.
 * Left and Right in the open popup set UPA_Menu_Step (a second RAWKEY handler, on the popup window); UP-Menustrip moves to the next menu.
 * The popup is never closed inside its own notification (a pick, Esc, the loss of activation): the close goes through
 * MUIM_Application_PushMethod (aamenu_push_close). OpenMUI reports IDCMP_INACTIVEWINDOW as MUIA_Window_Activate FALSE while that
 * IntuiMessage is still unreplied, then copies and replies it (window.c HandleWindowEvent, HandleInputEvent); a close in between runs
 * CloseWindow under the message, so the reply writes to a freed message and a freed port (guest hang or reset loop after a click
 * outside the popup or on another menu title). Queued closes are dropped (MUIM_Application_UnpushMethod) before the popup is
 * disposed and before it opens again.
 * shortcut: MUI 3.8 ignores UnpushMethod, so there a close still queued when the popup is disposed hits a dead object; the pick close
 * is queued before the UPA_Menu_Chosen notify so that UP-Menustrip's own queued bar close (which disposes the popup) runs after it.
 * Upgrade: keep closed popups alive until OM_DISPOSE if MUI 3.8 becomes a target again.
 * Esc is a state, not a notify chain: the list's Cancelled notify and Esc in the popup's own key handler run UPM_Menu_Cancel, which sets
 * d->cancelled (UPA_Menu_Cancelled, cleared at the next open) and queues the close; UP-Menustrip reads it in its Open FALSE check.
 * In UP-Menustrip's screen bar (UPM_Menu_Bar) the title is a slim flat UP-Button of the bar height, lit (UPA_Button_On) while the popup is
 * open, and the popup drops right below the bar window. */
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
#include <devices/inputevent.h>
#define KeymapBase up_mcc_menu_KeymapBase /* own base symbol: the library is opened per use, not by libnix */
#include <proto/keymap.h>

#include "map/mui_aa.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;
struct Library* KeymapBase;

typedef struct aamenu_data {
    Object *btn, *popup, *root, *list;
    int n, chosen, deact;
    int cancelled;                    /* UPA_Menu_Cancelled: Esc closed the popup since its last open */
    LONG bar_h;                       /* UPM_Menu_Bar height, 0 = a plain title button */
    const char* title;                /* UPA_Menu_Title, kept for the UPM_Menu_Bar title rebuild */
    const char* const* items;
    const UP_MenuItem* ent;           /* own (UPA_Menu_Entries given), NULL = plain UPA_Menu_Items */
    UP_MenuItem own[UP_MENU_MAXITEMS]; /* the items; their strings are the application's */
    ULONG checked, disabled;          /* bit i = item i */
    int stale;                        /* the items changed since the popup was built */
    LONG step;                        /* UPA_Menu_Step */
    char keys[UP_MENU_MAXITEMS][8];   /* the shown shortcut text */
    struct MUI_EventHandlerNode ehn, phn; /* RAWKEY: ehn Amiga+key in the menu's window, phn Left / Right in the popup */
    struct Hook row_hook; /* UPA_List_RowHook: the item text (h_Data is this data) */
    mui_aa_pop_t pop;
} aamenu_data;

static struct MUI_CustomClass* g_menu_class;

static uint32_t aamenu_now(void) {
    ULONG s = 0, m = 0;
    CurrentTime(&s, &m);
    return (uint32_t)(s * 1000UL + m / 1000UL);
}

static const char* aamenu_title(const aamenu_data* d, int i) {
    return d->ent != NULL ? d->ent[i].title : d->items[i];
}

static int aamenu_is_sep(const aamenu_data* d, int i) {
    return i >= 0 && i < d->n && aamenu_title(d, i)[0] == '-' && aamenu_title(d, i)[1] == '\0';
}

/* Only from cleanup or a closed popup's next open: never from the window's own notification. */
static void aamenu_drop(Object* obj, aamenu_data* d) {
    Object* app = NULL;
    if (d->popup == NULL) {
        return;
    }
    if (aa_live(obj)) {
        GetAttr(MUIA_ApplicationObject, _win(obj), (ULONG*)&app);
    }
    set(d->popup, MUIA_Window_Open, FALSE);
    DoMethod(d->popup, MUIM_Window_RemEventHandler, (ULONG)&d->phn);
    aa_pop_unpush(obj, d->popup); /* a queued close must not reach the disposed popup */
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
    if (d->ent != NULL && cell->col != 1) { /* the check column and the shortcut column */
        return cell->col == 0 ? (d->checked & (1UL << cell->row) ? "*" : " ") : d->keys[cell->row];
    }
    /* shortcut: a separator is a dashed row, not a thin rule */
    return aamenu_is_sep(d, cell->row) ? "--------------" : aamenu_title(d, cell->row);
}

/* The popup root: a Raised card around a UP-List of the items (same build as UP-Poplist). */
static int aamenu_build(aamenu_data* d) {
    Object *list = NULL, *group, *card;
    struct TagItem tl[8];
    static const UP_ListColumns cols = {3, {0, 1, 0}, {"*", NULL, "Amiga+W"}};
    d->row_hook.h_Entry = (__typeof__(d->row_hook.h_Entry))(void (*)(void))HookEntry;
    d->row_hook.h_SubEntry = (__typeof__(d->row_hook.h_SubEntry))(void (*)(void))aamenu_row;
    d->row_hook.h_Data = d;
    tl[0].ti_Tag = UPA_List_Count, tl[0].ti_Data = (ULONG)d->n;
    tl[1].ti_Tag = UPA_List_Active, tl[1].ti_Data = (ULONG)-1;
    tl[2].ti_Tag = d->ent != NULL ? UPA_List_CellHook : UPA_List_RowHook, tl[2].ti_Data = (ULONG)(uintptr_t)&d->row_hook;
    tl[3].ti_Tag = UPA_List_MaxRows, tl[3].ti_Data = (ULONG)(d->n < 16 ? d->n : 16);
    tl[4].ti_Tag = UPA_List_KeysUnfocused, tl[4].ti_Data = (ULONG)TRUE;
    tl[5].ti_Tag = UPA_List_Raised, tl[5].ti_Data = (ULONG)TRUE;
    tl[6].ti_Tag = d->ent != NULL ? UPA_List_Columns : TAG_IGNORE, tl[6].ti_Data = (ULONG)(uintptr_t)&cols;
    tl[7].ti_Tag = TAG_DONE, tl[7].ti_Data = 0;
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

/* The popup top: right below the bar window in the strip's screen bar, else 2 px below the title. */
static LONG aamenu_top(Object* obj, const aamenu_data* d) {
    struct Window* win = _window(obj);
    return d->bar_h > 0 ? win->TopEdge + win->Height : win->TopEdge + _bottom(obj) + 2;
}

static void aamenu_place(Object* obj, aamenu_data* d) {
    struct Window* win = _window(obj);
    set(d->popup, MUIA_Window_LeftEdge, (ULONG)(win->LeftEdge + _left(obj)));
    set(d->popup, MUIA_Window_TopEdge, (ULONG)aamenu_top(obj, d));
}

/* keys: the cursor keys opened it (UPV_Menu_Open_Keys), the popup takes the activation. */
static void aamenu_open(Object* obj, aamenu_data* d, int act, int keys) {
    Object* app = NULL;
    struct Window* win = _window(obj);
    ULONG dis = 0;
    GetAttr(MUIA_Disabled, obj, &dis);
    if (win == NULL || dis || d->pop.open || d->n <= 0 || !aa_live(obj)) {
        return;
    }
    if (d->stale && d->popup != NULL) { /* closed here (pop.open is 0): no notification of this popup runs */
        aamenu_drop(obj, d);
        act = MUI_AA_POP_OPEN_NEW;
    }
    d->stale = 0;
    GetAttr(MUIA_ApplicationObject, _win(obj), (ULONG*)&app);
    if (act == MUI_AA_POP_OPEN_NEW) {
        if (app == NULL || !aamenu_build(d)) {
            return;
        }
        d->popup = MUI_NewObject((char*)MUIC_Window, MUIA_Window_Screen, (ULONG)_screen(obj), MUIA_Window_Borderless, TRUE, MUIA_Window_DragBar, FALSE, MUIA_Window_CloseGadget, FALSE,
                                 MUIA_Window_SizeGadget, FALSE, MUIA_Window_DepthGadget, FALSE, MUIA_Window_NoMenus, TRUE, MUIA_Window_LeftEdge, (ULONG)(win->LeftEdge + _left(obj)),
                                 MUIA_Window_TopEdge, (ULONG)aamenu_top(obj, d), MUIA_Window_RootObject, (ULONG)d->root, TAG_DONE);
        if (d->popup == NULL) {
            MUI_DisposeObject(d->root); /* the window never took it */
            d->root = d->list = NULL;
            return;
        }
        DoMethod(app, OM_ADDMEMBER, (ULONG)d->popup);
        d->phn.ehn_Priority = 0, d->phn.ehn_Flags = 0, d->phn.ehn_Object = obj, d->phn.ehn_Class = g_menu_class->mcc_Class;
        d->phn.ehn_Events = IDCMP_RAWKEY;
        DoMethod(d->popup, MUIM_Window_AddEventHandler, (ULONG)&d->phn);
        /* the close first: it must be queued ahead of whatever the UPA_Menu_Chosen notify queues (see the header) */
        DoMethod(d->list, MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, obj, 1, UPM_Menu_PushClose);
        DoMethod(d->list, MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, obj, 3, MUIM_Set, UPA_Menu_Chosen, MUIV_TriggerValue);
        /* Esc ends the whole menu (Intuition: a cancel leaves menu mode): recorded as state, read by UP-Menustrip's check */
        DoMethod(d->list, MUIM_Notify, UPA_List_Cancelled, TRUE, obj, 1, UPM_Menu_Cancel);
        DoMethod(d->popup, MUIM_Notify, MUIA_Window_Activate, FALSE, obj, 1, UPM_Menu_Deactivated);
        DoMethod(d->popup, MUIM_Notify, MUIA_Window_Open, FALSE, obj, 1, UPM_Menu_Closed);
    } else {
        aa_pop_unpush(obj, d->popup); /* a close queued for the last open must not shut this one */
        aamenu_place(obj, d);
        set(d->list, UPA_List_Active, (ULONG)-1);
    }
    set(d->list, UPA_List_Disabled, d->disabled); /* grey rows; the mask may have changed since the last open */
    /* only from an active window: during a UP-Menustrip drag the application window keeps the input (pointer and button release) */
    set(d->popup, MUIA_Window_Activate, keys || (win->Flags & WFLG_WINDOWACTIVE) != 0);
    d->deact = 0; /* the Activate FALSE notify above is no loss of activation */
    d->cancelled = 0;
    mui_aa_pop_opened(&d->pop);
    set(d->popup, MUIA_Window_Open, TRUE);
    aa_pop_fit(obj, d->popup);
    if (d->bar_h > 0) {
        set(d->btn, UPA_Button_On, TRUE); /* the open title in the theme's selected colours */
    }
    set(obj, UPA_Menu_Open, TRUE);
}

static void aamenu_close(aamenu_data* d) {
    if (d->popup != NULL && d->pop.open) {
        set(d->popup, MUIA_Window_Open, FALSE); /* the Open notify runs UPM_Menu_Closed */
    }
}

/* The close for the popup's own notifications: queued, never run inside them. Closing a closed window does nothing, so a second
 * queued close (a pick that also deactivates) is harmless. Not open (the Activate FALSE set of aamenu_open): nothing to close. */
static void aamenu_push_close(Object* obj, aamenu_data* d) {
    if (d->pop.open) {
        aa_pop_push_close(obj, d->popup);
    }
}

static void aamenu_closed(Object* obj, aamenu_data* d) {
    int was = d->pop.open;
    mui_aa_pop_closed(&d->pop, d->deact, aamenu_now());
    d->deact = 0;
    if (was && d->bar_h > 0) {
        set(d->btn, UPA_Button_On, FALSE);
    }
    if (was) {
        set(obj, UPA_Menu_Open, FALSE);
    }
}

static ULONG aamenu_setup(struct IClass* cl, Object* obj, Msg msg) {
    aamenu_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    if (d->ent != NULL) {
        d->ehn.ehn_Priority = 0, d->ehn.ehn_Flags = 0, d->ehn.ehn_Object = obj, d->ehn.ehn_Class = cl;
        d->ehn.ehn_Events = IDCMP_RAWKEY;
        DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    }
    return TRUE;
}

/* Amiga+key of a one-character shortcut picks that item; Left / Right in the open popup set UPA_Menu_Step. Every other key goes on to MUI
 * (the popup's UP-List takes Up, Down, Return and Esc). */
static ULONG aamenu_key(Object* obj, aamenu_data* d, struct IntuiMessage* im) {
    struct InputEvent ie;
    char c[4];
    int i;
    ULONG dis = 0;
    struct Window* pw = NULL;
    if (im != NULL && d->popup != NULL) {
        GetAttr(MUIA_Window_Window, d->popup, (ULONG*)&pw);
    }
    if (pw != NULL && im->IDCMPWindow == pw) {
        if (im->Class == IDCMP_RAWKEY && (im->Code == 0x4E || im->Code == 0x4F)) { /* cursor Right, Left */
            set(obj, UPA_Menu_Step, im->Code == 0x4E ? 1UL : (ULONG)-1L);
            return MUI_EventHandlerRC_Eat;
        }
        if (im->Class == IDCMP_RAWKEY && im->Code == 0x45) { /* Esc: also when UP-List did not take it */
            DoMethod(obj, UPM_Menu_Cancel);
            return MUI_EventHandlerRC_Eat;
        }
        return 0;
    }
    GetAttr(MUIA_Disabled, obj, &dis);
    if (im == NULL || im->Class != IDCMP_RAWKEY || (im->Code & IECODE_UP_PREFIX) || !(im->Qualifier & IEQUALIFIER_RCOMMAND) || dis) {
        return 0;
    }
    if ((KeymapBase = OpenLibrary((CONST_STRPTR) "keymap.library", 37)) == NULL) {
        return 0;
    }
    memset(&ie, 0, sizeof ie);
    ie.ie_Class = IECLASS_RAWKEY, ie.ie_Code = im->Code, ie.ie_Qualifier = im->Qualifier & ~IEQUALIFIER_RCOMMAND;
    ie.ie_EventAddress = (APTR)*((ULONG*)im->IAddress); /* dead-key context */
    i = (int)MapRawKey(&ie, (STRPTR)c, sizeof c, NULL);
    CloseLibrary(KeymapBase);
    KeymapBase = NULL;
    /* d->ent may be NULL here: UP-Menustrip sends MUIM_HandleEvent to every menu, also to one without Entries */
    if (i == 1 && (i = mui_aa_menu_shortcut(d->ent, d->n, d->disabled, (UBYTE)c[0])) >= 0) {
        set(obj, UPA_Menu_Chosen, (ULONG)i);
        return MUI_EventHandlerRC_Eat;
    }
    return 0;
}

static ULONG aamenu_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aamenu_data* d = INST_DATA(cl, obj);
    if (d->ent != NULL) {
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    }
    aamenu_drop(obj, d);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aamenu_dispose(struct IClass* cl, Object* obj, Msg msg) {
    aamenu_data* d = INST_DATA(cl, obj);
    if (d->popup == NULL && d->root != NULL) {
        MUI_DisposeObject(d->root);
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* The shown shortcut texts, from the items. */
static void aamenu_keys(aamenu_data* d) {
    memset(d->keys, 0, sizeof d->keys);
    for (int i = 0; d->ent != NULL && i < d->n; i++) {
        const char* k = d->ent[i].shortcut;
        if (k != NULL && k[0] != '\0') {
            int one = k[1] == '\0' && !(d->ent[i].flags & UPV_MenuItem_CommandString);
            strncpy(d->keys[i], one ? "Amiga+" : "", sizeof d->keys[i] - 1);
            strncat(d->keys[i], k, sizeof d->keys[i] - 1 - strlen(d->keys[i]));
        }
    }
}

/* MUIM_Family_AddHead / AddTail / Insert / Remove on the items of an Entries menu. m[1] the UP_MenuItem (copied), m[2] Insert's
 * predecessor (NULL or not found = head); items are found by their title pointer. Checked / Disabled bits move with the items. */
static ULONG aamenu_family(Object* obj, aamenu_data* d, ULONG* m) {
    const UP_MenuItem* it = (const UP_MenuItem*)m[1];
    int p = -1, k;
    (void)obj;
    if (it == NULL || it->title == NULL) {
        return 0;
    }
    if (m[0] == MUIM_Family_Remove) {
        for (k = 0; k < d->n && p < 0; k++) {
            p = d->own[k].title == it->title ? k : -1;
        }
        if (p < 0) {
            return 0;
        }
        memmove(&d->own[p], &d->own[p + 1], (size_t)(d->n - p - 1) * sizeof d->own[0]);
        d->n--;
        d->checked = mui_aa_mask_remove(d->checked, p);
        d->disabled = mui_aa_mask_remove(d->disabled, p);
    } else {
        if (d->n >= UP_MENU_MAXITEMS) {
            return 0;
        }
        p = m[0] == MUIM_Family_AddTail ? d->n : 0;
        if (m[0] == MUIM_Family_Insert && m[2] != 0) {
            for (k = 0; k < d->n; k++) {
                if (d->own[k].title == ((const UP_MenuItem*)m[2])->title) {
                    p = k + 1;
                }
            }
        }
        memmove(&d->own[p + 1], &d->own[p], (size_t)(d->n - p) * sizeof d->own[0]);
        d->own[p] = *it;
        d->n++;
        d->checked = mui_aa_mask_insert(d->checked, p, (it->flags & UPV_MenuItem_Checked) != 0);
        d->disabled = mui_aa_mask_insert(d->disabled, p, (it->flags & UPV_MenuItem_Disabled) != 0);
    }
    if (d->chosen >= d->n) {
        d->chosen = -1;
    }
    aamenu_keys(d);
    d->stale = 1;
    if (d->list != NULL) { /* an open popup shows the change now, within its row count limit */
        SetAttrs(d->list, UPA_List_Count, (ULONG)d->n, UPA_List_Disabled, d->disabled, TAG_DONE);
    }
    return 1;
}

static ULONG aamenu_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aamenu_data* d;
    struct opSet nm;
    struct TagItem def[6];
    struct TagItem* tags = msg->ops_AttrList;
    const char* title = (const char*)GetTagData(UPA_Menu_Title, 0, tags);
    const char* const* items = (const char* const*)GetTagData(UPA_Menu_Items, 0, tags);
    const UP_MenuItem* ent = (const UP_MenuItem*)GetTagData(UPA_Menu_Entries, 0, tags);
    int n = 0;
    Object* btn;
    while (ent != NULL ? n < UP_MENU_MAXITEMS && ent[n].title != NULL : items != NULL && items[n] != NULL) {
        n++;
    }
    btn = MUI_NewObject((char*)"UP-Button.mcc", UPA_Button_Caption, (ULONG)(title != NULL ? title : ""), TAG_DONE);
    if (btn == NULL) {
        return 0;
    }
    def[0].ti_Tag = MUIA_Group_Horiz, def[0].ti_Data = (ULONG)TRUE;
    def[1].ti_Tag = MUIA_Frame, def[1].ti_Data = (ULONG)MUIV_Frame_None;
    def[2].ti_Tag = MUIA_Group_Child, def[2].ti_Data = (ULONG)btn;
    def[3].ti_Tag = MUIA_Disabled, def[3].ti_Data = (ULONG)!GetTagData(UPA_Menu_Enabled, TRUE, tags);
    def[4].ti_Tag = TAG_MORE, def[4].ti_Data = (ULONG)(uintptr_t)tags;
    nm = *msg;
    nm.ops_AttrList = def;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm);
    if (obj == NULL) {
        return 0; /* the group disposed the button */
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->btn = btn;
    d->title = title != NULL ? title : "";
    d->items = items;
    d->n = n;
    d->chosen = -1;
    if (ent != NULL) {
        memcpy(d->own, ent, (size_t)n * sizeof d->own[0]);
        d->ent = d->own;
    }
    for (int i = 0; ent != NULL && i < n; i++) {
        d->checked |= ent[i].flags & UPV_MenuItem_Checked ? 1UL << i : 0;
        d->disabled |= ent[i].flags & UPV_MenuItem_Disabled ? 1UL << i : 0;
    }
    aamenu_keys(d);
    d->checked = (ULONG)GetTagData(UPA_Menu_Checked, d->checked, tags);
    d->disabled = (ULONG)GetTagData(UPA_Menu_Disabled, d->disabled, tags);
    DoMethod(btn, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, UPM_Menu_Click);
    return (ULONG)obj;
}

static ULONG aamenu_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aamenu_data* d = INST_DATA(cl, obj);
    struct TagItem* ti;
    if ((ti = FindTagItem(UPA_Menu_Chosen, msg->ops_AttrList)) != NULL) {
        int v = (int)ti->ti_Data;
        if (v < -1 || v >= d->n || aamenu_is_sep(d, v) || (v >= 0 && (d->disabled & (1UL << v)))) {
            ti->ti_Tag = TAG_IGNORE; /* a separator, a disabled item or an out of range pick chooses nothing and does not notify */
        } else {
            ULONG c = d->ent != NULL && v >= 0
                          ? mui_aa_menu_pick(d->checked, v, (d->ent[v].flags & UPV_MenuItem_Checkit) != 0, (d->ent[v].flags & UPV_MenuItem_Toggle) != 0, d->ent[v].exclude)
                          : d->checked;
            d->chosen = v;
            if (c != d->checked) {
                SetAttrs(obj, UPA_Menu_Checked, c, TAG_DONE); /* MUIA_Menuitem_Checked notifies */
            }
        }
    }
    if ((ti = FindTagItem(UPA_Menu_Checked, msg->ops_AttrList)) != NULL) {
        d->checked = ti->ti_Data;
        if (d->list != NULL && d->pop.open) {
            MUI_Redraw(d->list, MADF_DRAWOBJECT);
        }
    }
    if ((ti = FindTagItem(UPA_Menu_Disabled, msg->ops_AttrList)) != NULL) {
        d->disabled = ti->ti_Data;
    }
    if ((ti = FindTagItem(UPA_Menu_Enabled, msg->ops_AttrList)) != NULL) {
        set(obj, MUIA_Disabled, !ti->ti_Data);
    }
    if ((ti = FindTagItem(UPA_Menu_Step, msg->ops_AttrList)) != NULL) {
        d->step = (LONG)ti->ti_Data; /* the super class notifies */
    }
    if ((ti = FindTagItem(UPA_Menu_Open, msg->ops_AttrList)) != NULL) {
        /* the notify of our own set(UPA_Menu_Open) comes back here: act only when the wish differs from the state */
        if (ti->ti_Data && !d->pop.open) {
            aamenu_open(obj, d, d->popup != NULL ? MUI_AA_POP_OPEN_REUSE : MUI_AA_POP_OPEN_NEW, ti->ti_Data == UPV_Menu_Open_Keys);
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
    if (msg->opg_AttrID == UPA_Menu_Checked || msg->opg_AttrID == UPA_Menu_Disabled) {
        *msg->opg_Storage = msg->opg_AttrID == UPA_Menu_Checked ? d->checked : d->disabled;
        return TRUE;
    }
    if (msg->opg_AttrID == UPA_Menu_Enabled) {
        ULONG dis = 0;
        GetAttr(MUIA_Disabled, obj, &dis);
        *msg->opg_Storage = !dis;
        return TRUE;
    }
    if (msg->opg_AttrID == UPA_Menu_Step) {
        *msg->opg_Storage = (ULONG)d->step;
        return TRUE;
    }
    if (msg->opg_AttrID == UPA_Menu_Cancelled) {
        *msg->opg_Storage = (ULONG)d->cancelled;
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
    case MUIM_Setup:
        return aamenu_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aamenu_cleanup(cl, obj, msg);
    case MUIM_HandleEvent:
        return aamenu_key(obj, INST_DATA(cl, obj), ((struct MUIP_HandleEvent*)msg)->imsg);
    case MUIM_Family_AddHead:
    case MUIM_Family_AddTail:
    case MUIM_Family_Insert:
    case MUIM_Family_Remove:
        d = INST_DATA(cl, obj);
        if (d->ent != NULL) {
            return aamenu_family(obj, d, (ULONG*)msg);
        }
        break;
    case UPM_Menu_Click:
        d = INST_DATA(cl, obj);
        {
            int act = mui_aa_pop_click(&d->pop, aamenu_now());
            if (act == MUI_AA_POP_CLOSE) {
                aamenu_close(d);
            } else if (act != MUI_AA_POP_NONE) {
                aamenu_open(obj, d, act, 0);
            }
        }
        return 0;
    case UPM_Menu_Deactivated:
        d = INST_DATA(cl, obj);
        d->deact = 1;
        /* fallthrough */
    case UPM_Menu_PushClose:
        aamenu_push_close(obj, INST_DATA(cl, obj));
        return 0;
    case UPM_Menu_Closed:
        aamenu_closed(obj, INST_DATA(cl, obj));
        return 0;
    case UPM_Menu_Cancel: /* from the list's notify or the popup's key handler: queued close, never a close here */
        d = INST_DATA(cl, obj);
        if (d->pop.open) {
            d->cancelled = 1;
            aamenu_push_close(obj, d);
            set(obj, UPA_Menu_Cancelled, TRUE); /* for application listeners; UP-Menustrip reads the state */
        }
        return 0;
    case UPM_Menu_Bar: /* ((LONG*)msg)[1] the bar height; the menu is not set up (the strip's bar is closed) */
        d = INST_DATA(cl, obj);
        d->cancelled = 0;
        {
            LONG h = (LONG)((ULONG*)msg)[1];
            Object* nb;
            if (h > 0 && h != d->bar_h &&
                (nb = MUI_NewObject((char*)"UP-Button.mcc", UPA_Button_Caption, (ULONG)d->title, UPA_Button_Flat, (ULONG)TRUE, UPA_Button_FixedHeight, (ULONG)h, TAG_DONE)) != NULL) {
                DoMethod(obj, MUIM_Group_InitChange);
                DoMethod(obj, OM_REMMEMBER, (ULONG)d->btn);
                MUI_DisposeObject(d->btn); /* its Pressed notify goes with it */
                DoMethod(obj, OM_ADDMEMBER, (ULONG)nb);
                DoMethod(obj, MUIM_Group_ExitChange);
                d->btn = nb;
                d->bar_h = h;
                DoMethod(nb, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, UPM_Menu_Click);
            }
        }
        return 0;
    case UPM_Menu_Track: /* the IntuiMessage of the strip's window, moved into the popup's coordinates; a button event is a click */
        d = INST_DATA(cl, obj);
        {
            struct IntuiMessage im = *(struct IntuiMessage*)((ULONG*)msg)[1];
            struct Window* pw = NULL;
            if (d->pop.open && d->list != NULL && im.IDCMPWindow != NULL) {
                GetAttr(MUIA_Window_Window, d->popup, (ULONG*)&pw);
            }
            if (pw == NULL) {
                return 0;
            }
            im.MouseX += im.IDCMPWindow->LeftEdge - pw->LeftEdge;
            im.MouseY += im.IDCMPWindow->TopEdge - pw->TopEdge;
            im.IDCMPWindow = pw;
            if (im.Class == IDCMP_MOUSEBUTTONS) {
                im.Code = SELECTDOWN;
            }
            return DoMethod(d->list, MUIM_HandleEvent, (ULONG)&im, 0UL); /* UP-List hovers or picks; a pick notifies UPA_Menu_Chosen */
        }
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
