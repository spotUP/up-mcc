/* Selecta_AAPanel (I667e, design section 3.8). MUI 3.8: MUI_CreateCustomClass on Group, same dispatcher pattern as
 * mui_aatext_class.c. The group carries the role colour as its MUIA_Background (children without a background of their own
 * inherit it). The Group super fills every gap between the children with that background, the square face included, so the class
 * paints after the super: only the corner wedges outside mui_aa_panel_radius, in the colour it sits on (bg_rgb), through aa_round_corners;
 * on truecolour it first paints the face in exact RGB around the inner group (I685w). No hardcoded colour, no ObtainBestPen. */
#include "mui_aapanel_class.h"
#include "mui_aa_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/intuition.h>
#define LayersBase aapanel_LayersBase /* own name, as mui_gfxbase.h does for GfxBase: opened only for a grip drag */
#include <proto/layers.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "map/mui_theme.h"
#include "mui_aa_draw.h"
#include "map/mui_aa_paint.h"
#include "map/mui_aascr.h"

extern struct Library* MUIMasterBase;
struct Library* LayersBase;

typedef struct aapanel_data {
    int role;
    int flat_tl;    /* UPA_Panel_FlatTopLeft: square top left corner */
    int popup;      /* I1349: root of a borderless popup window: square corners, no card pad */
    int fixed;      /* > 0: the art frame, no pad */
    Object* inner;  /* I685e5: the plain vertical group that holds the program's children */
    int grip;       /* UPA_Panel_SizeGrip: the root of a borderless window draws a size grip in its bottom right corner */
    int shown;      /* MUIM_Show seen, MUIM_Hide not yet (AA_SHOWN_CASES); outside aa_ctx, whose ABI 2 layout is frozen */
    int drag;       /* the grip is held: sx, sy the mouse at the press; ox, oy the window on the screen; rw, rh the outline drawn */
    int sx, sy, ox, oy, rw, rh;
    struct Screen* scr; /* the screen the outline is drawn on */
    int hooked;         /* fill is the window layer's backfill hook, old the one it replaced (LAYERS_BACKFILL is NULL) */
    struct Hook fill, *old;
    struct MUI_EventHandlerNode ehn;
    aa_ctx aa;
} aapanel_data;

static struct MUI_CustomClass* g_class;
static uint32_t g_made;
static struct Hook g_layout_hook; /* I685e5: one hook for every panel */
static char g_bg_spec[MUI_AA_PANEL_ROLES][40]; /* MUIA_Background strings; MUI keeps the pointer, so they live for the process */

/* I685e5: the pad comes from the screen the panel opened on (d->aa.aspect_q8, set in aa_setup), so it cannot be a creation-time
 * inner offsets (init-only in MUI 3.8, and no DrawInfo exists then). The layout hook adds it around the inner group. */
static void aapanel_pad(const aapanel_data* d, int* px, int* py) {
    *px = 0;
    *py = 0;
    if (d->fixed <= 0 && !d->popup) {
        mui_aa_panel_pad(d->aa.tok, d->aa.aspect_q8, px, py);
        mui_aa_panel_role_pad(d->role, px, py);
    }
}

