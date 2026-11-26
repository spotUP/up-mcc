/* I665 / I662 spike: pure geometry and colour picks for the flat rounded button and its antialiased caption (aa_draw).
 * No Amiga calls, so the host test reaches them. Plan 2026-10-25-amiga-mui.md section "MUI theme (I648, in-code half)":
 * every colour comes from a theme token; this module only derives from them. */
#ifndef SELECTA_MUI_AA_H
#define SELECTA_MUI_AA_H

#include <stdint.h>

#include <selecta/uimodel/tokens.h>
#include <mcc/up.h>
#include <mcc/up_metrics.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pixels left uncovered at the left and right end of row y (0 = top) of a w x h rounded rectangle with corner radius r
 * (clamped to h/2 and w/2). Row centres are sampled against the corner circle; the middle rows return 0. */
int mui_aa_round_inset(int w, int h, int r, int y);
/* I685w (design 5.2 item 1): 1 when the MCC fills write exact RGB (screen of 15+ bits and Picasso96API or cybergraphics open),
 * 0 when they go through palette pens (8 bits or less, or no RTG library). */
int mui_aa_truecolour(int depth, int have_p96, int have_cgx);
/* Ramp slot (0 = background .. steps-1 = full foreground) for an 8-bit glyph coverage; steps >= 2. */
int mui_aa_ramp_index(int coverage, int steps);
/* Colour (0xRRGGBB) of ramp slot i between bg (slot 0) and fg (slot steps-1). */
uint32_t mui_aa_ramp_rgb(uint32_t fg, uint32_t bg, int steps, int i);
/* Pressed face derived from the raised face token: darker on a light face, lighter on a dark one (no bevel pens). */
uint32_t mui_aa_pressed_rgb(uint32_t face);

/* I667a (design 2026-10-28-mui-custom-classes.md section 2): shared state, tier and metric tables of every Selecta_AA class.
 * Colours come from the token array (txt_theme.token), never from literals here. */
enum { MUI_AA_NORMAL, MUI_AA_HOVER, MUI_AA_PRESSED, MUI_AA_DISABLED, MUI_AA_SELECTED };
/* TOGGLE (I667b-2, AACheck): checkbox box; off = SURFACE face (desktop checkbox), on (state SELECTED) = ACCENT face, ink ACCENT_TEXT. */
/* TAB (I667j, AATabs): a tab caption on the PANEL, TEXT_MUTED idle, TEXT while its tab is active (state SELECTED); the face never changes
 * but for the hover token. */
enum { MUI_AA_ROLE_NORMAL = UPV_Button_Role_Normal, MUI_AA_ROLE_ACCENT = UPV_Button_Role_Accent, MUI_AA_ROLE_GHOST = UPV_Button_Role_Ghost,
       MUI_AA_ROLE_TOGGLE = UPV_Button_Role_Toggle, MUI_AA_ROLE_TAB = UPV_Button_Role_Tab };
/* Screen tiers by rows (section 3): S 640x256 and 320x256, M up to 600 rows, L up to 800, XL above. */
enum { MUI_AA_TIER_S, MUI_AA_TIER_M, MUI_AA_TIER_L, MUI_AA_TIER_XL };
/* Coverage ramp kinds (section 4): AGA four held token pens, RTG eight blended pens, TC none (ttengine blends). */
enum { MUI_AA_RAMP_AGA, MUI_AA_RAMP_RTG, MUI_AA_RAMP_TC };
#define MUI_AA_RAMP_MAX 8

/* Face colour of a control in a state: normal = face, hover = SURFACE_HOVER, pressed and selected = SURFACE_PRESSED,
 * disabled = the normal face. */
uint32_t mui_aa_state_rgb(const uint32_t* token, uint32_t face, int state);
/* Caption colour: text, or TEXT_DISABLED when disabled. */
uint32_t mui_aa_text_rgb(const uint32_t* token, uint32_t text, int state);
/* Face and caption of a button role in a state (accent: ACCENT and ACCENT_TEXT; ghost: PANEL, selected SURFACE_RAISED). */
void mui_aa_button_rgb(const uint32_t* token, int role, int state, uint32_t* face, uint32_t* text);
/* AACheck box paint plan (design 3.1, desktop d_checkbox): ops painted in order into the check_px square, each a rounded fill inset
 * from the box edge. Op 0 fills the square corners with the parent background (SURFACE = MUI_TH_PANEL); checked = one ACCENT
 * box; off = TEXT_MUTED outline (TEXT_DISABLED when disabled) then the face inside it. Corner radius = RADIUS_SMALL (tier S half). */
