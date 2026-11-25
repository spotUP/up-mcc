/* Selecta_AAField (I667h, design section 3.4). Same dispatcher pattern as mui_aatextarea_class.c: a MUIC_Area subclass that owns its
 * single line of text (field own edit, 2026-11-25). No MUIC_String inside: the wrapped String let Intuition paint the gadget in the
 * prefs font and pens on events the edit hook missed (rig 3: a triple click turned the release date to topaz under a bar). Colours,
 * outline and radius come from map/mui_aa.c (mui_aa_field_*); the frame paint and the clipboard are shared with the textarea
 * (mui_aaedit.c); caret, selection, word and scroll maths are mui_text_lines.c (host tested). The class answers the MUIA_String_*
 * attributes callers use (Contents with notify, MaxLen, Acknowledge on Return, Accept, Secret, BufferPos), so UPA_Field_String is
 * the field itself. */
#include "mui_aafield_class.h"
#include "mui_aa_class.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <mcc/up.h>

#include "map/mui_aa.h"
#include "mui_aa_draw.h"
#include "mui_aaedit.h"
#include "mui_aatip.h"
#include "mui_text_lines.h"

extern struct Library* MUIMasterBase;

#define AAFIELD_DEF_MAXLEN 80 /* MUI's String default; MaxLen counts the NUL */
#define AAFIELD_FIT_BUF 256   /* shortcut: unfocused text and placeholder are measured cut at 255 bytes, upgrade if a field shows more */
#define SHIFT_QUALS (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)
#define CMD_QUALS (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)

typedef struct aafield_data {
    char* buf;   /* cap bytes, NUL terminated: MUIA_String_Contents */
    char* stars; /* MUIA_String_Secret: cap bytes of '*' standing in for buf in every measure and draw; NULL when not secret */
    int cap, len, caret, anchor, scroll;
    int dragging;        /* left button held after a click inside: mouse moves extend the selection */
    int clicks;          /* 1, 2, 3: caret, word, all (the count of quick clicks on one spot) */
    int click_off;       /* text offset of the last click */
    ULONG click_s, click_us;
    const char* accept; /* MUIA_String_Accept: the bytes typing and pasting may insert; NULL = any; must outlive the field */
    mui_aa_field_t m;
    int max_w;
    const char* placeholder;
    aa_ctx aa;
    int shown;     /* MUIM_Show seen, MUIM_Hide not yet (AA_SHOWN_CASES); outside aa_ctx, whose ABI 2 layout is frozen */
    int esc_close; /* I1360: Esc in the active field requests the window close */
    int error;     /* UPA_Field_Error: ERROR outline until the next edit */
    LONG key;      /* UPA_Field_Key: last Up, Down, Esc or Tab raw code */
    struct MUI_EventHandlerNode ehn;
    aatip_slot tip;
} aafield_data;

typedef struct aafield_fit {
    aafield_data* d;
    Object* obj;
} aafield_fit;

static struct MUI_CustomClass* g_class;
static uint32_t g_made;

/* The bytes measured and drawn: the contents, or as many '*' for a secret field. Writable, so a measure can cut it in place. */
static char* aafield_shown(aafield_data* d) {
    if (d->stars == NULL) {
        return d->buf;
    }
    memset(d->stars, '*', (size_t)d->len);
    d->stars[d->len] = '\0';
    return d->stars;
}

/* MtlMeasure over aafield_shown: ink width of the first len bytes, cut in place for the call (no length limit). */
static int aafield_measure(void* ctx, const char* s, int len) {
    aafield_fit* f = (aafield_fit*)ctx;
    char* w = (char*)s;
    char keep = w[len];
    int px;
    w[len] = '\0';
    px = aa_text_width(&f->d->aa, f->obj, w);
    w[len] = keep;
    return px;
}

/* mui_aa_width_fn of mui_aa_text_fit, whose strings may be read-only: measured from a copy. */
static int aafield_width(void* ctx, const char* s, int len) {
    aafield_fit* f = (aafield_fit*)ctx;
    char b[AAFIELD_FIT_BUF];
    if (len >= AAFIELD_FIT_BUF) {
        len = AAFIELD_FIT_BUF - 1;
    }
    memcpy(b, s, (size_t)len);
    b[len] = '\0';
    return aa_text_width(&f->d->aa, f->obj, b);
}

