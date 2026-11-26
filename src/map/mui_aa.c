#include "mui_aa.h"

#include <selecta/uimodel/cols.h>
#include <string.h>

static int isqrt(int v) {
    int r = 0;
    while ((r + 1) * (r + 1) <= v) {
        r++;
    }
    return r;
}

int mui_aa_truecolour(int depth, int have_p96, int have_cgx) {
    return depth >= 15 && (have_p96 || have_cgx);
}

int mui_aa_round_inset(int w, int h, int r, int y) {
    int dy2, half;
    if (w <= 0 || h <= 0 || y < 0 || y >= h) {
        return 0;
    }
    if (r > h / 2) {
        r = h / 2;
    }
    if (r > w / 2) {
        r = w / 2;
    }
    if (r <= 0) {
        return 0;
    }
    if (y >= h - r) {
        y = h - 1 - y; /* mirror the bottom rows onto the top */
    }
    if (y >= r) {
        return 0;
    }
    /* Row centre at distance (r - y - 0.5) above the circle centre, doubled to stay integer: d = 2r - 2y - 1. */
    dy2 = 2 * r - 2 * y - 1;
    half = isqrt(4 * r * r - dy2 * dy2); /* twice the half chord */
    return r - (half + 1) / 2;
}

int mui_aa_ramp_index(int coverage, int steps) {
    if (steps < 2) {
        return 0;
    }
    if (coverage < 0) {
        coverage = 0;
    }
    if (coverage > 255) {
        coverage = 255;
    }
    return (coverage * (steps - 1) + 127) / 255;
}

uint32_t mui_aa_ramp_rgb(uint32_t fg, uint32_t bg, int steps, int i) {
    uint32_t out = 0;
    int sh;
    if (steps < 2) {
        return bg;
    }
    if (i < 0) {
        i = 0;
    }
    if (i > steps - 1) {
        i = steps - 1;
    }
    for (sh = 16; sh >= 0; sh -= 8) {
        int f = (int)((fg >> sh) & 0xFFu);
        int b = (int)((bg >> sh) & 0xFFu);
        out |= (uint32_t)(b + (f - b) * i / (steps - 1)) << sh;
    }
    return out;
}

uint32_t mui_aa_pressed_rgb(uint32_t face) {
    uint32_t out = 0;
    int sh;
    uint32_t lum = ((face >> 16) & 0xFFu) * 2126u / 10u + ((face >> 8) & 0xFFu) * 7152u / 10u + (face & 0xFFu) * 722u / 10u;
    int light = lum > 128000u; /* lum runs 0..255000 */
    for (sh = 16; sh >= 0; sh -= 8) {
        int c = (int)((face >> sh) & 0xFFu);
        c = light ? c * 4 / 5 : c + (255 - c) / 5;
        out |= (uint32_t)c << sh;
    }
    return out;
}

static uint32_t g_state_calls;

uint32_t mui_aa_state_calls(void) {
    return g_state_calls;
}

uint32_t mui_aa_state_rgb(const uint32_t* token, uint32_t face, int state) {
    g_state_calls++;
    switch (state) {
    case MUI_AA_HOVER:
        return token[SEL_GUI_TOKEN_SURFACE_HOVER];
    case MUI_AA_PRESSED:
    case MUI_AA_SELECTED:
        return token[SEL_GUI_TOKEN_SURFACE_PRESSED];
    default:
        return face;
    }
}

uint32_t mui_aa_text_rgb(const uint32_t* token, uint32_t text, int state) {
    g_state_calls++;
    return state == MUI_AA_DISABLED ? token[SEL_GUI_TOKEN_TEXT_DISABLED] : text;
}

void mui_aa_button_rgb(const uint32_t* token, int role, int state, uint32_t* face, uint32_t* text) {
    uint32_t f, t;
    if (role == MUI_AA_ROLE_ACCENT) {
        f = token[SEL_GUI_TOKEN_ACCENT];
        t = token[SEL_GUI_TOKEN_ACCENT_TEXT];
        /* The accent face keeps its colour on hover and press (no accent hover token); the press shows as the pressed token. */
        f = state == MUI_AA_PRESSED || state == MUI_AA_SELECTED ? mui_aa_state_rgb(token, f, state) : f;
    } else if (role == MUI_AA_ROLE_TOGGLE) {
        /* AACheck as the desktop d_checkbox: checked = ACCENT box with an ACCENT_TEXT check, unchecked = SURFACE box (hover shows the hover token). */
        f = state == MUI_AA_SELECTED ? token[SEL_GUI_TOKEN_ACCENT] : mui_aa_state_rgb(token, token[SEL_GUI_TOKEN_SURFACE], state);
        t = token[SEL_GUI_TOKEN_ACCENT_TEXT];
    } else if (role == MUI_AA_ROLE_GHOST) {
        /* Ghost sits on the panel (MUI_TH_PANEL = SURFACE in mui_theme.c); only a selected door is raised. */
        f = token[SEL_GUI_TOKEN_SURFACE];
        t = token[SEL_GUI_TOKEN_TEXT];
        f = state == MUI_AA_SELECTED ? token[SEL_GUI_TOKEN_SURFACE_RAISED] : mui_aa_state_rgb(token, f, state == MUI_AA_DISABLED ? MUI_AA_NORMAL : state);
    } else if (role == MUI_AA_ROLE_TAB) {
        /* The active tab is the paper (the raised card colour it sits flush against); an idle tab is halfway between the paper and the window surface. */
        if (state == MUI_AA_SELECTED || state == MUI_AA_PRESSED) {
            f = token[SEL_GUI_TOKEN_SURFACE_RAISED];
        } else {
            uint32_t a = token[SEL_GUI_TOKEN_SURFACE], b = token[SEL_GUI_TOKEN_SURFACE_RAISED];
            f = mui_aa_state_rgb(token, (((a ^ b) & 0xFEFEFEu) >> 1) + (a & b), state);
        }
        t = token[SEL_GUI_TOKEN_TEXT];
    } else {
        f = mui_aa_state_rgb(token, token[SEL_GUI_TOKEN_SURFACE_RAISED], state);
        t = token[SEL_GUI_TOKEN_TEXT];
    }
    *face = f;
    *text = mui_aa_text_rgb(token, t, state);
}