typedef struct mui_aa_check_op_t {
    int inset, radius;
    uint32_t rgb;
} mui_aa_check_op_t;
#define MUI_AA_CHECK_OPS 3
int mui_aa_check_plan(const uint32_t* token, const up_metrics_t* m, int state, mui_aa_check_op_t* out);
/* Number of times the three functions above ran: the sentinel that the classes (and not a copy) pick the colours. */
uint32_t mui_aa_state_calls(void);
/* Coverage ramp for a kind into out[] (>= MUI_AA_RAMP_MAX); returns the slot count. AGA: face, TEXT_DISABLED, TEXT_MUTED, TEXT
 * (fg is ignored); RTG: mui_aa_ramp_rgb steps; TC: 0 slots. */
int mui_aa_ramp_pens(int kind, const uint32_t* token, uint32_t fg, uint32_t bg, uint32_t* out);
/* Slot of an 8-bit coverage in a ramp kind (AGA quarters, RTG eight steps, TC always the top slot). */
int mui_aa_ramp_slot(int kind, int coverage);
int mui_aa_tier(int screen_rows);

/* I667d (design 3.7, AAText): text tones, alignment and the ellipsis cut. Pure maths, tested on the host. */
enum { MUI_AA_TONE_NORMAL = UPV_Text_Tone_Normal, MUI_AA_TONE_MUTED = UPV_Text_Tone_Muted, MUI_AA_TONE_TITLE = UPV_Text_Tone_Title, MUI_AA_TONE_HEADING = UPV_Text_Tone_Heading };
enum { MUI_AA_ALIGN_LEFT = UPV_Text_Align_Left, MUI_AA_ALIGN_CENTRE = UPV_Text_Align_Centre, MUI_AA_ALIGN_RIGHT = UPV_Text_Align_Right };
#define MUI_AA_TITLE_EXTRA_PX 4
/* I685w run 5: heading tone = twice the base size (Mac heading 32 px over a 16 px body); the bitmap tier stays on its font. */
#define MUI_AA_HEADING_FACTOR 2
/* I685w run 5: card padding = this many SPACING units (Mac 24 px = 3 x 8). */
#define MUI_AA_PANEL_PAD_UNITS 3
/* SEL_GUI_TOKEN_* a tone draws in: normal and title TEXT, muted TEXT_MUTED. */
int mui_aa_tone_token(int tone);
/* Font pixel size of a tone for the screen base size from up_font_px: title = base + 4, others = base; the bitmap tier (0) stays 0. */
int mui_aa_tone_font_px(int tone, int base_px);
/* Left edge of content of width cw in a box of width w for an alignment (never left of 0). */
int mui_aa_align_x(int align, int w, int cw);
/* I667b4: left edge of a button's icon+caption content of width cw in a button of width w: the alignment inside the box inset by
 * pad_x on both sides (mui_aa_align_x); centre = (w - cw) / 2, left = pad_x, right = w - cw - pad_x. */
int mui_aa_btn_content_x(int align, int w, int cw, int pad_x);
/* I1341: caption room of a button of width w: the box inset by pad_x on both sides, less the icon and its gap when it has one (never
 * below 0). A caption wider than this is cut with "..." (mui_aa_text_fit) so a button never draws past its face. */
int mui_aa_btn_text_room(int w, int pad_x, int icon_px, int icon_gap, int has_icon);
/* Ink width of the first len bytes of s, supplied by the drawing side (ttengine pixmap on the Amiga, an advance table in the test). */
typedef int (*mui_aa_width_fn)(void* ctx, const char* s, int len);
/* Text that fits max_w: s itself when it fits, else the longest prefix plus "..." that fits (just "..." when not even one character
 * does, empty when max_w cannot hold the dots). out gets at most cap-1 bytes; returns the length written. */
int mui_aa_text_fit(const char* s, int max_w, mui_aa_width_fn width, void* ctx, char* out, int cap);
/* I667f3: the widest content a text line asks for. A fixed line (the seek row time labels) takes only its default width defw so the
 * seek slider gets the rest of the row; any other line is unbounded (MUI_AA_TEXT_UNBOUNDED, the class maps it to MUI_MAXMAX). */
#define MUI_AA_TEXT_UNBOUNDED (-1)
int mui_aa_text_maxw(int fixed, int defw);

/* I667e (design 3.8, AAPanel): rounded panel groups. Role -> face token (panel PANEL, surface SURFACE, raised SURFACE_RAISED),
 * corner radius RADIUS_LARGE (tier S, font_px 0: half), inner padding and spacing SPACING. Tested on the host. */
enum { MUI_AA_PANEL_PANEL = UPV_Panel_Role_Panel, MUI_AA_PANEL_SURFACE = UPV_Panel_Role_Surface, MUI_AA_PANEL_RAISED = UPV_Panel_Role_Raised, MUI_AA_PANEL_POPUP = UPV_Panel_Role_Popup,
       MUI_AA_PANEL_ROLES = UPV_Panel_Role_Count };
