/* Host test of the UP-Dirlist sort order (map/mui_aa.c mui_aa_dir_cmp) and the RejectIcons name check (mui_aa_icon_name). */
#include <stdio.h>
#include "map/mui_aa.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    mui_aa_dirkey drw = {"Zeta", 0, 10, 0, 0, 1}, a = {"alpha", 500, 9, 0, 0, 0}, b = {"Beta", 100, 11, 0, 0, 0}, b2 = {"beta", 100, 11, 0, 0, 0};
    CHECK(mui_aa_dir_cmp(&a, &b, 0, 0, 0) < 0);  /* name, case blind */
    CHECK(mui_aa_dir_cmp(&a, &b, 0, 0, 1) > 0);  /* high to low */
    CHECK(mui_aa_dir_cmp(&b, &b2, 0, 0, 0) == 0);
    CHECK(mui_aa_dir_cmp(&drw, &a, 0, 0, 0) < 0); /* drawers first */
    CHECK(mui_aa_dir_cmp(&drw, &a, 0, 0, 1) < 0); /* high_low keeps the grouping */
    CHECK(mui_aa_dir_cmp(&drw, &a, 0, 1, 0) > 0); /* drawers last */
    CHECK(mui_aa_dir_cmp(&drw, &a, 0, 2, 0) > 0); /* mixed: by name */
    CHECK(mui_aa_dir_cmp(&a, &b, 1, 0, 0) < 0);  /* date: day 9 before 11 */
    CHECK(mui_aa_dir_cmp(&a, &b, 2, 0, 0) > 0);  /* size: 500 after 100 */
    CHECK(mui_aa_dir_cmp(&b, &b2, 2, 0, 0) == 0);
    CHECK(mui_aa_icon_name("Disk.INFO") && mui_aa_icon_name(".info") && !mui_aa_icon_name("info") && !mui_aa_icon_name("x.inf"));
    if (fails == 0) {
        printf("[OK] mui-dirlist\n");
    }
    return fails != 0;
}
