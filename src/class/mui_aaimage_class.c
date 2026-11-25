/* UP-Image. Dispatcher pattern of mui_aacolorfield_class.c. The picture comes from picture.datatype (loaded at Setup, remapped to the window
 * screen, kept until Cleanup) or from a caller bitmap. It is blitted exact; the scaled copy for UPA_Image_Fit is made once per size.
 * Without datatypes.library, picture.datatype or a readable file the box shows only its outline.
 * MUIC_Bitmap / MUIC_Bodychunk: a body is unpacked into an own planar bitmap (mui_aa_body_unpack); a caller bitmap or a body with
 * SourceColors or a MappingTable is copied pixel by pixel through the pen table into a remapped bitmap; a transparent colour or a body
 * mask plane becomes a chip-RAM mask for BltMaskBitMapRastPort. All of it is built at Setup and freed at Cleanup. */
#include "mui_aaimage_class.h"
#include "mui_aa_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <datatypes/pictureclass.h>
#include <graphics/gfx.h>
#include <graphics/scale.h>
#include <graphics/view.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#define DataTypesBase up_mcc_image_DataTypesBase /* own base symbol: the library is opened per use, not by libnix */
#include <proto/datatypes.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "map/mui_aa.h"
#include "mui_aa_draw.h"

extern struct Library* MUIMasterBase;
struct Library* DataTypesBase;
static int g_dt_users; /* instances holding the shared base open */

typedef struct aaimage_data {
    char file[256];
    struct BitMap* user_bm;           /* UPA_Image_Bitmap, not owned */
    Object* dt;                       /* picture.datatype object, owned, only between Setup and Cleanup */
    struct BitMap* dt_bm;             /* the datatype's remapped bitmap, owned by dt */
    struct BitMap* scaled;            /* cache for Fit, owned */
    int sw, sh, scaled_w, scaled_h;   /* source size, cached size */
    int fit, has_lib;
    const ULONG* colors;              /* UPA_Image_SourceColors, not owned */
    const UBYTE* map;                 /* UPA_Image_MappingTable, not owned */
    const UBYTE* body;                /* UPA_Image_Body, not owned */
    struct BitMap* body_bm;           /* the unpacked body, owned */
    struct BitMap* remap_bm;          /* the remapped copy, owned */
    UBYTE* mask;                      /* mask of the drawn bitmap, owned, chip RAM */
    struct ColorMap* cm;              /* where the pens in got came from */
    int bw, bh, prec, transp, use_friend, comp, depth, masking, mbpr;
    ULONG got[8];                     /* bit i = pens[i] was obtained and is released at Cleanup */
    UBYTE pens[256];
    aa_ctx aa;
} aaimage_data;

static struct MUI_CustomClass* g_image_class;

static struct BitMap* aaimage_src(aaimage_data* d) {
    return d->remap_bm != NULL ? d->remap_bm : d->user_bm != NULL ? d->user_bm : d->body_bm != NULL ? d->body_bm : d->dt_bm;
}

static void aaimage_free_scaled(aaimage_data* d) {
    if (d->scaled != NULL) {
        WaitBlit();
        FreeBitMap(d->scaled);
        d->scaled = NULL;
    }
    d->scaled_w = d->scaled_h = 0;
}

static void aaimage_unload(aaimage_data* d) {
    aaimage_free_scaled(d);
    if (d->dt != NULL) {
        DisposeDTObject(d->dt);
        d->dt = NULL;
    }
    d->dt_bm = NULL;
    WaitBlit();
    FreeBitMap(d->remap_bm), FreeBitMap(d->body_bm); /* NULL is a no-op */
    FreeVec(d->mask);
    d->remap_bm = d->body_bm = NULL, d->mask = NULL;
    for (int i = 0; i < 256; i++) {
        if (d->got[i >> 5] & (1UL << (i & 31))) {
            ReleasePen(d->cm, (ULONG)d->pens[i]);
        }
    }
    memset(d->got, 0, sizeof d->got);
    if (d->has_lib) {
        d->has_lib = 0;
        if (--g_dt_users == 0) {
            CloseLibrary(DataTypesBase);
            DataTypesBase = NULL;
        }
    }
}

static void aaimage_size(aaimage_data* d) {
    struct BitMap* bm = aaimage_src(d);
    d->sw = d->sh = 0;
    if (bm != NULL) {
        d->sw = (int)GetBitMapAttr(bm, BMA_WIDTH);
        d->sh = (int)GetBitMapAttr(bm, BMA_HEIGHT);
        d->sw = d->bw > 0 && d->bw < d->sw ? d->bw : d->sw;
        d->sh = d->bh > 0 && d->bh < d->sh ? d->bh : d->sh;
    }
}