static int aafield_focus(Object* obj) {
    ULONG active = 0;
    if (!aa_live(obj)) {
        return 0;
    }
    GetAttr(MUIA_Window_ActiveObject, _win(obj), &active);
    return (Object*)active == obj;
}

static int aafield_disabled(Object* obj) {
    ULONG dis = 0;
    GetAttr(MUIA_Disabled, obj, &dis);
    return dis != 0;
}

static void aafield_redraw(Object* obj) {
    if (aa_live(obj)) {
        aa_redraw(obj, MADF_DRAWOBJECT);
    }
}

/* Copy s into the buffer (cut to MaxLen - 1). Returns 1 when the text changed; s == the buffer (an edit announcing itself) is no change. */
static int aafield_set_text(aafield_data* d, const char* s) {
    int n;
    if (s == d->buf) {
        return 0;
    }
    if (s == NULL) {
        s = "";
    }
    n = (int)strlen(s);
    if (n > d->cap - 1) {
        n = d->cap - 1;
    }
    memmove(d->buf, s, (size_t)n);
    d->buf[n] = '\0';
    d->len = d->caret = d->anchor = n;
    return 1;
}

static void aafield_moved(Object* obj, aafield_data* d, int off, int extend) {
    d->caret = off;
    if (!extend) {
        d->anchor = off;
    }
    aafield_redraw(obj);
}

/* After an edit: clear the error, repaint, announce the text (set notifies; the pointer is the buffer, so no copy). */
static void aafield_edited(Object* obj, aafield_data* d) {
    d->error = 0;
    d->anchor = d->caret;
    aafield_redraw(obj);
    set(obj, MUIA_String_Contents, (ULONG)(uintptr_t)d->buf);
}

static int aafield_selection(const aafield_data* d, int* lo, int* hi) {
    return mtl_sel_range(d->anchor, d->caret, d->len, lo, hi);
}

/* Keep the bytes a single line may hold and MUIA_String_Accept allows; s is changed in place. Returns the new length. */
static int aafield_filter(const aafield_data* d, char* s, int n) {
    int i, k = 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 0x20 && c <= 0x7E) || c >= 0xA0) {
            if (d->accept == NULL || strchr(d->accept, (int)c) != NULL) {
                s[k++] = (char)c;
            }
        }
    }
    return k;
}

/* Replace the selection (or insert at the caret) by s. Returns 1 when the text changed. */
static int aafield_put(aafield_data* d, const char* s, int slen) {
    int lo, hi, put;
    if (!aafield_selection(d, &lo, &hi)) {
        lo = hi = d->caret;
    }
    put = mtl_replace(d->buf, d->cap, &d->len, lo, hi, s, slen);
    d->caret = lo + put;
    return hi > lo || put > 0;
}

static void aafield_key(Object* obj, aafield_data* d, LONG code) {
    d->key = code;
    SetAttrs(obj, UPA_Field_Key, (ULONG)code, TAG_DONE); /* notify only */
}

/* Ctrl or Amiga + C/X/V/A, as the textarea. Returns 1 when the key was a clipboard or select-all command (handled). */
static int aafield_clipboard_key(Object* obj, aafield_data* d, int code, int qual) {
    int lo, hi, edited = 0;
    int key = mtl_clip_key(code, qual & CMD_QUALS);
    if (key == 'a') {
        d->anchor = 0;
        aafield_moved(obj, d, d->len, 1);
        return 1;
    }
    if (key == 'c' || key == 'x') {
        if (aafield_selection(d, &lo, &hi) && d->stars == NULL) { /* a secret never goes to the clipboard */
            aaedit_clip_put(d->buf + lo, hi - lo);
            if (key == 'x') {
                edited = aafield_put(d, "", 0);
            }
        }
    } else if (key == 'v') {
        char* t = (char*)AllocVec((ULONG)d->cap, MEMF_ANY);
        if (t != NULL) {
            int n = aafield_filter(d, t, aaedit_clip_get(t, d->cap));
            if (n > 0) {
                edited = aafield_put(d, t, n);
            }
            FreeVec(t);
        }
    } else {
        return 0;
    }
    if (edited) {
        aafield_edited(obj, d);
    }
    return 1;
}

/* Text offset under the pointer x (window coordinates), from the AA widths and the current scroll. */
static int aafield_offset_at(Object* obj, aafield_data* d, int mx) {
    aafield_fit f;
    MtlLine line;
    f.d = d;
    f.obj = obj;
    line.start = 0;
    line.len = d->len;
    return mtl_offset_at(aafield_shown(d), d->len, &line, 1, 0, mx - (_mleft(obj) + d->m.pad_x) + d->scroll, aafield_measure, &f);
}

