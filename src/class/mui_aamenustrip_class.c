/* UP-Menustrip (MUIC_Menustrip, MUIA_Window_Menustrip). MUI's Window class takes only its own Menustrip class, so the strip is an
 * invisible Area object (MUIA_ShowMe FALSE) that the application puts anywhere in the window that gets the menus. It owns a borderless
 * "bar" window: a Popup UP-Panel around a horizontal group of the UP-Menu children (MUIA_Family_Child at creation, MUIM_Family_* later).
 * The items stay the UP_MenuItem arrays of the UP-Menu objects; there is no second item model.
 * An event handler on the application window opens the bar at the window's inner top-left corner on the right mouse button and gives
 * Amiga+key to each menu (UP-Menu's own MUIM_HandleEvent). A pick sets UPA_Menustrip_Trigger and closes the bar. The bar also closes
 * when it is not the active window and no menu popup is open. The right button closes an open bar from any of its windows: the bar
 * (a second event handler) and a menu popup (UP-List cancels on it) have no Intuition menus, so the press reaches MUI. Closing the bar
 * never leaves no window active: click style moves the activation into the bar or a popup, and on OS 3.x closing the active window
 * activates none, so the right button would never reach the application window again. A bar that lost the activation to another
 * window leaves it there.
 * The bar is never closed inside a notification of a menu or of a popup window: closing runs MUIM_Cleanup of the menus, which disposes
 * the popup whose notification is running. Every close goes through MUIM_Application_PushMethod.
 * Two styles: click (press and release the right button, then click a title and an item) and hold-drag-release (hold the right button,
 * move over the titles, release over an item). While the button is held the bar and the menu popups open inactive, so the application
 * window keeps getting the pointer moves and the release; the strip hit-tests the titles and hands moves and the release to the open menu
 * (UPM_Menu_Track). The decision is mui_aa_strip_track (host test mui-menu-check).
 * shortcut: no key opens the bar, only Amiga+key shortcuts work without a mouse; upgrade with an Open attribute when needed. */
#include "mui_aamenustrip_class.h"
#include "mui_aa_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <devices/inputevent.h>

#include "map/mui_aa.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

typedef struct aamenustrip_data {
    Object *bar, *row; /* the bar window (owned) and its group of UP-Menu objects */
    int enabled;
    int added;         /* the bar window is a member of the application (first open until cleanup) */
    ULONG trigger;
    int cur;           /* mui_aa_strip_track: -2 no hold, -1 held, else the menu the drag opened */
    struct MUI_EventHandlerNode ehn, bhn; /* bhn: the right button pressed in the active click-style bar */
} aamenustrip_data;

static struct MUI_CustomClass* g_menustrip_class;

/* Cursor for NextObject() over the menus. */
static Object* aamenustrip_head(const aamenustrip_data* d) {
    struct List* l = NULL;
    GetAttr(MUIA_Group_ChildList, d->row, (ULONG*)&l);
    return l != NULL ? (Object*)l->lh_Head : NULL;
}

