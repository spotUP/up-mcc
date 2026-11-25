/* UP- MUI class package: the only place the MUI tag base is written (design 2026-11-06-mui-mcc-package.md section 8.2, decision D1).
 * Every attribute and method number of the package is UP_TAG(class index, offset). Interim base until a registered MUI serial
 * exists; the swap is this one line plus a major version bump. */
#ifndef MCC_UP_H
#define MCC_UP_H

#if defined(__amigaos__) || defined(AMIGA)
#include <exec/types.h>
#include <intuition/classusr.h>
#include <utility/hooks.h>
#include <utility/tagitem.h>
typedef ULONG UP_Data; /* ti_Data */
#else
/* Host (tests, no Amiga headers): the names the macros, the tag lists and the hook tags need, with the Amiga values. */
#include <stddef.h>
#include <stdint.h>
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef char* STRPTR;
typedef void* APTR;
typedef uint16_t UWORD;
typedef int16_t WORD;
typedef uintptr_t UP_Data; /* ti_Data: wide enough for a pointer on the host */
typedef ULONG Object;
#define TAG_DONE 0UL
#define TAG_IGNORE 1UL
#define TAG_MORE 2UL
#define TAG_USER (1UL << 31)
struct TagItem {
    ULONG ti_Tag;
    UP_Data ti_Data;
};
struct Hook { /* the three fields the package reads; the Amiga struct has a MinNode in front */
    void* h_Entry;
    void* h_SubEntry;
    void* h_Data;
};
/* MUI tags the class reads (Amiga values; the real header is not on the host). */
#define MUIA_Background 0x8042545bUL
#define MUIA_ControlChar 0x8042120bUL
#define MUIA_CycleChain 0x80421ce7UL
#define MUIA_FixHeight 0x8042a92bUL
#define MUIA_FixWidth 0x8042a3f1UL
#define MUIA_Frame 0x8042ac64UL
#define MUIA_InputMode 0x8042fb04UL
#define MUIA_Numeric_Max 0x8042d78aUL
#define MUIA_Numeric_Min 0x8042e404UL
#define MUIA_Numeric_Value 0x8042ae3aUL
#define MUIA_Selected 0x8042654bUL
#define MUIA_ShortHelp 0x80423bbcUL
#define MUIA_ShowMe 0x80429ba8UL
#define MUIA_String_MaxLen 0x80424984UL
#define MUIA_String_Secret 0x80428769UL
#define MUIA_Weight 0x80421d1fUL
#define MUIV_Frame_None 0UL
#endif
#define UP_D(x) ((UP_Data)(uintptr_t)(x)) /* a value or pointer as ti_Data */

#define UP_TAGBASE (TAG_USER | 0x53410000UL)
#define UP_TAG(cls, n) (UP_TAGBASE | ((ULONG)(cls) << 8) | (ULONG)(n))

/* Class index byte (design 8.2) */
#define UP_CLS_TEXT 0x00     /* UP-Text */
#define UP_CLS_PROGRESS 0x01 /* UP-Progress / slider value */
#define UP_CLS_CYCLE 0x02
#define UP_CLS_TABS 0x03
#define UP_CLS_SCROLLER 0x04
#define UP_CLS_LIST 0x05
#define UP_CLS_BUTTON 0x06
#define UP_CLS_FIELD 0x07
#define UP_CLS_PANEL 0x08
#define UP_CLS_BALANCE 0x09
#define UP_CLS_CHECKMARK 0x0A
#define UP_CLS_RADIO 0x0B
#define UP_CLS_POPSTRING 0x0C
#define UP_CLS_POPLIST 0x0D
#define UP_CLS_NUMERIC 0x11 /* UP-Numericbutton */
#define UP_CLS_SCALE 0x12   /* UP-Scale */
#define UP_CLS_COLORFIELD 0x13 /* UP-Colorfield */
#define UP_CLS_PALETTE 0x14    /* UP-Palette */
#define UP_CLS_IMAGE 0x15   /* UP-Image */
#define UP_CLS_MENU 0x16    /* UP-Menu */
#define UP_CLS_TEXTAREA 0x17 /* UP-TextArea */
#define UP_CLS_RECTANGLE 0x18 /* UP-Rectangle */
#define UP_CLS_FLOATTEXT 0x19 /* UP-Floattext */
#define UP_CLS_POPOBJECT 0x1C /* UP-Popobject */
#define UP_CLS_POPASL 0x1D    /* UP-Popasl */
#define UP_CLS_POPSCREEN 0x1E /* UP-Popscreen (no own attributes) */
#define UP_CLS_SCROLLGROUP 0x0E
#define UP_CLS_VIRTGROUP 0x10
#define UP_CLS_SHARED 0x0F /* tooltip and shared */
#define UP_CLS_KNOB 0x1A       /* UP-Knob: MUIA_Numeric_* tags and methods, no own tags */
#define UP_CLS_LEVELMETER 0x1B /* UP-Levelmeter: MUIA_Numeric_* plus MUIA_Levelmeter_Label (ISG STRPTR, must outlive the object) */

/* ---- Attributes (UPA_), methods (UPM_) and enumeration values (UPV_) of every class (design 8.3). The numbers are written here
 * and nowhere else. I/S/G: settable at construction / later / readable. ---- */