int mui_aa_ramp_pens(int kind, const uint32_t* token, uint32_t fg, uint32_t bg, uint32_t* out) {
    int i;
    if (kind == MUI_AA_RAMP_AGA) {
        out[0] = bg;
        out[1] = token[SEL_GUI_TOKEN_TEXT_DISABLED];
        out[2] = token[SEL_GUI_TOKEN_TEXT_MUTED];
        out[3] = token[SEL_GUI_TOKEN_TEXT];
        return 4;
    }
    if (kind == MUI_AA_RAMP_RTG) {
        for (i = 0; i < MUI_AA_RAMP_MAX; i++) {
            out[i] = mui_aa_ramp_rgb(fg, bg, MUI_AA_RAMP_MAX, i);
        }
        return MUI_AA_RAMP_MAX;
    }
    return 0;
}

int mui_aa_ramp_slot(int kind, int coverage) {
    if (coverage < 0) {
        coverage = 0;
    }
    if (coverage > 255) {
        coverage = 255;
    }
    if (kind == MUI_AA_RAMP_AGA) {
        return coverage / 64;
    }
    return kind == MUI_AA_RAMP_RTG ? mui_aa_ramp_index(coverage, MUI_AA_RAMP_MAX) : 0;
}



int mui_aa_tier(int screen_rows) {
    return screen_rows <= 256 ? MUI_AA_TIER_S : screen_rows <= 600 ? MUI_AA_TIER_M : screen_rows <= 800 ? MUI_AA_TIER_L : MUI_AA_TIER_XL;
}







int mui_aa_check_plan(const uint32_t* token, const up_metrics_t* m, int state, mui_aa_check_op_t* out) {
    int r = (int)token[SEL_GUI_TOKEN_RADIUS_SMALL], line = m->check_px >= 16 ? 2 : 1, n = 0;
    uint32_t face, ink;
    if (m->check_px < 16) {
        r /= 2;
    }
    mui_aa_button_rgb(token, MUI_AA_ROLE_TOGGLE, state, &face, &ink);
    out[n].inset = 0, out[n].radius = 0, out[n++].rgb = token[SEL_GUI_TOKEN_SURFACE];
    if (state == MUI_AA_SELECTED) {
        out[n].inset = 0, out[n].radius = r, out[n++].rgb = face;
        return n;
    }
    out[n].inset = 0, out[n].radius = r, out[n++].rgb = state == MUI_AA_DISABLED ? token[SEL_GUI_TOKEN_TEXT_DISABLED] : token[SEL_GUI_TOKEN_TEXT_MUTED];
    out[n].inset = line, out[n].radius = r > line ? r - line : 0, out[n++].rgb = face;
    return n;
}

int mui_aa_tone_token(int tone) {
    return tone == MUI_AA_TONE_MUTED ? SEL_GUI_TOKEN_TEXT_MUTED : SEL_GUI_TOKEN_TEXT;
}

int mui_aa_tone_font_px(int tone, int base_px) {
    if (base_px > 0 && tone == MUI_AA_TONE_HEADING) {
        return base_px * MUI_AA_HEADING_FACTOR;
    }
    return tone == MUI_AA_TONE_TITLE && base_px > 0 ? base_px + MUI_AA_TITLE_EXTRA_PX : base_px;
}

int mui_aa_align_x(int align, int w, int cw) {
    int x = align == MUI_AA_ALIGN_CENTRE ? (w - cw) / 2 : align == MUI_AA_ALIGN_RIGHT ? w - cw : 0;
    return x > 0 ? x : 0;
}

int mui_aa_btn_content_x(int align, int w, int cw, int pad_x) {
    return pad_x + mui_aa_align_x(align, w - 2 * pad_x, cw);
}

int mui_aa_btn_text_room(int w, int pad_x, int icon_px, int icon_gap, int has_icon) {
    int room = w - 2 * pad_x - (has_icon ? icon_px + icon_gap : 0);
    return room > 0 ? room : 0;
}

int mui_aa_text_fit(const char* s, int max_w, mui_aa_width_fn width, void* ctx, char* out, int cap) {
    int len = (int)strlen(s), n, dots_w;
    static const char dots[] = "...";
    if (cap <= 0) {
        return 0;
    }
    if (width(ctx, s, len) <= max_w) {
        n = len < cap - 1 ? len : cap - 1;
        memcpy(out, s, (size_t)n);
        out[n] = '\0';
        return n;
    }
    dots_w = width(ctx, dots, 3);
    out[0] = '\0';
    if (dots_w > max_w || cap < 4) {
        return 0;
    }
    for (n = len - 1; n > 0; n--) {
        if (n > cap - 4) {
            continue;
        }
        if (width(ctx, s, n) + dots_w <= max_w) {
            break;
        }
    }
    memcpy(out, s, (size_t)n);
    memcpy(out + n, dots, 4);
    return n + 3;
}

int mui_aa_panel_token(int role) {
    return role == MUI_AA_PANEL_POPUP ? SEL_GUI_TOKEN_SURFACE_HOVER : role == MUI_AA_PANEL_SURFACE ? SEL_GUI_TOKEN_SURFACE : role == MUI_AA_PANEL_RAISED ? SEL_GUI_TOKEN_SURFACE_RAISED : SEL_GUI_TOKEN_PANEL;
}

uint32_t mui_aa_panel_rgb(const uint32_t* token, int role) {
    return token[mui_aa_panel_token(role)];
}

uint32_t mui_aa_parent_rgb(const uint32_t* token, int parent_role) {
    return parent_role < 0 ? token[SEL_GUI_TOKEN_BACKGROUND] : mui_aa_panel_rgb(token, parent_role);
}

int mui_aa_panel_radius(const uint32_t* token, int font_px) {
    int r = (int)token[SEL_GUI_TOKEN_RADIUS_LARGE];
    return font_px <= 0 ? r / 2 : r;
}

int mui_aa_panel_spacing(const uint32_t* token) {
    return (int)token[SEL_GUI_TOKEN_SPACING];
}

void mui_aa_panel_pad(const uint32_t* token, int aspect_q8, int* pad_x, int* pad_y) {
    int sp = mui_aa_panel_spacing(token) * MUI_AA_PANEL_PAD_UNITS;
    /* I949e2 (answer 4): on tall pixels the group spacing packs: half across, the same length counted in rows down. Square: unchanged. */
    *pad_x = aspect_q8 > 256 ? sp / 2 : sp;
    *pad_y = up_rows(sp, aspect_q8);
}