/* The raw picture: unpack the body, remap through the pen table, build the mask. A failed allocation leaves that step out. */
static void aaimage_raw(Object* obj, aaimage_data* d) {
    struct BitMap *src = d->user_bm, *fr = _screen(obj)->RastPort.BitMap, *draw;
    struct RastPort srp, drp;
    int w, h, x, y, i, dep, fd = (int)GetBitMapAttr(fr, BMA_DEPTH);
    if (src == NULL && d->body != NULL && d->bw > 0 && d->bh > 0 && d->depth >= 1 && d->depth <= 8) {
        d->body_bm = src = AllocBitMap((ULONG)d->bw, (ULONG)d->bh, (ULONG)d->depth, BMF_CLEAR, NULL);
    }
    if (src == NULL) {
        return;
    }
    w = (int)GetBitMapAttr(src, BMA_WIDTH), h = (int)GetBitMapAttr(src, BMA_HEIGHT);
    w = d->bw > 0 && d->bw < w ? d->bw : w, h = d->bh > 0 && d->bh < h ? d->bh : h;
    dep = (int)GetBitMapAttr(src, BMA_DEPTH);
    dep = dep > 8 ? 8 : dep;
    if (d->colors != NULL || d->map != NULL) {
        d->remap_bm = d->use_friend ? AllocBitMap((ULONG)w, (ULONG)h, (ULONG)fd, BMF_CLEAR | BMF_MINPLANES, fr)
                                    : AllocBitMap((ULONG)w, (ULONG)h, (ULONG)(fd > 8 ? 8 : fd), BMF_CLEAR, NULL);
    }
    draw = d->remap_bm != NULL ? d->remap_bm : src;
    if (d->transp >= 0 || (d->body_bm != NULL && d->masking == 1)) {
        /* shortcut: a non-standard (RTG) bitmap gets a word-aligned mask, the layout P96 and CGX read; per-plane modulo otherwise */
        d->mbpr = (GetBitMapAttr(draw, BMA_FLAGS) & BMF_STANDARD) ? draw->BytesPerRow : ((w + 15) >> 4) << 1;
        d->mask = AllocVec((ULONG)(d->mbpr * h), MEMF_CHIP | MEMF_CLEAR);
    }
    if (d->body_bm != NULL &&
        !mui_aa_body_unpack(d->body, d->comp, d->bw, d->bh, d->depth, d->masking == 1, d->body_bm->Planes, d->body_bm->BytesPerRow,
                            d->transp < 0 ? d->mask : NULL, d->mbpr)) {
        aaimage_unload(d); /* bad compression: an empty box */
        return;
    }
    InitRastPort(&srp);
    srp.BitMap = src;
    if (d->remap_bm != NULL) {
        d->cm = _screen(obj)->ViewPort.ColorMap;
        for (i = 0; i < (1 << dep); i++) {
            LONG p = d->map != NULL ? d->map[i] : ObtainBestPen(d->cm, d->colors[3 * i], d->colors[3 * i + 1], d->colors[3 * i + 2], OBP_Precision, (ULONG)d->prec, TAG_DONE);
            d->pens[i] = (UBYTE)(p < 0 ? 1 : p);
            if (d->map == NULL && p >= 0) {
                d->got[i >> 5] |= 1UL << (i & 31);
            }
        }
        InitRastPort(&drp);
        drp.BitMap = d->remap_bm;
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                SetAPen(&drp, d->pens[ReadPixel(&srp, x, y) & 0xFF]);
                WritePixel(&drp, x, y);
            }
        }
    }
    if (d->mask != NULL && d->transp >= 0) {
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                if (ReadPixel(&srp, x, y) != d->transp) {
                    d->mask[y * d->mbpr + (x >> 3)] |= (UBYTE)(0x80 >> (x & 7));
                }
            }
        }
    }
}