/* UP-Text */
#define UPA_Text_Contents UP_TAG(UP_CLS_TEXT, 0x01) /* ISG STRPTR copied into the object; redraws only when it changed */
#define UPA_Text_Widest UP_TAG(UP_CLS_TEXT, 0x02)   /* I STRPTR longest text the line shows (default width); absent = contents */
#define UPA_Text_Tone UP_TAG(UP_CLS_TEXT, 0x03)     /* I UPV_Text_Tone_* */
#define UPA_Text_Align UP_TAG(UP_CLS_TEXT, 0x04)    /* I UPV_Text_Align_* */
#define UPA_Text_Ellipsis UP_TAG(UP_CLS_TEXT, 0x05) /* I BOOL cut with "..." when wider than the box */
#define UPA_Text_Fixed UP_TAG(UP_CLS_TEXT, 0x06)    /* I BOOL never wider than the default width of the widest text */
#define UPA_Text_FillHeight UP_TAG(UP_CLS_TEXT, 0x07) /* I BOOL grow to the height of the row, so the erase covers the whole row */
#define UPA_Text_Timeout UP_TAG(UP_CLS_TEXT, 0x08)  /* IS ULONG ms: a non-empty UPA_Text_Contents set clears itself this long later (0 = stays); a newer text restarts the clock */
enum { UPV_Text_Tone_Normal, UPV_Text_Tone_Muted, UPV_Text_Tone_Title, UPV_Text_Tone_Heading };
enum { UPV_Text_Align_Left, UPV_Text_Align_Centre, UPV_Text_Align_Right };

/* UP-Progress and the slider value */
#define UPA_Progress_Value UP_TAG(UP_CLS_PROGRESS, 0x01) /* ISG LONG: filled part */
#define UPA_Progress_Max UP_TAG(UP_CLS_PROGRESS, 0x02)   /* ISG LONG: value that fills the bar */

/* UP-Checkmark: MUIA_Selected (ISG BOOL, notifies) is the checked value, as for MUIC_Checkmark */
#define UPA_Checkmark_Label UP_TAG(UP_CLS_CHECKMARK, 0x01) /* I STRPTR text right of the box; must outlive the object; absent = box only */

/* UP-Radio */
#define UPA_Radio_Entries UP_TAG(UP_CLS_RADIO, 0x01) /* I char**: NULL terminated, must outlive the object */
#define UPA_Radio_Active UP_TAG(UP_CLS_RADIO, 0x02)  /* ISG LONG: index of the chosen entry (notifies) */

/* UP-Virtgroup: a group whose contents (MUIA_Group_Child) are laid out at their own size, larger than the view, and shown through it at an offset */
#define UPA_Virtgroup_Left UP_TAG(UP_CLS_VIRTGROUP, 0x01)   /* ISG LONG: pixels the contents are shifted left; clamped to the contents */
#define UPA_Virtgroup_Top UP_TAG(UP_CLS_VIRTGROUP, 0x02)    /* ISG LONG: pixels the contents are shifted up; clamped to the contents */
#define UPA_Virtgroup_Width UP_TAG(UP_CLS_VIRTGROUP, 0x03)  /* G LONG: width of the contents */
#define UPA_Virtgroup_Height UP_TAG(UP_CLS_VIRTGROUP, 0x04) /* G LONG: height of the contents */

/* UP-Scrollgroup: a virtgroup with an UP-Scroller on the right that shows only while the contents are taller than the view */
#define UPA_Scrollgroup_Top UP_TAG(UP_CLS_SCROLLGROUP, 0x01) /* ISG LONG: pixels the contents are scrolled up */

/* UP-Cycle */
#define UPA_Cycle_Active UP_TAG(UP_CLS_CYCLE, 0x01)  /* ISG LONG: index of the shown entry (notifies) */
#define UPA_Cycle_Entries UP_TAG(UP_CLS_CYCLE, 0x02) /* I char**: NULL terminated, must outlive the object */

/* UP-Popstring: a UP-Field plus a button that opens a popup window under the object (MUIC_Popstring, simplified). One popup window per
 * object, created at the first open and reused; it closes on a pick, Esc, a click outside or another click on the button. */
#define UPA_Popstring_String UP_TAG(UP_CLS_POPSTRING, 0x01)   /* IG Object*: the string object (default a UP-Field); MUIA_String_Contents set/get are forwarded to it */
#define UPA_Popstring_Button UP_TAG(UP_CLS_POPSTRING, 0x02)   /* I Object*: the button that opens the popup (default a UP-Button with a chevron) */
#define UPA_Popstring_Popup UP_TAG(UP_CLS_POPSTRING, 0x03)    /* ISG Object*: root object of the popup window, owned by the class from then on; it is disposed with the owner window's cleanup (get then gives NULL), set a new one before the next open */
#define UPA_Popstring_OpenHook UP_TAG(UP_CLS_POPSTRING, 0x04) /* IS struct Hook*: called before the popup opens, object = the popup root, message = the string object; returns FALSE to refuse */
#define UPA_Popstring_CloseHook UP_TAG(UP_CLS_POPSTRING, 0x05) /* IS struct Hook*: called once after the popup closed, object = the popup root, message = the string object */
#define UPA_Popstring_Open UP_TAG(UP_CLS_POPSTRING, 0x06)     /* ISG BOOL: the popup is open; set TRUE opens, FALSE closes (notifies) */
#define UPA_Popstring_NoActivate UP_TAG(UP_CLS_POPSTRING, 0x07) /* I BOOL: the popup window opens inactive, so typing stays in the string (autocomplete); default FALSE */
#define UPM_Popstring_Click UP_TAG(UP_CLS_POPSTRING, 0x10)    /* method: the button was pressed (toggle). Internal, used by the button notify */
#define UPM_Popstring_Closed UP_TAG(UP_CLS_POPSTRING, 0x11)   /* method: the popup window closed. Internal */
#define UPM_Popstring_Deactivated UP_TAG(UP_CLS_POPSTRING, 0x12) /* method: the popup window lost activation. Internal */

