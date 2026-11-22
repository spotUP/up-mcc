/* I1491a: what the class libraries (.mcc/.mcp) call instead of linking it. Each name here is also defined by the static build
 * (src/map/mui_log.c, mui_theme.c, mui_icons.c); the libraries get theirs from up-mcc.library, so no stdio, no theme parser and no
 * baked icon data lands in a .mcc (about 51 KB of base each before this file). The archive members are pulled only for undefined
 * names, and these definitions come first. */
#include <stdarg.h>
#include <stdint.h>

#include <exec/types.h>
#include <proto/up_mcc.h>

#include "map/mui_icons.h"
#include "map/mui_log.h"
#include "map/mui_theme.h"

void aa_log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    UP_Log((STRPTR)fmt, (APTR)ap); /* m68k: a va_list is the stack pointer */
    va_end(ap);
}

/* The trace sink belongs to a host that links the static build; a library never sets one. */
void aa_trace(const char* phase) {
    (void)phase;
}

const char* const* mui_font_files(void) {
    return (const char* const*)UP_FontFiles();
}

int mui_icon_index(const char* name) {
    return (int)UP_IconFind((STRPTR)name);
}

const uint8_t* mui_icon_pick(int icon, int w, int h, int* bp, int* bh) {
    LONG wh[2] = {w, h};
    const uint8_t* body = (const uint8_t*)UP_IconBody(icon, w, h, wh);
    *bp = (int)wh[0];
    *bh = (int)wh[1];
    return body;
}
