/* aa_visible, aa_redraw, aa_inside, aa_pop_fit, aa_pop_push_close / aa_pop_unpush: clipping and hit tests in MUI geometry only, no draw code. Own file so a class library compiles it
 * itself (MCC_CLIENT_SOURCES) next to lib/classlib_draw_shims.c instead of pulling mui_aa_draw.o, and with it the whole draw layer, from
 * the archive. */
#include "mui_aa_draw.h"
#include "class/mui_aa_class.h" /* MUIM_Application_UnpushMethod */

#include <clib/alib_protos.h>
#include <intuition/intuitionbase.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

int aa_visible(Object* obj, int* x0, int* y0, int* x1, int* y1) {
    Object *o = obj, *p = NULL;
    int l = _left(obj), t = _top(obj), r = _right(obj), b = _bottom(obj);
    *x0 = -32768, *y0 = -32768, *x1 = 32767, *y1 = 32767;
    while (GetAttr(MUIA_Parent, o, (ULONG*)&p) && p != NULL) { /* a normal child lies inside its parent; only a virtual one sticks out */
        *x0 = _left(p) > *x0 ? _left(p) : *x0;
        *y0 = _top(p) > *y0 ? _top(p) : *y0;
        *x1 = _right(p) < *x1 ? _right(p) : *x1;
        *y1 = _bottom(p) < *y1 ? _bottom(p) : *y1;
        o = p;
        p = NULL;
    }
    return l <= *x1 && r >= *x0 && t <= *y1 && b >= *y0 && *x0 <= *x1 && *y0 <= *y1;
}

void aa_redraw(Object* obj, ULONG flags) {
    int x0, y0, x1, y1;
    APTR clip;
    if (!aa_live(obj)) {
        MUI_Redraw(obj, flags);
        return;
    }
    if (!aa_visible(obj, &x0, &y0, &x1, &y1)) {
        return; /* scrolled out of the view: the virtgroup repaints it when it comes back */
    }
    clip = MUI_AddClipping(muiRenderInfo(obj), x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    MUI_Redraw(obj, flags);
    MUI_RemoveClipping(muiRenderInfo(obj), clip);
}

int aa_inside(int shown, Object* obj, const struct IntuiMessage* im) {
    int x0, y0, x1, y1;
    return shown && im->MouseX >= _left(obj) && im->MouseX <= _right(obj) && im->MouseY >= _top(obj) && im->MouseY <= _bottom(obj) &&
           aa_visible(obj, &x0, &y0, &x1, &y1) && im->MouseX >= x0 && im->MouseX <= x1 && im->MouseY >= y0 && im->MouseY <= y1;
}

void aa_pop_fit(Object* obj, Object* popup) {
    struct Window *win = _window(obj), *pw = NULL;
    struct Screen* scr = _screen(obj);
    long x, y;
    GetAttr(MUIA_Window_Window, popup, (ULONG*)&pw);
    if (pw == NULL || win == NULL || scr == NULL) {
        return;
    }
    /* the size is only known once the window is open, so it opens below obj and moves here */
    x = mui_aa_pop_fit(pw->LeftEdge, scr->Width, pw->Width, scr->Width);
    y = mui_aa_pop_fit(pw->TopEdge, win->TopEdge + _top(obj) - 2, pw->Height, scr->Height);
    if (x != pw->LeftEdge || y != pw->TopEdge) {
        MoveWindow(pw, x - pw->LeftEdge, y - pw->TopEdge);
    }
}

static Object* aa_pop_app(Object* obj) {
    Object* app = NULL;
    if (aa_live(obj)) {
        GetAttr(MUIA_ApplicationObject, _win(obj), (ULONG*)&app);
    }
    return app;
}

void aa_pop_push_close(Object* obj, Object* popup) {
    Object* app = popup != NULL ? aa_pop_app(obj) : NULL;
    if (app != NULL) {
        DoMethod(app, MUIM_Application_PushMethod, (ULONG)popup, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    }
}

void aa_pop_unpush(Object* obj, Object* popup) {
    Object* app = popup != NULL ? aa_pop_app(obj) : NULL;
    if (app != NULL) {
        DoMethod(app, MUIM_Application_UnpushMethod, (ULONG)popup, 0, 0);
    }
}

uint32_t aa_pop_now(void) {
    ULONG s = 0, m = 0;
    CurrentTime(&s, &m);
    return (uint32_t)(s * 1000UL + m / 1000UL);
}

void aa_pop_reopen(Object* obj, Object* popup, int activate) {
    aa_pop_unpush(obj, popup);
    if (popup != NULL) {
        SetAttrs(popup, MUIA_NoNotify, TRUE, MUIA_Window_Activate, (ULONG)(activate != 0), TAG_DONE);
    }
}

static struct Window* aa_pop_active(void) {
    return ((struct IntuitionBase*)(void*)IntuitionBase)->ActiveWindow; /* a pointer compare only: no LockIBase */
}

/* True when act is the Intuition window of a popup opened from inside popup (MUIA_UserData = popup), or of one nested in that. Only
 * pointers are compared: the UserData of an application's own window is never followed. */
static int aa_pop_tree_has(Object* app, Object* popup, struct Window* act, int depth) {
    struct List* wl = NULL;
    Object *st, *w;
    ULONG u, iw;
    GetAttr(MUIA_Application_WindowList, app, (ULONG*)&wl);
    st = wl != NULL ? (Object*)wl->lh_Head : NULL;
    while (st != NULL && (w = NextObject((APTR)&st)) != NULL) {
        u = 0, iw = 0;
        GetAttr(MUIA_UserData, w, &u);
        if ((Object*)u != popup || w == popup) {
            continue;
        }
        GetAttr(MUIA_Window_Window, w, &iw);
        if ((struct Window*)iw == act || (depth < 4 && aa_pop_tree_has(app, w, act, depth + 1))) {
            return 1;
        }
    }
    return 0;
}

int aa_pop_holds_active(Object* obj, Object* popup) {
    Object* app = popup != NULL ? aa_pop_app(obj) : NULL;
    struct Window *act = aa_pop_active(), *pw = NULL;
    if (app == NULL || act == NULL) {
        return 0;
    }
    GetAttr(MUIA_Window_Window, popup, (ULONG*)&pw);
    return act == pw || aa_pop_tree_has(app, popup, act, 0);
}

void aa_pop_child_closed(Object* obj, int deact) {
    Object* owner = aa_live(obj) ? _win(obj) : NULL;
    struct Window* act = aa_pop_active();
    ULONG u = 0;
    if (owner == NULL || _window(obj) == NULL || !(_window(obj)->Flags & WFLG_BORDERLESS) || !GetAttr(MUIA_UserData, owner, &u) || u == 0) {
        return; /* obj's window is no popup */
    }
    if (!deact && (act == NULL || act == _window(obj))) {
        set(owner, MUIA_Window_Activate, TRUE);
    } else if (deact && !aa_pop_holds_active(obj, owner)) {
        set(owner, MUIA_Window_Activate, FALSE); /* runs the owner popup's own Deactivated: it closes as if it had lost the click itself */
    }
}
