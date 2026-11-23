/* UP-Radio: MUIC_Radio equivalent. An Area subclass: a vertical list of round dots with labels (UPA_Radio_Entries). UPA_Radio_Active
 * (ISG LONG, notifies) is the chosen index; a click picks a row, Up and Down step with wrap while the object is active. The chosen dot
 * is an ACCENT ring with an ACCENT centre. Plans and row maths are in map/mui_aasel.c. Amiga only. */
#ifndef MUI_AARADIO_CLASS_H
#define MUI_AARADIO_CLASS_H

#include <mcc/up.h>
#include <stdint.h>

/* Radios constructed since start. */
uint32_t mui_aaradio_made(void);
void mui_aaradio_class_free(void);

#endif