/* Load the file through picture.datatype; every failure leaves the box empty. */
static void aaimage_load(Object* obj, aaimage_data* d) {
    struct TagItem tl[5];
    struct BitMap* bm = NULL;
    aaimage_unload(d);
    if (d->file[0] != '\0' && DataTypesBase == NULL) {
        DataTypesBase = OpenLibrary((CONST_STRPTR) "datatypes.library", 39);
    }
    if (d->file[0] != '\0' && DataTypesBase != NULL) {
        d->has_lib = 1;
        g_dt_users++;
        tl[0].ti_Tag = DTA_SourceType, tl[0].ti_Data = DTST_FILE;
        tl[1].ti_Tag = DTA_GroupID, tl[1].ti_Data = GID_PICTURE;
        tl[2].ti_Tag = PDTA_Remap, tl[2].ti_Data = TRUE;
        tl[3].ti_Tag = PDTA_Screen, tl[3].ti_Data = (ULONG)(uintptr_t)_screen(obj);
        tl[4].ti_Tag = TAG_DONE, tl[4].ti_Data = 0;
        d->dt = NewDTObjectA((APTR)d->file, tl);
        if (d->dt != NULL) {
            DoMethod(d->dt, DTM_PROCLAYOUT, NULL, TRUE);
            GetDTAttrs(d->dt, PDTA_DestBitMap, (ULONG)(uintptr_t)&bm, TAG_DONE);
            if (bm == NULL) {
                GetDTAttrs(d->dt, PDTA_BitMap, (ULONG)(uintptr_t)&bm, TAG_DONE);
            }
            d->dt_bm = bm;
        }
        if (d->dt_bm == NULL) {
            aaimage_unload(d); /* picture.datatype missing or the file unreadable */
        }
    }
    aaimage_raw(obj, d);
    aaimage_size(d);
}

static ULONG aaimage_setup(struct IClass* cl, Object* obj, Msg msg) {
    aaimage_data* d = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_SURFACE_PRESSED]);
    aaimage_load(obj, d);
    return TRUE;
}

static ULONG aaimage_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aaimage_data* d = INST_DATA(cl, obj);
    aaimage_unload(d);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aaimage_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aaimage_data* d = INST_DATA(cl, obj);
    int w = d->sw > 0 ? d->sw : 2 * d->aa.font_px, h = d->sh > 0 ? d->sh : 2 * d->aa.font_px;
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->DefWidth += w;
    msg->MinMaxInfo->DefHeight += h;
    if (d->fit) {
        msg->MinMaxInfo->MinWidth += 8, msg->MinMaxInfo->MinHeight += 8;
        msg->MinMaxInfo->MaxWidth = msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;
    } else {
        msg->MinMaxInfo->MinWidth += w, msg->MinMaxInfo->MinHeight += h;
        msg->MinMaxInfo->MaxWidth += w, msg->MinMaxInfo->MaxHeight += h;
    }
    return 0;
}

static ULONG aaimage_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aaimage_data* d = INST_DATA(cl, obj);
    struct BitMap* src;
    struct RastPort* rp;
    int l, t, w, h, dw, dh, ox, oy, cx, cy, cw, ch;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    l = _mleft(obj), t = _mtop(obj), w = _mwidth(obj), h = _mheight(obj);
    aa_erase(&d->aa, obj, l, t, w, h);
    src = aaimage_src(d);
    if (src == NULL || d->sw <= 0 || d->sh <= 0) {
        aa_round_frame(&d->aa, rp, l, t, w, h, 0, d->aa.tok[SEL_GUI_TOKEN_SURFACE_PRESSED]);
        return 0;
    }
    mui_aa_image_fit(d->sw, d->sh, w, h, d->fit, &dw, &dh, &ox, &oy);
    if (dw != d->sw || dh != d->sh) {
        if (d->scaled == NULL || d->scaled_w != dw || d->scaled_h != dh) {
            struct BitScaleArgs bsa;
            aaimage_free_scaled(d);
            d->scaled = AllocBitMap((ULONG)dw, (ULONG)dh, GetBitMapAttr(rp->BitMap, BMA_DEPTH), BMF_MINPLANES, rp->BitMap);
            if (d->scaled == NULL) {
                return 0; /* out of memory: leave the panel background */
            }
            memset(&bsa, 0, sizeof bsa);
            bsa.bsa_SrcWidth = (UWORD)d->sw, bsa.bsa_SrcHeight = (UWORD)d->sh;
            bsa.bsa_XSrcFactor = (UWORD)d->sw, bsa.bsa_YSrcFactor = (UWORD)d->sh;
            bsa.bsa_DestWidth = (UWORD)dw, bsa.bsa_DestHeight = (UWORD)dh;
            bsa.bsa_XDestFactor = (UWORD)dw, bsa.bsa_YDestFactor = (UWORD)dh;
            bsa.bsa_SrcBitMap = src, bsa.bsa_DestBitMap = d->scaled;
            BitMapScale(&bsa);
            d->scaled_w = dw, d->scaled_h = dh;
        }
        src = d->scaled;
    }
    /* clip the centred picture to the box: a negative offset cuts the source instead */
    cx = ox < 0 ? -ox : 0, cy = oy < 0 ? -oy : 0;
    cw = (dw - cx < w ? dw - cx : w), ch = (dh - cy < h ? dh - cy : h);
    if (d->mask != NULL && src != d->scaled) { /* shortcut: a scaled picture is drawn without its mask */
        BltMaskBitMapRastPort(src, cx, cy, rp, l + (ox > 0 ? ox : 0), t + (oy > 0 ? oy : 0), cw, ch, 0xE0, d->mask);
    } else {
        BltBitMapRastPort(src, cx, cy, rp, l + (ox > 0 ? ox : 0), t + (oy > 0 ? oy : 0), cw, ch, 0xC0);
    }
    return 0;
}