static ULONG aapanel_layout(struct Hook* hook, Object* obj, struct MUI_LayoutMsg* lm) {
    aapanel_data* d = INST_DATA(g_class->mcc_Class, obj);
    int px, py;
    (void)hook;
    aapanel_pad(d, &px, &py);
    switch (lm->lm_Type) {
    case MUILM_MINMAX: {
        Object* in = d->inner;
        if (in == NULL) {
            return 0;
        }
        lm->lm_MinMax.MinWidth = (WORD)(_minwidth(in) + 2 * px);
        lm->lm_MinMax.MinHeight = (WORD)(_minheight(in) + 2 * py);
        lm->lm_MinMax.DefWidth = (WORD)(_defwidth(in) + 2 * px);
        lm->lm_MinMax.DefHeight = (WORD)(_defheight(in) + 2 * py);
        lm->lm_MinMax.MaxWidth = _maxwidth(in) >= MUI_MAXMAX ? MUI_MAXMAX : (WORD)(_maxwidth(in) + 2 * px);
        lm->lm_MinMax.MaxHeight = _maxheight(in) >= MUI_MAXMAX ? MUI_MAXMAX : (WORD)(_maxheight(in) + 2 * py);
        {
            /* A panel without children is a filler: an empty group reports max 0, which narrowed every Vert group the panel sat in. */
            struct List* kids = NULL;
            GetAttr(MUIA_Group_ChildList, in, (ULONG*)&kids);
            if (kids == NULL || kids->lh_Head == (struct Node*)&kids->lh_Tail) {
                lm->lm_MinMax.MaxWidth = MUI_MAXMAX;
                lm->lm_MinMax.MaxHeight = MUI_MAXMAX;
            }
        }
        return 0;
    }
    case MUILM_LAYOUT:
        if (d->inner != NULL) {
            MUI_Layout(d->inner, px, py, lm->lm_Layout.Width - 2 * px, lm->lm_Layout.Height - 2 * py, 0);
        }
        return TRUE;
    }
    return (ULONG)MUILM_UNKNOWN;
}

/* I685w run 6: the program's own row and column boxes (plain MUI Groups) inside the panel take the panel gap across and half of it
 * down, else they keep MUI's prefs spacing (2 px) and the card packs tight. The full gap down made the UP-Demo minimum taller than
 * the 600 row rig screen (the window did not open). A UP object, a nested panel included, keeps its own spacing. */
static void aapanel_space(Object* grp, int gx, int gy, int depth) {
    struct List* kids = NULL;
    Object *state, *child;
    struct IClass* group_cl = MUI_GetClass((char*)MUIC_Group);
    SetAttrs(grp, MUIA_Group_HorizSpacing, (ULONG)gx, MUIA_Group_VertSpacing, (ULONG)gy, TAG_DONE);
    GetAttr(MUIA_Group_ChildList, grp, (ULONG*)&kids);
    if (kids == NULL || depth >= 8 || group_cl == NULL) {
        return;
    }
    state = (Object*)kids->lh_Head;
    while ((child = NextObject(&state)) != NULL) {
        ULONG keep = 0;
        GetAttr(MUIA_UserData, child, &keep);
        if (OCLASS(child) == group_cl && keep != UPV_Panel_KeepSpacing) { /* I1371: a row that set its own spacing keeps it */
            aapanel_space(child, gx, depth == 0 ? gy / 2 : gy, depth + 1);
        }
    }
}

/* The size grip square, bottom right of the window's inner area (the MUI window margin and the shell's right band: right of the
 * footer buttons). 12 px at 1:1, rows scaled to the pixel aspect. False when the panel is not the grip root of an open window. */
#define AAPANEL_GRIP_PX 12
static int aapanel_grip_box(aapanel_data* d, Object* obj, struct Window** wp, int* x0, int* y0, int* x1, int* y1) {
    ULONG wptr = 0;
    struct Window* w;
    if (!d->grip || !d->shown || !aa_live(obj)) {
        return 0;
    }
    GetAttr(MUIA_Window_Window, _win(obj), &wptr);
    if (wptr == 0) {
        return 0;
    }
    w = (struct Window*)wptr;
    *x1 = w->Width - w->BorderRight - 1;
    *y1 = w->Height - w->BorderBottom - 1;
    *x0 = *x1 - AAPANEL_GRIP_PX + 1;
    *y0 = *y1 - up_rows(AAPANEL_GRIP_PX, d->aa.aspect_q8) + 1;
    *wp = w;
    return 1;
}