void mui_aa_panel_role_pad(int role, int* pad_x, int* pad_y) {
    /* I685w run 6: a Surface card sits inside a card (Mac: the sidebar's "Create your first playlist" box, 12 px against the 24 px
     * of the outer card), so its pad is half the card pad. Panel and Raised keep the full pad. */
    if (role == MUI_AA_PANEL_SURFACE) {
        *pad_x /= 2;
        *pad_y /= 2;
    }
}


static uint32_t g_slider_calls;

uint32_t mui_aa_slider_calls(void) {
    return g_slider_calls;
}


void mui_aa_slider_rgb(const uint32_t* token, int state, uint32_t* track, uint32_t* fill) {
    g_slider_calls++;
    if (track != NULL) {
        *track = token[SEL_GUI_TOKEN_TRACK];
    }
    if (fill != NULL) {
        *fill = state == MUI_AA_DISABLED ? token[SEL_GUI_TOKEN_TEXT_DISABLED] : token[SEL_GUI_TOKEN_ACCENT];
    }
}

int mui_aa_slider_fill_w(int w, long value, long lo, long hi) {
    long span = hi - lo;
    g_slider_calls++;
    if (span <= 0 || w <= 0 || value <= lo) {
        return 0;
    }
    return value >= hi ? w : (int)(((long)w * (value - lo) + span / 2) / span);
}

long mui_aa_slider_value_at(int w, int x, long lo, long hi) {
    long span = hi - lo, v;
    g_slider_calls++;
    if (w <= 0 || span <= 0 || x <= 0) {
        return lo;
    }
    if (x >= w) {
        return hi;
    }
    v = lo + ((long)x * span + w / 2) / w;
    return v > hi ? hi : v;
}

int mui_aa_slider_plan(const uint32_t* token, const up_slider_t* m, int state, int w, int h, long value, long lo, long hi, mui_aa_slider_op_t* out) {
    int n = 0, ty = (h - m->track_h) / 2;
    uint32_t track, fill;
    int fw = mui_aa_slider_fill_w(w, value, lo, hi);
    mui_aa_slider_rgb(token, state, &track, &fill);
    out[n].kind = MUI_AA_SLIDER_ERASE;
    out[n].x = 0;
    out[n].y = 0;
    out[n].w = w;
    out[n].h = h;
    out[n].radius = 0;
    out[n++].rgb = 0;
    out[n].kind = MUI_AA_SLIDER_TRACK;
    out[n].x = 0;
    out[n].y = ty;
    out[n].w = w;
    out[n].h = m->track_h;
    out[n].radius = m->radius;
    out[n++].rgb = track;
    if (fw > 0) {
        out[n].kind = MUI_AA_SLIDER_FILL;
        out[n].x = 0;
        out[n].y = ty;
        out[n].w = fw;
        out[n].h = m->track_h;
        out[n].radius = m->radius;
        out[n++].rgb = fill;
    }
    return n;
}

/* I739: the partial draw of a value change. Same pills as the full plan, no erase of the box: the pixels that stay track or stay fill are
 * never cleared, so the bar does not blink on each tick. Nothing to paint (0 ops) when the fill width did not change. */
static uint32_t g_slider_partial;

uint32_t mui_aa_slider_partial_count(void) {
    return g_slider_partial;
}

int mui_aa_slider_update_plan(const uint32_t* token, const up_slider_t* m, int state, int w, int h, int prev_fw, long value, long lo, long hi,
                              mui_aa_slider_op_t* out) {
    int n = 0, ty = (h - m->track_h) / 2;
    uint32_t track, fill;
    int fw = mui_aa_slider_fill_w(w, value, lo, hi);
    if (fw == prev_fw) {
        return 0;
    }
    mui_aa_slider_rgb(token, state, &track, &fill);
    g_slider_partial++;
    if (fw < prev_fw) { /* the fill shrank: the track shows again where it was */
        out[n].kind = MUI_AA_SLIDER_TRACK;
        out[n].x = 0;
        out[n].y = ty;
        out[n].w = w;
        out[n].h = m->track_h;
        out[n].radius = m->radius;
        out[n++].rgb = track;
    }
    if (fw > 0) {
        out[n].kind = MUI_AA_SLIDER_FILL;
        out[n].x = 0;
        out[n].y = ty;
        out[n].w = fw;
        out[n].h = m->track_h;
        out[n].radius = m->radius;
        out[n++].rgb = fill;
    }
    return n;
}

int mui_aa_progress_fill_w(int w, long value, long max) {
    g_slider_calls++;
    if (max <= 0 || w <= 0 || value <= 0) {
        return 0;
    }
    return value >= max ? w : (int)(((long)w * value + max / 2) / max);
}

static uint32_t g_cycle_calls;

uint32_t mui_aa_cycle_calls(void) {
    return g_cycle_calls;
}

void mui_aa_cycle_metrics(int font_px, int aspect_q8, mui_aa_cycle_t* out) {
    up_metrics_t b;
    g_cycle_calls++;
    up_metrics_aspect(font_px, aspect_q8, &b);
    out->height = b.height;
    out->pad_x = b.pad_x;
    out->radius = b.radius;
    out->chev_w = font_px <= 0 ? 6 : 8;
    out->chev_h = out->chev_w / 2;
    out->gap = font_px <= 0 ? 6 : 8;
}

void mui_aa_cycle_rgb(const uint32_t* token, int state, uint32_t* face, uint32_t* text, uint32_t* chevron, uint32_t* popup_bg, uint32_t* row_hover) {
    g_cycle_calls++;
    if (face != NULL) {
        *face = mui_aa_state_rgb(token, token[SEL_GUI_TOKEN_SURFACE_RAISED], state);
    }
    if (text != NULL) {
        *text = mui_aa_text_rgb(token, token[SEL_GUI_TOKEN_TEXT], state);
    }
    if (chevron != NULL) {
        *chevron = state == MUI_AA_DISABLED ? token[SEL_GUI_TOKEN_TEXT_DISABLED] : token[SEL_GUI_TOKEN_TEXT_MUTED];
    }
    if (popup_bg != NULL) {
        *popup_bg = token[SEL_GUI_TOKEN_SURFACE];
    }
    if (row_hover != NULL) {
        *row_hover = token[SEL_GUI_TOKEN_SURFACE_HOVER];
    }
}

int mui_aa_cycle_min_w(const mui_aa_cycle_t* m, int widest_text_w) {
    g_cycle_calls++;
    return 2 * m->pad_x + widest_text_w + m->gap + m->chev_w;
}

