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

/* U3 (v1.1): UP_SetLogHook(hook) sends every library and class log line to hook (a struct Hook*; message = the formatted
 * NUL terminated line, up to 255 bytes, object NULL), whether or not ENV:UP-MCC/Debug is set; NULL removes it. One hook per process. */
void UP_SetLogHook(APTR hook); /* APTR to match the generated SFD prototype; pass a struct Hook* */
void* UP_IconBody(LONG icon, LONG w, LONG h, LONG* bwh);

/* I1491a2 (library version 2): the draw layer. The class libraries keep their aa_ctx and aatip_slot in the instance data and hand the
 * pointer in; the code behind them (src/mui_aa_draw.c, mui_aatip.c, the paint shapes) is one copy in this library. aa_ctx and
 * aatip_slot layouts are frozen (ABI 2, append only): UP_DrawSetup(ctx, obj, rgb, nsize) takes nsize = n | sizeof(aatip_slot) << 8 |
 * sizeof(aa_ctx) << 16 and returns 0 without touching ctx on a mismatch, then every other call ignores that ctx (drawing nothing).
 * Returns 1 on success. Every call takes the context and RastPort of the calling class and runs on the calling task. */
#define UP_DRAW_NSIZE(n, ctx_size, slot_size) ((ULONG)(n) | ((ULONG)(slot_size) << 8) | ((ULONG)(ctx_size) << 16))
/* UP_DrawShape(ctx, rp, obj, shape): one entry for every fill; shape->kind selects the aa_* function (enum below), the fields are its
 * arguments. UP_SHAPE_RECT: x, y = (x0, y0) and w, h = (x1, y1), the inclusive corner. UP_SHAPE_ROUND_FILL_HOLE: p0 = int[4]
 * {hx, hy, hw, hh}. UP_SHAPE_ROUND_FILL_HOLES: p0 = const int* holes, n = count. UP_SHAPE_RUNS: x, y, p0 = slot bytes, p1 = slot
 * rgb, n = count. UP_SHAPE_ERASE uses obj. */
enum {
    UP_SHAPE_RECT = 0,
    UP_SHAPE_ROUND_FILL,
    UP_SHAPE_ROUND_FRAME,
    UP_SHAPE_ROUND_CORNERS,
    UP_SHAPE_ROUND_FILL_HOLE,
    UP_SHAPE_ROUND_FILL_HOLES,
    UP_SHAPE_LOW_OUTLINE,
    UP_SHAPE_RUNS,
    UP_SHAPE_ERASE,
    UP_SHAPE_COUNT
};
struct up_shape {
    UWORD kind;
    WORD x, y, w, h, radius;
    ULONG rgb;
    const void* p0;
    const void* p1;
    WORD n;
};
struct up_text {
    WORD x, y;
    STRPTR str;
    ULONG fg, bg;
};
/* UP_DrawFont flags: 0 = aa_set_font_px(px); UP_FONT_PLAIN (+ UP_FONT_BOLD) = aa_plain_font(px, bold). */
#define UP_FONT_PLAIN 1
#define UP_FONT_BOLD 2
LONG UP_DrawSetup(void* ctx, void* obj, const ULONG* rgb, ULONG nsize);
void UP_DrawCleanup(void* ctx);
void UP_DrawFont(void* ctx, void* obj, LONG px, LONG flags);
/* hold 1: aa_hold (returns 0); hold 0: aa_nearest_pen. */
LONG UP_DrawPen(void* ctx, ULONG rgb, LONG hold);
void UP_DrawShape(void* ctx, void* rp, void* obj, const void* shape);
/* str NULL: aa_text_height; else aa_text_width. */
LONG UP_DrawTextSize(void* ctx, void* obj, STRPTR str);
void UP_DrawText(void* ctx, void* obj, void* rp, const void* text);
LONG UP_DrawLive(void* obj);
ULONG UP_TipFire(void* obj, void* ctx, void* slot);
ULONG UP_TipSuperSet(void* cl, void* obj, void* msg, ULONG slotoff);
LONG UP_TipGet(void* msg, void* slot);
void UP_TipEvent(void* obj, void* slot, void* imsg, LONG in);
void UP_TipDrop(void* obj);

#endif