/* The grip: a triangle of six square dots in the muted text colour, the corner dot last, on the face the panel paints. */
static void aapanel_grip_paint(aapanel_data* d, Object* obj) {
    struct Window* w;
    int x0, y0, x1, y1, i, j, dw = 2, dh = up_rows(2, d->aa.aspect_q8), step = 4, rstep = up_rows(4, d->aa.aspect_q8);
    uint32_t ink = d->aa.tok[SEL_GUI_TOKEN_TEXT_MUTED];
    if (!aapanel_grip_box(d, obj, &w, &x0, &y0, &x1, &y1)) {
        return;
    }
    x1 -= 1; /* one px off the window edge, as the dots of a size gadget sit */
    y1 -= up_rows(1, d->aa.aspect_q8);
    for (j = 0; j < 3; j++) {         /* row from the bottom */
        for (i = 0; i <= 2 - j; i++) { /* dot from the right: 3, 2, 1 per row */
            int rx = x1 - i * step, by = y1 - j * rstep;
            aa_rect(&d->aa, _rp(obj), rx - dw + 1, by - dh + 1, rx, by, ink);
        }
    }
}

/* The drag outline: a one px frame of the new window box, complemented on the screen's own RastPort (a copy, so the screen's
 * draw mode stays as it is), as Intuition's size gadget draws it. Four strips that do not overlap, so drawing it twice erases it.
 * shortcut: the layers are locked only while one frame is drawn, not across the drag (the input.device task must stay free to
 * deliver the release); a window of another task that repaints under the frame mid-drag can leave a trace. */
static void aapanel_outline(aapanel_data* d) {
    struct RastPort rp;
    int x0 = d->ox, y0 = d->oy, x1 = d->ox + d->rw - 1, y1 = d->oy + d->rh - 1;
    rp = d->scr->RastPort;
    SetDrMd(&rp, COMPLEMENT);
    if (LayersBase != NULL) {
        LockLayers(&d->scr->LayerInfo);
    }
    RectFill(&rp, x0, y0, x1, y0);
    RectFill(&rp, x0, y1, x1, y1);
    if (y1 - y0 > 1) {
        RectFill(&rp, x0, y0 + 1, x0, y1 - 1);
        RectFill(&rp, x1, y0 + 1, x1, y1 - 1);
    }
    if (LayersBase != NULL) {
        UnlockLayers(&d->scr->LayerInfo);
    }
}

/* End of a drag: the outline erased, then the window takes the outline's size when w is given (the release) and differs. */
static void aapanel_drag_end(aapanel_data* d, struct Window* w) {
    if (!d->drag) {
        return;
    }
    d->drag = 0;
    aapanel_outline(d);
    if (w != NULL && (d->rw != w->Width || d->rh != w->Height)) {
        ChangeWindowBox(w, w->LeftEdge, w->TopEdge, d->rw, d->rh);
    }
}

/* Drag of the grip: an outline of the new box follows the mouse, kept in the root object's MUI limits plus the window margin, and
 * on screen; the window is resized once, on the release, and MUI lays it out again on IDCMP_NEWSIZE. Esc, the menu button or the
 * window going inactive cancels. */
static ULONG aapanel_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aapanel_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    struct Window* w;
    int x0, y0, x1, y1;
    if (im == NULL || !aapanel_grip_box(d, obj, &w, &x0, &y0, &x1, &y1)) {
        aapanel_drag_end(d, NULL);
        return 0;
    }
    if (im->Class == IDCMP_INACTIVEWINDOW) {
        aapanel_drag_end(d, NULL);
        return 0;
    }
    if (im->Class == IDCMP_MOUSEBUTTONS) {
        if (im->Code == SELECTDOWN && !d->drag && im->MouseX >= x0 && im->MouseX <= x1 && im->MouseY >= y0 && im->MouseY <= y1) {
            d->drag = 1;
            d->sx = im->MouseX, d->sy = im->MouseY;
            d->ox = w->LeftEdge, d->oy = w->TopEdge;
            d->rw = w->Width, d->rh = w->Height;
            d->scr = w->WScreen;
            aapanel_outline(d);
            return MUI_EventHandlerRC_Eat;
        }
        if (d->drag && (im->Code == SELECTUP || im->Code == MENUDOWN)) {
            aapanel_drag_end(d, im->Code == SELECTUP ? w : NULL);
            return MUI_EventHandlerRC_Eat;
        }
        return 0;
    }
    if (im->Class == IDCMP_RAWKEY && d->drag && im->Code == 0x45u /* Esc down */) {
        aapanel_drag_end(d, NULL);
        return MUI_EventHandlerRC_Eat;
    }
    if (im->Class == IDCMP_MOUSEMOVE && d->drag) {
        int ew = w->Width - _width(obj), eh = w->Height - _height(obj); /* the window margin around the root object */
        int nw = mui_aa_grip_size(w->Width, im->MouseX - d->sx, _minwidth(obj) + ew, _maxwidth(obj) >= MUI_MAXMAX ? 32767 : _maxwidth(obj) + ew,
                                  w->WScreen->Width - w->LeftEdge);
        int nh = mui_aa_grip_size(w->Height, im->MouseY - d->sy, _minheight(obj) + eh, _maxheight(obj) >= MUI_MAXMAX ? 32767 : _maxheight(obj) + eh,
                                  w->WScreen->Height - w->TopEdge);
        if (nw != d->rw || nh != d->rh) {
            aapanel_outline(d);
            d->rw = nw, d->rh = nh;
            aapanel_outline(d);
        }
        return MUI_EventHandlerRC_Eat;
    }
    return 0;
}