void mui_aa_cycle_chevron_row(const mui_aa_cycle_t* m, int r, int* x, int* w) {
    g_cycle_calls++;
    *x = r;
    *w = m->chev_w - 2 * r;
    if (*w < 1) {
        *w = 1;
    }
}

int mui_aa_cycle_step(int active, int n, int dir) {
    g_cycle_calls++;
    if (n <= 0) {
        return 0;
    }
    return ((active + dir) % n + n) % n;
}

int mui_aa_cycle_valid(int v, int n) {
    g_cycle_calls++;
    return v >= 0 && v < n;
}

int mui_aa_text_maxw(int fixed, int defw) {
    return fixed ? defw : MUI_AA_TEXT_UNBOUNDED;
}

static uint32_t g_field_calls;

static uint32_t g_field_placeholders;

int mui_aa_field_placeholder(const uint32_t* token, const char* contents, int focus, uint32_t* rgb) {
    if (focus || (contents != NULL && contents[0] != '\0')) {
        return 0;
    }
    *rgb = token[SEL_GUI_TOKEN_TEXT_MUTED];
    g_field_placeholders++;
    return 1;
}

int mui_aa_field_text(const uint32_t* token, const char* contents, int focus, uint32_t* rgb) {
    if (focus || contents == NULL || contents[0] == '\0') {
        return 0;
    }
    *rgb = token[SEL_GUI_TOKEN_TEXT];
    return 1;
}

uint32_t mui_aa_field_placeholders(void) {
    return g_field_placeholders;
}

uint32_t mui_aa_field_calls(void) {
    return g_field_calls;
}

void mui_aa_field_metrics(int font_px, int aspect_q8, mui_aa_field_t* out) {
    up_metrics_t b;
    g_field_calls++;
    up_metrics_aspect(font_px, aspect_q8, &b);
    out->height = b.height;
    out->radius = b.radius;
    out->pad_x = 8; /* same on every tier: the group's inner offsets are fixed at construction, before the screen tier is known */
    out->pad_y = 2;
    out->outline = 1;
}

int mui_aa_field_plan(const uint32_t* token, const mui_aa_field_t* m, int focus, mui_aa_check_op_t* out) {
    g_field_calls++;
    out[0].inset = 0, out[0].radius = m->radius, out[0].rgb = token[focus == MUI_AA_FIELD_ERROR ? SEL_GUI_TOKEN_ERROR : focus ? SEL_GUI_TOKEN_FOCUS_RING : MUI_AA_FIELD_LINE];
    out[1].inset = m->outline, out[1].radius = m->radius > m->outline ? m->radius - m->outline : 0, out[1].rgb = token[MUI_AA_FIELD_FACE];
    return MUI_AA_FIELD_OPS;
}

int mui_aa_field_min_width(const mui_aa_field_t* m, int pw) {
    return pw > 0 ? pw + 4 + 2 * m->pad_x : 0;
}

int mui_aa_field_height(const mui_aa_field_t* m, int ch) {
    int need = ch + 2 * (m->pad_y + m->outline);
    g_field_calls++;
    return need > m->height ? need : m->height;
}

int mui_aa_field_text_top(const mui_aa_field_t* m, int h, int ch) {
    int top = m->pad_y + m->outline;
    int extra = h - 2 * top - ch;
    g_field_calls++;
    return top + (extra > 0 ? extra / 2 : 0);
}

/* I667i (design 3.5 AAScroller, 3.6 AAList). */
static uint32_t g_scroller_calls;
static uint32_t g_list_calls;

uint32_t mui_aa_scroller_calls(void) {
    return g_scroller_calls;
}

uint32_t mui_aa_list_calls(void) {
    return g_list_calls;
}


void mui_aa_scroller_rgb(const uint32_t* token, int state, uint32_t* track, uint32_t* thumb) {
    g_scroller_calls++;
    if (track != NULL) {
        *track = token[SEL_GUI_TOKEN_SURFACE];
    }
    if (thumb != NULL) {
        /* I685w run 6: TRACK (3A3A44) on SURFACE (1E1E24) was near invisible on rig 2; the thumb takes the disabled-text grey, muted text on hover */
        *thumb = (state == MUI_AA_HOVER || state == MUI_AA_PRESSED) ? token[SEL_GUI_TOKEN_TEXT_MUTED] : token[SEL_GUI_TOKEN_TEXT_DISABLED];
    }
}

int mui_aa_scroller_geom(const up_scroller_t* m, int h, long first, long visible, long entries, int* thumb_y, int* thumb_h) {
    long range, th, travel;
    g_scroller_calls++;
    if (h <= 0 || entries <= 0 || visible >= entries || visible <= 0) {
        *thumb_y = 0;
        *thumb_h = 0;
        return 0; /* everything fits: no thumb */
    }
    th = h * visible / entries;
    if (th < m->min_thumb) {
        th = m->min_thumb;
    }
    if (th > h) {
        th = h;
    }
    range = entries - visible;
    first = first < 0 ? 0 : first > range ? range : first;
    travel = h - th;
    *thumb_h = (int)th;
    *thumb_y = (int)(travel * first / range);
    return 1;
}

long mui_aa_scroller_press(const up_scroller_t* m, int h, int y, long first, long visible, long entries, int* grab) {
    int ty, th;
    *grab = -1;
    if (!mui_aa_scroller_geom(m, h, first, visible, entries, &ty, &th)) {
        return first;
    }
    if (y >= ty && y < ty + th) {
        *grab = y - ty;
        return first;
    }
    first += y < ty ? -visible : visible; /* a click on the track pages towards the pointer */
    return first < 0 ? 0 : first > entries - visible ? entries - visible : first;
}

long mui_aa_scroller_first_at(const up_scroller_t* m, int h, int thumb_h_grab, int y, long visible, long entries) {
    long range, travel, th;
    int ty, thh;
    g_scroller_calls++;
    if (!mui_aa_scroller_geom(m, h, 0, visible, entries, &ty, &thh)) {
        return 0;
    }
    th = thh;
    range = entries - visible;
    travel = h - th;
    y -= thumb_h_grab; /* the pointer holds the thumb at this offset from its top */
    if (travel <= 0 || y <= 0) {
        return 0;
    }
    if (y >= travel) {
        return range;
    }
    return (range * y + travel / 2) / travel;
}


int mui_aa_list_col_x(const up_list_t* m, int col, const int* w) {
    int x = m->pad_x, k;
    g_list_calls++;
    for (k = 0; k < col && w != NULL; k++) {
        if (w[k] > 0) { /* an absent column (width 0) takes no gap, as in sel_uim_cols_split */
            x += w[k] + 2 * m->gap + 1;
        }
    }
    return x;
}


