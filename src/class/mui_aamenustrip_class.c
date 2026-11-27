/* UP-Menustrip (MUIC_Menustrip, MUIA_Window_Menustrip). MUI's Window class takes only its own Menustrip class, so the strip is an
 * invisible Area object (MUIA_ShowMe FALSE) that the application puts anywhere in the window that gets the menus; it takes no space.
 * It owns a borderless "bar" window over the screen's title bar (Intuition menu style): left 0, top 0, the screen's width, BarHeight + 1
 * high, never inside the application window. Its root is a Popup UP-Panel (theme colours) around the horizontal group of the UP-Menu
 * children (MUIA_Family_Child at creation, MUIM_Family_* later), left-aligned by a filler. Before each open every menu gets UPM_Menu_Bar:
 * a slim flat title of the bar height, lit while its popup is open, the popup right below the bar.
 * The items stay the UP_MenuItem arrays of the UP-Menu objects; there is no second item model.
 * An event handler on the application window opens the bar on the right mouse button and gives
 * Amiga+key to each menu (UP-Menu's own MUIM_HandleEvent). A pick sets UPA_Menustrip_Trigger and closes the bar. The bar also closes
 * when it is not the active window and no menu popup is open. The right button closes an open bar from any of its windows: the bar
 * (a second event handler) and a menu popup (UP-List cancels on it) have no Intuition menus, so the press reaches MUI. Closing the bar
 * never leaves no window active: click style moves the activation into the bar or a popup, and on OS 3.x closing the active window
 * activates none, so the right button would never reach the application window again. A bar that lost the activation to another
 * window leaves it there.
 * The bar is never closed inside a notification of a menu or of a popup window: closing runs MUIM_Cleanup of the menus, which disposes
 * the popup whose notification is running. Every close goes through MUIM_Application_PushMethod, and so does the UPA_Menustrip_Trigger
 * set of a pick, since an application notify on it may close the windows (MUIA_Application_Iconified).
 * MUIM_DisconnectParent drops the pushes still queued for the strip, the bar and the window reactivation (MUIM_Application_UnpushMethod, MUI V20 and OpenMUI;
 * MUI 3.8 has none and ignores the method), so a strip removed and disposed while the application runs leaves no dangling target.
 * Two styles: click (press and release the right button, then click a title and an item) and hold-drag-release (hold the right button,
 * move over the titles, release over an item). While the button is held the bar and the menu popups open inactive, so the application
 * window keeps getting the pointer moves and the release; the strip hit-tests the titles and hands moves and the release to the open menu
 * (UPM_Menu_Track). The decision is mui_aa_strip_track (host test mui-menu-check).
 * Keyboard: a tap of the Right Amiga key (down and up with no key between) in the application window opens the bar with its first enabled
 * menu's popup active (UPV_Menu_Open_Keys); a second tap closes it. In the popup UP-List takes Up, Down, Return (pick) and Esc (close; the
 * menu sets UPA_Menu_Cancelled and the bar closes with it); Left and Right set UPA_Menu_Step and the strip opens the next enabled menu (mui_aa_strip_step, host
 * test mui-menu-check), the new one first as in the drag. Esc in the active bar with no popup open closes the bar (bhn takes RAWKEY).
 * Cancel is menu state (UPA_Menu_Cancelled, cleared by UPM_Menu_Bar and at each menu open): the menu's Open FALSE check closes the bar
 * when a menu was cancelled, even while the bar is the active window. No notify chain carries it.
 * A close is queued; d->closing marks it, so a right click before it ran reopens (drops the queued close) in place of queueing a second.
 * shortcut: Right Amiga again while a popup is active does not close the bar (the key goes to the popup); Esc does.
 * WFLG_RMBTRAP: Intuition (input.device task) decides at press time whether a right press becomes MENUDOWN or its own menu mode.
 * MUI 3.8's Window class clears the flag behind the strip, and a repair in an event handler runs too late for a press that arrives
 * before the application handled the event before it. So between MUIM_Setup and MUIM_Cleanup an input.device handler at priority 51
 * (Intuition: 50) sets the flag on the application window or the bar when that window is the active one and the event is the right
 * press. The handler touches only the trap struct in the instance data; the window pointers are cleared before the windows close. */
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
#include <devices/input.h>
#include <devices/inputevent.h>
#include <exec/interrupts.h>
#include <exec/io.h>