/* UP-Poplist: a Popstring whose popup is a UP-List; the chosen entry is copied into the string. */
#define UPA_Poplist_Entries UP_TAG(UP_CLS_POPLIST, 0x01) /* I char**: NULL terminated, must outlive the object */
#define UPA_Poplist_Active UP_TAG(UP_CLS_POPLIST, 0x02)  /* ISG LONG: chosen entry, -1 none (default); setting it fills the string; notifies */
/* Poplist also takes UPA_Popstring_String, _Button, _OpenHook, _CloseHook, _Open. */

/* UP-Popobject (MUIC_Popobject): a Popstring with any object as popup. Every UP-Pop* class also takes the MUI 3.8 ids of its own and of
 * MUIC_Popstring (MUIA_Popobject_*, MUIA_Popasl_*, MUIA_Popstring_String/_Button/_OpenHook/_CloseHook) and the methods MUIM_Popstring_Open
 * and MUIM_Popstring_Close (result TRUE runs the ObjStrHook). Hooks take the MUI order: object = the popup object, message = the string. */
#define UPA_Popobject_Object UP_TAG(UP_CLS_POPOBJECT, 0x01)     /* IG Object*: the popup object (as UPA_Popstring_Popup) */
#define UPA_Popobject_StrObjHook UP_TAG(UP_CLS_POPOBJECT, 0x02) /* ISG struct Hook*: before the popup opens (string to object); FALSE refuses the open */
#define UPA_Popobject_ObjStrHook UP_TAG(UP_CLS_POPOBJECT, 0x03) /* ISG struct Hook*: after a successful close, MUIM_Popstring_Close TRUE (object to string) */
#define UPA_Popobject_WindowHook UP_TAG(UP_CLS_POPOBJECT, 0x04) /* ISG struct Hook*: before every open, object = the popup object, message = the popup window */
#define UPA_Popobject_Follow UP_TAG(UP_CLS_POPOBJECT, 0x05)     /* ISG BOOL: stored for MUI compatibility (default TRUE); the popup is placed under the object at every open */
#define UPA_Popobject_Light UP_TAG(UP_CLS_POPOBJECT, 0x06)      /* ISG BOOL: stored for MUI compatibility (default FALSE); the popup window is always borderless */
#define UPA_Popobject_Volatile UP_TAG(UP_CLS_POPOBJECT, 0x07)   /* ISG BOOL: stored for MUI compatibility (default TRUE); a hidden object always closes its popup */

/* UP-Popasl (MUIC_Popasl): the button opens an ASL file, font or screen mode requester; ASL tags in the creation list go to the
 * requester. Without a StopHook the result is written into the string: the path, "name/size", or the display ID as 0x%08lX. */
#define UPA_Popasl_Type UP_TAG(UP_CLS_POPASL, 0x01)      /* IG ULONG: ASL_FileRequest (default), ASL_FontRequest or ASL_ScreenModeRequest */
#define UPA_Popasl_Active UP_TAG(UP_CLS_POPASL, 0x02)    /* G BOOL: the requester is open (notifies) */
#define UPA_Popasl_StartHook UP_TAG(UP_CLS_POPASL, 0x03) /* ISG struct Hook*: object = the Popasl, message = the requester tag list (may be changed); FALSE cancels */
#define UPA_Popasl_StopHook UP_TAG(UP_CLS_POPASL, 0x04)  /* ISG struct Hook*: after a successful request, object = the Popasl, message = the requester; replaces the string fill */

/* UP-Popscreen (MUIC_Popscreen): a Poplist whose entries are the public screen names, read again at every open. */

/* UP-Numericbutton (MUIC_Numericbutton equivalent): a button showing the number; a click opens a small popup slider under it, a
 * horizontal drag over the button changes the value directly (two button widths sweep the range). */
#define UPA_Numeric_Min UP_TAG(UP_CLS_NUMERIC, 0x01)   /* ISG LONG: lowest value (default 0) */
#define UPA_Numeric_Max UP_TAG(UP_CLS_NUMERIC, 0x02)   /* ISG LONG: highest value (default 100) */
#define UPA_Numeric_Value UP_TAG(UP_CLS_NUMERIC, 0x03) /* ISG LONG: current value, clamped to Min..Max (notifies) */
#define UPM_Numeric_Deactivated UP_TAG(UP_CLS_NUMERIC, 0x10) /* method: the popup window lost activation. Internal */
#define UPM_Numeric_Closed UP_TAG(UP_CLS_NUMERIC, 0x11)      /* method: the popup window closed. Internal */

/* UP-Scale (MUIC_Scale equivalent): a horizontal tick ruler with the end and middle values as labels, meant under a UP-Slider. */
#define UPA_Scale_Min UP_TAG(UP_CLS_SCALE, 0x01)   /* ISG LONG: value at the left end (default 0) */
#define UPA_Scale_Max UP_TAG(UP_CLS_SCALE, 0x02)   /* ISG LONG: value at the right end (default 100) */
#define UPA_Scale_Ticks UP_TAG(UP_CLS_SCALE, 0x03) /* ISG LONG: number of ticks, 2..64 (default 11) */

