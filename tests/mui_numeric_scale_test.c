/* Host test of the UP-Numericbutton and UP-Scale pure maths (map/mui_aa.c): drag value, number text, tick plan, tick metrics. */
#include <stdio.h>
#include <string.h>
#include "map/mui_aa.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    mui_aa_scale_tick_t t[MUI_AA_SCALE_MAX];
    char buf[16];
    int n, i, a, b, c;

    /* drag: two button widths sweep the range, clamped at both ends */
    CHECK(mui_aa_numeric_drag(0, 0, 50, 0, 100) == 0);
    CHECK(mui_aa_numeric_drag(0, 50, 50, 0, 100) == 50);
    CHECK(mui_aa_numeric_drag(0, 100, 50, 0, 100) == 100);
    CHECK(mui_aa_numeric_drag(50, 1000, 50, 0, 100) == 100);
    CHECK(mui_aa_numeric_drag(50, -1000, 50, 0, 100) == 0);
    CHECK(mui_aa_numeric_drag(5, 10, 0, 0, 100) == 0); /* no width: lo */
    CHECK(mui_aa_numeric_drag(5, 10, 50, 7, 7) == 7);  /* empty range */

    /* text */
    CHECK(mui_aa_numeric_text(0, buf, sizeof buf) == 1 && strcmp(buf, "0") == 0);
    CHECK(mui_aa_numeric_text(1234, buf, sizeof buf) == 4 && strcmp(buf, "1234") == 0);
    CHECK(mui_aa_numeric_text(-56, buf, sizeof buf) == 3 && strcmp(buf, "-56") == 0);
    CHECK(mui_aa_numeric_text(-2147483647L - 1, buf, sizeof buf) == 11 && strcmp(buf, "-2147483648") == 0);
    CHECK(mui_aa_numeric_text(123456, buf, 4) == 3 && strcmp(buf, "123") == 0); /* truncated, still terminated */

    /* scale plan: ends on the box edges, even spread, majors at both ends and the middle of an odd count */
    n = mui_aa_scale_plan(101, 0, 100, 11, t);
    CHECK(n == 11 && t[0].x == 0 && t[10].x == 100 && t[0].value == 0 && t[10].value == 100);
    CHECK(t[5].x == 50 && t[5].value == 50 && t[5].major && t[0].major && t[10].major && !t[1].major);
    a = b = c = 0;
    for (i = 0; i < n; i++) {
        a += t[i].major;
        if (i > 0 && t[i].x <= t[i - 1].x) {
            b++;
        }
        c += t[i].x < 0 || t[i].x > 100;
    }
    CHECK(a == 3 && b == 0 && c == 0);
    n = mui_aa_scale_plan(40, 0, 10, 4, t); /* even count: only the ends are major */
    CHECK(n == 4 && t[0].major && t[3].major && !t[1].major && !t[2].major && t[3].x == 39);
    CHECK(mui_aa_scale_plan(100, 0, 10, 1, t) == 2);                          /* ticks clamp up to 2 */
    CHECK(mui_aa_scale_plan(1000, 0, 10, 500, t) == MUI_AA_SCALE_MAX);        /* and down to the maximum */
    CHECK(mui_aa_scale_plan(100, 5, 5, 11, t) == 0 && mui_aa_scale_plan(1, 0, 9, 11, t) == 0);
    n = mui_aa_scale_plan(61, -50, 50, 3, t);
    CHECK(n == 3 && t[1].value == 0 && t[1].x == 30 && t[1].major);

    /* metrics */
    mui_aa_scale_metrics(16, &a, &b, &c);
    CHECK(b == 8 && a == 4 && c == 2);
    mui_aa_scale_metrics(2, &a, &b, &c);
    CHECK(b == 4 && a == 2);

    printf(fails ? "mui_numeric_scale_test: %d FAILED\n" : "mui_numeric_scale_test: OK\n", fails);
    return fails != 0;
}