/* The window layer's backfill: the area a resize exposes is filled with the face at once. Layers' default fills it with pen 0,
 * which showed until MUI's relayout reached the new strip: the flash on the grip's release. Called by layers in the task that
 * sizes the window (input.device for ChangeWindowBox), rp without a layer, bounds in rp's bitmap. */
struct aapanel_fill_msg {
    struct Layer* layer;
    struct Rectangle bounds;
    LONG ox, oy;
};
static ULONG aapanel_fill(struct Hook* h, struct RastPort* rp, struct aapanel_fill_msg* m) {
    aapanel_data* d = (aapanel_data*)h->h_Data;
    aa_rect(&d->aa, rp, m->bounds.MinX, m->bounds.MinY, m->bounds.MaxX, m->bounds.MaxY, mui_aa_panel_rgb(d->aa.tok, d->role));
    return 0;
}

/* MUIM_Show installs the hook on the open window's layer, MUIM_Hide (the window still open) puts the old one back. */
static void aapanel_show(aapanel_data* d, Object* obj, int show) {
    d->shown = show;
    if (!d->grip || LayersBase == NULL || _window(obj) == NULL || show == d->hooked) {
        return;
    }
    if (show) {
        d->fill.h_Entry = (__typeof__(d->fill.h_Entry))(void (*)(void))HookEntry;
        d->fill.h_SubEntry = (__typeof__(d->fill.h_SubEntry))(void (*)(void))aapanel_fill;
        d->fill.h_Data = d;
        d->old = InstallLayerHook(_window(obj)->WLayer, &d->fill);
    } else {
        InstallLayerHook(_window(obj)->WLayer, d->old);
    }
    d->hooked = show;
}

static ULONG aapanel_setup(struct IClass* cl, Object* obj, Msg msg) {
    aapanel_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    if (d->inner != NULL) { /* I685w run 6: the gap between children is its own rung of the spacing scale, not the card pad */
        int gx, gy;
        up_panel_gap(mui_aa_panel_spacing(d->aa.tok), d->aa.aspect_q8, d->role == MUI_AA_PANEL_PANEL, &gx, &gy);
        aapanel_space(d->inner, gx, gy, 0);
    }
    if (d->grip) {
        aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_TEXT_MUTED]);
        aa_hold(&d->aa, mui_aa_panel_rgb(d->aa.tok, d->role)); /* the backfill hook's face on a palette screen */
        if (LayersBase == NULL) { /* the outline locks the screen's layers; the backfill hook is installed on the window's layer */
            LayersBase = OpenLibrary((CONST_STRPTR) "layers.library", 39);
        }
        d->drag = 0;
        d->ehn.ehn_Priority = 0;
        d->ehn.ehn_Flags = 0;
        d->ehn.ehn_Object = obj;
        d->ehn.ehn_Class = cl;
        d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW;
        DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    }
    return TRUE;
}

