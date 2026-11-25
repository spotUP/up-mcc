/* Host test of mui_aa_scroller_press (map/mui_aa.c), the SELECTDOWN rule UP-Scroller and UP-TextArea's own bar share. */
#include <stdio.h>
#include "map/mui_aa.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    up_scroller_t m;
    int ty, th, grab;
    up_scroller_metrics(8, 256, &m);
    /* 100 entries, 10 visible, first 45, track 200: thumb 20 high at 90 */
    CHECK(mui_aa_scroller_geom(&m, 200, 45, 10, 100, &ty, &th) && ty == 90 && th == 20);
    CHECK(mui_aa_scroller_press(&m, 200, ty + 5, 45, 10, 100, &grab) == 45 && grab == 5); /* on the thumb: drag, no move */
    CHECK(mui_aa_scroller_press(&m, 200, 10, 45, 10, 100, &grab) == 35 && grab == -1);    /* above: one page up */
    CHECK(mui_aa_scroller_press(&m, 200, 190, 45, 10, 100, &grab) == 55 && grab == -1);   /* below: one page down */
    CHECK(mui_aa_scroller_press(&m, 200, 10, 4, 10, 100, &grab) == 0);                    /* clamped at the top */
    CHECK(mui_aa_scroller_press(&m, 200, 0, 88, 10, 100, &grab) == 78);
    CHECK(mui_aa_scroller_press(&m, 200, 199, 85, 10, 100, &grab) == 90);                 /* clamped at the end */
    CHECK(mui_aa_scroller_press(&m, 200, 50, 0, 10, 8, &grab) == 0 && grab == -1);       /* everything fits: no thumb, nothing moves */
    /* dragging the held thumb to the track end reaches the last page */
    CHECK(mui_aa_scroller_first_at(&m, 200, 5, 200 - 20 + 5, 10, 100) == 90);
    if (fails == 0) {
        printf("mui_aascroller_test: all passed\n");
    }
    return fails != 0;
}
