/* Pure-C IFF FTXT (FORM FTXT / CHRS) encode and decode for the Amiga clipboard. No Amiga headers, no allocation. */
#ifndef MUI_FTXT_H
#define MUI_FTXT_H

/* Bytes needed to hold text of n bytes as FORM FTXT. */
int ftxt_size(int n);
/* Write text[0..n) as FORM FTXT with one CHRS chunk (padded to an even length). Returns the bytes written, or -1 when cap is too small. */
int ftxt_encode(const char *text, int n, unsigned char *out, int cap);
/* Collect every CHRS chunk of a FORM FTXT in in[0..n) into out (cap bytes, NUL terminated). Only LF and printable bytes (0x20-0x7E,
 * 0xA0-0xFF) are kept: CR, TAB and control bytes are dropped, so a paste can never insert what typing refuses. A chunk cut short by
 * n is read as far as it goes. Returns the text length, or -1 when in is not a FORM FTXT. */
int ftxt_decode(const unsigned char *in, int n, char *out, int cap);

#endif
