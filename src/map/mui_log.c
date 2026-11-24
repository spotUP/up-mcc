/* UP- MUI class package: aa_log, the only stderr writer of the package (design section 4, decision D5). */
#include "mui_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__amigaos__) || defined(AMIGA)
#include <proto/dos.h>
#endif

/* -1 not read yet, 0 off, 1 on. Read once on the first call; the answer is cached for the life of the process. */
static int g_debug = -1;

static int debug_enabled(void) {
    if (g_debug < 0) {
#if defined(__amigaos__) || defined(AMIGA)
        /* dos.library GetVar reads ENV:UP-MCC/Debug. A missing variable just returns -1; no requester opens. */
        char buf[8];
        g_debug = GetVar((STRPTR) "UP-MCC/Debug", buf, (LONG)sizeof buf, 0) >= 0 ? 1 : 0;
#else
        /* A slash is not legal in a POSIX variable name, so the host spells the same switch UP_MCC_DEBUG. */
        const char* v = getenv("UP_MCC_DEBUG");
        g_debug = v != NULL && v[0] != '\0' ? 1 : 0;
#endif
    }
    return g_debug;
}

/* Null means stderr. The test sets a sink to read the line back; m68k libc cannot freopen stderr onto a file. */
static void (*g_sink)(const char*);
void aa_log_set_sink(void (*fn)(const char* line)) {
    g_sink = fn;
}

void aa_log(const char* fmt, ...) {
    va_list ap;
    if (!debug_enabled()) return;
    va_start(ap, fmt);
    if (g_sink != NULL) {
        char line[512];
        vsnprintf(line, sizeof line, fmt, ap);
        g_sink(line);
    } else {
        vfprintf(stderr, fmt, ap);
    }
    va_end(ap);
}

/* I625a6: trace sink set by the host; the package never writes it anywhere itself. */
static void (*g_trace_fn)(const char*);
void aa_trace_set(void (*fn)(const char* phase)) {
    g_trace_fn = fn;
}
void aa_trace(const char* phase) {
    if (g_trace_fn != NULL) {
        g_trace_fn(phase);
    }
}
