/* Host test of the UP-Popstring / UP-Poplist popup state machine (map/mui_aa.c mui_aa_pop_*): first open creates, later opens reuse,
 * a double click, a second click on the dropdown, the click that also deactivates the popup, and a repeated close notification. */
#include <stdio.h>
#include "map/mui_aa.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    mui_aa_pop_t s = {0};
    /* first click creates, popup open */
    CHECK(mui_aa_pop_click(&s, 1000) == MUI_AA_POP_OPEN_NEW);
    mui_aa_pop_opened(&s);
    /* second click while open closes (the cycle ignored or crashed here) */
    CHECK(mui_aa_pop_click(&s, 1500) == MUI_AA_POP_CLOSE);
    mui_aa_pop_closed(&s, 0, 1501);
    mui_aa_pop_closed(&s, 0, 1502); /* a repeated close notification changes nothing */
    CHECK(!s.open && s.closed_ms == 1501);
    /* third click reuses the window */
    CHECK(mui_aa_pop_click(&s, 3000) == MUI_AA_POP_OPEN_REUSE);
    mui_aa_pop_opened(&s);
    /* click on the button deactivates the popup first, then lands: it must toggle shut, not reopen */
    mui_aa_pop_closed(&s, 1, 4000);
    CHECK(mui_aa_pop_click(&s, 4100) == MUI_AA_POP_NONE);
    /* the guard is spent: the next click opens */
    CHECK(mui_aa_pop_click(&s, 4200) == MUI_AA_POP_OPEN_REUSE);
    mui_aa_pop_opened(&s);
    /* deactivated close, but the next click comes much later: opens */
    mui_aa_pop_closed(&s, 1, 5000);
    CHECK(mui_aa_pop_click(&s, 5000 + MUI_AA_POP_GUARD_MS) == MUI_AA_POP_OPEN_REUSE);
    mui_aa_pop_opened(&s);
    /* pick closes (not by deactivation): the next click opens at once */
    mui_aa_pop_closed(&s, 0, 6000);
    CHECK(mui_aa_pop_click(&s, 6001) == MUI_AA_POP_OPEN_REUSE);
    /* millisecond counter wrap */
    s.open = 0; s.by_deactivate = 1; s.closed_ms = 0xFFFFFFF0u;
    CHECK(mui_aa_pop_click(&s, 10) == MUI_AA_POP_NONE);
    /* many open/close cycles keep the state consistent */
    {
        int i;
        mui_aa_pop_t t = {0};
        for (i = 0; i < 1000; i++) {
            int a = mui_aa_pop_click(&t, (uint32_t)i * 1000u);
            CHECK(a == (i == 0 ? MUI_AA_POP_OPEN_NEW : MUI_AA_POP_OPEN_REUSE));
            mui_aa_pop_opened(&t);
            CHECK(mui_aa_pop_click(&t, (uint32_t)i * 1000u + 10u) == MUI_AA_POP_CLOSE);
            mui_aa_pop_closed(&t, 0, (uint32_t)i * 1000u + 11u);
        }
    }
    printf(fails ? "mui-pop-state FAILED\n" : "mui-pop-state ok\n");
    return fails != 0;
}
