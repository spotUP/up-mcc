/* aa_visible, aa_redraw, aa_inside: clipping and hit tests in MUI geometry only, no draw code. Own file so a class library compiles it
 * itself (MCC_CLIENT_SOURCES) next to lib/classlib_draw_shims.c instead of pulling mui_aa_draw.o, and with it the whole draw layer, from
 * the archive. */
#include "mui_aa_draw.h"

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