static ULONG aafield_setup(struct IClass* cl, Object* obj, Msg msg) {
    aafield_data* d = INST_DATA(cl, obj);
    mui_aa_check_op_t ops[MUI_AA_FIELD_OPS];
    int k, n;
    if (!DoSuperMethodA(cl, obj, msg)) {
        return FALSE;
    }
    aa_setup(obj, &d->aa, NULL, 0);
    mui_aa_field_metrics(d->aa.font_px, d->aa.aspect_q8, &d->m);
    for (k = 0; k <= MUI_AA_FIELD_ERROR; k++) {
        n = mui_aa_field_plan(d->aa.tok, &d->m, k, ops);
        while (n-- > 0) {
            aa_hold(&d->aa, ops[n].rgb);
        }
    }
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_ACCENT]);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_ACCENT_TEXT]);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_TEXT]);
    aa_hold(&d->aa, d->aa.tok[SEL_GUI_TOKEN_TEXT_MUTED]);
    DoMethod(_win(obj), MUIM_Notify, MUIA_Window_ActiveObject, MUIV_EveryTime, (ULONG)obj, 1, UPM_Field_Redraw);
    d->ehn.ehn_Priority = 0;
    d->ehn.ehn_Flags = 0;
    d->ehn.ehn_Object = obj;
    d->ehn.ehn_Class = cl;
    d->ehn.ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_VANILLAKEY;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&d->ehn);
    return TRUE;
}

static ULONG aafield_cleanup(struct IClass* cl, Object* obj, Msg msg) {
    aafield_data* d = INST_DATA(cl, obj);
    DoMethod(_win(obj), MUIM_KillNotifyObj, MUIA_Window_ActiveObject, (ULONG)obj);
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&d->ehn);
    d->tip.in = 0;
    d->dragging = 0;
    aatip_drop(obj);
    aa_cleanup(&d->aa);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG aafield_askminmax(struct IClass* cl, Object* obj, struct MUIP_AskMinMax* msg) {
    aafield_data* d = INST_DATA(cl, obj);
    struct MUI_MinMax* mm;
    int h = mui_aa_field_height(&d->m, aa_text_height(&d->aa, obj)), pw = 2 * d->m.pad_x, px = d->aa.font_px;
    DoSuperMethodA(cl, obj, (Msg)msg);
    mm = msg->MinMaxInfo;
    mm->MinHeight = mm->DefHeight = mm->MaxHeight = h;
    mm->MinWidth += pw + 4 * px;
    mm->DefWidth += pw + 16 * px;
    mm->MaxWidth = MUI_MAXMAX;
    if (d->placeholder != NULL) { /* the placeholder is the field's label: a row may not squeeze it into "..." */
        int ph = mui_aa_field_min_width(&d->m, aa_text_width(&d->aa, obj, d->placeholder));
        if (mm->MinWidth < ph) {
            mm->MinWidth = ph;
        }
        if (mm->DefWidth < mm->MinWidth) {
            mm->DefWidth = mm->MinWidth;
        }
    }
    if (d->max_w > 0 && mm->MaxWidth > d->max_w) {
        mm->MaxWidth = d->max_w;
        if (mm->DefWidth > d->max_w) {
            mm->DefWidth = d->max_w;
        }
    }
    return 0;
}