int mui_aa_panel_token(int role);
uint32_t mui_aa_panel_rgb(const uint32_t* token, int role);
/* What an object (a panel's corner wedges included) sits on: the role colour of the nearest UP-Panel ancestor, parent_role < 0 when
 * there is none (the window BACKGROUND). A raised card on a SURFACE panel rounds its corners in SURFACE, not in BACKGROUND. */
uint32_t mui_aa_parent_rgb(const uint32_t* token, int parent_role);
int mui_aa_panel_radius(const uint32_t* token, int font_px);
int mui_aa_panel_spacing(const uint32_t* token);
/* I949e2: group inner padding and spacing across (pad_x) and down (pad_y) for a screen of aspect_q8. 256 gives SPACING both ways. */
void mui_aa_panel_pad(const uint32_t* token, int aspect_q8, int* pad_x, int* pad_y);
/* I685w run 6: scale a pad from mui_aa_panel_pad for the role: a Surface (nested) card takes half. */
void mui_aa_panel_role_pad(int role, int* pad_x, int* pad_y);

/* I667f (design 3.2, AASlider and AAProgress): track height and radius per tier, the colours by state, and the value <-> pixel maths.
 * No knob (owner decision 2026-10-29): only the TRACK track and the ACCENT fill are drawn, like the Mac desktop seek bar. */
/* Track and filled part colours in a state (MUI_AA_NORMAL, _HOVER, _DISABLED): TRACK, ACCENT; disabled fill TEXT_DISABLED. Any out
 * pointer may be NULL. */
void mui_aa_slider_rgb(const uint32_t* token, int state, uint32_t* track, uint32_t* fill);
/* Filled pixels of a track of width w for value in [lo, hi] (clamped). */
int mui_aa_slider_fill_w(int w, long value, long lo, long hi);
/* Value for a pointer at box x, clamped to [lo, hi]. */
long mui_aa_slider_value_at(int w, int x, long lo, long hi);
/* Paint plan of an update draw (box-relative ops, painted in order). Op 0 is ALWAYS the erase of the full box to the parent
 * background (MADF_DRAWUPDATE does not repaint it, so without it every earlier fill stays visible), then the TRACK pill, then the
 * ACCENT fill pill when the fill is wider than 0. There is no knob op. Returns the op count (max MUI_AA_SLIDER_OPS). */
enum { MUI_AA_SLIDER_ERASE, MUI_AA_SLIDER_TRACK, MUI_AA_SLIDER_FILL };
typedef struct mui_aa_slider_op_t {
    int kind, x, y, w, h, radius;
    uint32_t rgb;
} mui_aa_slider_op_t;
#define MUI_AA_SLIDER_OPS 3
int mui_aa_slider_plan(const uint32_t* token, const up_slider_t* m, int state, int w, int h, long value, long lo, long hi, mui_aa_slider_op_t* out);
/* I739: partial-draw plan of a value change (no erase): fill only when it grew, track then fill when it shrank, 0 ops when the fill width
 * equals prev_fw (prev_fw < 0: never drawn, so always paints both). mui_aa_slider_partial_count counts the non-empty plans. */
int mui_aa_slider_update_plan(const uint32_t* token, const up_slider_t* m, int state, int w, int h, int prev_fw, long value, long lo, long hi,
                              mui_aa_slider_op_t* out);
uint32_t mui_aa_slider_partial_count(void);
/* AAProgress: filled pixels of a bar of width w for value of max (clamped, max <= 0 gives 0). */
int mui_aa_progress_fill_w(int w, long value, long max);
/* Calls of the slider/progress maths above: the sentinel that the classes (and not a copy) run it. */
uint32_t mui_aa_slider_calls(void);

/* I667g (design 3.3, AACycle): face, text, chevron and popup colours by state, the chevron triangle, the minimum width, and the
 * Up/Down step with wrap. Tested on the host (mui-cycle-step). Chevron 8 px wide, gap 8 (tier S: 6 and 6). */
typedef struct mui_aa_cycle_t {
    int height, pad_x, radius, chev_w, chev_h, gap;
} mui_aa_cycle_t;
void mui_aa_cycle_metrics(int font_px, int aspect_q8, mui_aa_cycle_t* out);
/* Face SURFACE_RAISED (hover/pressed per state), value text TEXT (TEXT_DISABLED when disabled), chevron TEXT_MUTED (TEXT_DISABLED when
 * disabled), popup background SURFACE, popup row hover SURFACE_HOVER. Any out pointer may be NULL. */