/* Every picture attribute of a tag list into d; TRUE when one was there. */
static int aaimage_tags(aaimage_data* d, struct TagItem* tags) {
    struct TagItem *state = tags, *ti;
    int seen = 0;
    while ((ti = NextTagItem(&state)) != NULL) {
        ULONG v = ti->ti_Data;
        switch (ti->ti_Tag) {
        case UPA_Image_File:
            memset(d->file, 0, sizeof d->file);
            if (v != 0) {
                strncpy(d->file, (const char*)v, sizeof d->file - 1);
            }
            break;
        case UPA_Image_Bitmap: d->user_bm = (struct BitMap*)v; break;
        case UPA_Image_Fit: d->fit = v != 0; break;
        case UPA_Image_Width: d->bw = (int)v; break;
        case UPA_Image_Height: d->bh = (int)v; break;
        case UPA_Image_SourceColors: d->colors = (const ULONG*)v; break;
        case UPA_Image_MappingTable: d->map = (const UBYTE*)v; break;
        case UPA_Image_Precision: d->prec = (int)v; break;
        case UPA_Image_Transparent: d->transp = (int)v; break;
        case UPA_Image_UseFriend: d->use_friend = v != 0; break;
        case UPA_Image_Body: d->body = (const UBYTE*)v; break;
        case UPA_Image_Compression: d->comp = (int)v; break;
        case UPA_Image_Depth: d->depth = (int)v; break;
        case UPA_Image_Masking: d->masking = (int)v; break;
        default: continue;
        }
        seen = 1;
    }
    return seen;
}

static ULONG aaimage_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aaimage_data* d;
    struct opSet nm;
    struct TagItem def[2];
    def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
    def[1].ti_Tag = TAG_MORE, def[1].ti_Data = (ULONG)(uintptr_t)msg->ops_AttrList;
    nm = *msg;
    nm.ops_AttrList = def;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&nm);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->prec = PRECISION_GUI, d->transp = -1;
    aaimage_tags(d, msg->ops_AttrList);
    return (ULONG)obj;
}

static ULONG aaimage_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aaimage_data* d = INST_DATA(cl, obj);
    int fit = d->fit; /* Fit is I only */
    if (aaimage_tags(d, msg->ops_AttrList)) {
        d->fit = fit;
        if (aa_live(obj)) {
            aaimage_load(obj, d);
            aa_redraw(obj, MADF_DRAWOBJECT); /* the box keeps its layout size; a different picture size needs a relayout by the caller */
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG __attribute__((used)) aaimage_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aaimage_dispatch(void);
const struct aa_class_desc aaimage_class_desc = {MUIC_Area, sizeof(aaimage_data), aaimage_dispatch, &g_image_class};
__asm__(".text\n.even\n.globl _aaimage_dispatch\n_aaimage_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aaimage_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aaimage_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aaimage_new(cl, obj, (struct opSet*)msg);
    case OM_SET:
        return aaimage_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        if (((struct opGet*)msg)->opg_AttrID == UPA_Image_RemappedBitmap) {
            aaimage_data* d = INST_DATA(cl, obj);
            *((struct opGet*)msg)->opg_Storage = (ULONG)(d->remap_bm != NULL ? d->remap_bm : d->body_bm != NULL ? d->body_bm : d->user_bm);
            return TRUE;
        }
        break;
    case MUIM_Setup:
        return aaimage_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aaimage_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aaimage_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aaimage_draw(cl, obj, (struct MUIP_Draw*)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

Object* UP_NewImage(const struct TagItem* tags) {
    if (g_image_class == NULL) {
        g_image_class = AA_CLASS_CREATE(&aaimage_class_desc);
    }
    if (g_image_class == NULL) {
        return NULL;
    }
    return NewObjectA(g_image_class->mcc_Class, NULL, (struct TagItem*)tags);
}

void mui_aaimage_class_free(void) {
    if (g_image_class != NULL) {
        MUI_DeleteCustomClass(g_image_class);
        g_image_class = NULL;
    }
}
