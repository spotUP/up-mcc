/* UP-Rectangle (UP_CLS_RECTANGLE): MUI 3.8 MUIC_Rectangle in the up-mcc theme. Area subclass, same dispatcher pattern as
 * mui_aatext_class.c. It takes the MUI tags themselves (MUIA_Rectangle_HBar, _VBar, _BarTitle; I, readable with get) so a
 * MUIC_Rectangle call site only changes its class name. Draw: the super fills the parent background; on truecolour the whole
 * box is erased in the panel token (as UP-Text). A bar is one pixel in the DIVIDER token, centred across the box; a bar title is
 * TEXT_MUTED text centred on a horizontal bar, the bar left and right of it. Hidden objects never draw (AA_SHOWN_CASES). */
#include "mui_aarect_class.h"
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

#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;

#define AAR_CAP 80 /* bar title bytes kept */

typedef struct aarect_data {
    char title[AAR_CAP];
    int hbar, vbar, shown, tw, th;
    aa_ctx aa;
} aarect_data;

static struct MUI_CustomClass* g_class;

static ULONG aarect_setup(struct IClass* cl, Object* obj, Msg msg) {
    aarect_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    if (!d->aa.tc) {
        d->aa.bg_rgb = d->aa.tok[SEL_GUI_TOKEN_PANEL];
    }
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_DIVIDER]);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_TEXT_MUTED]);
    aa_hold(&d->aa, d->aa.bg_rgb);
    d->tw = d->title[0] ? aa_text_width(&d->aa, obj, d->title) : 0;
    d->th = d->title[0] ? aa_text_height(&d->aa, obj) : 0;
    return TRUE;
}

static ULONG aarect_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aarect_data* d = INST_DATA(cl, obj);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

/* MUIC_Rectangle sizes: a spacer is 0 by default and grows without limit; a bar needs its line (2 px so the centre stays on the
 * box), a title its text. Every case grows to MUI_MAXMAX, as a MUI rectangle does. */
static ULONG aarect_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aarect_data* d = INST_DATA(cl, obj);
    struct MUI_MinMax* mm;
    DoSuperMethodA(cl, obj, (Msg)msg);
    mm = msg->MinMaxInfo;
    if (d->title[0]) {
        mm->MinWidth += d->tw + 8;
        mm->DefWidth += d->tw + 8;
        mm->MinHeight += d->th;
        mm->DefHeight += d->th;
    } else if (d->hbar) {
        mm->MinHeight += 2;
        mm->DefHeight += 2;
    } else if (d->vbar) {
        mm->MinWidth += 2;
        mm->DefWidth += 2;
    }
    mm->MaxWidth = MUI_MAXMAX;
    mm->MaxHeight = MUI_MAXMAX;
    return 0;
}

static ULONG aarect_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aarect_data* d = INST_DATA(cl, obj);
    struct RastPort* rp;
    uint32_t line;
    int l, t, r, b, cy, cx;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE)) || !d->shown) {
        return 0;
    }
    if (d->aa.tc) {
        aa_erase(&d->aa, obj, _left(obj), _top(obj), _width(obj), _height(obj));
    }
    rp = _rp(obj);
    line = d->aa.tok[SEL_GUI_TOKEN_DIVIDER];
    l = _mleft(obj);
    t = _mtop(obj);
    r = _mright(obj);
    b = _mbottom(obj);
    cy = t + _mheight(obj) / 2;
    cx = l + _mwidth(obj) / 2;
    if (d->title[0]) {
        int tw = d->tw < _mwidth(obj) ? d->tw : _mwidth(obj);
        int x = cx - tw / 2;
        aa_text(&d->aa, obj, rp, x, t + (_mheight(obj) - d->th) / 2, d->title, d->aa.tok[SEL_GUI_TOKEN_TEXT_MUTED], d->aa.bg_rgb);
        if (x - 4 > l) {
            aa_rect(&d->aa, rp, l, cy, x - 4, cy, line);
        }
        if (x + tw + 4 < r) {
            aa_rect(&d->aa, rp, x + tw + 4, cy, r, cy, line);
        }
    } else if (d->hbar) {
        aa_rect(&d->aa, rp, l, cy, r, cy, line);
    } else if (d->vbar) {
        aa_rect(&d->aa, rp, cx, t, cx, b, line);
    }
    return 0;
}

static ULONG aarect_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aarect_data* d = INST_DATA(cl, obj);
    switch (msg->opg_AttrID) {
    case MUIA_Rectangle_HBar: *msg->opg_Storage = (ULONG)d->hbar; return TRUE;
    case MUIA_Rectangle_VBar: *msg->opg_Storage = (ULONG)d->vbar; return TRUE;
    case MUIA_Rectangle_BarTitle: *msg->opg_Storage = d->title[0] ? (ULONG)(uintptr_t)d->title : 0; return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aarect_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aarect_data* d;
    struct TagItem* tags = msg->ops_AttrList;
    const char* title;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)msg);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    title = (const char*)GetTagData(MUIA_Rectangle_BarTitle, 0, tags);
    if (title != NULL) {
        strncpy(d->title, title, sizeof d->title - 1);
    }
    d->hbar = GetTagData(MUIA_Rectangle_HBar, FALSE, tags) != 0 || d->title[0];
    d->vbar = !d->hbar && GetTagData(MUIA_Rectangle_VBar, FALSE, tags) != 0;
    return (ULONG)obj;
}

static ULONG __attribute__((used)) aarect_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aarect_dispatch(void);
const struct aa_class_desc aarect_class_desc = {MUIC_Area, sizeof(aarect_data), aarect_dispatch, &g_class};
__asm__(".text\n.even\n.globl _aarect_dispatch\n_aarect_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aarect_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aarect_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aarect_new(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aarect_get(cl, obj, (struct opGet*)msg);
    case MUIM_Setup:
        return aarect_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aarect_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aarect_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aarect_draw(cl, obj, (struct MUIP_Draw*)msg);
    AA_SHOWN_CASES(aarect_data)
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* The tag list is the whole interface: MUIA_Rectangle_* are read here, every other MUIA_* tag goes to the Area. */
Object* UP_NewRectangle(const struct TagItem* tags) {
    if (g_class == NULL) {
        g_class = AA_CLASS_CREATE(&aarect_class_desc);
    }
    if (g_class == NULL) {
        return NULL;
    }
    return NewObjectA(g_class->mcc_Class, NULL, (struct TagItem*)tags);
}

void mui_aarect_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
}