void mui_aa_cycle_rgb(const uint32_t* token, int state, uint32_t* face, uint32_t* text, uint32_t* chevron, uint32_t* popup_bg, uint32_t* row_hover);
/* Minimum width: padding both sides, the widest entry, the gap and the chevron. */
int mui_aa_cycle_min_w(const mui_aa_cycle_t* m, int widest_text_w);
/* Row r (0 .. chev_h-1, top first) of the downward chevron triangle: x offset from the chevron left edge and the width. */
void mui_aa_cycle_chevron_row(const mui_aa_cycle_t* m, int r, int* x, int* w);
/* Active entry after a step of dir (-1 up, +1 down) in n entries, wrapping at both ends; n <= 0 gives 0. */
int mui_aa_cycle_step(int active, int n, int dir);
/* Whether v is a valid UPA_Cycle_Active for n entries (a popup pick of "no row" is ignored). */
int mui_aa_cycle_valid(int v, int n);
/* Calls of the cycle maths above: the sentinel that the class (and not a copy) runs it. */
uint32_t mui_aa_cycle_calls(void);

/* I667h (design 3.4, AAField): the string gadget's flat rounded field. Face SURFACE, 1 px outline DIVIDER (FOCUS_RING while the field
 * is the active object), radius per tier (RADIUS 6, tier S 4), height as AAButton, inner pad 8 on every tier (more than the radius,
 * so the inner stock String never reaches the rounded corners). Tested on the host (mui-field-plan). */
typedef struct mui_aa_field_t {
    int height, pad_x, pad_y, radius, outline;
} mui_aa_field_t;
void mui_aa_field_metrics(int font_px, int aspect_q8, mui_aa_field_t* out);
/* Paint plan, painted in order, box-relative: op 0 the outline colour over the whole box (rounded), op 1 the face inset by the outline.
 * Returns the op count (MUI_AA_FIELD_OPS). */
#define MUI_AA_FIELD_OPS 2
/* Field face and outline tokens (I667h2): the face is one step lighter than the SURFACE panels the fields sit on, as the
 * desktop search field is lighter than its bar; the outline is TRACK so the edge reads on AGA too. */
#define MUI_AA_FIELD_FACE SEL_GUI_TOKEN_SURFACE_RAISED
#define MUI_AA_FIELD_LINE SEL_GUI_TOKEN_TRACK
/* focus: 0 idle, 1 active (FOCUS_RING), MUI_AA_FIELD_ERROR (UPA_Field_Error / UPA_TextArea_Error set: the outline turns ERROR, over focus). */
#define MUI_AA_FIELD_ERROR 2
int mui_aa_field_plan(const uint32_t* token, const mui_aa_field_t* m, int focus, mui_aa_check_op_t* out);
/* I667n: background of the inner String gadget in a focus state: the face of the plan (op 1). MUI 3.8 String fills its box with the
 * gadget's StringExtend background pen (Pens[1], ActivePens[1] when active), not with MUIA_Background, so the class sets that pen. */
uint32_t mui_aa_field_string_bg(const uint32_t* token, int focus);
/* Text of the inner String gadget (Pens[0], ActivePens[0]): the theme TEXT. The MUI prefs string pens (black by default) were
 * unreadable on a dark theme face. */
uint32_t mui_aa_field_string_fg(const uint32_t* token);
/* Smallest field width that shows the placeholder (ink width pw) uncut: the text fit margin (4) plus the inner pad on both sides.
 * 0 when there is no placeholder. */
int mui_aa_field_min_width(const mui_aa_field_t* m, int pw);
/* I667n: the gadget MUIA_Gadget_Gadget hands back carries a StringInfo with the pens: a classic GTYP_STRGADGET, or the
 * GTYP_CUSTOMGADGET (strgclass) MUI 3.8 String really makes (rig 2: GadgetType 0005). */
int mui_aa_field_string_gadget(unsigned gadget_type);
/* Field height for a content (text) height ch: the AAButton height, taller when the text needs it. */
int mui_aa_field_height(const mui_aa_field_t* m, int ch);
/* Top edge of the text line (height ch) inside a field of height h: the room above and below the line is equal, so the group
 * centres the inner string with one flexible VSpace(0) on each side (I667h4). */
int mui_aa_field_text_top(const mui_aa_field_t* m, int h, int ch);
/* I733 (design 3.4): the placeholder of an empty, unfocused field. Returns 1 and the TEXT_MUTED colour when contents is empty (or NULL)
 * and the field is not the active object, else 0. Every 1 counts on mui_aa_field_placeholders(), the sentinel that the draw painted one. */
int mui_aa_field_placeholder(const uint32_t* token, const char* contents, int focus, uint32_t* rgb);
uint32_t mui_aa_field_placeholders(void);
/* D3: the contents of a filled, unfocused field. MUI 3.8 String renders its inactive contents in its own MUI prefs pen (black on the
 * dark face), past MPEN_TEXT and the StringExtend pens; only the active gadget (typing) uses those. Returns 1 and the theme TEXT
 * colour when contents is not empty and the field is not the active object, else 0 (the String draws it while active). */
