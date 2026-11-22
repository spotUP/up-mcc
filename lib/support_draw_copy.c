/* I1491a2: the library's own copy of src/mui_aa_draw.c. UP_DRAW_LIBRARY makes the context list one per system (guarded by a
 * semaphore) and attaches every context to the theme registry directly; the file itself is the one the static build compiles. A
 * source property cannot differ per target, so the define lives in this wrapper. It is listed before sel_mcc_static on the link line,
 * so the archive member of the same name is not pulled. */
#define UP_DRAW_LIBRARY 1
#include "../src/mui_aa_draw.c"