#include "map/mui_aa.h"
#include "mui_aa_draw.h"
#include "mui_aa_trace.h"

extern struct Library* MUIMasterBase;

/* All the input handler may read. win[0] the application window (set at its first event), win[1] the open bar. */
typedef struct aamenustrip_trap {
    struct IntuitionBase* ib;
    struct Window* volatile win[2];
} aamenustrip_trap;

typedef struct aamenustrip_data {
    aamenustrip_trap trap;
    struct Interrupt trap_is;
    struct MsgPort* trap_port;
    struct IOStdReq* trap_io; /* non-NULL: the handler is installed */
    Object *bar, *row; /* the bar window (owned) and its group of UP-Menu objects */
    int enabled;
    int added;         /* the bar window is a member of the application (first open until cleanup) */
    ULONG trigger;
    int cur;           /* mui_aa_strip_track: -2 no hold, -1 held, else the menu the drag opened */
    int tap;           /* Right Amiga is down with no other key or button since */
    int px, py;        /* screen point of the right press that opened the bar (cur -1) */
    struct MUI_EventHandlerNode ehn, bhn; /* bhn: the right button pressed in the active click-style bar */
    Object* app;       /* the application seen at MUIM_ConnectParent, cleared at the unpush: a non-NULL _app(obj) is no proof of life */
    Object* act_win;   /* the window of the last queued MUIA_Window_Activate push, and its PushMethod token (OpenMUI: the queue node) */
    ULONG act_tok;
    int closing;       /* a bar close is queued and has not run */
} aamenustrip_data;

static struct MUI_CustomClass* g_menustrip_class;

/* input.device task context: no library calls, no allocation. Only compares the pointers unless one is the active window. */
static struct InputEvent* __attribute__((used)) aamenustrip_input_c(struct InputEvent* ev, aamenustrip_trap* t) {
    struct InputEvent* e;
    for (e = ev; e != NULL; e = e->ie_NextEvent) {
        if (e->ie_Class == IECLASS_RAWMOUSE && e->ie_Code == IECODE_RBUTTON) {
            struct Window* aw = t->ib->ActiveWindow;
            if (aw != NULL && (aw == t->win[0] || aw == t->win[1])) {
                aw->Flags |= WFLG_RMBTRAP;
            }
        }
    }
    return ev;
}
extern void aamenustrip_input(void); /* a0 the event chain, a1 is_Data; d0 the chain */
__asm__(".text\n.even\n_aamenustrip_input:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a0,-(sp)\n\tjbsr _aamenustrip_input_c\n\taddq.l #8,sp\n\trts\n");

static void aamenustrip_trap_add(aamenustrip_data* d) {
    d->trap.ib = (struct IntuitionBase*)(void*)IntuitionBase;
    d->trap.win[0] = d->trap.win[1] = NULL;
    d->trap_is.is_Node.ln_Type = NT_INTERRUPT, d->trap_is.is_Node.ln_Pri = 51, d->trap_is.is_Node.ln_Name = (char*)"UP-Menustrip";
    d->trap_is.is_Data = &d->trap, d->trap_is.is_Code = aamenustrip_input;
    if ((d->trap_port = CreateMsgPort()) != NULL &&
        (d->trap_io = (struct IOStdReq*)CreateIORequest(d->trap_port, sizeof(struct IOStdReq))) != NULL) {
        if (OpenDevice((STRPTR) "input.device", 0, (struct IORequest*)d->trap_io, 0) == 0) {
            d->trap_io->io_Command = IND_ADDHANDLER, d->trap_io->io_Data = &d->trap_is;
            DoIO((struct IORequest*)d->trap_io);
            return;
        }
        DeleteIORequest((struct IORequest*)d->trap_io);
        d->trap_io = NULL;
    }
    DeleteMsgPort(d->trap_port); /* NULL is fine; without the handler the repair in aamenustrip_event is all there is */
    d->trap_port = NULL;
}

