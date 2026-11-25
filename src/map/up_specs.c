/* MUI gap E: image and frame spec strings (see up_specs.h). */
#include "up_specs.h"

#include <string.h>

static int up_hex(int c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* One 32-bit gun "XXXXXXXX" at s: its top byte, or -1 when s has fewer than 2 hex digits. */
static int up_gun(const char* s) {
    int hi = up_hex(s[0]), lo = hi < 0 ? -1 : up_hex(s[1]);
    return lo < 0 ? -1 : hi * 16 + lo;
}

int up_imspec_parse(const char* spec, const char** tail, uint32_t* rgb, int* has_rgb) {
    const char* g;
    int r, gr, b;
    *has_rgb = 0;
    *tail = "";
    if (spec == NULL || spec[0] < '0' || spec[0] > '7' || spec[1] != ':') {
        return -1;
    }
    *tail = spec + 2;
    g = spec + 2;
    if (spec[0] == '2' && g[0] == 'r') {
        const char *c1 = strchr(g, ','), *c2 = c1 != NULL ? strchr(c1 + 1, ',') : NULL;
        r = up_gun(g + 1);
        gr = c1 != NULL ? up_gun(c1 + 1) : -1;
        b = c2 != NULL ? up_gun(c2 + 1) : -1;
        if (r >= 0 && gr >= 0 && b >= 0) {
            *rgb = (uint32_t)r << 16 | (uint32_t)gr << 8 | (uint32_t)b;
            *has_rgb = 1;
        }
    }
    return spec[0] - '0';
}

void up_imspec_format(int kind, const char* value, char* out) {
    size_t n = strlen(value);
    if (n > UP_IMSPEC_LEN - 3) {
        n = UP_IMSPEC_LEN - 3;
    }
    out[0] = (char)('0' + (kind < 0 || kind > 7 ? 0 : kind));
    out[1] = ':';
    memcpy(out + 2, value, n);
    out[2 + n] = '\0';
}

void up_frspec_parse(const char* spec, int v[6]) {
    int i, end = 0;
    for (i = 0; i < 6; i++) {
        int x = end || spec == NULL ? -1 : up_hex(spec[i]);
        end = end || spec == NULL || spec[i] == '\0';
        v[i] = x < 0 ? 0 : x;
    }
    v[1] = v[1] != 0;
}

void up_frspec_format(const int v[6], char* out) {
    int i;
    for (i = 0; i < 6; i++) {
        int x = v[i] < 0 ? 0 : v[i] > (i == 1 ? 1 : 15) ? (i == 1 ? 1 : 15) : v[i];
        out[i] = "0123456789abcdef"[x];
    }
    out[6] = '\0';
}
