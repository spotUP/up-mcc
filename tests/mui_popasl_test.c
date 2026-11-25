/* Host test of the UP-Popasl string <-> requester mapping (map/mui_aa.c mui_aa_path_*, _font_*, _hex_*): drawer/file split and join
 * as PathPart/FilePart/AddPart, the "name/size" font spec both ways, the screen mode display ID as hex, and cut-off text. */
#include <stdio.h>
#include <string.h>
#include "map/mui_aa.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    char d[32], f[32], o[32];
    long sz = -1;
    CHECK(mui_aa_path_split("Work:a/b.lha", d, 32, f, 32) && !strcmp(d, "Work:a") && !strcmp(f, "b.lha"));
    CHECK(mui_aa_path_split("RAM:x", d, 32, f, 32) && !strcmp(d, "RAM:") && !strcmp(f, "x"));
    CHECK(mui_aa_path_split("Work:a/", d, 32, f, 32) && !strcmp(d, "Work:a") && !strcmp(f, ""));
    CHECK(mui_aa_path_split("file", d, 32, f, 32) && !strcmp(d, "") && !strcmp(f, "file"));
    CHECK(mui_aa_path_split("", d, 32, f, 32) && !strcmp(d, "") && !strcmp(f, ""));
    CHECK(mui_aa_path_join("Work:a", "b", o, 32) && !strcmp(o, "Work:a/b"));
    CHECK(mui_aa_path_join("RAM:", "x", o, 32) && !strcmp(o, "RAM:x"));
    CHECK(mui_aa_path_join("Work:a/", "x", o, 32) && !strcmp(o, "Work:a/x"));
    CHECK(mui_aa_path_join("", "x", o, 32) && !strcmp(o, "x"));
    CHECK(mui_aa_path_join("Work:a", "", o, 32) && !strcmp(o, "Work:a"));
    CHECK(!mui_aa_path_join("Work:abcdef", "ghij", o, 8) && !strcmp(o, "Work:ab")); /* cut, still terminated */
    CHECK(mui_aa_font_spec("topaz.font", 8, o, 32) && !strcmp(o, "topaz/8"));
    CHECK(mui_aa_font_spec("XHelvetica", 11, o, 32) && !strcmp(o, "XHelvetica/11"));
    CHECK(mui_aa_font_parse("topaz/8", f, 32, &sz) && !strcmp(f, "topaz.font") && sz == 8);
    CHECK(mui_aa_font_parse("helvetica.font/13", f, 32, &sz) && !strcmp(f, "helvetica.font") && sz == 13);
    CHECK(mui_aa_font_parse("courier", f, 32, &sz) && !strcmp(f, "courier.font") && sz == 0);
    CHECK(mui_aa_font_parse("", f, 32, &sz) && !strcmp(f, "") && sz == 0);
    CHECK(mui_aa_hex_text(0x50011000UL, o, 32) && !strcmp(o, "0x50011000"));
    CHECK(mui_aa_hex_parse("0x50011000") == 0x50011000UL && mui_aa_hex_parse("a00f") == 0xA00FUL && mui_aa_hex_parse("") == 0);
    if (fails == 0) {
        printf("[OK] mui-popasl\n");
    }
    return fails != 0;
}
