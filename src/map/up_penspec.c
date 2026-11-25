/* MUI pen specification text, parse and format (up_penspec.h). No stdio: the class libraries must not link libnix printf. */
#include "up_penspec.h"

#include <string.h>

static int pen_hex(const char** s, uint32_t* v) {
    int i, c;
    *v = 0;
    for (i = 0; i < 8; i++) {
        c = (unsigned char)(*s)[i];
        c = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (c < 0) {
            return 0;
        }
        *v = *v << 4 | (uint32_t)c;
    }
    *s += 8;
    return 1;
}

int up_penspec_parse(const char* s, up_penspec_t* out) {
    long n = 0;
    const char* p;
    memset(out, 0, sizeof *out);
    if (s == NULL) {
        return UP_PEN_NONE;
    }
    if (s[0] == 'r') {
        p = s + 1;
        if (pen_hex(&p, &out->r) && *p++ == ',' && pen_hex(&p, &out->g) && *p++ == ',' && pen_hex(&p, &out->b)) {
            return out->kind = UP_PEN_RGB;
        }
        memset(out, 0, sizeof *out);
        return UP_PEN_NONE;
    }
    if ((s[0] != 'p' && s[0] != 'm') || s[1] < '0' || s[1] > '9') {
        return UP_PEN_NONE;
    }
    for (p = s + 1; *p >= '0' && *p <= '9' && n < 65536; p++) {
        n = n * 10 + (*p - '0');
    }
    out->n = n;
    return out->kind = s[0] == 'p' ? UP_PEN_CMAP : UP_PEN_MUI;
}

static char* pen_put_hex(char* o, uint32_t v) {
    int i;
    for (i = 28; i >= 0; i -= 4) {
        *o++ = "0123456789abcdef"[(v >> i) & 15];
    }
    return o;
}

void up_penspec_format(const up_penspec_t* p, char* buf) {
    char tmp[12], *o = buf;
    unsigned long n;
    int k = 0;
    if (p->kind == UP_PEN_RGB) {
        *o++ = 'r';
        o = pen_put_hex(o, p->r);
        *o++ = ',';
        o = pen_put_hex(o, p->g);
        *o++ = ',';
        o = pen_put_hex(o, p->b);
    } else if (p->kind == UP_PEN_CMAP || p->kind == UP_PEN_MUI) {
        *o++ = p->kind == UP_PEN_CMAP ? 'p' : 'm';
        n = p->n < 0 ? 0 : (unsigned long)p->n;
        do {
            tmp[k++] = (char)('0' + n % 10);
            n /= 10;
        } while (n != 0 && k < 10);
        while (k > 0) {
            *o++ = tmp[--k];
        }
    }
    *o = 0;
}

uint32_t up_penspec_rgb24(uint32_t r, uint32_t g, uint32_t b) {
    return (r >> 24) << 16 | (g >> 24) << 8 | b >> 24;
}

uint32_t up_penspec_gun32(uint32_t v8) {
    return (v8 & 0xFFu) * 0x01010101u;
}