/* UP-Colorfield (MUIC_Coloradjust-like swatch): shows one RGB colour; a click notifies MUIA_Pressed, as a UP-Button does. */
#define UPA_Colorfield_RGB UP_TAG(UP_CLS_COLORFIELD, 0x01) /* ISG ULONG: 0xRRGGBB shown (default 0, black) */

/* UP-Palette: a grid of colour swatches, one active; a click or the cursor keys (while it is the active object) choose it. */
#define UPA_Palette_Entries UP_TAG(UP_CLS_PALETTE, 0x01) /* I ULONG*: array of 0xRRGGBB, copied at construction (at most 256) */
#define UPA_Palette_Count UP_TAG(UP_CLS_PALETTE, 0x02)   /* I LONG: number of entries in UPA_Palette_Entries */
#define UPA_Palette_Active UP_TAG(UP_CLS_PALETTE, 0x03)  /* ISG LONG: index of the chosen swatch (default 0; notifies) */
#define UPA_Palette_Columns UP_TAG(UP_CLS_PALETTE, 0x04) /* I LONG: swatches per row (default 8, at most the entry count) */

/* UP-Image (MUIC_Dtpic-like): a picture from a file (picture.datatype, remapped to the window screen) or from a caller bitmap, on the panel
 * background. Without picture.datatype, a readable file or any bitmap the box stays empty (an outline only). */
#define UPA_Image_File UP_TAG(UP_CLS_IMAGE, 0x01)   /* IS STRPTR: path of the picture, copied (at most 255 characters); set reloads and redraws */
#define UPA_Image_Bitmap UP_TAG(UP_CLS_IMAGE, 0x02) /* IS struct BitMap*: caller bitmap shown instead of a file; NOT copied, must stay valid; wins over the file */
#define UPA_Image_Fit UP_TAG(UP_CLS_IMAGE, 0x03)    /* I BOOL: scale to the box keeping the aspect (the box then stretches); default FALSE = the picture size */

/* UP-Menu (MUIC_Menustrip-like, one menu): a UP-Button caption that opens a dark popup list of items under it. Several UP-Menu objects side by
 * side in a horizontal group make a menu strip. One popup window, created at the first open and reused (as UP-Poplist). */
#define UPA_Menu_Title UP_TAG(UP_CLS_MENU, 0x01)  /* I STRPTR: caption of the menu button; must outlive the object */
#define UPA_Menu_Items UP_TAG(UP_CLS_MENU, 0x02)  /* I char**: NULL terminated item texts, "-" is a separator row; must outlive the object */
#define UPA_Menu_Chosen UP_TAG(UP_CLS_MENU, 0x03) /* ISG LONG: index of the item last chosen, -1 before any (notifies on every pick, also the same item again) */
#define UPA_Menu_Open UP_TAG(UP_CLS_MENU, 0x04)   /* ISG BOOL: the popup is open; set TRUE opens, FALSE closes */
#define UPM_Menu_Click UP_TAG(UP_CLS_MENU, 0x10)  /* method: the title button was pressed. Internal */
#define UPM_Menu_Closed UP_TAG(UP_CLS_MENU, 0x11) /* method: the popup window closed. Internal */
#define UPM_Menu_Deactivated UP_TAG(UP_CLS_MENU, 0x12) /* method: the popup window lost activation. Internal */

/* UP-TextArea: a minimal multi-line text editor (word wrap, caret, vertical scroll, Shift and mouse-drag selection, clipboard text via Ctrl or Amiga + C, X, V). Click or the cycle chain focuses it. */
#define UPA_TextArea_Text UP_TAG(UP_CLS_TEXTAREA, 0x01)        /* ISG STRPTR: the text, copied in on set; get returns the object's own NUL terminated buffer (valid until the next edit). Notifies on every change: notify with MUIV_EveryTime, the pointer stays the same */
#define UPA_TextArea_MaxLen UP_TAG(UP_CLS_TEXTAREA, 0x02)      /* IG LONG: capacity in bytes, default 1024; longer text is cut */
#define UPA_TextArea_Placeholder UP_TAG(UP_CLS_TEXTAREA, 0x03) /* I STRPTR hint while empty and not active; must outlive the object */
#define UPA_TextArea_Error UP_TAG(UP_CLS_TEXTAREA, 0x04) /* SG BOOL: outline in the ERROR colour (a validation error names this field); a user edit clears it */
#define UPM_TextArea_Redraw UP_TAG(UP_CLS_TEXTAREA, 0x10)      /* method: the window's active object changed, repaint. Internal */

/* UP-Rectangle: MUIC_Rectangle (spacer, divider bar, bar title). It takes the MUI tags MUIA_Rectangle_HBar, _VBar and _BarTitle (I, readable with get); no UPA_ tags of its own. */

/* UP-Floattext: MUIC_Floattext (read-only word-wrapped text, wheel and scroller scrolling). It takes the MUI tags MUIA_Floattext_Text (ISG, copied), _Justify (ISG), _SkipChars and _TabSize (IS, applied when the text is next set). */
#define UPA_Floattext_First UP_TAG(UP_CLS_FLOATTEXT, 0x01)    /* ISG LONG: top visible line, clamped to the text (notifies) */
#define UPA_Floattext_Scroller UP_TAG(UP_CLS_FLOATTEXT, 0x02) /* I Object*: the UP-Scroller kept in step (UP_NewFloattext sets it itself; a by-name builder passes it) */