int mui_aa_field_text(const uint32_t* token, const char* contents, int focus, uint32_t* rgb);
/* Calls of the field maths above: the sentinel that the class (and not a copy) runs it. */
uint32_t mui_aa_field_calls(void);

/* I667i (design 3.5, AAScroller): the flat thumb-only scroll bar. Width 10 (tier S 6), thumb radius 4 (S 2), thumb minimum 20 high, track
 * SURFACE (the list face), thumb TRACK, SURFACE_PRESSED while hovered or dragged. No arrow buttons. Tested on the host (mui-scroller-geom). */
void mui_aa_scroller_rgb(const uint32_t* token, int state, uint32_t* track, uint32_t* thumb);
/* Thumb top and height inside a track of height h for the first visible entry; both 0 and a 0 return when everything fits. The thumb
 * is at least min_thumb (at most h); first is clamped to [0, entries - visible]. */
int mui_aa_scroller_geom(const up_scroller_t* m, int h, long first, long visible, long entries, int* thumb_y, int* thumb_h);
/* First entry for a pointer at track y while the thumb is held grab pixels below its top; clamped to the ends. */
long mui_aa_scroller_first_at(const up_scroller_t* m, int h, int grab, int y, long visible, long entries);
/* SELECTDOWN at track y: on the thumb *grab is y minus the thumb top (a drag starts) and first is returned as is; on the track *grab is
 * -1 and the first entry one page towards the pointer is returned. No thumb (everything fits): first, *grab -1. */
long mui_aa_scroller_press(const up_scroller_t* m, int h, int y, long first, long visible, long entries, int* grab);
uint32_t mui_aa_scroller_calls(void);

/* I667i (design 3.6, AAList; popup look 3.3): row model maths of the virtual list. Row height font + 8 (tier S 10), left pad 10, a vertical pad
 * of 4 (S 2) inside the face above the first and below the last row, column gap 8. Rows: SURFACE, hover SURFACE_HOVER, selected
 * SURFACE_PRESSED, playing title ACCENT. Tested on the host (mui-list-rows). */
enum { MUI_AA_LIST_KEY_UP, MUI_AA_LIST_KEY_DOWN, MUI_AA_LIST_KEY_PAGEUP, MUI_AA_LIST_KEY_PAGEDOWN, MUI_AA_LIST_KEY_HOME, MUI_AA_LIST_KEY_END };
/* Whole rows that fit a list face of height h (after the vertical pads). */
int mui_aa_list_visible(const up_list_t* m, int h);
long mui_aa_list_clamp_first(long first, long visible, long count);
/* Active row clamped into [0, count - 1]; -1 for an empty list. */
long mui_aa_list_clamp_active(long active, long count);
/* Active row after a key (MUI_AA_LIST_KEY_*), clamped (no wrap); an empty list gives -1, no active row starts at the first or last. */
long mui_aa_list_move(long active, long count, long visible, int key);
/* First row after scrolling just enough to show the active row. */
long mui_aa_list_scroll_to(long first, long visible, long count, long active);
/* Row under a pointer at list-relative y (-1 on the pads or past the last row). */
long mui_aa_list_row_at(const up_list_t* m, int y, long first, long count);
/* State of a list row: MUI_AA_DISABLED when bit row (0..31) of dis is set, else _SELECTED when sel, _HOVER when row == hover, else _NORMAL. */
int mui_aa_list_row_state(unsigned long dis, long row, int sel, long hover);
/* Row face (MUI_AA_NORMAL, _HOVER, _SELECTED, _DISABLED = the resting face) and text colour (TEXT_DISABLED when disabled, else ACCENT when
 * playing). Any out pointer may be NULL. */
void mui_aa_list_row_rgb(const uint32_t* token, int row_state, int playing, uint32_t* face, uint32_t* text);
uint32_t mui_aa_list_calls(void);
/* Left edge (list-relative x) of the text of column col (0-based) when column k is w[k] wide: pad_x for column 0, then each column adds its
 * width, the gap on both sides of the divider and the 1 px divider. w may be NULL for column 0. Johan 2026-10-29: text sat 5 px from the frame
 * and from the dividers; the target is pad_x at the edge and gap (8) on each side of a divider, header row included. */
int mui_aa_list_col_x(const up_list_t* m, int col, const int* w);
/* MUIA_List_Format DELTA of a column followed by a divider (BAR): MUI centres the BAR in the delta, so gap on each side needs 2 * gap. */
/* I667i2: column widths of a multi-column list face of width total. Column k with weight[k] > 0 shares what is left after the fixed columns
 * (fixed[k] px each, weight 0) and the dividers (2 * gap + 1 each) and both pads, by weight; out[k] is its ink width (never negative). */
