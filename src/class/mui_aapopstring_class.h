/* UP-Popstring, UP-Poplist, UP-Popobject, UP-Popasl, UP-Popscreen (the MUIC_Pop* equivalents; Popasl runs an ASL requester instead). A horizontal Group of a UP-Field and a chevron button; the
 * button opens a borderless popup window under the object. Poplist builds a UP-List popup and copies the chosen entry into the string.
 * All five classes live in mui_aapopstring_class.c. Amiga only; the open/close state machine is in map/mui_aa.c (mui_aa_pop_*). */
#ifndef MUI_AAPOPSTRING_CLASS_H
#define MUI_AAPOPSTRING_CLASS_H

#include <mcc/up.h>

void mui_aapopstring_class_free(void);

#endif