int mui_aa_list_visible(const up_list_t* m, int h) {
    g_list_calls++;
    h -= 2 * m->pad_y;
    return h < m->row_h ? 0 : h / m->row_h;
}

long mui_aa_list_clamp_first(long first, long visible, long count) {
    long max;
    g_list_calls++;
    max = count - visible;
    if (max < 0) {
        max = 0;
    }
    return first < 0 ? 0 : first > max ? max : first;
}

long mui_aa_list_clamp_active(long active, long count) {
    g_list_calls++;
    if (count <= 0) {
        return -1;
    }
    return active < 0 ? 0 : active >= count ? count - 1 : active;
}

long mui_aa_list_move(long active, long count, long visible, int key) {
    long step;
    g_list_calls++;
    if (count <= 0) {
        return -1;
    }
    step = visible > 1 ? visible - 1 : 1;
    switch (key) {
    case MUI_AA_LIST_KEY_UP:
        active = active < 0 ? count - 1 : active - 1;
        break;
    case MUI_AA_LIST_KEY_DOWN:
        active = active < 0 ? 0 : active + 1;
        break;
    case MUI_AA_LIST_KEY_PAGEUP:
        active = active < 0 ? 0 : active - step;
        break;
    case MUI_AA_LIST_KEY_PAGEDOWN:
        active = active < 0 ? 0 : active + step;
        break;
    case MUI_AA_LIST_KEY_HOME:
        active = 0;
        break;
    case MUI_AA_LIST_KEY_END:
        active = count - 1;
        break;
    default:
        break;
    }
    return active < 0 ? 0 : active >= count ? count - 1 : active; /* no wrap in a list: clamps at both ends */
}

long mui_aa_list_scroll_to(long first, long visible, long count, long active) {
    g_list_calls++;
    if (active >= 0) {
        if (active < first) {
            first = active;
        } else if (visible > 0 && active >= first + visible) {
            first = active - visible + 1;
        }
    }
    return mui_aa_list_clamp_first(first, visible, count);
}

long mui_aa_list_row_at(const up_list_t* m, int y, long first, long count) {
    long row;
    g_list_calls++;
    y -= m->pad_y;
    if (y < 0) {
        return -1;
    }
    row = first + y / m->row_h;
    return row >= count ? -1 : row;
}

void mui_aa_list_row_rgb(const uint32_t* token, int row_state, int playing, uint32_t* face, uint32_t* text) {
    g_list_calls++;
    if (face != NULL) {
        *face = row_state == MUI_AA_SELECTED ? token[SEL_GUI_TOKEN_SURFACE_PRESSED] : row_state == MUI_AA_HOVER ? token[SEL_GUI_TOKEN_SURFACE_HOVER] : token[SEL_GUI_TOKEN_SURFACE];
    }
    if (text != NULL) {
        *text = row_state == MUI_AA_DISABLED ? token[SEL_GUI_TOKEN_TEXT_DISABLED] : playing ? token[SEL_GUI_TOKEN_ACCENT] : token[SEL_GUI_TOKEN_TEXT];
    }
}

int mui_aa_list_row_state(unsigned long dis, long row, int sel, long hover) {
    if (row >= 0 && row < 32 && ((dis >> row) & 1UL)) {
        return MUI_AA_DISABLED;
    }
    return sel ? MUI_AA_SELECTED : row == hover ? MUI_AA_HOVER : MUI_AA_NORMAL;
}

/* I1079b: a thin wrapper over the shared splitter sel_uim_cols_split (design 2026-11-05-I1079 section 3.4). A column with
 * neither weight nor fixed width is absent: width 0 and no gap (mui_aa_list_col_x skips it the same way). */
void mui_aa_list_col_widths(const up_list_t* m, int total, int n, const int* weight, const int* fixed, int* out) {
    sel_uim_col c[SEL_UIM_COLS_MAX];
    int32_t x[SEL_UIM_COLS_MAX], w[SEL_UIM_COLS_MAX];
    int k, cnt = n < SEL_UIM_COLS_MAX ? n : SEL_UIM_COLS_MAX;
    g_list_calls++;
    memset(c, 0, sizeof c);
    for (k = 0; k < cnt; k++) {
        c[k].weight = weight[k] > 0 ? weight[k] : 0;
        c[k].fixed = weight[k] > 0 ? 0 : fixed[k];
    }
    (void)sel_uim_cols_split(total - 2 * m->pad_x, 2 * m->gap + 1, c, (uint32_t)cnt, x, w);
    for (k = 0; k < n; k++) {
        out[k] = k < cnt ? w[k] : 0;
    }
}

int mui_aa_list_title_h(const up_list_t* m, int has_title) {
    g_list_calls++;
    return has_title ? m->row_h : 0;
}

void mui_aa_list_header_rgb(const uint32_t* token, uint32_t* muted, uint32_t* divider) {
    g_list_calls++;
    if (muted != NULL) {
        *muted = token[SEL_GUI_TOKEN_TEXT_MUTED];
    }
    if (divider != NULL) {
        *divider = token[SEL_GUI_TOKEN_DIVIDER];
    }
}

static uint32_t g_tabs_calls;

uint32_t mui_aa_tabs_calls(void) {
    return g_tabs_calls;
}

int mui_aa_tabs_click(int i, int n) {
    g_tabs_calls++;
    return i >= 0 && i < n ? i : -1;
}

int mui_aa_tabs_key(int code, int active, int n) {
    int a;
    g_tabs_calls++;
    if (n <= 0) {
        return 0;
    }
    a = active + (code == 0x4E ? 1 : code == 0x4F ? -1 : 0);
    return a < 0 ? 0 : a >= n ? n - 1 : a;
}

void mui_aa_tabs_line(int font_px, int x, int y, int w, int h, int* lx, int* ly, int* lw, int* lh) {
    int t = font_px > 0 ? MUI_AA_TABS_LINE_PX : 1;
    g_tabs_calls++;
    if (t > h) {
        t = h;
    }
    if (lx != NULL) {
        *lx = x;
    }
    if (ly != NULL) {
        *ly = y + h - t;
    }
    if (lw != NULL) {
        *lw = w;
    }
    if (lh != NULL) {
        *lh = t;
    }
}