void mui_aa_list_col_widths(const up_list_t* m, int total, int n, const int* weight, const int* fixed, int* out);
/* Height of the title row (the column header): row_h when the list has one, else 0. */
int mui_aa_list_title_h(const up_list_t* m, int has_title);
/* Secondary column text (TEXT_MUTED, also the header text) and the header divider / column divider colour (DIVIDER). Any out pointer may be NULL. */
void mui_aa_list_header_rgb(const uint32_t* token, uint32_t* muted, uint32_t* divider);

/* I667j (design 3.9, AATabs): a row of tab captions with a 2 px ACCENT underline under the active one. Tested on the host (mui-tabs). */
#define MUI_AA_TABS_LINE_PX 2
/* Index after a click on tab i (out of range gives -1: no change) and after a key (Left 0x4F -1, Right 0x4E +1, clamped, no wrap; n <= 0 gives 0). */
int mui_aa_tabs_click(int i, int n);
int mui_aa_tabs_key(int code, int active, int n);
/* Underline rectangle for the active tab whose box is (x, y, w, h): full tab width, MUI_AA_TABS_LINE_PX high (tier S, font_px 0: 1), at the bottom edge. */
void mui_aa_tabs_line(int font_px, int x, int y, int w, int h, int* lx, int* ly, int* lw, int* lh);
/* Underline ACCENT; caption colour idle TEXT_MUTED, active TEXT. Any out pointer may be NULL. */
void mui_aa_tabs_rgb(const uint32_t* token, uint32_t* line, uint32_t* idle_text, uint32_t* active_text);
uint32_t mui_aa_tabs_calls(void);

/* I667l (design 3.10, AABalance): the pane divider is the panel gap itself. Box width = SPACING token (tier S, font_px 0: half), one pixel
 * DIVIDER line centred in it, inset by the same gap top and bottom. Box colour BACKGROUND. Tested on the host (mui-balance-geom). */
int mui_aa_balance_width(const uint32_t* token, int font_px);
/* Line rectangle inside a box of box_w x box_h (origin 0,0); inset is the gap of that tier; a box shorter than two insets gets inset 0 (clamp). */
void mui_aa_balance_line(const uint32_t* token, int font_px, int box_w, int box_h, int* lx, int* ly, int* lw, int* lh);
/* Box colour BACKGROUND, line colour DIVIDER. Any out pointer may be NULL. */
void mui_aa_balance_rgb(const uint32_t* token, uint32_t* box, uint32_t* line);
uint32_t mui_aa_balance_calls(void);

/* I731 (plan 2026-10-31-I731-mui-flat-tooltips.md section 2.1): the flat tooltip. Pure plan, placement and hover state; the Amiga module
 * (../mui_aatip.c) draws and times. Tested on the host (mui-tooltip-flat). */
enum { MUI_AA_TIP_ABOVE, MUI_AA_TIP_BELOW, MUI_AA_TIP_RIGHT, MUI_AA_TIP_LEFT };
typedef struct mui_aa_tip_t {
    int w, h, pad_x, pad_y, radius, gap;
    uint32_t face, text, corner; /* 0xRRGGBB */
} mui_aa_tip_t;
/* Box size and colours: face = SURFACE_PRESSED, text = TEXT, corner = SURFACE (the panel colour the wedges outside the rounded box are
 * painted with), radius = RADIUS_SMALL clamped to h / 2, pad_x = SPACING, pad_y = SPACING / 2 (tier S: font_px <= 0 -> pad_x 6, pad_y 3,
 * radius 4), gap 4 (tier S 2). w = text_w + 2 * pad_x, h = text_h + 2 * pad_y. */
void mui_aa_tip_plan(const uint32_t* token, int font_px, int text_w, int text_h, mui_aa_tip_t* out);
/* Screen position of a w x h box next to the anchor rect (screen coordinates), side pref first; ABOVE that does not fit flips BELOW and back,
 * RIGHT flips LEFT and back; the box is then clamped inside 0..scr_w-w, 0..scr_h-h. Returns the side used. */
