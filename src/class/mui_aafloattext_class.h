/* UP-Floattext: the MUIC_Floattext counterpart. Read-only text, word wrapped to the box width, scrolled by the wheel and a
 * UP-Scroller (UP_NewFloattext pairs them as UP_NewList does). Takes MUIA_Floattext_Text, _Justify, _SkipChars and _TabSize.
 * Amiga only; the wrap is mtl_wrap, the TAB and skip pass mtl_float_prep, the justify spread mtl_justify_gap (mui_text_lines.c). */
#ifndef MUI_AAFLOATTEXT_CLASS_H
#define MUI_AAFLOATTEXT_CLASS_H

#include <mcc/up.h>

void mui_aafloattext_class_free(void);

#endif