static ULONG aapanel_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aapanel_data* d = INST_DATA(cl, obj);
    if (d->grip) {
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
        aapanel_drag_end(d, NULL);
        if (LayersBase != NULL) {
            CloseLibrary(LayersBase);
            LayersBase = NULL;
        }
    }
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

/* I685w run 6: the rectangles the face must not cover. A plain MUI Group (the program's own row and column boxes) fills its gaps
 * with MUI's pen-snapped copy of the inherited background, a second colour inside one card; so the walk goes through plain groups
 * (only the shown page of a page group) and keeps their leaves as holes. A UP object, a nested panel included, paints itself. */
/* I1355: *full is set when a leaf did not fit in MUI_AA_PAINT_MAX_HOLES; the caller must not paint the face with a short list
 * (the face covered every child past the cap). max_depth bounds the walk through plain groups. */
static int aapanel_holes(Object* grp, int* holes, int n, int depth, int max_depth, int* full) {
    struct List* kids = NULL;
    Object *state, *child;
    static struct IClass *group_cl, *rect_cl;
    LONG page = -1;
    ULONG pm = 0;
    if (group_cl == NULL) {
        group_cl = MUI_GetClass((char*)MUIC_Group);
        rect_cl = MUI_GetClass((char*)MUIC_Rectangle);
    }
    GetAttr(MUIA_Group_ChildList, grp, (ULONG*)&kids);
    if (kids == NULL) {
        return n;
    }
    if (GetAttr(MUIA_Group_PageMode, grp, &pm) && pm) {
        ULONG ap = 0;
        GetAttr(MUIA_Group_ActivePage, grp, &ap);
        page = (LONG)ap;
    }
    state = (Object*)kids->lh_Head;
    {
        LONG i = 0;
        while (!*full && (child = NextObject(&state)) != NULL) {
            ULONG shown = TRUE;
            if (page >= 0 && i++ != page) {
                continue;
            }
            GetAttr(MUIA_ShowMe, child, &shown);
            if (!shown) {
                continue; /* D9: a hidden child (the footer Cancel button) keeps its last box; as a hole it left a stale patch beside Back */
            }
            if (rect_cl != NULL && OCLASS(child) == rect_cl) {
                continue; /* a plain Rectangle is a spacer that paints nothing; as a hole it left MUI's darker snapped pen in the card */
            }
            if (depth < max_depth && group_cl != NULL && OCLASS(child) == group_cl) {
                n = aapanel_holes(child, holes, n, depth + 1, max_depth, full);
                continue;
            }
            if (n >= MUI_AA_PAINT_MAX_HOLES) {
                *full = 1;
                break;
            }
            holes[4 * n] = _left(child);
            holes[4 * n + 1] = _top(child);
            holes[4 * n + 2] = _width(child);
            holes[4 * n + 3] = _height(child);
            n++;
        }
    }
    return n;
}

