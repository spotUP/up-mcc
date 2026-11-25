#include "mui_ftxt.h"

#include <string.h>

static unsigned long be32(const unsigned char *p) {
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

static void put32(unsigned char *p, unsigned long v) {
    p[0] = (unsigned char)(v >> 24), p[1] = (unsigned char)(v >> 16), p[2] = (unsigned char)(v >> 8), p[3] = (unsigned char)v;
}

int ftxt_size(int n) {
    return 12 + 8 + n + (n & 1);
}

int ftxt_encode(const char *text, int n, unsigned char *out, int cap) {
    int total;
    if (n < 0 || cap < (total = ftxt_size(n))) {
        return -1;
    }
    memcpy(out, "FORM", 4);
    put32(out + 4, (unsigned long)(total - 8));
    memcpy(out + 8, "FTXTCHRS", 8);
    put32(out + 16, (unsigned long)n);
    memcpy(out + 20, text, (size_t)n);
    if (n & 1) {
        out[20 + n] = 0;
    }
    return total;
}

int ftxt_decode(const unsigned char *in, int n, char *out, int cap) {
    int pos = 12, len = 0;
    unsigned long form;
    if (cap < 1 || n < 12 || memcmp(in, "FORM", 4) != 0 || memcmp(in + 8, "FTXT", 4) != 0) {
        return -1;
    }
    form = be32(in + 4);
    if (form + 8 < (unsigned long)n) {
        n = (int)(form + 8); /* ignore bytes after the form */
    }
    while (pos + 8 <= n) {
        unsigned long clen = be32(in + pos + 4);
        int avail = n - (pos + 8), take = clen < (unsigned long)avail ? (int)clen : avail, i;
        if (memcmp(in + pos, "CHRS", 4) == 0) {
            for (i = 0; i < take && len < cap - 1; i++) {
                unsigned char c = in[pos + 8 + i];
                if (c == '\n' || (c >= 0x20 && c <= 0x7E) || c >= 0xA0) {
                    out[len++] = (char)c;
                }
            }
        }
        if (clen > (unsigned long)avail) {
            break;
        }
        pos += 8 + (int)clen + (int)(clen & 1);
    }
    out[len] = '\0';
    return len;
}
