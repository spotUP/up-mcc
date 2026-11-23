/* Library bases that the Amiga build of sel_mcc_static references and the host program normally defines: src/mui_gfxbase.h
 * renames GfxBase to sel_mui_GfxBase (I719), mui_aa_draw.c calls MUI_ObtainPen / MUI_ReleasePen. Selecta defines them in
 * plugins/ui/mui/mui_ui.c, up-mcc.library in lib/support_libheader.c; a test that links the static package is its own host
 * program and supplies them here. Never opened: the tests do not draw. */
struct Library;
struct Library* sel_mui_GfxBase;
struct Library* MUIMasterBase;
