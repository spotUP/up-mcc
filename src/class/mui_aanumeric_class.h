/* UP-Numericbutton: MUIC_Numericbutton equivalent. An Area subclass drawn as a button showing the value as a number. A click (press and
 * release without a drag) opens a small popup with a UP-Slider under it; a horizontal drag over the button changes the value directly
 * (two button widths sweep the range). Value maths and the popup state machine are pure (map/mui_aa.c). Amiga only. */
#ifndef MUI_AANUMERIC_CLASS_H
#define MUI_AANUMERIC_CLASS_H

#include <mcc/up.h>
#include <stdint.h>

void mui_aanumeric_class_free(void);

#endif