void mui_aa_tabs_rgb(const uint32_t* token, uint32_t* line, uint32_t* idle_text, uint32_t* active_text) {
    g_tabs_calls++;
    if (line != NULL) {
        *line = token[SEL_GUI_TOKEN_ACCENT];
    }
    if (idle_text != NULL) {
        *idle_text = token[SEL_GUI_TOKEN_TEXT_MUTED];
    }
    if (active_text != NULL) {
        *active_text = token[SEL_GUI_TOKEN_TEXT];
    }
}

static uint32_t g_balance_calls;

uint32_t mui_aa_balance_calls(void) {
    return g_balance_calls;
}

int mui_aa_balance_width(const uint32_t* token, int font_px) {
    int g = (int)token[SEL_GUI_TOKEN_SPACING];
    g_balance_calls++;
    return font_px <= 0 ? g / 2 : g;
}

void mui_aa_balance_line(const uint32_t* token, int font_px, int box_w, int box_h, int* lx, int* ly, int* lw, int* lh) {
    int inset = mui_aa_balance_width(token, font_px);
    if (box_h < 2 * inset + 1) {
        inset = 0;
    }
    if (lx != NULL) {
        *lx = (box_w - 1) / 2;
    }
    if (ly != NULL) {
        *ly = inset;
    }
    if (lw != NULL) {
        *lw = box_w > 0 ? 1 : 0;
    }
    if (lh != NULL) {
        *lh = box_h - 2 * inset > 0 ? box_h - 2 * inset : 0;
    }
}

void mui_aa_balance_rgb(const uint32_t* token, uint32_t* box, uint32_t* line) {
    g_balance_calls++;
    if (box != NULL) {
        *box = token[SEL_GUI_TOKEN_BACKGROUND];
    }
    if (line != NULL) {
        *line = token[SEL_GUI_TOKEN_DIVIDER];
    }
}

void mui_aabtn_minmax(uint32_t content_w, uint32_t content_h, uint32_t fixed_w, uint32_t fixed_h, uint32_t toggle, mui_btn_box_t* out) {
    out->min_w = out->def_w = fixed_w != 0u ? fixed_w : content_w;
    out->min_h = out->def_h = fixed_h != 0u ? fixed_h : content_h;
    out->w_fixed = (fixed_w != 0u || toggle) ? 1u : 0u;
    out->h_fixed = 1u;
}

int mui_aa_pop_click(mui_aa_pop_t* s, uint32_t now_ms) {
    if (s->open) {
        return MUI_AA_POP_CLOSE;
    }
    if (s->by_deactivate && (uint32_t)(now_ms - s->closed_ms) < MUI_AA_POP_GUARD_MS) {
        s->by_deactivate = 0;
        return MUI_AA_POP_NONE;
    }
    s->by_deactivate = 0;
    return s->created ? MUI_AA_POP_OPEN_REUSE : MUI_AA_POP_OPEN_NEW;
}

void mui_aa_pop_opened(mui_aa_pop_t* s) {
    s->created = 1;
    s->open = 1;
    s->by_deactivate = 0;
}

void mui_aa_pop_closed(mui_aa_pop_t* s, int by_deactivate, uint32_t now_ms) {
    if (!s->open) {
        return;
    }
    s->open = 0;
    s->by_deactivate = by_deactivate;
    s->closed_ms = now_ms;
}