static ULONG aapanel_paint(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aapanel_data* d = INST_DATA(cl, obj);
    int rad = d->popup ? 0 : mui_aa_panel_radius(d->aa.tok, d->aa.font_px); /* I1349: a popup window is a rectangle */
    ULONG r;
    /* The face first, around the children's new boxes (the super fills nothing: FillArea FALSE): painted after them, the gaps and
     * the margin ring kept the old layout's pixels for the whole child pass of a relayout, a flicker on every resize. */
    if (msg->flags & MADF_DRAWOBJECT) {
        if (d->aa.tc) { /* I685w: MUI's background spec snaps to a pen; paint the face in exact RGB around the children, whatever MUI filled */
            int holes[MUI_AA_PAINT_MAX_HOLES * 4], n = 0, full = 0;
            Object* grp = d->inner != NULL ? d->inner : obj;
            n = aapanel_holes(grp, holes, 0, 0, 8, &full);
            if (full) { /* I1355: too many leaves; the direct children as holes (gaps inside plain groups keep MUI's fill) */
                full = 0;
                n = aapanel_holes(grp, holes, 0, 0, 0, &full);
            }
            {   /* I685w: the window margin around the root object is filled by MUI with the snapped spec too; the root repaints the ring in exact RGB */
                ULONG root = 0, wptr = 0;
                GetAttr(MUIA_Window_RootObject, _win(obj), &root);
                GetAttr(MUIA_Window_Window, _win(obj), &wptr);
                if ((Object*)root == obj && wptr != 0) {
                    struct Window* w = (struct Window*)wptr;
                    int ix0 = w->BorderLeft, iy0 = w->BorderTop, ix1 = w->Width - w->BorderRight - 1, iy1 = w->Height - w->BorderBottom - 1;
                    int ox0 = _left(obj), oy0 = _top(obj), ox1 = _left(obj) + _width(obj) - 1, oy1 = _top(obj) + _height(obj) - 1;
                    uint32_t face = mui_aa_panel_rgb(d->aa.tok, d->role);
                    if (oy0 > iy0) aa_rect(&d->aa, _rp(obj), ix0, iy0, ix1, oy0 - 1, face);
                    if (oy1 < iy1) aa_rect(&d->aa, _rp(obj), ix0, oy1 + 1, ix1, iy1, face);
                    if (ox0 > ix0) aa_rect(&d->aa, _rp(obj), ix0, oy0, ox0 - 1, oy1, face);
                    if (ox1 < ix1) aa_rect(&d->aa, _rp(obj), ox1 + 1, oy0, ix1, oy1, face);
                }
            }
            {   /* I685w: the object's own frame and inner spacing (outer box minus the inner box) is MUI-filled with the snapped spec as well */
                int ox0 = _left(obj), oy0 = _top(obj), ox1 = _left(obj) + _width(obj) - 1, oy1 = _top(obj) + _height(obj) - 1;
                int mx0 = _mleft(obj), my0 = _mtop(obj), mx1 = _mright(obj), my1 = _mbottom(obj);
                uint32_t face = mui_aa_panel_rgb(d->aa.tok, d->role);
                if (my0 > oy0) aa_rect(&d->aa, _rp(obj), ox0, oy0, ox1, my0 - 1, face);
                if (my1 < oy1) aa_rect(&d->aa, _rp(obj), ox0, my1 + 1, ox1, oy1, face);
                if (mx0 > ox0) aa_rect(&d->aa, _rp(obj), ox0, my0, mx0 - 1, my1, face);
                if (mx1 < ox1) aa_rect(&d->aa, _rp(obj), mx1 + 1, my0, ox1, my1, face);
            }
            if (!full) { /* I1355: still over the cap: leave MUI's fill rather than cover a child */
                aa_round_fill_holes(&d->aa, _rp(obj), _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj), rad,
                                    mui_aa_panel_rgb(d->aa.tok, d->role), holes, n);
            }
        }
    }
    r = DoSuperMethodA(cl, obj, (Msg)msg);
    if (msg->flags & MADF_DRAWOBJECT) {
        aa_round_corners(&d->aa, _rp(obj), _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj), rad, d->aa.bg_rgb); /* the face the panel sits on (a card on a SURFACE panel: SURFACE, not BACKGROUND); a retheme resets it */
        aa_low_outline(&d->aa, _rp(obj), _mleft(obj), _mtop(obj), _mwidth(obj), _mheight(obj), rad);
        if (d->flat_tl && rad > 0) { /* square the top left: the face over the wedge the round corner left in the parent colour */
            aa_rect(&d->aa, _rp(obj), _mleft(obj), _mtop(obj), _mleft(obj) + rad - 1, _mtop(obj) + up_rows(rad, d->aa.aspect_q8) - 1,
                    mui_aa_panel_rgb(d->aa.tok, d->role));
        }
        aapanel_grip_paint(d, obj); /* last: over the face and the window margin ring */
    }
    return r;
}

/* A card inside an UP-Virtgroup is taller than the view. After a relayout (the Scrollgroup bar shown or hidden on a page switch)
 * MUI repaints a moved child directly, past the virtgroup's clipping MUIM_Draw, so the card painted its whole face, rounded bottom
 * included, over the footer below the view. Clip every draw to the part the parents show, as aa_redraw does for own repaints. */