static void aamenustrip_close(Object* obj, aamenustrip_data* d) {
    ULONG open = 0;
    GetAttr(MUIA_Window_Open, d->bar, &open);
    if (open && aa_live(obj)) {
        DoMethod(_app(obj), MUIM_Application_PushMethod, (ULONG)d->bar, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    }
}

/* The right mouse button: open the bar over the window's top-left inner corner, or close an open bar. */
static void aamenustrip_toggle(Object* obj, aamenustrip_data* d) {
    struct Window* w = _window(obj);
    ULONG open = 0;
    GetAttr(MUIA_Window_Open, d->bar, &open);
    if (open) {
        aamenustrip_close(obj, d);
        return;
    }
    d->cur = -1;
    if (!d->added) {
        DoMethod(_app(obj), OM_ADDMEMBER, (ULONG)d->bar);
        d->added = 1;
        d->bhn.ehn_Object = obj, d->bhn.ehn_Class = d->ehn.ehn_Class, d->bhn.ehn_Events = IDCMP_MOUSEBUTTONS;
        DoMethod(d->bar, MUIM_Window_AddEventHandler, (ULONG)&d->bhn);
        DoMethod(d->bar, MUIM_Notify, MUIA_Window_Open, FALSE, (ULONG)obj, 1, UPM_Menustrip_Check);
    }
    SetAttrs(d->bar, MUIA_Window_Screen, (ULONG)w->WScreen, MUIA_Window_LeftEdge, (ULONG)(w->LeftEdge + w->BorderLeft), MUIA_Window_TopEdge,
             (ULONG)(w->TopEdge + w->BorderTop), TAG_DONE);
    set(d->bar, MUIA_Window_Activate, FALSE); /* the held button stays with this window; a click-style release activates the bar */
    set(d->bar, MUIA_Window_Open, TRUE);
}

static Object* aamenustrip_nth(const aamenustrip_data* d, int n) {
    Object *st = aamenustrip_head(d), *o;
    while (st != NULL && (o = NextObject((APTR)&st)) != NULL) {
        if (n-- == 0) {
            return o;
        }
    }
    return NULL;
}

/* The number of the title under screen point x,y in the bar window bw, -1 none. */
static int aamenustrip_hit(const aamenustrip_data* d, const struct Window* bw, int x, int y) {
    Object *st = aamenustrip_head(d), *o;
    int i = 0;
    x -= bw->LeftEdge, y -= bw->TopEdge;
    while (st != NULL && (o = NextObject((APTR)&st)) != NULL) {
        if (x >= _left(o) && x <= _right(o) && y >= _top(o) && y <= _bottom(o)) {
            return i;
        }
        i++;
    }
    return -1;
}

/* A pointer move or the right button release while the button is held since it opened the bar. */
static ULONG aamenustrip_drag(Object* obj, aamenustrip_data* d, struct IntuiMessage* im) {
    struct Window *w = _window(obj), *bw = NULL;
    int old = d->cur, up = im->Class == IDCMP_MOUSEBUTTONS;
    Object* o;
    GetAttr(MUIA_Window_Window, d->bar, (ULONG*)&bw);
    if (bw == NULL) { /* the bar closed under the hold (Amiga+key, another window) */
        d->cur = -2;
        return 0;
    }
    switch (mui_aa_strip_track(&d->cur, aamenustrip_hit(d, bw, w->LeftEdge + im->MouseX, w->TopEdge + im->MouseY), up)) {
    case MUI_AA_STRIP_OPEN: /* open the new menu first: the old one's Open FALSE notify then sees an open menu and keeps the bar */
        if ((o = aamenustrip_nth(d, d->cur)) != NULL) {
            set(o, UPA_Menu_Open, TRUE);
        }
        if ((o = aamenustrip_nth(d, old)) != NULL) {
            set(o, UPA_Menu_Open, FALSE);
        }
        break;
    case MUI_AA_STRIP_PICK: /* over an item it picks (Picked closes the bar), anywhere else nothing; the bar closes either way */
        if ((o = aamenustrip_nth(d, old)) != NULL) {
            DoMethod(o, UPM_Menu_Track, (ULONG)im);
        }
        aamenustrip_close(obj, d);
        break;
    case MUI_AA_STRIP_KEEP:
        set(d->bar, MUIA_Window_Activate, TRUE);
        break;
    default:
        if ((o = aamenustrip_nth(d, old)) != NULL) {
            DoMethod(o, UPM_Menu_Track, (ULONG)im); /* hover the item under the pointer */
        }
    }
    return up ? MUI_EventHandlerRC_Eat : 0;
}

/* The bar lost activation or a menu popup closed: an inactive bar with no open menu goes away.
 * The bar closed: if the activation was in the bar or in a closed popup, it goes back to the strip's window. */
static void aamenustrip_check(Object* obj, aamenustrip_data* d) {
    struct Window *bw = NULL, *aw;
    Object *st = aamenustrip_head(d), *o;
    ULONG open = 0;
    GetAttr(MUIA_Window_Window, d->bar, (ULONG*)&bw);
    GetAttr(MUIA_Window_Open, d->bar, &open);
    if (!open) {
        aw = ((struct IntuitionBase*)(void*)IntuitionBase)->ActiveWindow; /* a pointer compare only: no LockIBase */
        if ((aw == NULL || aw == bw) && aa_live(obj)) { /* bw: the notify may run before the Intuition window closed */
            DoMethod(_app(obj), MUIM_Application_PushMethod, (ULONG)_win(obj), 3, MUIM_Set, MUIA_Window_Activate, TRUE);
        }
        return;
    }
    if (bw == NULL || (bw->Flags & WFLG_WINDOWACTIVE)) { /* the Intuition flag: MUIA_Window_Activate lags behind inside a notification */
        return;
    }
    while (st != NULL && (o = NextObject((APTR)&st)) != NULL) {
        ULONG v = 0;
        GetAttr(UPA_Menu_Open, o, &v);
        if (v) {
            return;
        }
    }
    aamenustrip_close(obj, d);
}

static void aamenustrip_add(Object* obj, aamenustrip_data* d, Object* menu) {
    DoMethod(d->row, OM_ADDMEMBER, (ULONG)menu);
    DoMethod(menu, MUIM_Notify, UPA_Menu_Chosen, MUIV_EveryTime, (ULONG)obj, 3, UPM_Menustrip_Picked, (ULONG)menu, MUIV_TriggerValue);
    DoMethod(menu, MUIM_Notify, UPA_Menu_Open, FALSE, (ULONG)obj, 1, UPM_Menustrip_Check);
}

/* MUIM_Family_AddHead / AddTail / Insert / Remove on the menus. m[1] the menu, m[2] the predecessor of Insert (NULL = head). */
static ULONG aamenustrip_family(Object* obj, aamenustrip_data* d, ULONG* m) {
    Object* menu = (Object*)m[1];
    if (menu == NULL) {
        return 0;
    }
    DoMethod(d->row, MUIM_Group_InitChange);
    if (m[0] == MUIM_Family_Remove) {
        DoMethod(menu, MUIM_KillNotifyObj, UPA_Menu_Chosen, (ULONG)obj);
        DoMethod(menu, MUIM_KillNotifyObj, UPA_Menu_Open, (ULONG)obj);
        DoMethod(d->row, OM_REMMEMBER, (ULONG)menu);
    } else {
        aamenustrip_add(obj, d, menu); /* now the last child */
        if (m[0] != MUIM_Family_AddTail) {
            ULONG order[UP_MENUSTRIP_MAXMENUS + 2]; /* MUIM_Group_Sort, the menus, NULL */
            Object *pred = m[0] == MUIM_Family_Insert ? (Object*)m[2] : NULL, *st = aamenustrip_head(d), *o;
            int n = 1;
            if (pred == NULL) {
                order[n++] = (ULONG)menu;
            }
            while (st != NULL && n < UP_MENUSTRIP_MAXMENUS && (o = NextObject((APTR)&st)) != NULL) {
                if (o != menu) {
                    order[n++] = (ULONG)o;
                    if (o == pred) {
                        order[n++] = (ULONG)menu;
                    }
                }
            }
            if (st == NULL || NextObject((APTR)&st) == NULL) { /* all menus fit: more than the maximum keeps the tail position */
                order[0] = MUIM_Group_Sort;
                order[n] = 0;
                DoMethodA(d->row, (Msg)order);
            }
        }
    }
    DoMethod(d->row, MUIM_Group_ExitChange);
    return 0;
}

static ULONG aamenustrip_event(Object* obj, aamenustrip_data* d, struct IntuiMessage* im) {
    struct Window* w = _window(obj);
    Object *st, *o;
    if (im == NULL || w == NULL) {
        return 0;
    }
    if (!(w->Flags & WFLG_RMBTRAP)) { /* without it Intuition keeps the right button for its own menus */
        Forbid();
        w->Flags |= WFLG_RMBTRAP;
        Permit();
    }
    if (!d->enabled) {
        return 0;
    }
    if (im->Class == IDCMP_MOUSEBUTTONS && im->Code == MENUDOWN) {
        aamenustrip_toggle(obj, d);
        return MUI_EventHandlerRC_Eat;
    }
    if (d->cur > -2 && (im->Class == IDCMP_MOUSEMOVE || (im->Class == IDCMP_MOUSEBUTTONS && im->Code == MENUUP))) {
        return aamenustrip_drag(obj, d, im);
    }
    if (im->Class == IDCMP_RAWKEY && (im->Qualifier & IEQUALIFIER_RCOMMAND)) {
        st = aamenustrip_head(d);
        while (st != NULL && (o = NextObject((APTR)&st)) != NULL) {
            if (DoMethod(o, MUIM_HandleEvent, (ULONG)im, 0UL)) { /* UP-Menu: Amiga+key picks the item, the Chosen notify comes back here */
                return MUI_EventHandlerRC_Eat;
            }
        }
    }
    return 0;
}

static ULONG aamenustrip_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aamenustrip_data* d;
    struct opSet nm;
    struct TagItem def[2];
    struct TagItem *ti, *st = msg->ops_AttrList;
    Object *row, *card, *bar;
    if ((row = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE)) == NULL) {
        return 0;
    }
    card = MUI_NewObject((char*)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Popup, UPA_Panel_Popup, (ULONG)TRUE, MUIA_Group_Child, (ULONG)row, TAG_DONE);
    if (card == NULL) {
        card = row;
    }
    bar = MUI_NewObject((char*)MUIC_Window, MUIA_Window_Borderless, TRUE, MUIA_Window_DragBar, FALSE, MUIA_Window_CloseGadget, FALSE, MUIA_Window_SizeGadget, FALSE,
                        MUIA_Window_DepthGadget, FALSE, MUIA_Window_NoMenus, TRUE, MUIA_Window_RootObject, (ULONG)card, TAG_DONE);
    if (bar == NULL) {
        MUI_DisposeObject(card); /* the window never took it */
        return 0;
    }
    def[0].ti_Tag = MUIA_ShowMe, def[0].ti_Data = (ULONG)FALSE;
    def[1].ti_Tag = TAG_MORE, def[1].ti_Data = (ULONG)(uintptr_t)msg->ops_AttrList;
    nm = *msg;
    nm.ops_AttrList = def;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm);
    if (obj == NULL) {
        MUI_DisposeObject(bar);
        return 0;
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->bar = bar;
    d->row = row;
    d->cur = -2;
    d->enabled = GetTagData(UPA_Menustrip_Enabled, TRUE, msg->ops_AttrList) != 0;
    while ((ti = NextTagItem(&st)) != NULL) {
        if (ti->ti_Tag == MUIA_Family_Child && ti->ti_Data != 0) {
            aamenustrip_add(obj, d, (Object*)ti->ti_Data);
        }
    }
    DoMethod(bar, MUIM_Notify, MUIA_Window_Activate, FALSE, (ULONG)obj, 1, UPM_Menustrip_Check);
    return (ULONG)obj;
}

