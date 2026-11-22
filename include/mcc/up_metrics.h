/* up_metrics.h: control sizes and paddings of the UP- classes, pure integer maths (68020: no float, no division beyond what the
 * rounding needs). No Intuition, MUI or Amiga include, so the same source compiles for the host and for Amiga. Part of the up-mcc
 * SDK (U5, design 2026-11-22-up-mcc-api-cut.md rows 24, 25, 28): a client compiles src/up_metrics.c next to the classes it sizes. */
#ifndef UP_METRICS_H
#define UP_METRICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Font pixel size for the screen: 13 up to 600 rows, 15 up to 800, 17 above, 0 (bitmap topaz 8) at 256 rows or less. */
int up_font_px(int screen_rows, int screen_cols);
/* I972: bitmap font for the own screen, from a up_font_px size, so MUI's stock objects and the PLAIN text backend (no
 * ttengine or no Inter: text drawn in _font(obj), the screen font) grow with the tier like the antialiased text. Stock
 * Workbench 3.x helvetica (9 11 13 15 18 24) at the largest size not above font_px + 1. Returns 0 for font_px 0 (the 256-row
 * tier keeps the Workbench font, topaz 8) and fills nothing. */
typedef struct up_screen_font_t {
    const char* name; /* diskfont name, e.g. "helvetica.font" */
    int ysize;
} up_screen_font_t;
int up_screen_font(int font_px, up_screen_font_t* out);
typedef struct up_metrics_t {
    int font_px, pad_x, pad_y, icon_px, icon_gap, height, radius, check_px; /* check_px: AACheck box edge (16, tier S 12) */
    int aspect_q8; /* I949a: pixel height over width x 256; 256 square, 512 tall */
    int icon_h, check_h; /* I949e: icon and AACheck box height in ROWS: icon_px / check_px as a physical square, so half on a tall-pixel screen */
} up_metrics_t;

/* Control heights and paddings of one tier table for a font size from up_font_px. */
void up_metrics(int font_px, up_metrics_t* out);
/* I949e (design 2026-11-01-I949 section 4, answer 4): the same on a screen of pixel aspect aspect_q8. Square pixels give up_metrics.
 * Tall pixels at the bitmap tier select the tight tier: pad_x 4, icon_gap 4, vertical sizes counted in rows (a row is aspect_q8/256 wide). */
void up_metrics_aspect(int font_px, int aspect_q8, up_metrics_t* out);
/* A length of px square pixels as rows on a screen of aspect_q8 (rounded, at least 1). The one place vertical sizes are scaled. */
int up_rows(int px, int aspect_q8);
/* I949a: pixel aspect (height over width, x 256) from DisplayInfo.Resolution (ticks per pixel): hires PAL non-laced 22x44 gives 512,
 * laced and RTG about 1:1 give 256; clamped to 256..512. up_pixel_aspect_calls counts the calls (sentinel for mui_screen_open). */
int up_pixel_aspect_q8(int res_x, int res_y);
uint32_t up_pixel_aspect_calls(void);
typedef struct up_scroller_t {
    int width, radius, min_thumb, min_h; /* min_h: smallest track height, so the bar is never a stub (I685w run 5) */
} up_scroller_t;
void up_scroller_metrics(int font_px, int aspect_q8, up_scroller_t* out);
typedef struct up_slider_t {
    int track_h, radius, min_w, height;
} up_slider_t;
void up_slider_metrics(int font_px, int aspect_q8, up_slider_t* out);
typedef struct up_list_t {
    int row_h, pad_x, pad_y, gap, scroller_w, radius;
} up_list_t;
/* Gap between the children of a panel: the page level (role Panel) is UP_PAGE_GAP_UNITS SPACING units, a card UP_CARD_GAP_UNITS.
 * spacing is the SPACING token; page is nonzero for the Panel role. Same aspect packing as the pad. */
#define UP_CARD_GAP_UNITS 1
#define UP_PAGE_GAP_UNITS 2
void up_panel_gap(int spacing, int aspect_q8, int page, int* gap_x, int* gap_y);
void up_list_metrics(int font_px, int aspect_q8, up_list_t* out);
/* Width of one gap between list columns: twice the column gap. */
int up_list_delta(const up_list_t* m);

#ifdef __cplusplus
}
#endif

#endif