/* UP-Tabs */
#define UPA_Tabs_Active UP_TAG(UP_CLS_TABS, 0x01)   /* ISG LONG: index of the active tab (notifies) */
#define UPA_Tabs_Captions UP_TAG(UP_CLS_TABS, 0x02) /* I char**: NULL terminated, at most MUI_AATABS_MAX, must outlive the object */

/* UP-Scroller */
#define UPA_Scroller_First UP_TAG(UP_CLS_SCROLLER, 0x01)   /* ISG LONG: first visible entry (notifies) */
#define UPA_Scroller_Visible UP_TAG(UP_CLS_SCROLLER, 0x02) /* ISG LONG: entries that fit the owner */
#define UPA_Scroller_Entries UP_TAG(UP_CLS_SCROLLER, 0x03) /* ISG LONG: entries in the owner */

/* UP-List */
#define UPA_List_Count UP_TAG(UP_CLS_LIST, 0x01)  /* ISG LONG: rows (notifies) */
#define UPA_List_Active UP_TAG(UP_CLS_LIST, 0x02) /* ISG LONG: selected row, -1 none (notifies) */
#define UPA_List_First UP_TAG(UP_CLS_LIST, 0x03)  /* ISG LONG: first visible row (notifies) */
#define UPA_List_Picked UP_TAG(UP_CLS_LIST, 0x04) /* G LONG: row chosen by click or Return (notifies every time) */
#define UPA_List_Columns UP_TAG(UP_CLS_LIST, 0x05) /* IS struct UP_ListColumns*, copied: the shown columns, may change while live */
#define UPA_List_Playing UP_TAG(UP_CLS_LIST, 0x06) /* IS LONG: row whose first column is drawn in ACCENT, -1 none */
#define UPA_List_RowHook UP_TAG(UP_CLS_LIST, 0x07) /* I struct Hook*: single column text; msg struct UP_ListCell (col 0), returns const char* */
#define UPA_List_CellHook UP_TAG(UP_CLS_LIST, 0x08) /* I struct Hook*: multi column text; row -1 asks the column title; same msg */
#define UPA_List_MaxRows UP_TAG(UP_CLS_LIST, 0x09)  /* I LONG: default height in rows (absent or 0 = 8) */
#define UPA_List_Title UP_TAG(UP_CLS_LIST, 0x0A)    /* I BOOL: a title row above the rows (the cell hook is asked for row -1) */
#define UPA_List_DoublePick UP_TAG(UP_CLS_LIST, 0x0B) /* I BOOL: UPA_List_Picked only on a double click or Return */
#define UPA_List_NoSelect UP_TAG(UP_CLS_LIST, 0x0C) /* I BOOL: no hover, no selection, wheel only */
#define UPA_List_KeysUnfocused UP_TAG(UP_CLS_LIST, 0x0D) /* I BOOL: Up/Down/Return act while no object is active (popup list; MUI draws a white frame round an active object) */
#define UPA_List_Raised UP_TAG(UP_CLS_LIST, 0x0F)    /* I BOOL: resting rows use the Raised token, for a list in a Raised popup panel (I1349) */
#define UPA_List_Scroller UP_TAG(UP_CLS_LIST, 0x10)  /* I Object*: the UP-Scroller the list keeps in step (UP_NewList sets it itself; a by-name builder passes it) */
#define UPA_List_Cancelled UP_TAG(UP_CLS_LIST, 0x0E) /* S BOOL: set TRUE by Esc when UPA_List_KeysUnfocused is on (popup list; notify to close the popup) */
/* UPM_List_GetRow (I713): the read-back method for tools that drive the window from outside. The caller fills row and text/cap;
 * the method returns TRUE when the row exists and sets: text = the cell texts joined by two spaces (truncated to cap), x/y/w/h =
 * the row's box in window coordinates (all 0 when scrolled out of view or not shown), selected = 1 on the active row. */
#define UPM_List_GetRow UP_TAG(UP_CLS_LIST, 0x10)
#define UP_LIST_COLS 5
typedef struct UP_ListColumns { /* weight > 0 shares the free width by weight, weight 0 is as wide as sample[k] (NULL = empty) */
    int n;
    int weight[UP_LIST_COLS];
    const char* sample[UP_LIST_COLS];
} UP_ListColumns;
typedef struct UP_ListCell { /* the message of both list hooks; the hook's h_Data is the owner's context */
    LONG row, col;
} UP_ListCell;
typedef struct UP_ListGetRow {
    ULONG MethodID; /* UPM_List_GetRow */
    LONG row;
    char* text;
    LONG cap;
    LONG x, y, w, h;
    LONG selected;
} UP_ListGetRow;

