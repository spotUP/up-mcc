/* UP-Checkmark: MUIC_Checkmark equivalent. An Area subclass in toggle input mode, so MUIA_Selected (ISG BOOL) and its notification
 * are MUI's own. Drawn as a rounded box (outline off, ACCENT on) with a drawn tick, and an optional label (UPA_Checkmark_Label) right
 * of it. Plans and tick geometry are in map/mui_aasel.c. Amiga only. */
#ifndef MUI_AACHECKMARK_CLASS_H
#define MUI_AACHECKMARK_CLASS_H

#include <mcc/up.h>
#include <stdint.h>

/* Checkmarks constructed since start. */
uint32_t mui_aacheckmark_made(void);
void mui_aacheckmark_class_free(void);

#endif