static void aamenustrip_trap_rem(aamenustrip_data* d) {
    d->trap.win[0] = d->trap.win[1] = NULL;
    if (d->trap_io != NULL) {
        d->trap_io->io_Command = IND_REMHANDLER, d->trap_io->io_Data = &d->trap_is;
        DoIO((struct IORequest*)d->trap_io); /* synchronous: the handler does not run after this */
        CloseDevice((struct IORequest*)d->trap_io);
        DeleteIORequest((struct IORequest*)d->trap_io);
        DeleteMsgPort(d->trap_port);
        d->trap_io = NULL, d->trap_port = NULL;
    }
}

/* Cursor for NextObject() over the menus. */
static Object* aamenustrip_head(const aamenustrip_data* d) {
    struct List* l = NULL;
    GetAttr(MUIA_Group_ChildList, d->row, (ULONG*)&l);
    return l != NULL ? (Object*)l->lh_Head : NULL;
}

static void aamenustrip_close(Object* obj, aamenustrip_data* d) {
    ULONG open = 0;
    GetAttr(MUIA_Window_Open, d->bar, &open);
    UPMS("close open=%ld live=%ld closing=%ld cur=%ld", open, (ULONG)aa_live(obj), (ULONG)d->closing, (ULONG)d->cur);
    if (open && aa_live(obj)) {
        DoMethod(_app(obj), MUIM_Application_PushMethod, (ULONG)d->bar, 3, MUIM_Set, MUIA_Window_Open, FALSE);
        d->closing = 1;
    }
}