static ULONG aafield_draw(struct IClass* cl, Object* obj, struct MUIP_Draw* msg) {
    aafield_data* d = INST_DATA(cl, obj);
    mui_aa_check_op_t ops[MUI_AA_FIELD_OPS];
    struct RastPort* rp;
    aafield_fit f;
    const uint32_t* tok;
    uint32_t face, rgb;
    char* t;
    int l, top, w, h, x0, tw, th, ty, focus, n;
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
        return 0;
    }
    rp = _rp(obj);
    tok = d->aa.tok;
    face = tok[MUI_AA_FIELD_FACE];
    l = _mleft(obj);
    top = _mtop(obj);
    h = _mheight(obj);
    focus = aafield_focus(obj);
    n = mui_aa_field_plan(tok, &d->m, d->error ? MUI_AA_FIELD_ERROR : focus, ops);
    aaedit_frame(&d->aa, rp, l, top, _mwidth(obj), h, ops, n);
    x0 = l + d->m.pad_x;
    w = _mwidth(obj) - 2 * d->m.pad_x;
    if (w < 1) {
        return 0;
    }
    th = aa_text_height(&d->aa, obj);
    ty = top + mui_aa_field_text_top(&d->m, h, th);
    f.d = d;
    f.obj = obj;
    t = aafield_shown(d);
    if (!focus) {
        /* D3 and I733: the contents in TEXT, or the placeholder in TEXT_MUTED while empty; cut with "..." inside the box (I1479) */
        const char* show = NULL;
        if (mui_aa_field_text(tok, d->buf, 0, &rgb)) {
            show = t;
        } else if (d->placeholder != NULL && mui_aa_field_placeholder(tok, d->buf, 0, &rgb)) {
            show = d->placeholder;
        }
        d->scroll = 0;
        if (show != NULL) {
            char fit[AAFIELD_FIT_BUF];
            mui_aa_text_fit(show, w - 4, aafield_width, &f, fit, (int)sizeof fit);
            aa_text(&d->aa, obj, rp, x0, ty, fit, aafield_disabled(obj) ? tok[SEL_GUI_TOKEN_TEXT_MUTED] : rgb, face);
        }
        return 0;
    }
    {
        int cw = d->aa.font_px >= 16 ? 2 : 1, cx, c0, c1, lo, hi;
        tw = aafield_measure(&f, t, d->len);
        cx = aafield_measure(&f, t, d->caret);
        d->scroll = mtl_scroll_x(cx, cw, w, d->scroll, tw);
        mtl_visible_cols(t, d->len, d->scroll, w, aafield_measure, &f, &c0, &c1);
        if (c1 > c0) {
            char keep = t[c1];
            t[c1] = '\0';
            aa_text(&d->aa, obj, rp, x0 + aafield_measure(&f, t, c0) - d->scroll, ty, t + c0, tok[SEL_GUI_TOKEN_TEXT], face);
            t[c1] = keep;
        }
        if (aafield_selection(d, &lo, &hi)) {
            int sx0 = x0 + aafield_measure(&f, t, lo) - d->scroll, sx1 = x0 + aafield_measure(&f, t, hi) - d->scroll;
            int a = lo > c0 ? lo : c0, b = hi < c1 ? hi : c1;
            sx0 = sx0 < x0 ? x0 : sx0;
            sx1 = sx1 > x0 + w ? x0 + w : sx1;
            if (sx1 > sx0) {
                aa_rect(&d->aa, rp, sx0, ty, sx1 - 1, ty + th - 1, tok[SEL_GUI_TOKEN_ACCENT]);
                if (b > a) {
                    char keep = t[b];
                    t[b] = '\0';
                    aa_text(&d->aa, obj, rp, x0 + aafield_measure(&f, t, a) - d->scroll, ty, t + a, tok[SEL_GUI_TOKEN_ACCENT_TEXT], tok[SEL_GUI_TOKEN_ACCENT]);
                    t[b] = keep;
                }
            }
        }
        cx = x0 + cx - d->scroll;
        if (cx > x0 + w - cw) {
            cx = x0 + w - cw;
        }
        aa_rect(&d->aa, rp, cx, ty, cx + cw - 1, ty + th - 1, tok[SEL_GUI_TOKEN_TEXT]);
    }
    return 0;
}

/* A press inside: 1 click places the caret (Shift extends), a quick 2nd on the same spot selects the word, a 3rd selects all. */
static void aafield_click(Object* obj, aafield_data* d, struct IntuiMessage* im) {
    int off = aafield_offset_at(obj, d, im->MouseX), lo, hi;
    int near = off - d->click_off <= 1 && d->click_off - off <= 1;
    if (d->clicks > 0 && near && DoubleClick(d->click_s, d->click_us, im->Seconds, im->Micros)) {
        d->clicks = d->clicks % 3 + 1;
    } else {
        d->clicks = 1;
    }
    d->click_off = off;
    d->click_s = im->Seconds;
    d->click_us = im->Micros;
    if (d->clicks == 2) {
        mtl_word_range(d->buf, d->len, off, &lo, &hi);
        d->anchor = lo;
        aafield_moved(obj, d, hi, 1);
    } else if (d->clicks == 3) {
        d->anchor = 0;
        aafield_moved(obj, d, d->len, 1);
    } else {
        aafield_moved(obj, d, off, (im->Qualifier & SHIFT_QUALS) != 0);
    }
}

