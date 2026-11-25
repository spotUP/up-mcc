/* Host test of the UP-Image fit maths (map/mui_aa.c). */
#include <stdio.h>
#include "map/mui_aa.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    int w, h, x, y;
    mui_aa_image_fit(40, 20, 100, 100, 0, &w, &h, &x, &y); /* natural size, centred */
    CHECK(w == 40 && h == 20 && x == 30 && y == 40);
    mui_aa_image_fit(40, 20, 100, 100, 1, &w, &h, &x, &y); /* width limits */
    CHECK(w == 100 && h == 50 && x == 0 && y == 25);
    mui_aa_image_fit(20, 40, 100, 100, 1, &w, &h, &x, &y); /* height limits */
    CHECK(w == 50 && h == 100 && x == 25 && y == 0);
    mui_aa_image_fit(1000, 1, 10, 10, 1, &w, &h, &x, &y); /* never below one pixel */
    CHECK(w == 10 && h == 1);
    mui_aa_image_fit(40, 20, 10, 10, 0, &w, &h, &x, &y); /* larger than the box: negative offset */
    CHECK(w == 40 && x == -15);
    mui_aa_image_fit(0, 0, 10, 10, 1, &w, &h, &x, &y); /* no picture */
    CHECK(w == 0 && h == 0);
    if (fails == 0) {
        printf("mui_image_fit_test OK\n");
    }
    return fails != 0;
}