long mui_aa_numeric_clamp(long v, long lo, long hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

long mui_aa_numeric_drag(long start, int dx, int w, long lo, long hi) {
    if (w <= 0 || hi <= lo) {
        return lo;
    }
    return mui_aa_numeric_clamp(start + (long)dx * (hi - lo) / (2L * w), lo, hi);
}

int mui_aa_numeric_text(long v, char* out, int cap) {
    char tmp[16];
    int n = 0, len = 0;
    unsigned long u = v < 0 ? 0UL - (unsigned long)v : (unsigned long)v;
    if (cap < 1) {
        return 0;
    }
    do {
        tmp[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u != 0 && n < 15);
    if (v < 0 && len < cap - 1) {
        out[len++] = '-';
    }
    while (n > 0 && len < cap - 1) {
        out[len++] = tmp[--n];
    }
    out[len] = '\0';
    return len;
}

int mui_aa_scale_plan(int w, long lo, long hi, int ticks, mui_aa_scale_tick_t* out) {
    int i, n = ticks < 2 ? 2 : ticks > MUI_AA_SCALE_MAX ? MUI_AA_SCALE_MAX : ticks;
    long span = hi - lo;
    if (span <= 0 || w < 2) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        out[i].x = (int)(((long)i * (w - 1) + (n - 1) / 2) / (n - 1));
        out[i].value = lo + ((long)i * span + (n - 1) / 2) / (n - 1);
        out[i].major = i == 0 || i == n - 1 || ((n & 1) && i == n / 2);
    }
    return n;
}

void mui_aa_scale_metrics(int font_px, int* minor_h, int* major_h, int* gap) {
    int maj = font_px / 2 < 4 ? 4 : font_px / 2;
    *major_h = maj;
    *minor_h = maj / 2 < 2 ? 2 : maj / 2;
    *gap = 2;
}

int mui_aa_palette_rows(int n, int cols) {
    return n <= 0 || cols <= 0 ? 0 : (n + cols - 1) / cols;
}

void mui_aa_palette_cell(int i, int cols, int cw, int ch, int gap, int* x, int* y) {
    *x = (i % cols) * (cw + gap);
    *y = (i / cols) * (ch + gap);
}

int mui_aa_palette_hit(int x, int y, int cols, int n, int cw, int ch, int gap) {
    int c, r, i;
    if (x < 0 || y < 0 || cols <= 0 || cw <= 0 || ch <= 0) {
        return -1;
    }
    c = x / (cw + gap);
    r = y / (ch + gap);
    if (c >= cols || x % (cw + gap) >= cw || y % (ch + gap) >= ch) {
        return -1;
    }
    i = r * cols + c;
    return i < n ? i : -1;
}

int mui_aa_palette_move(int active, int n, int cols, int dx, int dy) {
    int c, r, ni;
    if (n <= 0 || cols <= 0 || active < 0 || active >= n) {
        return active;
    }
    c = active % cols + dx;
    r = active / cols + dy;
    if (c < 0 || c >= cols || r < 0 || r >= mui_aa_palette_rows(n, cols)) {
        return active;
    }
    ni = r * cols + c;
    return ni >= n ? n - 1 : ni;
}

void mui_aa_image_fit(int sw, int sh, int bw, int bh, int fit, int* dw, int* dh, int* ox, int* oy) {
    int w = sw, h = sh;
    if (fit && sw > 0 && sh > 0 && bw > 0 && bh > 0) {
        if ((long)bw * sh <= (long)bh * sw) { /* width limits */
            w = bw;
            h = (int)((long)sh * bw / sw);
        } else {
            h = bh;
            w = (int)((long)sw * bh / sh);
        }
        w = w < 1 ? 1 : w;
        h = h < 1 ? 1 : h;
    }
    *dw = w, *dh = h, *ox = (bw - w) / 2, *oy = (bh - h) / 2;
}

/* UP-Knob / UP-Levelmeter dial maths (mui_aa.h). Integer only: the m68k class libraries link no float code. */
static const short g_dial_sin[91] = {
    0, 286, 572, 857, 1143, 1428, 1713, 1997, 2280, 2563, 2845, 3126, 3406,
    3686, 3964, 4240, 4516, 4790, 5063, 5334, 5604, 5872, 6138, 6402, 6664, 6924,
    7182, 7438, 7692, 7943, 8192, 8438, 8682, 8923, 9162, 9397, 9630, 9860, 10087,
    10311, 10531, 10749, 10963, 11174, 11381, 11585, 11786, 11982, 12176, 12365, 12551, 12733,
    12911, 13085, 13255, 13421, 13583, 13741, 13894, 14044, 14189, 14330, 14466, 14598, 14726,
    14849, 14968, 15082, 15191, 15296, 15396, 15491, 15582, 15668, 15749, 15826, 15897, 15964,
    16026, 16083, 16135, 16182, 16225, 16262, 16294, 16322, 16344, 16362, 16374, 16382, 16384};

int mui_aa_dial_sin_q14(int deg) {
    deg %= 360;
    if (deg < 0) {
        deg += 360;
    }
    if (deg <= 90) {
        return g_dial_sin[deg];
    }
    if (deg <= 180) {
        return g_dial_sin[180 - deg];
    }
    return deg <= 270 ? -g_dial_sin[deg - 180] : -g_dial_sin[360 - deg];
}

int mui_aa_dial_deg(long v, long lo, long hi, int start_deg, int sweep_deg) {
    long span = hi - lo;
    if (span <= 0) {
        return start_deg;
    }
    v = mui_aa_numeric_clamp(v, lo, hi);
    /* shortcut: (v - lo) * sweep overflows past a span of about 2^31 / 360; Numeric ranges in MUI GUIs are far smaller */
    return start_deg - (int)((v - lo) * sweep_deg / span);
}

void mui_aa_dial_point(int deg, int r, int* dx, int* dy) {
    long c = mui_aa_dial_sin_q14(deg + 90), s = mui_aa_dial_sin_q14(deg);
    *dx = (int)((c * r + (c < 0 ? -8192 : 8192)) / 16384); /* rounded to the nearest pixel */
    *dy = (int)-((s * r + (s < 0 ? -8192 : 8192)) / 16384);
}

long mui_aa_knob_key(long v, long lo, long hi, int code, int shift, int* used) {
    long step = shift ? (hi - lo) / 10 : 1;
    if (step < 1) {
        step = 1;
    }
    *used = 1;
    if (code == 0x4C || code == 0x4E) {
        return mui_aa_numeric_clamp(v + step, lo, hi);
    }
    if (code == 0x4D || code == 0x4F) {
        return mui_aa_numeric_clamp(v - step, lo, hi);
    }
    *used = 0;
    return v;
}

/* Copy n bytes of src into out (cap bytes), NUL-terminated; 0 when cut. */
static int aa_put(char* out, int cap, int at, const char* src, int n) {
    int ok = 1;
    if (cap <= 0) {
        return 0;
    }
    if (at + n >= cap) {
        n = cap - 1 - at;
        ok = 0;
    }
    if (n > 0) {
        memcpy(out + at, src, (size_t)n);
        at += n;
    }
    out[at < cap ? at : cap - 1] = '\0';
    return ok;
}

int mui_aa_path_split(const char* path, char* drawer, int dcap, char* file, int fcap) {
    int len = (int)strlen(path), cut = len;
    while (cut > 0 && path[cut - 1] != '/' && path[cut - 1] != ':') {
        cut--;
    }
    /* "a/b": the drawer is "a" (no trailing slash, as PathPart); "Work:b" keeps the colon */
    int dlen = cut > 0 && path[cut - 1] == '/' ? cut - 1 : cut;
    int ok = aa_put(drawer, dcap, 0, path, dlen);
    return aa_put(file, fcap, 0, path + cut, len - cut) && ok;
}

int mui_aa_path_join(const char* drawer, const char* file, char* out, int cap) {
    int dl = (int)strlen(drawer), ok = aa_put(out, cap, 0, drawer, dl), at = (int)strlen(out);
    if (*file == '\0') {
        return ok;
    }
    if (dl > 0 && drawer[dl - 1] != ':' && drawer[dl - 1] != '/') {
        ok = aa_put(out, cap, at, "/", 1) && ok;
        at = (int)strlen(out);
    }
    return aa_put(out, cap, at, file, (int)strlen(file)) && ok;
}

int mui_aa_font_spec(const char* name, long size, char* out, int cap) {
    char num[12];
    int nl = (int)strlen(name), at, ok;
    if (nl >= 5 && strcmp(name + nl - 5, ".font") == 0) {
        nl -= 5;
    }
    ok = aa_put(out, cap, 0, name, nl);
    at = (int)strlen(out);
    ok = aa_put(out, cap, at, "/", 1) && ok;
    at = (int)strlen(out);
    ok = mui_aa_numeric_text(size, num, (int)sizeof num) > 0 && ok;
    return aa_put(out, cap, at, num, (int)strlen(num)) && ok;
}

int mui_aa_font_parse(const char* spec, char* name, int cap, long* size) {
    const char* slash = strrchr(spec, '/');
    int nl = slash != NULL ? (int)(slash - spec) : (int)strlen(spec), ok;
    long v = 0;
    ok = aa_put(name, cap, 0, spec, nl);
    if (nl > 0 && !(nl >= 5 && strncmp(spec + nl - 5, ".font", 5) == 0)) {
        ok = aa_put(name, cap, (int)strlen(name), ".font", 5) && ok;
    }
    for (slash = slash != NULL ? slash + 1 : ""; *slash >= '0' && *slash <= '9'; slash++) {
        v = v * 10 + (*slash - '0');
    }
    *size = v;
    return ok;
}

int mui_aa_hex_text(uint32_t v, char* out, int cap) {
    char t[11] = "0x";
    int i;
    for (i = 0; i < 8; i++) {
        t[2 + i] = "0123456789ABCDEF"[(v >> (28 - 4 * i)) & 15];
    }
    return aa_put(out, cap, 0, t, 10);
}

uint32_t mui_aa_hex_parse(const char* s) {
    uint32_t v = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    for (;; s++) {
        int c = *s, d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) {
            return v;
        }
        v = v * 16 + (uint32_t)d;
    }
}