static ULONG aafield_handle(struct IClass* cl, Object* obj, struct MUIP_HandleEvent* msg) {
    aafield_data* d = INST_DATA(cl, obj);
    struct IntuiMessage* im = msg->imsg;
    int in, code, shift, has_sel, lo, hi;
    if (im == NULL) {
        return 0;
    }
    in = aa_inside(d->shown, obj, im);
    aatip_event(obj, &d->tip, im, in);
    if (im->Class == IDCMP_MOUSEBUTTONS) {
        if (im->Code == SELECTDOWN && in && !aafield_disabled(obj)) {
            if (!aafield_focus(obj) && aa_live(obj)) {
                set(_win(obj), MUIA_Window_ActiveObject, (ULONG)obj);
            }
            aafield_click(obj, d, im);
            d->dragging = 1;
            return MUI_EventHandlerRC_Eat;
        }
        if (im->Code == (SELECTDOWN | IECODE_UP_PREFIX)) {
            d->dragging = 0;
        }
        return 0;
    }
    if (im->Class == IDCMP_MOUSEMOVE) {
        if (d->dragging && aafield_focus(obj)) {
            aafield_moved(obj, d, aafield_offset_at(obj, d, im->MouseX), 1); /* past an edge the draw scrolls the caret into view */
        }
        return 0;
    }
    if (!aafield_focus(obj) || aafield_disabled(obj)) {
        return 0;
    }
    code = im->Code;
    if (im->Class == IDCMP_RAWKEY) {
        if (code & IECODE_UP_PREFIX) {
            return 0;
        }
        shift = im->Qualifier & SHIFT_QUALS;
        has_sel = aafield_selection(d, &lo, &hi);
        switch (code) {
        case 0x4C: /* Up, Down and Shift+Tab: announced, then left to the window */
        case 0x4D:
        case 0x42:
            aafield_key(obj, d, code);
            return 0;
        case 0x45: /* Esc as a raw key (no vanilla code was asked for) */
            break;
        case 0x4E: /* right: without Shift a selection collapses to its end */
            aafield_moved(obj, d, !shift && has_sel ? hi : d->caret < d->len ? d->caret + 1 : d->caret, shift);
            return MUI_EventHandlerRC_Eat;
        case 0x4F: /* left: ... or to its start */
            aafield_moved(obj, d, !shift && has_sel ? lo : d->caret > 0 ? d->caret - 1 : 0, shift);
            return MUI_EventHandlerRC_Eat;
        case 0x70: /* Home, as aalist */
            aafield_moved(obj, d, 0, shift);
            return MUI_EventHandlerRC_Eat;
        case 0x71: /* End */
            aafield_moved(obj, d, d->len, shift);
            return MUI_EventHandlerRC_Eat;
        default:
            return 0;
        }
    } else if (im->Class != IDCMP_VANILLAKEY) {
        return 0;
    } else if (code != 0x1B) {
        char c = (char)code;
        if (aafield_clipboard_key(obj, d, code, im->Qualifier)) {
            return MUI_EventHandlerRC_Eat;
        }
        if (im->Qualifier & CMD_QUALS) {
            return 0; /* other Amiga-key shortcuts stay with the window */
        }
        has_sel = aafield_selection(d, &lo, &hi);
        if (code == 0x08 || code == 0x7F) { /* Backspace, Delete: a selection goes whole, else one byte */
            if (has_sel) {
                aafield_put(d, "", 0);
                aafield_edited(obj, d);
            } else if (code == 0x08 && d->caret > 0) {
                d->caret = mtl_backspace(d->buf, &d->len, d->caret);
                aafield_edited(obj, d);
            } else if (code == 0x7F && mtl_delete(d->buf, &d->len, d->caret)) {
                aafield_edited(obj, d);
            }
        } else if (code == 0x0D) {
            set(obj, MUIA_String_Acknowledge, (ULONG)(uintptr_t)d->buf); /* Return: notify, the field stays active */
        } else if (code == 0x09) {
            /* Asking for VANILLAKEY makes Intuition deliver Tab only as vanilla 0x09, so MUI's forward cycle never sees it: announce
             * it as the raw Tab (UPA_Field_Key 0x42) and cycle here. Shift+Tab has no vanilla code and arrives as RAWKEY 0x42. */
            aafield_key(obj, d, 0x42);
            set(_win(obj), MUIA_Window_ActiveObject, (ULONG)MUIV_Window_ActiveObject_Next);
        } else if (aafield_filter(d, &c, 1) == 1) {
            if (aafield_put(d, &c, 1)) {
                aafield_edited(obj, d);
            }
        } else if (!((code >= 0x20 && code <= 0x7E) || code >= 0xA0)) {
            return 0; /* other control codes stay with the window; a printable byte Accept refuses is eaten */
        }
        return MUI_EventHandlerRC_Eat;
    }
    /* Esc (vanilla 0x1B or raw 0x45): announced; with UPA_Field_EscClose it closes the window. Eaten either way, as the String did. */
    aafield_key(obj, d, 0x45);
    if (d->esc_close) {
        set(_win(obj), MUIA_Window_CloseRequest, TRUE); /* the dialog's mui_close_on_request notify closes it */
    }
    return MUI_EventHandlerRC_Eat;
}

