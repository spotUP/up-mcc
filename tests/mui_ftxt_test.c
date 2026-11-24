/* Host test of the FTXT clipboard encoding (src/mui_ftxt.c). */
#include "mui_ftxt.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    unsigned char b[64];
    char t[32];
    int n;
    /* round trip, even and odd length */
    n = ftxt_encode("abcd", 4, b, sizeof b);
    assert(n == 24 && n == ftxt_size(4) && !memcmp(b, "FORM\0\0\0\x10" "FTXTCHRS\0\0\0\x04" "abcd", 24));
    assert(ftxt_decode(b, n, t, sizeof t) == 4 && !strcmp(t, "abcd"));
    n = ftxt_encode("abc", 3, b, sizeof b);
    assert(n == 24 && b[23] == 0 && b[7] == 0x10); /* padded to even */
    assert(ftxt_decode(b, n, t, sizeof t) == 3 && !strcmp(t, "abc"));
    n = ftxt_encode("", 0, b, sizeof b);
    assert(n == 20 && ftxt_decode(b, n, t, sizeof t) == 0 && t[0] == 0);
    assert(ftxt_encode("abcd", 4, b, 23) == -1);
    /* decode drops CR, TAB and controls, keeps LF and Latin-1 */
    n = ftxt_encode("a\r\nb\tc\x01\xe5", 8, b, sizeof b);
    assert(ftxt_decode(b, n, t, sizeof t) == 5 && !strcmp(t, "a\nb" "c\xe5"));
    /* output cap cuts the text */
    n = ftxt_encode("abcdefgh", 8, b, sizeof b);
    assert(ftxt_decode(b, n, t, 4) == 3 && !strcmp(t, "abc"));
    /* not FTXT, too short, truncated chunk, two chunks, other chunk skipped */
    assert(ftxt_decode((const unsigned char *)"FORM\0\0\0\4ILBM", 12, t, sizeof t) == -1);
    assert(ftxt_decode(b, 5, t, sizeof t) == -1);
    n = ftxt_encode("abcdef", 6, b, sizeof b);
    assert(ftxt_decode(b, 23, t, sizeof t) == 3 && !strcmp(t, "abc")); /* cut after 3 payload bytes */
    {
        unsigned char m[80];
        memcpy(m, "FORM\0\0\0\x28" "FTXT" "AUTH\0\0\0\2xy" "CHRS\0\0\0\2hi" "CHRS\0\0\0\3yo!\0", 12 + 10 + 10 + 12);
        m[7] = 12 + 10 + 10 + 12 - 8 + 0;
        assert(ftxt_decode(m, 12 + 10 + 10 + 12, t, sizeof t) == 5 && !strcmp(t, "hiyo!"));
    }
    puts("mui_ftxt_test: ok");
    return 0;
}