/* UP-Button */
#define UPA_Button_Caption UP_TAG(UP_CLS_BUTTON, 0x01)     /* ISG STRPTR shown text; a set relayouts the parent (a fit button grows) and updates the tooltip; copied, up to 47 bytes */
#define UPA_Button_Widest UP_TAG(UP_CLS_BUTTON, 0x02)      /* I STRPTR longest caption (sets the minimum width); absent = fit the caption */
#define UPA_Button_Icon UP_TAG(UP_CLS_BUTTON, 0x03)        /* I STRPTR Lucide icon name of the baked set ("play", "skip-forward"); NULL, absent or unknown = none */
#define UPA_Button_AltIcon UP_TAG(UP_CLS_BUTTON, 0x04)     /* I STRPTR icon name swapped in by mui_aabtn_set_alt; NULL, absent or unknown = none */
#define UPA_Button_IconOnly UP_TAG(UP_CLS_BUTTON, 0x05)    /* I BOOL no caption drawn */
#define UPA_Button_Role UP_TAG(UP_CLS_BUTTON, 0x06)        /* I UPV_Button_Role_* */
#define UPA_Button_FixedWidth UP_TAG(UP_CLS_BUTTON, 0x07)  /* I LONG exact width in px, 0 = from the contents */
#define UPA_Button_FixedHeight UP_TAG(UP_CLS_BUTTON, 0x08) /* I LONG exact height in px, 0 = from the contents */
#define UPA_Button_Align UP_TAG(UP_CLS_BUTTON, 0x09)       /* I UPV_Button_Align_* */
#define UPA_Button_IconData UP_TAG(UP_CLS_BUTTON, 0x0A)    /* I struct UP_IconData*: caller icon for a glyph not in the set; used when UPA_Button_Icon names none. The struct and its mask are NOT copied: they must stay valid until the object is disposed */
#define UPA_Button_Alt UP_TAG(UP_CLS_BUTTON, 0x0C)         /* IS BOOL: show UPA_Button_AltIcon instead of UPA_Button_Icon (play/pause); redraws and relayouts when live */
#define UPA_Button_On UP_TAG(UP_CLS_BUTTON, 0x0B)          /* IS BOOL: lit state (a tab strip lights its active tab); redraws when live */
#define UPA_Button_Flat UP_TAG(UP_CLS_BUTTON, 0x0D)        /* I BOOL: no face in the normal state (the parent shows through); hover and press show it. A square fixed size draws a circle */
typedef struct UP_IconData { /* an alpha mask, w x h bytes row by row (0 clear .. 255 full); drawn like a baked icon, scaled to the icon box */
    const unsigned char* alpha;
    LONG w, h;
} UP_IconData;
enum { UPV_Button_Role_Normal, UPV_Button_Role_Accent, UPV_Button_Role_Ghost, UPV_Button_Role_Toggle, UPV_Button_Role_Tab };
enum { UPV_Button_Align_Left, UPV_Button_Align_Centre, UPV_Button_Align_Right };

/* UP-Field */
#define UPM_Field_Redraw UP_TAG(UP_CLS_FIELD, 0x01)     /* method: the window's active object changed, repaint the outline */
#define UPA_Field_Tokens UP_TAG(UP_CLS_FIELD, 0x02)     /* I const uint32_t*: theme token array */
#define UPA_Field_String UP_TAG(UP_CLS_FIELD, 0x03)     /* G Object*: the field itself (it answers MUIA_String_Contents, _Acknowledge, _Accept, _MaxLen); kept for older callers */
#define UPA_Field_Placeholder UP_TAG(UP_CLS_FIELD, 0x04) /* I STRPTR hint while empty and not active; must outlive the field */
#define UPA_Field_MaxWidth UP_TAG(UP_CLS_FIELD, 0x05)   /* I LONG cap of the field's MaxWidth in px, 0 = unbounded */
#define UPA_Field_EscClose UP_TAG(UP_CLS_FIELD, 0x06)   /* I S BOOL: Esc in the active field sets MUIA_Window_CloseRequest on its window (I1360); the field eats Esc either way */
#define UPA_Field_Error UP_TAG(UP_CLS_FIELD, 0x07)     /* SG BOOL: outline in the ERROR colour (a validation error names this field); a user edit clears it */
#define UPM_Field_Edited UP_TAG(UP_CLS_FIELD, 0x08)    /* method: clears UPA_Field_Error and repaints (an edit does the same itself). Internal */
#define UPA_Field_Key UP_TAG(UP_CLS_FIELD, 0x09)        /* G LONG: raw code of the last Up (0x4C), Down (0x4D), Esc (0x45) or Tab (0x42) pressed in the active field; notifies on every press; Up, Down and Shift+Tab then reach the window, Tab cycles to the next object */
/* the string length limit is MUIA_String_MaxLen */

/* UP-Panel */
#define UPA_Panel_Tokens UP_TAG(UP_CLS_PANEL, 0x01)      /* I const uint32_t*: theme token array */
#define UPA_Panel_Role UP_TAG(UP_CLS_PANEL, 0x02)        /* I UPV_Panel_Role_* */
#define UPA_Panel_FixedWidth UP_TAG(UP_CLS_PANEL, 0x03)  /* I LONG > 0: fixed width and height, no inner padding */
#define UPA_Panel_FixedHeight UP_TAG(UP_CLS_PANEL, 0x04) /* I LONG > 0 with FixedWidth: the fixed height when it differs */
#define UPA_Panel_Self UP_TAG(UP_CLS_PANEL, 0x05)        /* G Object*: the panel itself. A MUI Group passes an unknown OM_GET on to its \
                                                            children, so a Role answer counts only when Self names the asked object */
#define UPA_Panel_Popup UP_TAG(UP_CLS_PANEL, 0x06)       /* I BOOL: root of a borderless popup window: square corners, no card padding (I1349) */
#define UPA_Panel_FlatTopLeft UP_TAG(UP_CLS_PANEL, 0x07) /* I BOOL: square top left corner, so the card sits flush against the first tab */
enum { UPV_Panel_Role_Panel, UPV_Panel_Role_Surface, UPV_Panel_Role_Raised, UPV_Panel_Role_Popup, UPV_Panel_Role_Count }; /* Popup: lighter than Raised, so a dropdown stands out from the card */
/* I1371: MUIA_UserData value of a plain MUI Group inside a panel that keeps the spacing it was created with (a tool row of buttons). */
#define UPV_Panel_KeepSpacing 0x55505331UL

