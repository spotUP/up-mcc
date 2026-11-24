/* Library bases that sel_mcc_static's mui_aa_draw.c references (the graphics base under its package name, and MUIMasterBase).
 * In shipped code lib/classheader.c and lib/support_libheader.c define them and open the libraries at init; a test executable
 * has no library init, so it defines them here (left NULL: the tests never draw). */
struct Library;
struct Library* sel_mui_GfxBase;
struct Library* MUIMasterBase;
