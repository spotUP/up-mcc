/* UP-Virtgroup: a Group subclass whose contents (MUIA_Group_Child, one inner group) are laid out at their own size and shown through
 * the view at UPA_Virtgroup_Left / UPA_Virtgroup_Top, clipped to the view. Offset maths is in map/mui_aascr.c. Amiga only. */
#ifndef MUI_AAVIRTGROUP_CLASS_H
#define MUI_AAVIRTGROUP_CLASS_H

#include <mcc/up.h>
#include <stdint.h>

/* Virtgroups constructed since start. */
uint32_t mui_aavirtgroup_made(void);
void mui_aavirtgroup_class_free(void);

#endif