/* Shared: tooltip (private to the package) */
#define UPM_Shared_Fire UP_TAG(UP_CLS_SHARED, 0x31) /* private method the timer node sends to the owner */
#define UPA_Shared_Side UP_TAG(UP_CLS_SHARED, 0x32) /* I/S: preferred side, MUI_AA_TIP_* */
/* UPA_Shared_Origin
 *   G  struct Library*  The base of the UP-<Name>.mcc library the object's class was loaded from. Every class answers it; the
 *                       answer is non-NULL for an object made by name from MUI:Libs/mui and NULL (not answered, storage left 0)
 *                       in the static Selecta build. Proof that a program uses the shipped library and not linked-in class code
 *                       (gate item 1, section 10). Not settable, not notified. */
#define UPA_Shared_Origin UP_TAG(UP_CLS_SHARED, 0x33)

/* ---- Class names (U1): the by-name MUI_NewObject() / MUI_NewObjectA() spelling of each class library in MUI:Libs/mui. ---- */
#define MUIC_UP_Button "UP-Button.mcc"
#define MUIC_UP_Text "UP-Text.mcc"
#define MUIC_UP_Slider "UP-Slider.mcc"
#define MUIC_UP_Progress "UP-Progress.mcc"
#define MUIC_UP_Cycle "UP-Cycle.mcc"
#define MUIC_UP_Tabs "UP-Tabs.mcc"
#define MUIC_UP_Scroller "UP-Scroller.mcc"
#define MUIC_UP_List "UP-List.mcc"
#define MUIC_UP_Field "UP-Field.mcc"
#define MUIC_UP_Panel "UP-Panel.mcc"
#define MUIC_UP_Balance "UP-Balance.mcc"
#define MUIC_UP_Rectangle "UP-Rectangle.mcc"
#define MUIC_UP_Floattext "UP-Floattext.mcc"
#define MUIC_UP_Knob "UP-Knob.mcc"
#define MUIC_UP_Levelmeter "UP-Levelmeter.mcc"

/* ---- Constructors: the tag list is the whole interface. Defaults a caller does not give (no frame, cycle chain) are the class's. ----
 * A client gets inlines over MUI_NewObjectA: the class library loads from MUI:Libs/mui, no class code is linked in. The class build
 * itself (UP_MCC_LIBRARY, or UP_MCC_STATIC_CTORS for the static archive) keeps the real constructors. */
#if defined(__amigaos__) || defined(AMIGA)
#if defined(UP_MCC_LIBRARY) || defined(UP_MCC_STATIC_CTORS)
Object* UP_NewButton(const struct TagItem* tags);
Object* UP_NewText(const struct TagItem* tags);
Object* UP_NewSlider(const struct TagItem* tags);
Object* UP_NewProgress(const struct TagItem* tags);
Object* UP_NewCycle(const struct TagItem* tags);
Object* UP_NewTabs(const struct TagItem* tags);
Object* UP_NewScroller(const struct TagItem* tags);
Object* UP_NewField(const struct TagItem* tags);
Object* UP_NewPopstring(const struct TagItem* tags);
Object* UP_NewPoplist(const struct TagItem* tags);
Object* UP_NewPopobject(const struct TagItem* tags);
Object* UP_NewPopasl(const struct TagItem* tags);
Object* UP_NewPopscreen(const struct TagItem* tags);
Object* UP_NewNumericbutton(const struct TagItem* tags);
Object* UP_NewScale(const struct TagItem* tags);
Object* UP_NewColorfield(const struct TagItem* tags);
Object* UP_NewPalette(const struct TagItem* tags);
Object* UP_NewImage(const struct TagItem* tags);
Object* UP_NewMenu(const struct TagItem* tags);
Object* UP_NewTextArea(const struct TagItem* tags);
Object* UP_NewPanel(const struct TagItem* tags);
Object* UP_NewBalance(const struct TagItem* tags);
Object* UP_NewCheckmark(const struct TagItem* tags);
Object* UP_NewRadio(const struct TagItem* tags);
Object* UP_NewVirtgroup(const struct TagItem* tags);   /* MUIA_Group_Child / MUIA_Group_Horiz describe the contents */
Object* UP_NewScrollgroup(const struct TagItem* tags); /* MUIA_Group_Child / MUIA_Group_Horiz describe the contents */
Object* UP_NewKnob(const struct TagItem* tags);       /* MUIC_Knob: Numeric subclass, drag or cursor keys */
Object* UP_NewLevelmeter(const struct TagItem* tags); /* MUIC_Levelmeter: Numeric subclass, MUIA_Levelmeter_Label */
Object* UP_NewList(const struct TagItem* tags, Object** list_out); /* returns the list+scroller group; *list_out the list */
Object* UP_NewRectangle(const struct TagItem* tags);
Object* UP_NewFloattext(const struct TagItem* tags, Object** ft_out); /* returns the text+scroller group; *ft_out the text */
#else
#include <libraries/mui.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h> /* DoMethod for UP_NewList, declared before any other header can imply it */
#define UP_CLIENT_CTOR(fn, cls) static inline Object* fn(const struct TagItem* tags) { return MUI_NewObjectA((char*)(cls), (struct TagItem*)tags); }
UP_CLIENT_CTOR(UP_NewButton, MUIC_UP_Button)
UP_CLIENT_CTOR(UP_NewText, MUIC_UP_Text)
UP_CLIENT_CTOR(UP_NewSlider, MUIC_UP_Slider)
UP_CLIENT_CTOR(UP_NewProgress, MUIC_UP_Progress)
UP_CLIENT_CTOR(UP_NewCycle, MUIC_UP_Cycle)
UP_CLIENT_CTOR(UP_NewTabs, MUIC_UP_Tabs)
UP_CLIENT_CTOR(UP_NewScroller, MUIC_UP_Scroller)
UP_CLIENT_CTOR(UP_NewField, MUIC_UP_Field)
UP_CLIENT_CTOR(UP_NewPanel, MUIC_UP_Panel)
UP_CLIENT_CTOR(UP_NewBalance, MUIC_UP_Balance)
UP_CLIENT_CTOR(UP_NewRectangle, MUIC_UP_Rectangle)
UP_CLIENT_CTOR(UP_NewKnob, MUIC_UP_Knob)
UP_CLIENT_CTOR(UP_NewLevelmeter, MUIC_UP_Levelmeter)
#undef UP_CLIENT_CTOR
/* The list is a UP-List and a UP-Scroller in a horizontal group; the scroller follows the list through UPA_List_First. Returns the group; *list_out the list. */
static inline Object* UP_NewList(const struct TagItem* tags, Object** list_out) {
    struct TagItem sc[] = {{MUIA_Frame, (UP_Data)MUIV_Frame_None}, {TAG_DONE, 0}};
    Object* scroller = MUI_NewObjectA((char*)MUIC_UP_Scroller, sc);
    struct TagItem lt[] = {{UPA_List_Scroller, UP_D(scroller)}, {TAG_MORE, UP_D(tags)}};
    Object* list = MUI_NewObjectA((char*)MUIC_UP_List, lt);
    Object* group = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 0, MUIA_Group_HorizSpacing, 0, Child, (ULONG)list, Child,
                                  (ULONG)scroller, TAG_DONE);
    if (list == NULL || scroller == NULL || group == NULL) {
        if (group != NULL) {
            MUI_DisposeObject(group);
        } else {
            if (list != NULL) MUI_DisposeObject(list);
            if (scroller != NULL) MUI_DisposeObject(scroller);
        }
        return NULL;
    }
    DoMethod(scroller, MUIM_Notify, UPA_Scroller_First, MUIV_EveryTime, list, 3, MUIM_Set, UPA_List_First, MUIV_TriggerValue);
    if (list_out != NULL) *list_out = list;
    return group;
}
#endif
#endif

