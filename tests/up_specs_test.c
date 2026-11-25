/* Host test of the MUI image and frame spec strings (map/up_specs.c): kind and tail, the RGB of a "2:r" pen spec, bad specs,
 * format cut to the buffer, frame digits both ways with clamping and short specs. */
#include <stdio.h>
#include <string.h>
#include "map/up_specs.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    const char* t;
    uint32_t rgb = 7;
    int has, v[6], w[6] = {9, 3, 1, 2, 20, -1};
    char o[UP_IMSPEC_LEN], f[UP_FRSPEC_LEN], big[100];
    CHECK(up_imspec_parse("2:rFFFFFFFF,80000000,00000000", &t, &rgb, &has) == 2 && has && rgb == 0xFF8000u);
    CHECK(up_imspec_parse("2:m5", &t, &rgb, &has) == 2 && !has && !strcmp(t, "m5"));
    CHECK(up_imspec_parse("2:rFF", &t, &rgb, &has) == 2 && !has);
    CHECK(up_imspec_parse("5:Work:pics/sky.iff", &t, &rgb, &has) == 5 && !strcmp(t, "Work:pics/sky.iff"));
    CHECK(up_imspec_parse("6:128", &t, &rgb, &has) == 6 && !strcmp(t, "128"));
    CHECK(up_imspec_parse("8:x", &t, &rgb, &has) == -1 && !strcmp(t, ""));
    CHECK(up_imspec_parse("5", &t, &rgb, &has) == -1 && up_imspec_parse(NULL, &t, &rgb, &has) == -1);
    up_imspec_format(1, "11", o);
    CHECK(!strcmp(o, "1:11"));
    memset(big, 'a', 99), big[99] = 0;
    up_imspec_format(5, big, o);
    CHECK(strlen(o) == UP_IMSPEC_LEN - 1 && o[0] == '5' && o[1] == ':');
    up_frspec_parse("3021af", v);
    CHECK(v[0] == 3 && v[1] == 0 && v[2] == 2 && v[3] == 1 && v[4] == 10 && v[5] == 15);
    up_frspec_parse("91", v);
    CHECK(v[0] == 9 && v[1] == 1 && v[2] == 0 && v[5] == 0);
    up_frspec_parse(NULL, v);
    CHECK(v[0] == 0 && v[1] == 0);
    up_frspec_format(w, f);
    CHECK(!strcmp(f, "9112f0"));
    up_frspec_parse(f, v);
    up_frspec_format(v, f);
    CHECK(!strcmp(f, "9112f0"));
    if (fails == 0) {
        printf("[OK] up-specs\n");
    }
    return fails != 0;
}
