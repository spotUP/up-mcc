/* up-mcc.library API (design 2026-11-06-mui-mcc-package.md section 8.4). One source, lib/support_lib.c, compiled into the static
 * build (Selecta plugin, host tests) and into up-mcc.library; the vectors follow dist/sfd/up-mcc_lib.sfd. Included from up.h. */
#ifndef MCC_UP_LIB_H
#define MCC_UP_LIB_H

/* UP_ThemeLoad(path): validate the theme file at path (empty or NULL: ENV:UP-MCC/theme) and apply it to every UP- object.
 * Returns 0, or -1 with the tokens untouched when the file is missing or malformed. */
LONG UP_ThemeLoad(STRPTR path);
/* UP_ThemeReload(): resolve the theme again (built-in, ENV file, last accepted path) and apply it. Returns 0. */
LONG UP_ThemeReload(void);
/* UP_ThemeSetTokens(rgb, count): store count 0xRRGGBB tokens (UPTOK_* order) over the front of the token array, then recolour
 * every UP- object. Extra entries are ignored. Returns 0, or -1 for a NULL array or zero count. */
LONG UP_ThemeSetTokens(const ULONG* rgb, ULONG count);
/* UP_ThemeTokens(): the token array in force, UPTOK_END_ entries; valid until the next set or reload. */
const ULONG* UP_ThemeTokens(void);
/* UP_ThemeAttach(key, obj, app, hook) (I685n): register an object for recolours from any process. key identifies the entry (its draw
 * context), app is the object's MUI application, hook a struct Hook* whose h_Entry runs on the application's task with the key as the
 * message's first longword. Returns 0, or -1. UP_ThemeDetach(key) removes it (0, or -1 when unknown); UP_ThemeObjects() counts entries. */
LONG UP_ThemeAttach(void* key, void* obj, void* app, void* hook);
LONG UP_ThemeDetach(void* key);
ULONG UP_ThemeObjects(void);
/* UP_IconFind(name): index of the baked Lucide icon ("play"), usable with UPA_Button_Icon; -1 when unknown. */
LONG UP_IconFind(STRPTR name);
/* UP_SetFontFile(path): append a TrueType file to the font search list (up to 4, duplicates ignored). Returns 0, or -1. */
LONG UP_SetFontFile(STRPTR path);

/* I1491a: the calls the class libraries make instead of linking stdio, the theme parser and the icon data (about 51 KB base each).
 * UP_Log(fmt, args): aa_log through the library's one stderr writer; args is the caller's va_list (a stack pointer on m68k).
 * UP_FontFiles(): the NULL terminated TrueType search list (const char* const*). UP_IconBody(icon, w, h, bwh): the baked body of icon
 * at w x h with the fallbacks of the button (exact w x h, then size w, then the largest set); bwh[0], bwh[1] get the body's width
 * and height; NULL when the icon has no body. */
void UP_Log(STRPTR fmt, void* args);
void* UP_FontFiles(void);
void* UP_IconBody(LONG icon, LONG w, LONG h, LONG* bwh);

#endif