/* ---- MUI gap D: UP-Pendisplay (MUIC_Pendisplay), UP-Poppen (MUIC_Poppen), UP-Penadjust (MUIC_Penadjust and MUIC_Coloradjust). ----
 * They answer the MUI 3.8 attributes and methods of the classes they replace (MUIA_Pendisplay_*, MUIM_Pendisplay_*, MUIA_Penadjust_PSIMode,
 * MUIA_Coloradjust_*), so the tag values are those of <libraries/mui.h>; only the extras below are UP tags. UP-Poppen is a UP-Pendisplay
 * that opens a UP-Penadjust in a popup on a click. UP-Penadjust also answers MUIA_Pendisplay_Spec (ISG, notifies). */
#define UP_CLS_PENDISPLAY 0x1F /* UP-Pendisplay */
#define UP_CLS_POPPEN 0x20     /* UP-Poppen */
#define UP_CLS_PENADJUST 0x21  /* UP-Penadjust (+ Coloradjust mode) */
#define UPM_Poppen_Click UP_TAG(UP_CLS_POPPEN, 0x10)       /* method: the swatch was pressed (toggle). Internal */
#define UPM_Poppen_Closed UP_TAG(UP_CLS_POPPEN, 0x11)      /* method: the popup window closed. Internal */
#define UPM_Poppen_Deactivated UP_TAG(UP_CLS_POPPEN, 0x12) /* method: the popup window lost activation. Internal */
#define UPA_Penadjust_Coloradjust UP_TAG(UP_CLS_PENADJUST, 0x01) /* I BOOL: RGB sliders only, no pen kind choice (MUIC_Coloradjust); default FALSE */
#define UPM_Penadjust_Changed UP_TAG(UP_CLS_PENADJUST, 0x10)     /* method: a child gadget changed. Internal */
#define MUIC_UP_Pendisplay "UP-Pendisplay.mcc"
#define MUIC_UP_Poppen "UP-Poppen.mcc"
#define MUIC_UP_Penadjust "UP-Penadjust.mcc"
#if (defined(__amigaos__) || defined(AMIGA)) && (defined(UP_MCC_LIBRARY) || defined(UP_MCC_STATIC_CTORS))
Object* UP_NewPendisplay(const struct TagItem* tags);
Object* UP_NewPoppen(const struct TagItem* tags);
Object* UP_NewPenadjust(const struct TagItem* tags);
#endif

/* Theme token names (I685d2): UPTOK_<id> equals SEL_GUI_TOKEN_<id> for every token of the SEL_GUI_TOKENS list in
 * selecta/uimodel/tokens.h. The names are generated from that list, so a token added or removed there appears or goes here. */
#include <selecta/uimodel/tokens.h>
enum {
#define UPTOK_X(id, name, def, lo, hi) UPTOK_##id = SEL_GUI_TOKEN_##id,
    SEL_GUI_TOKENS(UPTOK_X)
#undef UPTOK_X
    UPTOK_END_
};

#endif

/* Support library functions (UP_ThemeLoad, UP_IconFind, ...). */
#include "up_lib.h"