int mui_aa_tip_place(int ax, int ay, int aw, int ah, int w, int h, int scr_w, int scr_h, int gap, int pref, int* x, int* y);
/* The hover state machine (plan section 4). */
enum { MUI_AA_TIP_IDLE, MUI_AA_TIP_PENDING, MUI_AA_TIP_SHOWN, MUI_AA_TIP_MUTED };
enum { MUI_AA_TIP_EV_ENTER, MUI_AA_TIP_EV_LEAVE, MUI_AA_TIP_EV_TIMER, MUI_AA_TIP_EV_PRESS, MUI_AA_TIP_EV_KEY, MUI_AA_TIP_EV_INACTIVE, MUI_AA_TIP_EV_GONE };
enum { MUI_AA_TIP_DO_NONE = 0, MUI_AA_TIP_DO_ARM = 1, MUI_AA_TIP_DO_DISARM = 2, MUI_AA_TIP_DO_SHOW = 4, MUI_AA_TIP_DO_HIDE = 8 };
int mui_aa_tip_step(int state, int ev, int* actions); /* returns the next state, ORs the actions into *actions */
#define MUI_AA_TIP_DELAY_MS 600 /* desktop TIP_DELAY_MS */
uint32_t mui_aa_tip_calls(void); /* sentinel: mui_aa_tip_plan calls */

/* I973b: an icon button's size request, pure. fixed_w / fixed_h in px (0 = none). The deltas are added to the Area's own insets.
 * A fixed side has min = def = max, so no weight can stretch the square; a labelled button keeps the unbounded width. */
typedef struct mui_btn_box {
    uint32_t min_w, def_w, min_h, def_h;
    uint32_t w_fixed; /* 1: MaxWidth = MinWidth, else unbounded */
    uint32_t h_fixed; /* always 1 today: a button never grows taller than its contents */
} mui_btn_box_t;
void mui_aabtn_minmax(uint32_t content_w, uint32_t content_h, uint32_t fixed_w, uint32_t fixed_h, uint32_t toggle, mui_btn_box_t* out);

/* UP-Popstring / UP-Poplist popup state, pure (host test mui-pop-state). One popup window per object, created at the first open and
 * REUSED: it is never disposed while it may still be running its own notification (the UP-Cycle crash on the second click came from
 * leaving a closed popup half alive). A click on the button while the popup is open closes it. Closing by window deactivation (the
 * click that lands on the button first deactivates the popup) is remembered for MUI_AA_POP_GUARD_MS so that same click toggles shut
 * instead of reopening. */
#define MUI_AA_POP_GUARD_MS 300
enum { MUI_AA_POP_NONE, MUI_AA_POP_OPEN_NEW, MUI_AA_POP_OPEN_REUSE, MUI_AA_POP_CLOSE };
typedef struct mui_aa_pop_t {
    int created, open, by_deactivate;
    uint32_t closed_ms;
} mui_aa_pop_t;
int mui_aa_pop_click(mui_aa_pop_t* s, uint32_t now_ms);           /* the action the caller must take; updates nothing but the guard */
void mui_aa_pop_opened(mui_aa_pop_t* s);                         /* the window is now open (also the first time) */
void mui_aa_pop_closed(mui_aa_pop_t* s, int by_deactivate, uint32_t now_ms); /* the window closed, for any reason; idempotent */

/* UP-Numericbutton / UP-Scale pure maths (host test mui-numeric-scale). The button shows value as text; a drag of dx pixels from the
 * value at the press moves it by dx * (hi - lo) / (2 * w) (a travel of two button widths sweeps the range), clamped; fewer than
 * MUI_AA_NUM_DRAG_SLOP pixels is a click (opens the popup slider). */
#define MUI_AA_NUM_DRAG_SLOP 3
long mui_aa_numeric_clamp(long v, long lo, long hi);
long mui_aa_numeric_drag(long start, int dx, int w, long lo, long hi);
int mui_aa_numeric_text(long v, char* out, int cap); /* decimal text, NUL terminated; returns its length */
/* Tick ruler of a width w: ticks (clamped 2..MUI_AA_SCALE_MAX) evenly spread over x 0..w-1, x[0] = 0 and x[n-1] = w-1. A tick is major
 * when it is an end or the middle of an odd count; value is lo..hi at that tick (rounded). hi <= lo or w < 2 gives 0 ticks. */
#define MUI_AA_SCALE_MAX 64
typedef struct mui_aa_scale_tick_t {
    int x, major;
    long value;
} mui_aa_scale_tick_t;
int mui_aa_scale_plan(int w, long lo, long hi, int ticks, mui_aa_scale_tick_t* out);
/* Tick lengths for a font of font_px: minor = major / 2 (at least 2), major = font_px / 2 (at least 4), gap above the labels. */
void mui_aa_scale_metrics(int font_px, int* minor_h, int* major_h, int* gap);

/* UP-Palette grid maths (host test mui-palette). Cells of cw x ch pixels with gap pixels between them, row-major, cols per row. */
int mui_aa_palette_rows(int n, int cols);                /* rows needed for n cells */
void mui_aa_palette_cell(int i, int cols, int cw, int ch, int gap, int* x, int* y); /* top-left of cell i */
int mui_aa_palette_hit(int x, int y, int cols, int n, int cw, int ch, int gap);     /* cell under x,y (box-relative), -1 on a gap or outside */
/* Cursor key step: dx/dy of -1,0,1. No wrap; a step down into a short last row lands on the last cell; a step off the grid stays. */
int mui_aa_palette_move(int active, int n, int cols, int dx, int dy);

