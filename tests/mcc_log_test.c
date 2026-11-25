/* vendored from Selecta tests/ui/mcc_log_test.c at 679ff5fc; Selecta will consume this repo, not the reverse */
/* I685e2: the log gate of the MUI class package (design 2026-11-06-mui-mcc-package.md section 4, D5).
 * gate-off:  UP_MCC_DEBUG unset -> aa_log writes nothing.
 * gate-on:   UP_MCC_DEBUG=1 (set before the first call; the gate is read once) -> the line appears with the "up-mcc: " prefix.
 * source:    no fprintf(stderr outside mui_log.c in the package, no "selecta text inside an aa_log call. */
#include "map/mui_log.h"
#include <mcc/up.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__amigaos__) || defined(AMIGA)
#include <proto/dos.h>
#include <dos/var.h>
#endif

static int g_fail;
#define CHECK(c, ...) do { if (!(c)) { g_fail = 1; fprintf(stdout, "FAIL %s:%d: ", __FILE__, __LINE__); fprintf(stdout, __VA_ARGS__); fputc('\n', stdout); } } while (0)

static char g_got[256];
static void sink(const char* line) {
    strncat(g_got, line, sizeof g_got - strlen(g_got) - 1);
}

/* The Amiga gate reads ENV:UP-MCC/Debug (GetVar), the host reads UP_MCC_DEBUG. Set before the first aa_log: the gate is read once. */
static void set_gate(int on) {
#if defined(__amigaos__) || defined(AMIGA)
    if (on) SetVar((STRPTR) "UP-MCC/Debug", (STRPTR) "1", 1, GVF_LOCAL_ONLY);
    else DeleteVar((STRPTR) "UP-MCC/Debug", GVF_LOCAL_ONLY);
#else
    if (on) setenv("UP_MCC_DEBUG", "1", 1); else unsetenv("UP_MCC_DEBUG");
#endif
}

/* path is kept for the CLI (ctest passes one) but unused: aa_log is read back through the sink, not a redirected stderr. */
static void test_gate(int on, const char* path) {
    (void)path;
    set_gate(on);
    aa_log_set_sink(sink);
    aa_log("up-mcc: probe %d\n", 42);
    if (on) CHECK(strcmp(g_got, "up-mcc: probe 42\n") == 0, "gate on: got '%s'", g_got);
    else CHECK(g_got[0] == '\0', "gate off: wrote '%s'", g_got);
}

/* U3: the hook gets the formatted line with the debug gate off; removing it stops delivery. */
static char g_hooked[256];
static unsigned long hook_fn(struct Hook* h, void* obj, const char* line) {
    (void)h, (void)obj;
    snprintf(g_hooked, sizeof g_hooked, "%s", line);
    return 0;
}
static void test_hook(void) {
    struct Hook h = {(void*)hook_fn, NULL, NULL};
    unsetenv("UP_MCC_DEBUG");
    aa_log_hook_set(&h);
    aa_log("up-mcc: hooked %d\n", 7);
    CHECK(strcmp(g_hooked, "up-mcc: hooked 7\n") == 0, "hook got '%s'", g_hooked);
    g_hooked[0] = '\0';
    aa_log_hook_set(NULL);
    aa_log("up-mcc: after %d\n", 8);
    CHECK(g_hooked[0] == '\0', "hook called after removal");
}

static int read_file(const char* path, char* buf, size_t cap) {
    FILE* f = fopen(path, "rb");
    size_t n;
    if (f == NULL) return 0;
    n = fread(buf, 1, cap - 1, f);
    buf[n] = '\0';
    fclose(f);
    return 1;
}

static void test_source(void) {
    static const char* const files[] = {"/src/mui_aa_draw.c", "/src/mui_aatip.c", "/src/map/mui_theme.c", "/src/class/mui_aafield_class.c", "/src/map/mui_log.c"};
    static char buf[300000];
    size_t i;
    int routed = 0;
    for (i = 0; i < sizeof files / sizeof files[0]; i++) {
        char path[1024];
        const char* p;
        snprintf(path, sizeof path, "%s%s", MCC_DIR, files[i]);
        CHECK(read_file(path, buf, sizeof buf), "cannot read %s", path);
        if (strstr(files[i], "mui_log.c") == NULL) CHECK(strstr(buf, "fprintf(stderr") == NULL, "%s still writes stderr directly", files[i]);
        if (strstr(files[i], "mui_log.c") != NULL) continue; /* the definition, not a call */
        for (p = buf; (p = strstr(p, "aa_log(")) != NULL; p++) {
            const char* e = strchr(p, ';');
            char call[1024];
            size_t len = e != NULL ? (size_t)(e - p) : 0;
            if (len == 0 || len >= sizeof call) continue;
            memcpy(call, p, len);
            call[len] = '\0';
            routed++;
            CHECK(strstr(call, "\"selecta") == NULL && strstr(call, "up-mcc: ") != NULL, "%s: aa_log text not prefixed up-mcc: %s", files[i], call);
        }
    }
    CHECK(routed >= 4, "only %d aa_log calls found in the four sites", routed);
}

int main(int argc, char** argv) {
    const char* t = argc > 1 ? argv[1] : "";
    const char* path = argc > 2 ? argv[2] : "mcc_log_capture.txt";
    if (!strcmp(t, "gate-off")) test_gate(0, path);
    else if (!strcmp(t, "gate-on")) test_gate(1, path);
    else if (!strcmp(t, "hook")) test_hook();
    else if (!strcmp(t, "source")) test_source();
    else { fprintf(stdout, "unknown test '%s'\n", t); return 2; }
    if (g_fail) return 1;
    fprintf(stdout, "ok %s\n", t);
    return 0;
}
