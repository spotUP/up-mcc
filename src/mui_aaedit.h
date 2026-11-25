/* Shared parts of the UP-Field and UP-TextArea own editors (Amiga only): the clipboard (clipboard.device unit 0, FTXT through
 * mui_ftxt.c) and the flat field frame paint. The pure caret, selection and buffer maths is in mui_text_lines.c (host tested). */
#ifndef MUI_AAEDIT_H
#define MUI_AAEDIT_H

#include "map/mui_aa.h"
#include "mui_aa_draw.h"

/* Write n bytes of s to the clipboard as FTXT CHRS. Failure (no device, no memory) is silent. */
void aaedit_clip_put(const char* s, int n);
/* Read the clip text into out (cap bytes, NUL terminated). Returns its length, 0 when empty, not text, or on failure. */
int aaedit_clip_get(char* out, int cap);
/* Paint the field box (l, t, w, h) from a mui_aa_field_plan: parent colour in the corners, each ring once, the face once, then the
 * low-colour outline. Every pixel of the box is painted, nothing outside it. */
void aaedit_frame(aa_ctx* aa, struct RastPort* rp, int l, int t, int w, int h, const mui_aa_check_op_t* ops, int n);

#endif