/* UP-Image (host test mui-image-fit). Size of an sw x sh picture shown in a bw x bh box: fit=0 the picture size, fit=1 scaled to the box
 * keeping the aspect (never below 1 pixel); the offsets centre it in the box (may be negative when the picture is larger). */
void mui_aa_image_fit(int sw, int sh, int bw, int bh, int fit, int* dw, int* dh, int* ox, int* oy);
/* UP-Image body (host test mui-image-body). Unpack an ILBM BODY of w x h, depth planes (compression 0 none, 1 ByteRun1) into planes[p]
 * rows of bpr bytes. has_mask: a mask plane follows each row's planes; it goes to mask (rows of mbpr bytes) or is skipped when mask is NULL.
 * Returns 0 for an unknown compression or bad sizes. ByteRun1 reads no length: the body must hold all rows. */
int mui_aa_body_unpack(const unsigned char* body, int compression, int w, int h, int depth, int has_mask, unsigned char* const* planes, int bpr,
                       unsigned char* mask, int mbpr);

/* UP-Menu check state (host test mui-menu-check). Mask of checked items after item i (0..31) is picked: no checkit = unchanged; unchecked
 * = checked and the exclude items cleared; checked = unchecked with toggle, else unchanged. */
unsigned long mui_aa_menu_pick(unsigned long checked, int i, int checkit, int toggle, unsigned long exclude);

/* UP-Knob / UP-Levelmeter dial maths (host test mui-dial). Angles are whole degrees, 0 = right, counter clockwise positive; a dial
 * starts at start_deg for lo and turns clockwise through sweep_deg to hi. Knob: 225 / 270 (lower left round to lower right);
 * Levelmeter: 180 / 180 (left through top to right). mui_aa_dial_point gives the screen offset (y down) of radius r at deg. */
#define MUI_AA_KNOB_START 225
#define MUI_AA_KNOB_SWEEP 270
#define MUI_AA_METER_START 180
#define MUI_AA_METER_SWEEP 180
int mui_aa_dial_sin_q14(int deg); /* sin(deg) * 16384, any deg */
int mui_aa_dial_deg(long v, long lo, long hi, int start_deg, int sweep_deg);
void mui_aa_dial_point(int deg, int r, int* dx, int* dy);
/* Knob keys: raw code 0x4C up / 0x4E right add one, 0x4D down / 0x4F left take one; with shift a tenth of the range (at least one).
 * Returns the new clamped value, or v unchanged (and *used 0) for any other key. */
long mui_aa_knob_key(long v, long lo, long hi, int code, int shift, int* used);

/* UP-Popasl string <-> ASL requester mapping, pure (host test mui-popasl). No dos.library: the class libraries do not open it.
 * File: drawer/file split and join as PathPart/FilePart/AddPart do ("Work:a/b" <-> "Work:a" + "b", "RAM:" + "x" -> "RAM:x").
 * Font: MUI's "name/size" spec without ".font"; parse puts ".font" back for ASLFO_InitialName (size 0 when absent).
 * Screen mode: the display ID as "0x%08lX". Every writer NUL-terminates and returns 0 when the text was cut. */
int mui_aa_path_split(const char* path, char* drawer, int dcap, char* file, int fcap);
int mui_aa_path_join(const char* drawer, const char* file, char* out, int cap);
int mui_aa_font_spec(const char* name, long size, char* out, int cap);
int mui_aa_font_parse(const char* spec, char* name, int cap, long* size);
int mui_aa_hex_text(uint32_t v, char* out, int cap);
uint32_t mui_aa_hex_parse(const char* s); /* "0x..." or bare hex; stops at the first non-hex digit */

/* UP-Dirlist sort order (MUIA_Dirlist_SortType / SortDirs / SortHighLow), pure (host test mui-dirlist). type 0 name (case blind), 1 date,
 * 2 size; dirs 0 drawers first, 1 drawers last, 2 mixed; high_low reverses the key, never the drawer grouping. Ties fall back to the name. */
typedef struct mui_aa_dirkey {
    const char* name;
    long size, days, mins, ticks;
    int dir;
} mui_aa_dirkey;
int mui_aa_dir_cmp(const mui_aa_dirkey* a, const mui_aa_dirkey* b, int type, int dirs, int high_low);
int mui_aa_icon_name(const char* name); /* the name ends in ".info" (any case): MUIA_Dirlist_RejectIcons drops it */

#ifdef __cplusplus
}
#endif
#endif
