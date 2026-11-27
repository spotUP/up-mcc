/* File trace for the UP-Menustrip right-button hunt (branch fix-menustrip-rmb-reopen). One macro: set UPMS_TRACE to 0 and every
 * line compiles away. Lines are appended to RAM:upms.log, prefix "UPMS:" (the serial port of the FS-UAE rig did not deliver
 * RawPutChar output). Process context only: it calls dos.library. Arguments are ULONG; use %ld and %lx. Every call passes at
 * least one argument. */
#ifndef MUI_AA_TRACE_H
#define MUI_AA_TRACE_H

#ifndef UPMS_TRACE
#define UPMS_TRACE 0
#endif

#if UPMS_TRACE
#include <dos/dos.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
/* RawDoFmt PutChProc, d0 = character, a3 = buffer: move.b d0,(a3)+; rts */
static const UWORD upms_putch[] = {0x16C0, 0x4E75};
static void upms_write(const char* fmt, ULONG* args) {
    char buf[256]; /* shortcut: no bound check, trace lines are short constant formats */
    struct DosLibrary* DOSBase = (struct DosLibrary*)OpenLibrary((STRPTR) "dos.library", 36);
    BPTR f;
    int n = 0;
    RawDoFmt((STRPTR)fmt, (APTR)args, (void (*)(void))upms_putch, buf);
    while (buf[n] != 0) {
        n++;
    }
    if (DOSBase != NULL) {
        if ((f = Open((STRPTR) "RAM:upms.log", MODE_READWRITE)) != 0) {
            Seek(f, 0, OFFSET_END);
            Write(f, buf, n);
            Close(f);
        }
        CloseLibrary((struct Library*)DOSBase);
    }
}
#define UPMS(fmt, ...)                               \
    do {                                             \
        ULONG upms_a[] = {__VA_ARGS__};              \
        upms_write("UPMS: " fmt "\n", upms_a);       \
    } while (0)
#else
#define UPMS(fmt, ...) ((void)0)
#endif

#endif
