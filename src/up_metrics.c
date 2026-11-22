#include <mcc/up_metrics.h>

#include <stddef.h>

int up_font_px(int screen_rows, int screen_cols) {
    (void)screen_cols;
    return screen_rows <= 256 ? 0 : screen_rows <= 600 ? 13 : screen_rows <= 800 ? 15 : 17;
}

int up_screen_font(int font_px, up_screen_font_t* out) {
    static const int sizes[] = {24, 18, 15, 13, 11, 9};
    unsigned i = 0;
    if (font_px <= 0 || out == NULL) {
        return 0;
    }
    while (i + 1 < sizeof sizes / sizeof sizes[0] && sizes[i] > font_px + 1) {
        i++;
    }
    out->name = "helvetica.font";
    out->ysize = sizes[i];
    return 1;
}

int up_rows(int px, int aspect_q8) {
    int a = aspect_q8 < 256 ? 256 : aspect_q8;
    int r = (px * 256 + a / 2) / a;
    return r < 1 ? 1 : r;
}

void up_metrics_aspect(int font_px, int aspect_q8, up_metrics_t* out) {
    int small = font_px <= 0;
    int tight = small && aspect_q8 > 256; /* 640x256 PAL: tall pixels and 256 rows */
    out->font_px = small ? 8 : font_px;
    out->pad_x = tight ? 4 : small ? 6 : 10;
    out->pad_y = small ? up_rows(3, aspect_q8) : 6;
    /* I667m (design 3.11): the one icon table, by tier font: S 12, M 18, L 20, XL 24 (desktop icon:text about 1.4). */
    out->icon_px = tight ? 16 : small ? 12 : font_px <= 13 ? 18 : font_px <= 15 ? 20 : 24; /* I949e2: tight tier = the baked 16x8 set */
    out->icon_gap = tight ? 4 : 6;
    out->icon_h = up_rows(out->icon_px, aspect_q8); /* tight: 16 -> 8, the 16x8 body */
    out->radius = small ? 4 : 6;
    out->check_px = small ? 12 : 16;
    out->check_h = up_rows(out->check_px, aspect_q8);
    out->height = small ? out->font_px + 2 * out->pad_y : (out->font_px > out->icon_px ? out->font_px : out->icon_px) + 2 * out->pad_y;
    out->aspect_q8 = aspect_q8 < 256 ? 256 : aspect_q8;
}

void up_metrics(int font_px, up_metrics_t* out) {
    up_metrics_aspect(font_px, 256, out);
}

static uint32_t g_aspect_calls;

int up_pixel_aspect_q8(int res_x, int res_y) {
    int q;
    g_aspect_calls++;
    if (res_x <= 0 || res_y <= 0) {
        return 256;
    }
    q = (int)(((long)res_y * 256L + res_x / 2) / res_x);
    return q < 256 ? 256 : q > 512 ? 512 : q;
}

uint32_t up_pixel_aspect_calls(void) {
    return g_aspect_calls;
}

void up_panel_gap(int spacing, int aspect_q8, int page, int* gap_x, int* gap_y) {
    /* I685w run 6: spacing scale 8 / 16 / 24. Inside a card (Raised, Surface) children sit one unit apart; on the page (Panel role) the
     * cards sit two units apart; the card pad is three (mui_aa_panel_pad). */
    int sp = spacing * (page ? UP_PAGE_GAP_UNITS : UP_CARD_GAP_UNITS);
    *gap_x = aspect_q8 > 256 ? sp / 2 : sp;
    *gap_y = up_rows(sp, aspect_q8);
}

void up_slider_metrics(int font_px, int aspect_q8, up_slider_t* out) {
    /* I965: vertical sizes count in rows (up_rows) at the screen aspect, so a 2x-tall pixel counts as two; square (256): unchanged. */
    out->track_h = font_px <= 0 ? up_rows(2, aspect_q8) : 6; /* I685w run 6: the Mac track reads about 6 px; 4 looked like a hairline on rig 2 */
    out->radius = out->track_h / 2;
    out->min_w = 80;
    out->height = font_px <= 0 ? up_rows(12, aspect_q8) : font_px < 15 ? 16 : font_px < 17 ? 18 : 20; /* hit height; only the track and fill are drawn */
}

void up_scroller_metrics(int font_px, int aspect_q8, up_scroller_t* out) {
    out->width = font_px <= 0 ? (aspect_q8 > 256 ? 4 : 6) : 10; /* I965: tight tier (tall pixels, small font) packs across */
    out->radius = font_px <= 0 ? 2 : 4;
    out->min_thumb = 20;
    out->min_h = 4 * out->min_thumb;
}

void up_list_metrics(int font_px, int aspect_q8, up_list_t* out) {
    up_scroller_t s;
    up_metrics_t b;
    up_scroller_metrics(font_px, aspect_q8, &s);
    up_metrics_aspect(font_px, aspect_q8, &b); /* I965: the pad and gap across come from the one metrics table (tight tier: 4) */
    out->row_h = font_px <= 0 ? 10 : font_px + 8;
    out->pad_x = font_px <= 0 ? b.pad_x : 10;
    out->pad_y = font_px <= 0 ? up_rows(2, aspect_q8) : 4; /* inside the list face above the first and below the last row (I667i: the cycle popup had none) */
    out->gap = font_px <= 0 ? b.icon_gap : 8;
    out->scroller_w = s.width;
    out->radius = font_px <= 0 ? 4 : 6;
}

int up_list_delta(const up_list_t* m) {
    return 2 * m->gap;
}
