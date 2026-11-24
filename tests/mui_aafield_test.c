/* Host test of the UP-Field and UP-Panel colour and width rules (src/map/mui_aa.c):
 * typed text is the theme TEXT, not the MUI prefs pen (black on a dark face);
 * a field is never narrower than its placeholder; a rounded corner wedge takes
 * the colour of the panel the object sits on, not the window BACKGROUND. */
#include "map/mui_aa.h"

#include <stdio.h>

#define CHECK(c)                                                 \
    do {                                                         \
        if (!(c)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            return 1;                                            \
        }                                                        \
    } while (0)

int main(void) {
    uint32_t tok[SEL_GUI_TOKEN_COUNT];
    mui_aa_field_t m;
    int i;
    for (i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        tok[i] = 0x102030u + (uint32_t)i * 0x010101u;
    }
    /* input text is drawn in the theme text colour, which differs from the field face */
    CHECK(mui_aa_field_string_fg(tok) == tok[SEL_GUI_TOKEN_TEXT]);
    CHECK(mui_aa_field_string_fg(tok) != mui_aa_field_string_bg(tok, 0));
    /* placeholder fits: text width + the 4 px the draw keeps + both inner paddings */
    mui_aa_field_metrics(0, 256, &m);
    CHECK(mui_aa_field_min_width(&m, 100) == 104 + 2 * m.pad_x);
    CHECK(mui_aa_field_min_width(&m, 0) == 0);
    /* corner wedges of a card on a Surface panel are Surface; outside any panel, BACKGROUND */
    CHECK(mui_aa_parent_rgb(tok, MUI_AA_PANEL_SURFACE) == tok[SEL_GUI_TOKEN_SURFACE]);
    CHECK(mui_aa_parent_rgb(tok, MUI_AA_PANEL_SURFACE) != tok[SEL_GUI_TOKEN_BACKGROUND]);
    CHECK(mui_aa_parent_rgb(tok, MUI_AA_PANEL_RAISED) == tok[SEL_GUI_TOKEN_SURFACE_RAISED]);
    CHECK(mui_aa_parent_rgb(tok, -1) == tok[SEL_GUI_TOKEN_BACKGROUND]);
    puts("mui_aafield_test: ok");
    return 0;
}