/* The right mouse button: open the bar over the screen's title bar, or close an open bar. */
static void aamenustrip_toggle(Object* obj, aamenustrip_data* d) {
    struct Window *w = _window(obj), *bw = NULL;
    Object *st, *o;
    ULONG open = 0;
    GetAttr(MUIA_Window_Open, d->bar, &open);
    GetAttr(MUIA_Window_Window, d->bar, (ULONG*)&bw);
    UPMS("toggle open=%ld bw=%lx closing=%ld cur=%ld", open, (ULONG)bw, (ULONG)d->closing, (ULONG)d->cur);
    if (bw == NULL) { /* no Intuition window: the bar is closed, whatever the attribute or a stale closing flag says */
        open = 0;
        d->closing = 0;
    }
    if (open && d->closing) {
        UPMS("toggle branch=unpush %ld", 0UL);
        /* the close is still queued: drop it and keep the bar.
         * shortcut: MUI 3.8 has no UnpushMethod, there the queued close still runs; upgrade if MUI 3.8 becomes a target again */
        DoMethod(_app(obj), MUIM_Application_UnpushMethod, (ULONG)d->bar, 0, MUIM_Set);
        d->closing = 0;
        d->cur = -1;
        return;
    }
    if (open) {
        UPMS("toggle branch=close %ld", 0UL);
        aamenustrip_close(obj, d);
        return;
    }
    d->cur = -1;
    st = aamenustrip_head(d);
    while (st != NULL && (o = NextObject((APTR)&st)) != NULL) { /* the bar is closed: the menus are not set up */
        DoMethod(o, UPM_Menu_Bar, (ULONG)(w->WScreen->BarHeight + 1));
    }
    if (!d->added) {
        DoMethod(_app(obj), OM_ADDMEMBER, (ULONG)d->bar);
        d->added = 1;
        d->bhn.ehn_Object = obj, d->bhn.ehn_Class = d->ehn.ehn_Class, d->bhn.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY; /* RAWKEY: Esc */
        DoMethod(d->bar, MUIM_Window_AddEventHandler, (ULONG)&d->bhn);
        DoMethod(d->bar, MUIM_Notify, MUIA_Window_Open, FALSE, (ULONG)obj, 1, UPM_Menustrip_Check);
    }
    SetAttrs(d->bar, MUIA_Window_Screen, (ULONG)w->WScreen, MUIA_Window_LeftEdge, 0UL, MUIA_Window_TopEdge, 0UL, TAG_DONE);
    set(d->bar, MUIA_Window_Activate, FALSE); /* the held button stays with this window; a click-style release activates the bar */
    set(d->bar, MUIA_Window_Open, TRUE);
    open = 0, bw = NULL;
    GetAttr(MUIA_Window_Open, d->bar, &open);
    GetAttr(MUIA_Window_Window, d->bar, (ULONG*)&bw);
    d->trap.win[1] = bw;
    UPMS("toggle branch=open result open=%ld bw=%lx cur=%ld", open, (ULONG)bw, (ULONG)d->cur);
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
    struct Window *w = im->IDCMPWindow != NULL ? im->IDCMPWindow : _window(obj), *bw = NULL; /* the strip's window or the bar (bhn) */
    int old = d->cur, up = im->Class == IDCMP_MOUSEBUTTONS, x = w->LeftEdge + im->MouseX, y = w->TopEdge + im->MouseY;
    int dx = x > d->px ? x - d->px : d->px - x, dy = y > d->py ? y - d->py : d->py - y;
    Object* o;
    GetAttr(MUIA_Window_Window, d->bar, (ULONG*)&bw);
    if (bw == NULL) { /* the bar closed under the hold (Amiga+key, another window) */
        d->cur = -2;
        return 0;
    }
    switch (mui_aa_strip_track(&d->cur, aamenustrip_hit(d, bw, x, y), up, dx > dy ? dx : dy)) {
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
    case MUI_AA_STRIP_CANCEL: /* held and moved but never reached a title (or the moves were not seen): release elsewhere cancels */
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

/* Open the menu dir (-1 / +1) from menu from (-1 none) with its popup active, then close from. Disabled menus are skipped. */
static void aamenustrip_key_open(aamenustrip_data* d, int from, int dir) {
    Object *st = aamenustrip_head(d), *o;
    ULONG dis_mask = 0;
    int n = 0, j;
    while (st != NULL && (o = NextObject((APTR)&st)) != NULL) {
        ULONG dis = 0;
        GetAttr(MUIA_Disabled, o, &dis);
        dis_mask |= dis && n < 32 ? 1UL << n : 0;
        n++;
    }
    j = mui_aa_strip_step(from, dir, n, dis_mask);
    if (j < 0 || j == from) {
        return;
    }
    if ((o = aamenustrip_nth(d, j)) != NULL) {
        set(o, UPA_Menu_Open, UPV_Menu_Open_Keys);
    }
    if ((o = aamenustrip_nth(d, from)) != NULL) {
        set(o, UPA_Menu_Open, FALSE);
    }
}

/* The bar lost activation or a menu popup closed: an inactive bar with no open menu goes away.
 * The bar closed: if the activation was in the bar or in a closed popup, it goes back to the strip's window. */
static void aamenustrip_check(Object* obj, aamenustrip_data* d) {
    struct Window *bw = NULL, *aw, *w;
    Object *st = aamenustrip_head(d), *o;
    ULONG open = 0, cancel = 0;
    GetAttr(MUIA_Window_Window, d->bar, (ULONG*)&bw);
    GetAttr(MUIA_Window_Open, d->bar, &open);
    UPMS("check open=%ld bw=%lx closing=%ld cur=%ld", open, (ULONG)bw, (ULONG)d->closing, (ULONG)d->cur);
    /* shortcut: stale from CloseWindow until this notify (same set call); the handler only compares it with the active window.
     * Upgrade: clear it before the close if a wrong window ever shows up trapped. */
    d->trap.win[1] = open ? bw : NULL;
    if (!open) {
        d->closing = 0;
        aw = ((struct IntuitionBase*)(void*)IntuitionBase)->ActiveWindow; /* a pointer compare only: no LockIBase */
        w = aa_live(obj) ? _window(obj) : NULL;
        /* bw: the notify may run before the Intuition window closed. A borderless window on the strip window's IDCMP port is one of our
         * menu popups. shortcut: assumes MUI gives all windows of one application one UserPort; upgrade to a popup list if it does not */
        if (w != NULL && aw != w && (aw == NULL || aw == bw || ((aw->Flags & WFLG_BORDERLESS) && aw->UserPort == w->UserPort))) {
            d->act_win = _win(obj);
            d->act_tok = DoMethod(_app(obj), MUIM_Application_PushMethod, (ULONG)d->act_win, 3, MUIM_Set, MUIA_Window_Activate, TRUE);
            UPMS("check closed: reactivate app window, active was %lx", (ULONG)aw);
        }
        return;
    }
    if (bw == NULL) {
        return;
    }
    while (st != NULL && (o = NextObject((APTR)&st)) != NULL) {
        ULONG v = 0, c = 0;
        GetAttr(UPA_Menu_Open, o, &v);
        if (v) {
            UPMS("check keep: menu open %ld", 0UL);
            return;
        }
        GetAttr(UPA_Menu_Cancelled, o, &c);
        cancel |= c;
    }
    /* the Intuition flag: MUIA_Window_Activate lags behind inside a notification. cur > -2: the right button is held, the bar is
     * inactive on purpose and the release (aamenustrip_drag) decides; a check in between must not close the bar just opened */
    if (!cancel && ((bw->Flags & WFLG_WINDOWACTIVE) || d->cur > -2)) {
        UPMS("check keep: active=%ld cur=%ld", (ULONG)(bw->Flags & WFLG_WINDOWACTIVE), (ULONG)d->cur);
        return;
    }
    UPMS("check branch=close cancel=%ld", cancel);
    aamenustrip_close(obj, d);
}

static void aamenustrip_add(Object* obj, aamenustrip_data* d, Object* menu) {
    DoMethod(d->row, OM_ADDMEMBER, (ULONG)menu);
    DoMethod(menu, MUIM_Notify, UPA_Menu_Chosen, MUIV_EveryTime, (ULONG)obj, 3, UPM_Menustrip_Picked, (ULONG)menu, MUIV_TriggerValue);
    DoMethod(menu, MUIM_Notify, UPA_Menu_Open, FALSE, (ULONG)obj, 1, UPM_Menustrip_Check);
    DoMethod(menu, MUIM_Notify, UPA_Menu_Step, MUIV_EveryTime, (ULONG)obj, 3, UPM_Menustrip_Step, (ULONG)menu, MUIV_TriggerValue);
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
        DoMethod(menu, MUIM_KillNotifyObj, UPA_Menu_Step, (ULONG)obj);
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
    d->trap.win[0] = w;
    if (im->Class != IDCMP_MOUSEMOVE || !(w->Flags & WFLG_RMBTRAP)) {
        UPMS("ev class=%lx code=%lx win=%lx trap=%ld", (ULONG)im->Class, (ULONG)im->Code, (ULONG)im->IDCMPWindow, (ULONG)((w->Flags & WFLG_RMBTRAP) != 0));
    }
    if (!(w->Flags & WFLG_RMBTRAP)) { /* without it Intuition keeps the right button for its own menus */
        Forbid();
        w->Flags |= WFLG_RMBTRAP;
        Permit();
        UPMS("event: RMBTRAP was off on %lx", (ULONG)w);
    }
    if (im->Class == IDCMP_MOUSEBUTTONS && (im->Code == MENUDOWN || im->Code == MENUUP)) {
        UPMS("event %s win=%lx strip-win=%lx enabled=%ld cur=%ld closing=%ld", (ULONG)(im->Code == MENUDOWN ? "MENUDOWN" : "MENUUP"),
             (ULONG)im->IDCMPWindow, (ULONG)w, (ULONG)d->enabled, (ULONG)d->cur, (ULONG)d->closing);
    }
    if (!d->enabled) {
        return 0;
    }
    if (im->Class == IDCMP_RAWKEY && im->Code == 0x45) { /* Esc in the active bar (bhn) with no popup open closes the bar */
        struct Window* bw = NULL;
        GetAttr(MUIA_Window_Window, d->bar, (ULONG*)&bw);
        if (bw != NULL && im->IDCMPWindow == bw) {
            ULONG v = 0;
            st = aamenustrip_head(d);
            while (st != NULL && (o = NextObject((APTR)&st)) != NULL && !v) {
                GetAttr(UPA_Menu_Open, o, &v);
            }
            if (!v) {
                aamenustrip_close(obj, d);
            }
            return MUI_EventHandlerRC_Eat;
        }
    }
    if (im->Class == IDCMP_RAWKEY && (im->Code & ~IECODE_UP_PREFIX) == 0x67) { /* Right Amiga: a tap opens or closes the bar */
        if (!(im->Code & IECODE_UP_PREFIX)) {
            d->tap = 1;
            return 0;
        }
        if (d->tap) {
            ULONG open = 0;
            d->tap = 0;
            GetAttr(MUIA_Window_Open, d->bar, &open);
            aamenustrip_toggle(obj, d);
            if (!open) {
                d->cur = -2; /* no button hold */
                aamenustrip_key_open(d, -1, 1);
            }
            return MUI_EventHandlerRC_Eat;
        }
        return 0;
    }
    if ((im->Class == IDCMP_RAWKEY && !(im->Code & IECODE_UP_PREFIX)) || im->Class == IDCMP_MOUSEBUTTONS) {
        d->tap = 0; /* Amiga+key or Amiga+click: no tap */
    }
    if (im->Class == IDCMP_MOUSEBUTTONS && im->Code == MENUDOWN) {
        if (im->IDCMPWindow != NULL) {
            d->px = im->IDCMPWindow->LeftEdge + im->MouseX, d->py = im->IDCMPWindow->TopEdge + im->MouseY;
        }
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
    Object *row, *line, *card, *bar;
    if ((row = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_HorizWeight, 0UL, TAG_DONE)) == NULL) {
        return 0;
    }
    /* the titles at the left, a filler takes the rest of the screen width; aamenustrip_head walks row only */
    line = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 0UL, MUIA_Group_Child, (ULONG)row, MUIA_Group_Child,
                         (ULONG)MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE), TAG_DONE);
    if (line == NULL) {
        MUI_DisposeObject(row);
        return 0;
    }
    card = MUI_NewObject((char*)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Popup, UPA_Panel_Popup, (ULONG)TRUE, MUIA_Group_Child, (ULONG)line, TAG_DONE);
    if (card == NULL) {
        card = line;
    }
    bar = MUI_NewObject((char*)MUIC_Window, MUIA_Window_Borderless, TRUE, MUIA_Window_DragBar, FALSE, MUIA_Window_CloseGadget, FALSE, MUIA_Window_SizeGadget, FALSE,
                        MUIA_Window_DepthGadget, FALSE, MUIA_Window_NoMenus, TRUE, MUIA_Window_Width, (ULONG)MUIV_Window_Width_Screen(100), MUIA_Window_RootObject, (ULONG)card,
                        TAG_DONE);
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
        aamenustrip_trap_add(d);
        return TRUE;
    case MUIM_ConnectParent:
        DoSuperMethodA(cl, obj, msg); /* copies the parent's global info */
        d = INST_DATA(cl, obj);
        d->app = muiGlobalInfo(obj) != NULL ? _app(obj) : NULL;
        return TRUE;
    case MUIM_DisconnectParent:
        /* The last point where the application is known live: removal from the window or the window from the application always passes
         * here (Group/Window forward it), and a dispose by the application frees its own queue. A pick still queued for a strip being
         * removed is dropped. The stored pointer, not _app(obj): after the window left the application and the application was
         * disposed, removing the strip from its detached group sends DisconnectParent again with a stale _app (OpenMUI group.c:1774). */
        d = INST_DATA(cl, obj);
        if (d->app != NULL) {
            DoMethod(d->app, MUIM_Application_UnpushMethod, (ULONG)obj, 0, 0);
            DoMethod(d->app, MUIM_Application_UnpushMethod, (ULONG)d->bar, 0, 0);
            /* The window is not the strip's own: unpush by token, not by (window, MUIM_Set), which would drop other parties' pushes.
             * shortcut: a token already run and freed can be reused by another push to the same window; that one is dropped too, which
             * only happens while the strip leaves the window. Upgrade: clear act_tok from a pushed method that runs first. */
            if (d->act_tok != 0) {
                DoMethod(d->app, MUIM_Application_UnpushMethod, (ULONG)d->act_win, d->act_tok, MUIM_Set);
                d->act_tok = 0;
            }
            d->app = NULL;
        }
        break;
    case MUIM_Cleanup:
        d = INST_DATA(cl, obj);
        aamenustrip_trap_rem(d); /* before the windows close */
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
        if (d->added) { /* no reactivation of a window that is going away */
            DoMethod(d->bar, MUIM_KillNotifyObj, MUIA_Window_Open, (ULONG)obj);
            DoMethod(d->bar, MUIM_Window_RemEventHandler, (ULONG)&d->bhn);
        }
        set(d->bar, MUIA_Window_Open, FALSE);
        d->closing = 0;
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
    case UPM_Menustrip_Picked: /* m[1] the menu, m[2] its UPA_Menu_Chosen (-1 no pick) */
        d = INST_DATA(cl, obj);
        if ((LONG)m[2] >= 0) {
            Object *st = aamenustrip_head(d), *o;
            ULONG i = 0;
            while (st != NULL && (o = NextObject((APTR)&st)) != NULL && o != (Object*)m[1]) {
                i++;
            }
            /* Deferred: this runs inside the popup list's notify or HandleEvent, and an application notify on the Trigger may close
             * the windows (Iconify), whose Cleanup disposes that popup. Pushed before the close, so the pick runs first. */
            if (aa_live(obj)) {
                DoMethod(_app(obj), MUIM_Application_PushMethod, (ULONG)obj, 3, MUIM_Set, UPA_Menustrip_Trigger, (i << 8) | m[2]);
            } else {
                SetAttrs(obj, UPA_Menustrip_Trigger, (i << 8) | m[2], TAG_DONE); /* notifies */
            }
        }
        aamenustrip_close(obj, d); /* a pick ends the menu; the bar's Open FALSE notify gives the activation back */
        return 0;
    case UPM_Menustrip_Check:
        aamenustrip_check(obj, INST_DATA(cl, obj));
        return 0;
    case UPM_Menustrip_Step: /* m[1] the menu, m[2] its UPA_Menu_Step: -1 Left, +1 Right */
        d = INST_DATA(cl, obj);
        {
            Object *st = aamenustrip_head(d), *o = NULL;
            int i = 0;
            while (st != NULL && (o = NextObject((APTR)&st)) != NULL && o != (Object*)m[1]) {
                i++;
            }
            if (o != NULL) {
                aamenustrip_key_open(d, i, (LONG)m[2] < 0 ? -1 : 1);
            }
        }
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