static int aa_lower(int c) {
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static int aa_stricmp(const char* a, const char* b) {
    while (*a != 0 && aa_lower((unsigned char)*a) == aa_lower((unsigned char)*b)) {
        a++, b++;
    }
    return aa_lower((unsigned char)*a) - aa_lower((unsigned char)*b);
}

int mui_aa_dir_cmp(const mui_aa_dirkey* a, const mui_aa_dirkey* b, int type, int dirs, int high_low) {
    long c = 0;
    if (dirs != 2 && a->dir != b->dir) {
        return (a->dir ? -1 : 1) * (dirs == 1 ? -1 : 1);
    }
    if (type == 1) {
        c = a->days != b->days ? a->days - b->days : a->mins != b->mins ? a->mins - b->mins : a->ticks - b->ticks;
    } else if (type == 2) {
        c = a->size - b->size;
    }
    if (c == 0) {
        c = aa_stricmp(a->name, b->name);
    }
    c = c < 0 ? -1 : c > 0;
    return high_low ? (int)-c : (int)c;
}

int mui_aa_icon_name(const char* name) {
    size_t n = strlen(name);
    return n >= 5 && aa_stricmp(name + n - 5, ".info") == 0;
}

/* One body row of n bytes into dst (NULL = skip). ByteRun1: n >= 0 copies n+1 bytes, -127..-1 repeats the next byte 1-n times, -128 is a
 * no-op. A run past the row end is cut there (well formed ILBM never crosses rows). */
static const unsigned char* mui_aa_body_row(const unsigned char* s, int compression, unsigned char* dst, int n) {
    int o = 0;
    if (compression == 0) {
        if (dst != NULL) {
            memcpy(dst, s, (size_t)n);
        }
        return s + n;
    }
    while (o < n) {
        int c = (signed char)*s++, k;
        if (c >= 0) {
            for (k = 0; k <= c; k++, s++) {
                if (o < n && dst != NULL) {
                    dst[o] = *s;
                }
                o++;
            }
        } else if (c != -128) {
            for (k = 0; k <= -c; k++, o++) {
                if (o < n && dst != NULL) {
                    dst[o] = *s;
                }
            }
            s++;
        }
    }
    return s;
}

int mui_aa_body_unpack(const unsigned char* body, int compression, int w, int h, int depth, int has_mask, unsigned char* const* planes, int bpr,
                       unsigned char* mask, int mbpr) {
    int rb = ((w + 15) >> 4) << 1, y, p;
    if (body == NULL || (compression != 0 && compression != 1) || w <= 0 || h <= 0 || depth < 1 || depth > 8 || bpr < rb ||
        (mask != NULL && mbpr < rb)) {
        return 0;
    }
    for (y = 0; y < h; y++) {
        for (p = 0; p < depth; p++) {
            body = mui_aa_body_row(body, compression, planes[p] + (long)y * bpr, rb);
        }
        if (has_mask) {
            body = mui_aa_body_row(body, compression, mask != NULL ? mask + (long)y * mbpr : NULL, rb);
        }
    }
    return 1;
}

unsigned long mui_aa_menu_pick(unsigned long checked, int i, int checkit, int toggle, unsigned long exclude) {
    unsigned long bit;
    if (!checkit || i < 0 || i > 31) {
        return checked;
    }
    bit = 1UL << i;
    if (checked & bit) {
        return toggle ? checked & ~bit : checked;
    }
    return (checked | bit) & ~(exclude & ~bit);
}

static int menu_upper(int c) {
    c &= 0xFF;
    return (c >= 'a' && c <= 'z') || (c >= 0xE0 && c != 0xF7 && c != 0xFF) ? c - 32 : c;
}

int mui_aa_menu_shortcut(const UP_MenuItem* ent, int n, unsigned long disabled, int c) {
    int k;
    for (k = 0; ent != NULL && k < n && k < 32; k++) {
        const char* s = ent[k].shortcut;
        if (s != NULL && s[0] != '\0' && s[1] == '\0' && !(ent[k].flags & UPV_MenuItem_CommandString) && !(disabled & (1UL << k)) &&
            menu_upper((unsigned char)s[0]) == menu_upper(c)) {
            return k;
        }
    }
    return -1;
}

int mui_aa_strip_track(int* cur, int title, int up, int dist) {
    int r = MUI_AA_STRIP_NONE;
    if (*cur < -1) {
        return r;
    }
    if (up) { /* a release away from the press with no menu open was a drag that missed the titles: Intuition closes, it does not keep */
        r = *cur >= 0 ? MUI_AA_STRIP_PICK : dist > MUI_AA_STRIP_SLOP ? MUI_AA_STRIP_CANCEL : MUI_AA_STRIP_KEEP;
        *cur = -2;
    } else if (title >= 0 && title != *cur) {
        *cur = title;
        r = MUI_AA_STRIP_OPEN;
    }
    return r;
}

unsigned long mui_aa_mask_insert(unsigned long m, int p, int on) {
    unsigned long lo;
    if (p < 0 || p > 31) {
        return m;
    }
    lo = m & ((1UL << p) - 1UL);
    return (lo | (((m & ~lo) << 1) & 0xFFFFFFFFUL) | (on ? 1UL << p : 0UL)) & 0xFFFFFFFFUL;
}

unsigned long mui_aa_mask_remove(unsigned long m, int p) {
    unsigned long lo;
    if (p < 0 || p > 31) {
        return m;
    }
    lo = m & ((1UL << p) - 1UL);
    return lo | (p < 31 ? ((m & 0xFFFFFFFFUL) >> (p + 1)) << p : 0UL); /* p 31: no shift by 32 */
}

int mui_aa_strip_step(int cur, int dir, int n, unsigned long disabled) {
    int k;
    for (k = 1; k <= n; k++) {
        int j = ((cur + dir * k) % n + n) % n;
        if (j >= 32 || !(disabled & (1UL << j))) {
            return j;
        }
    }
    return -1;
}
