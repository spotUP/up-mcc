/* Selecta_AAField (I667h, design 2026-10-28-mui-custom-classes.md section 3.4): MUI 3.8 Area subclass that owns its single line of text
 * (no inner MUIC_String since 2026-11-25). It paints the flat rounded field (face SURFACE_RAISED, 1 px outline, FOCUS_RING while
 * active), the text in the AA font, the selection in ACCENT and the caret, and answers the MUIA_String_* attributes callers use.
 * Amiga only; the maths is in map/mui_aa.c and mui_text_lines.c. */
#ifndef MUI_AAFIELD_CLASS_H
#define MUI_AAFIELD_CLASS_H

#include <mcc/up.h>
#include <stdint.h>

typedef struct mui_aafield_spec {
    const uint32_t* tokens; /* theme token array (SEL_GUI_TOKEN_*) */
    int maxlen;
    int max_w; /* I615d: cap the field's MaxWidth in px (the top bar field); 0 = unbounded */
    const char* placeholder; /* I733: hint drawn in TEXT_MUTED while the field is empty and not active; NULL = none; must outlive the field */
    long id; /* MUIM_Application_ReturnID on MUIA_String_Acknowledge; 0 = no notification */
    int secret; /* I1469m: 1 = the field shows '*' per byte (MUIA_String_Secret), for the account password */
} mui_aafield_spec;

Object* mui_aafield_new(const mui_aafield_spec* spec);
/* Fields constructed since start (sentinel: the number of string sites the window build made). */
uint32_t mui_aafield_made(void);
void mui_aafield_class_free(void);

#endif