static ULONG aafield_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aafield_data* d = INST_DATA(cl, obj);
    struct TagItem *tag, *list = msg->ops_AttrList;
    while ((tag = NextTagItem(&list)) != NULL) {
        if (tag->ti_Tag == MUIA_String_Contents) {
            if (aafield_set_text(d, (const char*)tag->ti_Data)) {
                d->scroll = 0;
                aafield_redraw(obj);
            }
            tag->ti_Data = (ULONG)(uintptr_t)d->buf; /* the notify delivers the buffer, never the caller's temporary string */
        } else if (tag->ti_Tag == MUIA_String_Accept) {
            d->accept = (const char*)tag->ti_Data;
        } else if (tag->ti_Tag == MUIA_String_BufferPos) {
            int p = (int)tag->ti_Data;
            d->caret = d->anchor = p < 0 ? 0 : p > d->len ? d->len : p;
            aafield_redraw(obj);
        } else if (tag->ti_Tag == UPA_Field_EscClose) {
            d->esc_close = tag->ti_Data != 0;
        } else if (tag->ti_Tag == UPA_Field_Error && d->error != (tag->ti_Data != 0)) {
            d->error = tag->ti_Data != 0;
            aafield_redraw(obj);
        } else if (tag->ti_Tag == MUIA_Disabled) {
            aafield_redraw(obj);
        }
    }
    return aatip_super_set(cl, obj, msg, offsetof(aafield_data, tip));
}

static ULONG aafield_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aafield_data* d = INST_DATA(cl, obj);
    ULONG v;
    switch (msg->opg_AttrID) {
    case MUIA_String_Contents:
    case MUIA_String_Acknowledge:
        v = (ULONG)(uintptr_t)d->buf;
        break;
    case MUIA_String_MaxLen:
        v = (ULONG)d->cap;
        break;
    case MUIA_String_BufferPos:
        v = (ULONG)d->caret;
        break;
    case MUIA_String_Accept:
        v = (ULONG)(uintptr_t)d->accept;
        break;
    case MUIA_String_Secret:
        v = d->stars != NULL;
        break;
    case UPA_Field_Key:
        v = (ULONG)d->key;
        break;
    case UPA_Field_String:
        v = (ULONG)(uintptr_t)obj; /* the field is its own string object now */
        break;
    case UPA_Field_Error:
        v = (ULONG)d->error;
        break;
    default:
        if (aatip_get(msg, &d->tip)) {
            return TRUE;
        }
        return DoSuperMethodA(cl, obj, (Msg)msg);
    }
    *msg->opg_Storage = v;
    return TRUE;
}