static ULONG __attribute__((used)) aamenustrip_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aamenustrip_dispatch(void);
const struct aa_class_desc aamenustrip_class_desc = {MUIC_Area, sizeof(aamenustrip_data), aamenustrip_dispatch, &g_menustrip_class};
__asm__(".text\n.even\n.globl _aamenustrip_dispatch\n_aamenustrip_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aamenustrip_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aamenustrip_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    aamenustrip_data* d;
    struct TagItem* ti;
    ULONG* m = (ULONG*)msg;
    switch (msg->MethodID) {
    case OM_NEW:
        return aamenustrip_new(cl, obj, (struct opSet*)msg);
    case OM_DISPOSE:
        d = INST_DATA(cl, obj);
        MUI_DisposeObject(d->bar); /* cleanup took it out of the application; it disposes the panel, the group and the menus */
        break;
    case OM_SET:
        d = INST_DATA(cl, obj);
        if ((ti = FindTagItem(UPA_Menustrip_Enabled, ((struct opSet*)msg)->ops_AttrList)) != NULL) {
            d->enabled = ti->ti_Data != 0;
        }
        if ((ti = FindTagItem(UPA_Menustrip_Trigger, ((struct opSet*)msg)->ops_AttrList)) != NULL) {
            d->trigger = ti->ti_Data;
        }
        break;
    case OM_GET:
        d = INST_DATA(cl, obj);
        if (((struct opGet*)msg)->opg_AttrID == UPA_Menustrip_Enabled || ((struct opGet*)msg)->opg_AttrID == UPA_Menustrip_Trigger) {
            *((struct opGet*)msg)->opg_Storage = ((struct opGet*)msg)->opg_AttrID == UPA_Menustrip_Enabled ? (ULONG)d->enabled : d->trigger;
            return TRUE;
        }
        break;
    case MUIM_Setup:
        if (!DoSuperMethodA(cl, obj, msg)) {
            return FALSE;
        }
        d = INST_DATA(cl, obj);
        d->ehn.ehn_Priority = 0, d->ehn.ehn_Flags = 0, d->ehn.ehn_Object = obj, d->ehn.ehn_Class = cl;
        d->ehn.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY | IDCMP_ACTIVEWINDOW; /* MOUSEMOVE: the drag; ACTIVEWINDOW: sets WFLG_RMBTRAP before the first click */
        DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
        return TRUE;
    case MUIM_Cleanup:
        d = INST_DATA(cl, obj);
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
        if (d->added) { /* no reactivation of a window that is going away */
            DoMethod(d->bar, MUIM_KillNotifyObj, MUIA_Window_Open, (ULONG)obj);
            DoMethod(d->bar, MUIM_Window_RemEventHandler, (ULONG)&d->bhn);
        }
        set(d->bar, MUIA_Window_Open, FALSE);
        if (d->added) {
            DoMethod(_app(obj), OM_REMMEMBER, (ULONG)d->bar);
            d->added = 0;
        }
        break;
    case MUIM_HandleEvent:
        return aamenustrip_event(obj, INST_DATA(cl, obj), ((struct MUIP_HandleEvent*)msg)->imsg);
    case MUIM_Family_AddHead:
    case MUIM_Family_AddTail:
    case MUIM_Family_Insert:
    case MUIM_Family_Remove:
        return aamenustrip_family(obj, INST_DATA(cl, obj), m);
    case UPM_Menustrip_Picked: /* m[1] the menu, m[2] its UPA_Menu_Chosen */
        d = INST_DATA(cl, obj);
        if ((LONG)m[2] >= 0) {
            Object *st = aamenustrip_head(d), *o;
            ULONG i = 0;
            while (st != NULL && (o = NextObject((APTR)&st)) != NULL && o != (Object*)m[1]) {
                i++;
            }
            SetAttrs(obj, UPA_Menustrip_Trigger, (i << 8) | m[2], TAG_DONE); /* notifies */
            aamenustrip_close(obj, d); /* the bar's Open FALSE notify gives the activation back */
        }
        return 0;
    case UPM_Menustrip_Check:
        aamenustrip_check(obj, INST_DATA(cl, obj));
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* UP_NewMenustrip(const struct TagItem* tags) {
    if (g_menustrip_class == NULL) {
        g_menustrip_class = AA_CLASS_CREATE(&aamenustrip_class_desc);
    }
    if (g_menustrip_class == NULL) {
        return NULL;
    }
    return NewObjectA(g_menustrip_class->mcc_Class, NULL, (struct TagItem*)tags);
}

void mui_aamenustrip_class_free(void) {
    if (g_menustrip_class != NULL) {
        MUI_DeleteCustomClass(g_menustrip_class);
        g_menustrip_class = NULL;
    }
}
