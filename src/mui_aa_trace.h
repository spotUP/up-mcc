/* Serial trace for the UP-Menustrip right-button hunt (branch fix-menustrip-rmb-reopen). One macro: set UPMS_TRACE to 0 and every
 * line compiles away. Lines go to the serial port through exec RawDoFmt and RawPutChar (LVO -516), prefix "UPMS:".
 * Arguments are ULONG; use %ld and %lx. Every call passes at least one argument. */
#ifndef MUI_AA_TRACE_H
#define MUI_AA_TRACE_H

#ifndef UPMS_TRACE
#define UPMS_TRACE 1
#endif

#if UPMS_TRACE
#include <exec/types.h>
#include <proto/exec.h>
/* RawDoFmt PutChProc, d0 = character: move.l a6,-(sp); movea.l 4.w,a6; jsr -516(a6); movea.l (sp)+,a6; rts */
static const UWORD upms_putch[] = {0x2F0E, 0x2C78, 0x0004, 0x4EAE, 0xFDFC, 0x2C5F, 0x4E75};
#define UPMS(fmt, ...)                                                                             \
    do {                                                                                           \
        ULONG upms_a[] = {__VA_ARGS__};                                                            \
        RawDoFmt((STRPTR) "UPMS: " fmt "\n", (APTR)upms_a, (void (*)(void))upms_putch, NULL);      \
    } while (0)
#else
#define UPMS(fmt, ...) ((void)0)
#endif

#endif