/* OM_NEW: UP_NewField and a by-name MUI_NewObject("UP-Field.mcc") both land here. */
static ULONG aafield_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    aafield_data* d;
    struct opSet nm;
    struct TagItem def[4];
    struct TagItem* tags = msg->ops_AttrList;
    LONG maxlen = (LONG)GetTagData(MUIA_String_MaxLen, AAFIELD_DEF_MAXLEN, tags);
    def[0].ti_Tag = MUIA_Frame, def[0].ti_Data = (ULONG)MUIV_Frame_None;
    def[1].ti_Tag = MUIA_CycleChain, def[1].ti_Data = (ULONG)TRUE;
    def[2].ti_Tag = MUIA_FillArea, def[2].ti_Data = (ULONG)FALSE; /* the draw paints every pixel; a MUI fill first would flash */
    def[3].ti_Tag = TAG_MORE, def[3].ti_Data = (ULONG)(uintptr_t)tags;
    nm = *msg;
    nm.ops_AttrList = def;
    obj = (Object*)aatip_super_set(cl, obj, &nm, offsetof(aafield_data, tip));
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj); /* BOOPSI instance data starts cleared; the tooltip slot above is already filled, so no memset */
    d->cap = maxlen < 2 ? 2 : (int)maxlen;
    d->buf = (char*)AllocVec((ULONG)d->cap, MEMF_CLEAR);
    if (GetTagData(MUIA_String_Secret, FALSE, tags)) {
        d->stars = (char*)AllocVec((ULONG)d->cap, MEMF_CLEAR);
    }
    if (d->buf == NULL || (GetTagData(MUIA_String_Secret, FALSE, tags) && d->stars == NULL)) {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }
    d->max_w = (int)GetTagData(UPA_Field_MaxWidth, 0, tags);
    d->placeholder = (const char*)GetTagData(UPA_Field_Placeholder, 0, tags);
    d->accept = (const char*)GetTagData(MUIA_String_Accept, 0, tags);
    d->esc_close = GetTagData(UPA_Field_EscClose, FALSE, tags) != 0;
    aafield_set_text(d, (const char*)GetTagData(MUIA_String_Contents, 0, tags));
    return (ULONG)obj;
}

static ULONG aafield_dispose(struct IClass* cl, Object* obj, Msg msg) {
    aafield_data* d = INST_DATA(cl, obj);
    FreeVec(d->buf);
    FreeVec(d->stars);
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG __attribute__((used)) aafield_dispatch_c(struct IClass* cl, Object* obj, Msg msg);

extern ULONG aafield_dispatch(void);
const struct aa_class_desc aafield_class_desc = {MUIC_Area, sizeof(aafield_data), aafield_dispatch, &g_class};
__asm__(".text\n.even\n.globl _aafield_dispatch\n_aafield_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aafield_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static ULONG __attribute__((used)) aafield_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return aafield_new(cl, obj, (struct opSet*)msg);
    case OM_DISPOSE:
        return aafield_dispose(cl, obj, msg);
    case OM_SET:
        return aafield_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aafield_get(cl, obj, (struct opGet*)msg);
    case UPM_Shared_Fire: {
        aafield_data* d = INST_DATA(cl, obj);
        return aatip_fire(obj, &d->aa, &d->tip);
    }
    case MUIM_Show:
        ((aafield_data*)INST_DATA(cl, obj))->shown = 1;
        break;
    case MUIM_Hide:
        ((aafield_data*)INST_DATA(cl, obj))->shown = 0;
        ((aafield_data*)INST_DATA(cl, obj))->dragging = 0;
        aatip_drop(obj);
        break;
    case MUIM_HandleEvent:
        return aafield_handle(cl, obj, (struct MUIP_HandleEvent*)msg);
    case MUIM_Setup:
        return aafield_setup(cl, obj, msg);
    case MUIM_Cleanup:
        return aafield_cleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return aafield_askminmax(cl, obj, (struct MUIP_AskMinMax*)msg);
    case MUIM_Draw:
        return aafield_draw(cl, obj, (struct MUIP_Draw*)msg);
    case UPM_Field_Edited:
        ((aafield_data*)INST_DATA(cl, obj))->error = 0;
        /* fall through: the outline repaints */
    case UPM_Field_Redraw:
        aafield_redraw(obj);
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* Tags: UPA_Field_* and the MUIA_String_* above; UPA_Field_Tokens is required (the class draws from the screen's theme). */
Object* UP_NewField(const struct TagItem* tags) {
    Object* o;
    if (GetTagData(UPA_Field_Tokens, 0, (struct TagItem*)tags) == 0) {
        return NULL;
    }
    if (g_class == NULL) {
        g_class = AA_CLASS_CREATE(&aafield_class_desc);
    }
    if (g_class == NULL) {
        return NULL;
    }
    o = NewObjectA(g_class->mcc_Class, NULL, (struct TagItem*)tags);
    if (o != NULL) {
        g_made++;
    }
    return o;
}

uint32_t mui_aafield_made(void) {
    return g_made;
}

void mui_aafield_class_free(void) {
    if (g_class != NULL) {
        MUI_DeleteCustomClass(g_class);
        g_class = NULL;
    }
}