static ULONG aapanel_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    int x0, y0, x1, y1;
    APTR clip;
    ULONG r;
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE)) || !aa_live(obj)) {
        return aapanel_paint(cl, obj, msg);
    }
    if (!aa_visible(obj, &x0, &y0, &x1, &y1)) {
        return 0; /* wholly outside the view: the virtgroup paints it when it scrolls in */
    }
    if (x0 <= _left(obj) && y0 <= _top(obj) && x1 >= _right(obj) && y1 >= _bottom(obj)) {
        return aapanel_paint(cl, obj, msg);
    }
    clip = MUI_AddClipping(muiRenderInfo(obj), x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    r = aapanel_paint(cl, obj, msg);
    MUI_RemoveClipping(muiRenderInfo(obj), clip);
    return r;
}

static void aapanel_hook_init(void) {
    if (g_layout_hook.h_Entry == NULL) {
        g_layout_hook.h_Entry = (__typeof__(g_layout_hook.h_Entry))(void (*)(void))HookEntry;
        g_layout_hook.h_SubEntry = (__typeof__(g_layout_hook.h_SubEntry))(void (*)(void))aapanel_layout;
    }
}

/* I685u / I685w run 6: created by name (UP-Panel.mcc, no UP_NewPanel) the object has no background tag, so MUI would paint the
 * window background. Add the role colour from the theme store, exactly what UP_NewPanel passes, and build the same shape: the
 * program's MUIA_Group_Child objects go into the inner group and the layout hook puts the card pad around it. Without the inner
 * group and the hook the pad (aapanel_pad) was never reached for a by-name panel: children sat on the card edge. */
static ULONG aapanel_new_by_name(struct IClass* cl, Object* obj, struct opSet* ops, int role) {
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
    mui_theme_bg_spec(mui_aa_panel_rgb(mui_theme_tokens(), role), g_bg_spec[role], sizeof g_bg_spec[role]);
    aapanel_hook_init();
    t = copy;
    t->ti_Tag = MUIA_Background, (t++)->ti_Data = (ULONG)g_bg_spec[role];
    t->ti_Tag = MUIA_FillArea, (t++)->ti_Data = (ULONG)FALSE; /* I685w: the draw paints the face; a MUI fill would be the snapped pen */
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
        } else if (tag->ti_Tag != MUIA_Group_Horiz && tag->ti_Tag != MUIA_Background && tag->ti_Tag != MUIA_FillArea) {
            *t++ = *tag;
        }
    }
    t->ti_Tag = TAG_DONE;
    nm.ops_AttrList = copy;
    made = DoSuperMethodA(cl, obj, (Msg)&nm); /* on failure the Group super disposes its children, the inner group with them */
    FreeVec(copy);
    if (made != 0) { /* I685w: the by-name panel has no UP_NewPanel to record the role the face is painted in */
        aapanel_data* d = (aapanel_data*)INST_DATA(cl, (Object*)made);
        d->role = role;
        d->popup = GetTagData(UPA_Panel_Popup, FALSE, ops->ops_AttrList) ? 1 : 0;
        d->flat_tl = GetTagData(UPA_Panel_FlatTopLeft, FALSE, ops->ops_AttrList) ? 1 : 0;
        d->grip = GetTagData(UPA_Panel_SizeGrip, FALSE, ops->ops_AttrList) ? 1 : 0;
        d->inner = inner;
        g_made++;
    }
    return made;
}

