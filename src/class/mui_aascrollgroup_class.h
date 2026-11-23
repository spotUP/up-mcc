/* UP-Scrollgroup: a Group subclass of one UP-Virtgroup (the contents) and one UP-Scroller on its right edge. The bar takes width only
 * while the contents are taller than the view (map/mui_aascr.c); the vertical bar drives UPA_Virtgroup_Top. Amiga only. */
#ifndef MUI_AASCROLLGROUP_CLASS_H
#define MUI_AASCROLLGROUP_CLASS_H

#include <mcc/up.h>
#include <stdint.h>

/* Scrollgroups constructed since start. */
uint32_t mui_aascrollgroup_made(void);
void mui_aascrollgroup_class_free(void);

#endif
