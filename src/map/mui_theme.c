/* I1491a: the loader half of the theme code: file reading, parsing, the font list. Linked into up-mcc.library and the host build only;
 * the class libraries reach it through the library API. The pure half (look_for, readable, pick_pen, counters) is mui_theme_pure.c. */
#include "mui_theme.h"
#include "mui_log.h"

#include <selecta/uimodel/tokens.h>

#include <stdio.h>
#include <string.h>

/* I685t: the file reader. Default stdio; up-mcc.library installs a dos.library reader (mui_theme_set_reader) because libnix stdio
 * must not run where the library is opened. Returns the bytes read, -1 when the file cannot be opened. */
static long stdio_read(const char* path, char* buf, size_t cap) {
    FILE* f = fopen(path, "rb");
    size_t n;
    if (f == NULL) {
        return -1;
    }
    n = fread(buf, 1, cap, f);
    fclose(f);
    return (long)n;
}
static mui_theme_reader g_reader = stdio_read;

void mui_theme_set_reader(mui_theme_reader rd) {
    g_reader = rd != NULL ? rd : stdio_read;
}

/* Parses one theme file over *t; *t is untouched unless the whole file is readable and valid. */
static int theme_apply_file(txt_theme* t, const char* path) {
    char buf[4096];
    long n;
    int bad = 0;
    txt_theme work = *t;
    if (path == NULL || path[0] == '\0' || (n = g_reader(path, buf, sizeof buf - 1)) < 0) {
        return 0;
    }
    buf[n] = '\0';
    if (txt_theme_parse(&work, buf, &bad) < 0) {
        aa_log("up-mcc: theme '%s' line %d is not a role or token line in range, ignored\n", path, bad);
        return 0;
    }
    *t = work;
    return 1;
}

int mui_theme_load(txt_theme* t, const char* env_path, const char* arg_path) {
    if (t == NULL) {
        return 0;
    }
    return theme_apply_file(t, env_path) + theme_apply_file(t, arg_path);
}

/* I685e2: the TrueType search list. Order: paths the host added (mui_font_file_add), the theme key font.file, the default. */
#define FONT_ADD_MAX 4
static char g_font_add[FONT_ADD_MAX][128];
static int g_font_add_n;
static char g_font_theme[128];
static const char* g_font_list[FONT_ADD_MAX + 3];

int mui_font_file_add(const char* path) {
    int i;
    if (path == NULL || path[0] == '\0' || strlen(path) >= sizeof g_font_add[0]) {
        return -1;
    }
    for (i = 0; i < g_font_add_n; i++) {
        if (strcmp(g_font_add[i], path) == 0) {
            return 0;
        }
    }
    if (g_font_add_n >= FONT_ADD_MAX) {
        return -1;
    }
    snprintf(g_font_add[g_font_add_n++], sizeof g_font_add[0], "%s", path);
    return 0;
}

static int font_listed(int n, const char* p) {
    int i;
    for (i = 0; i < n; i++) {
        if (strcmp(g_font_list[i], p) == 0) {
            return 1;
        }
    }
    return 0;
}

const char* const* mui_font_files(void) {
    int n = 0, i;
    for (i = 0; i < g_font_add_n; i++) {
        g_font_list[n++] = g_font_add[i];
    }
    if (g_font_theme[0] != '\0' && !font_listed(n, g_font_theme)) {
        g_font_list[n++] = g_font_theme;
    }
    if (!font_listed(n, MUI_FONT_DEFAULT)) {
        g_font_list[n++] = MUI_FONT_DEFAULT;
    }
    g_font_list[n] = NULL;
    return g_font_list;
}

/* I676b: the theme the next window open reads. A THEME request that validated stores its file here; mui_theme_resolve is the one
 * place that turns (built-in, ENV file, launch --theme, request) into tokens, for the window open and for the tests. */
static char g_request_path[256];
static uint32_t g_requests;

void mui_theme_resolve(txt_theme* t, const char* env_path, const char* launch_arg) {
    txt_theme_default(t);
    mui_theme_load(t, env_path, g_request_path[0] != '\0' ? g_request_path : launch_arg);
    snprintf(g_font_theme, sizeof g_font_theme, "%s", t->font_file);
}

int mui_theme_request(const char* env_path, const char* path) {
    txt_theme probe;
    const char* file = path != NULL && path[0] != '\0' ? path : env_path;
    if (file == NULL || file[0] == '\0' || (path != NULL && strlen(path) >= sizeof g_request_path)) {
        return -1;
    }
    txt_theme_default(&probe);
    if (mui_theme_load(&probe, NULL, file) != 1) { /* missing, unreadable or malformed: the whole request is refused */
        return -1;
    }
    snprintf(g_request_path, sizeof g_request_path, "%s", path != NULL && path[0] != '\0' ? path : "");
    g_requests++;
    return 0;
}

uint32_t mui_theme_requests(void) {
    return g_requests;
}

/* I685n: the token store (mui_theme_tokens, mui_theme_tokens_set) lives in up_store.c; the class libraries get theirs from up-mcc.library. */

static uint32_t g_live_applied;

int mui_theme_apply_live(const mui_theme_live_ops* ops, const char* env_path, const char* launch_arg) {
    txt_theme th;
    if (ops == NULL || ops->palette == NULL || ops->classes == NULL) {
        return -1;
    }
    mui_theme_resolve(&th, env_path, launch_arg);
    mui_theme_tokens_set(th.token);
    ops->palette(ops->ctx, mui_theme_tokens());
    ops->classes(ops->ctx);
    g_live_applied++;
    return 0;
}

uint32_t mui_theme_live_applied(void) {
    return g_live_applied;
}