static ULONG __attribute__((used)) aapanel_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aapanel_dispatch(void);
const struct aa_class_desc aapanel_class_desc = {MUIC_Group, sizeof(aapanel_data), aapanel_dispatch, &g_class};
__asm__(".text\n.even\n.globl _aapanel_dispatch\n_aapanel_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aapanel_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aapanel_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW: {
        struct opSet* ops = (struct opSet*)msg;
        if (FindTagItem(MUIA_Background, ops->ops_AttrList) == NULL) {
            int role = (int)GetTagData(UPA_Panel_Role, UPV_Panel_Role_Panel, ops->ops_AttrList);
            if ((unsigned)role < MUI_AA_PANEL_ROLES) {
                return aapanel_new_by_name(cl, obj, ops, role);
            }
        }
        break;
    }
    case OM_GET: { /* I685w: children read the role to take their erase colour from the panel they sit in */
        struct opGet* og = (struct opGet*)msg;
        if (og->opg_AttrID == UPA_Panel_Role) {
            *og->opg_Storage = (ULONG)((aapanel_data*)INST_DATA(cl, obj))->role;
            return TRUE;
        }
        if (og->opg_AttrID == UPA_Panel_Self) {
            *og->opg_Storage = (ULONG)(uintptr_t)obj;
            return TRUE;
        }
        break;
    }
    case MUIM_Setup:
        return aapanel_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aapanel_cleanup(cl, obj, msg);
    case MUIM_Draw:
        return aapanel_draw(cl, obj, (struct MUIP_Draw*)msg);
    case MUIM_HandleEvent:
        return aapanel_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    case MUIM_Show:
    case MUIM_Hide:
        aapanel_show(INST_DATA(cl, obj), obj, msg->MethodID == MUIM_Show);
        break;
    case OM_ADDMEMBER:
    case OM_REMMEMBER: {
        aapanel_data* d = INST_DATA(cl, obj);
        if (d->inner != NULL) { /* I685e5: callers add to the panel, the children live in the inner group */
            return DoMethodA(d->inner, msg);
        }
        break;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* Tags: UPA_Panel_Tokens (required), UPA_Panel_Role, UPA_Panel_FixedWidth, UPA_Panel_FixedHeight. The inner group is built here (design 4.1.1). */
Object* UP_NewPanel(const struct TagItem* tags) {
    Object *o, *inner;
    const uint32_t* tokens = (const uint32_t*)GetTagData(UPA_Panel_Tokens, 0, (struct TagItem*)tags);
    int role = (int)GetTagData(UPA_Panel_Role, UPV_Panel_Role_Panel, (struct TagItem*)tags);
    int fixed_w = (int)GetTagData(UPA_Panel_FixedWidth, 0, (struct TagItem*)tags);
    int fixed_h = (int)GetTagData(UPA_Panel_FixedHeight, 0, (struct TagItem*)tags);
    if ((unsigned)role >= MUI_AA_PANEL_ROLES || tokens == NULL) {
        return NULL;
    }
    if (g_class == NULL) {
        g_class = AA_CLASS_CREATE(&aapanel_class_desc);
    }
    if (g_class == NULL) {
        return NULL;
    }
    aapanel_hook_init();
    inner = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, (ULONG)FALSE, MUIA_FillArea, (ULONG)FALSE, TAG_DONE); /* I685w: no snapped MUI fill under the children */
    if (inner == NULL) {
        return NULL;
    }
    mui_theme_bg_spec(mui_aa_panel_rgb(tokens, role), g_bg_spec[role], sizeof g_bg_spec[role]);
    o = NewObject(g_class->mcc_Class, NULL, MUIA_Frame, (ULONG)MUIV_Frame_None, MUIA_Background, (ULONG)g_bg_spec[role], MUIA_FillArea, (ULONG)FALSE,
                  MUIA_Group_Spacing, (ULONG)0, MUIA_Group_LayoutHook, (ULONG)(uintptr_t)&g_layout_hook, MUIA_Group_Child, (ULONG)(uintptr_t)inner,
                  fixed_w > 0 ? (ULONG)MUIA_FixWidth : (ULONG)TAG_IGNORE, (ULONG)fixed_w,
                  fixed_w > 0 ? (ULONG)MUIA_FixHeight : (ULONG)TAG_IGNORE, (ULONG)(fixed_h > 0 ? fixed_h : fixed_w), TAG_DONE);
    if (o == NULL) {
        MUI_DisposeObject(inner);
    } else {
        aapanel_data* d = INST_DATA(g_class->mcc_Class, o);
        d->role = role;
        d->fixed = fixed_w;
        d->inner = inner;
        g_made++;
    }
    return o;
}

uint32_t mui_aapanel_made(void) {
    return g_made;
}

void mui_aapanel_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
}
