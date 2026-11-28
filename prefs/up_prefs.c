/* UP-MCC.mcp prefs class (I685j, design 2026-11-06-mui-mcc-package.md section 5). Subclass of Mccprefs.mui: the shared page for
 * the package, not one per class, because the settings are the theme tokens. The page shows the theme file, four colours, the corner
 * radius and a reset button. Configuration to gadgets reads the theme in force (built-in, then ENV:UP-MCC/theme); gadgets to
 * configuration writes ENV:UP-MCC/theme and ENVARC:UP-MCC/theme through txt_theme_write (the one writer, beside the one parser) and
 * calls UP_ThemeReload. The gadget/token mapping is src/map/up_prefs_map.c, host-tested (ctest mcc-prefs-*).
 * MUI calls GadgetsToConfig for both Use and Save and the page cannot tell them apart, so both files are written every time.
 * When the theme written is a shipped one (Dark, Light), its OpenMUI twin replaces OpenMUI's global.prefs too (openmui_follow). */
#if defined(__amigaos__) || defined(AMIGA)

#include "mui_aa_class.h"

#include <mcc/up.h>

#include <selecta/uimodel/theme.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <libraries/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "map/mui_theme.h"
#include "map/up_prefs_map.h"
#ifdef UP_CLASS_LIBRARY
/* I685n: Save recolours through up-mcc.library (opened by lib/classheader.c), not through a copy of the store in this library. */
#include <proto/up_mcc.h>
extern struct Library* UPMCCBase;
#endif

#define UP_CLS_PREFS 0x0A
#define UPM_Prefs_Load UP_TAG(UP_CLS_PREFS, 0x01)  /* the theme file string changed: read it into the colour gadgets */
#define UPM_Prefs_Reset UP_TAG(UP_CLS_PREFS, 0x02) /* "Reset to defaults" */

#define PREFS_ENV_DIR "ENV:UP-MCC"
#define PREFS_ENV_FILE "ENV:UP-MCC/theme"
#define PREFS_ENVARC_DIR "ENVARC:UP-MCC"
#define PREFS_ENVARC_FILE "ENVARC:UP-MCC/theme"
/* OpenMUI follows the theme: the installer keeps the OpenMUI twin of each shipped theme (Themes/OpenMUI/<name>.prefs, made by
 * makerelease theme2prefs.py) in PREFS_OPENMUI_SRC; the page copies the twin of the theme it writes over OpenMUI's global.prefs. */
#define PREFS_THEME_DIR "ENVARC:UP-MCC/"
#define PREFS_OPENMUI_SRC "ENVARC:UP-MCC/OpenMUI/"
#define OPENMUI_ENV_DIR "ENV:OpenMUI"
#define OPENMUI_ENV_FILE "ENV:OpenMUI/global.prefs"
#define OPENMUI_ENVARC_DIR "ENVARC:OpenMUI"
#define OPENMUI_ENVARC_FILE "ENVARC:OpenMUI/global.prefs"
#define OPENMUI_ENVARC_BAK "ENVARC:OpenMUI/global.prefs.before-up"

typedef struct prefs_data {
    Object *file, *accent, *surface, *text, *background, *radius;
    txt_theme th; /* the full token set the page edits; the five gadgets overlay it */
} prefs_data;

static struct MUI_CustomClass* g_class;

extern ULONG up_prefs_dispatch(void);
const struct aa_class_desc up_prefs_class_desc = {MUIC_Mccprefs, sizeof(prefs_data), up_prefs_dispatch, &g_class};
static ULONG __attribute__((used)) up_prefs_dispatch_c(struct IClass* cl, Object* obj, Msg msg);
__asm__(".text\n.even\n.globl _up_prefs_dispatch\n_up_prefs_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _up_prefs_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

/* Explicit MUI_NewObject calls: the proto header makes MUI_NewObject a function-like macro, so the StringObject ... End shortcuts
 * of mui.h (which close the argument list with a macro) do not expand correctly here. */
static Object* hex_string(void) {
    return MUI_NewObject((char*)MUIC_String, MUIA_Frame, (ULONG)MUIV_Frame_String, MUIA_String_MaxLen, 10UL, MUIA_String_Accept,
                         (ULONG) "0123456789abcdefABCDEFxX#", TAG_DONE);
}

static Object* label(const char* text) {
    return (Object*)MUI_MakeObject(MUIO_Label, (ULONG)text, (ULONG)MUIO_Label_DoubleFrame);
}

static long prefs_theme_read(const char* path, char* buf, size_t cap);

static ULONG prefs_new(struct IClass* cl, Object* obj, struct opSet* msg) {
    Object *file, *pop, *accent, *surface, *text, *background, *radius, *reset, *page;
    struct TagItem tags[3];
    prefs_data* d;
    mui_theme_set_reader(prefs_theme_read);
    file = MUI_NewObject((char*)MUIC_String, MUIA_Frame, (ULONG)MUIV_Frame_String, MUIA_String_MaxLen, 255UL, TAG_DONE);
    pop = MUI_NewObject((char*)MUIC_Popasl, MUIA_Popasl_Type, (ULONG)ASL_FileRequest, MUIA_Popstring_String, (ULONG)file,
                        MUIA_Popstring_Button, (ULONG)MUI_MakeObject(MUIO_PopButton, (ULONG)MUII_PopFile), TAG_DONE);
    accent = hex_string();
    surface = hex_string();
    text = hex_string();
    background = hex_string();
    radius = MUI_NewObject((char*)MUIC_Slider, MUIA_Numeric_Min, 0UL, MUIA_Numeric_Max, 32UL, MUIA_Numeric_Value, 6UL, TAG_DONE);
    reset = (Object*)MUI_MakeObject(MUIO_Button, (ULONG) "Reset to defaults");
    page = MUI_NewObject((char*)MUIC_Group, MUIA_Group_Columns, 2UL, MUIA_Group_Child, (ULONG)label("Theme file"), MUIA_Group_Child,
                         (ULONG)pop, MUIA_Group_Child, (ULONG)label("Accent colour"), MUIA_Group_Child, (ULONG)accent, MUIA_Group_Child,
                         (ULONG)label("Surface colour"), MUIA_Group_Child, (ULONG)surface, MUIA_Group_Child, (ULONG)label("Text colour"),
                         MUIA_Group_Child, (ULONG)text, MUIA_Group_Child, (ULONG)label("Background colour"), MUIA_Group_Child,
                         (ULONG)background, MUIA_Group_Child, (ULONG)label("Corner radius"), MUIA_Group_Child, (ULONG)radius,
                         MUIA_Group_Child, (ULONG)MUI_NewObject((char*)MUIC_Rectangle, TAG_DONE), MUIA_Group_Child, (ULONG)reset, TAG_DONE);
    if (page == NULL) {
        return 0;
    }
    tags[0].ti_Tag = MUIA_Group_Child;
    tags[0].ti_Data = (ULONG)page;
    tags[1].ti_Tag = TAG_MORE;
    tags[1].ti_Data = (ULONG)msg->ops_AttrList;
    tags[2].ti_Tag = TAG_DONE;
    msg->ops_AttrList = tags;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)msg);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->file = file;
    d->accent = accent;
    d->surface = surface;
    d->text = text;
    d->background = background;
    d->radius = radius;
    txt_theme_default(&d->th);
    DoMethod(file, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, obj, 1, UPM_Prefs_Load);
    DoMethod(reset, MUIM_Notify, MUIA_Pressed, FALSE, obj, 1, UPM_Prefs_Reset);
    return (ULONG)obj;
}

static void tokens_to_page(prefs_data* d) {
    up_prefs_gadgets g;
    up_prefs_tokens_to_gadgets(d->th.token, &g);
    set(d->accent, MUIA_String_Contents, (ULONG)g.accent);
    set(d->surface, MUIA_String_Contents, (ULONG)g.surface);
    set(d->text, MUIA_String_Contents, (ULONG)g.text);
    set(d->background, MUIA_String_Contents, (ULONG)g.background);
    set(d->radius, MUIA_Numeric_Value, (ULONG)g.radius);
}

static void copy_string(Object* o, char* dst, size_t cap) {
    ULONG v = 0;
    get(o, MUIA_String_Contents, &v);
    dst[0] = '\0';
    if (v != 0) {
        strncpy(dst, (const char*)v, cap - 1);
        dst[cap - 1] = '\0';
    }
}

/* Returns 0, or -1 when a colour string is not valid (the tokens stay). */
static int page_to_tokens(prefs_data* d, uint32_t* tok) {
    up_prefs_gadgets g;
    ULONG r = 0;
    copy_string(d->accent, g.accent, sizeof g.accent);
    copy_string(d->surface, g.surface, sizeof g.surface);
    copy_string(d->text, g.text, sizeof g.text);
    copy_string(d->background, g.background, sizeof g.background);
    get(d->radius, MUIA_Numeric_Value, &r);
    g.radius = (int)r;
    return up_prefs_gadgets_to_tokens(&g, tok);
}

static ULONG prefs_config_to_gadgets(struct IClass* cl, Object* obj) {
    prefs_data* d = INST_DATA(cl, obj);
    mui_theme_resolve(&d->th, PREFS_ENV_FILE, NULL);
    tokens_to_page(d);
    return 0;
}

static void write_theme_file(const char* dir, const char* path, const char* text, size_t len) {
    BPTR lock = Lock((CONST_STRPTR)dir, ACCESS_READ);
    BPTR fh;
    if (lock != 0) {
        UnLock(lock);
    } else {
        lock = CreateDir((CONST_STRPTR)dir);
        if (lock != 0) {
            UnLock(lock);
        }
    }
    fh = Open((CONST_STRPTR)path, MODE_NEWFILE);
    if (fh != 0) {
        (void)Write(fh, (APTR)text, (LONG)len);
        Close(fh);
    }
}

/* Reads the whole file into buf. Returns its length, or -1 when it is missing, unreadable or does not fit. */
static LONG read_small_file(const char* path, char* buf, LONG cap) {
    BPTR fh = Open((CONST_STRPTR)path, MODE_OLDFILE);
    LONG n;
    if (fh == 0) {
        return -1;
    }
    n = Read(fh, (APTR)buf, cap);
    Close(fh);
    return (n < 0 || n >= cap) ? -1 : n;
}
/* The theme reader for mui_theme_load/resolve in this library: its default stdio reader (fopen) mallocs through libnix's __memsema,
 * which a class library never sets up (no constructors run), and ObtainSemaphore on it hung the prefs page in MUI 5 and OpenMUI. */
static long prefs_theme_read(const char* path, char* buf, size_t cap) {
    return read_small_file(path, buf, (LONG)cap);
}

/* The theme name of a theme file path: its base name without ".theme" (any case), into dst. 0 when the path names no .theme file. */
static int theme_name_of(const char* path, char* dst, size_t cap) {
    const char* base = path;
    const char* p;
    size_t n, i;
    for (p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == ':') {
            base = p + 1;
        }
    }
    n = strlen(base);
    if (n <= 6 || n - 6 >= cap) {
        return 0;
    }
    for (i = 0; i < 6; ++i) {
        if ((base[n - 6 + i] | 0x20) != ".theme"[i]) {
            return 0;
        }
    }
    memcpy(dst, base, n - 6);
    dst[n - 6] = '\0';
    return 1;
}

/* 1 when the installed theme file name (PREFS_THEME_DIR<name>.theme) resolves to exactly the tokens tok. */
static int theme_is(const char* name, const uint32_t* tok) {
    char path[96];
    txt_theme t;
    if (strlen(name) > sizeof path - sizeof PREFS_THEME_DIR - 7) {
        return 0;
    }
    strcpy(path, PREFS_THEME_DIR);
    strcat(path, name);
    strcat(path, ".theme");
    txt_theme_default(&t);
    return mui_theme_load(&t, NULL, path) == 1 && memcmp(t.token, tok, sizeof t.token) == 0;
}

/* OpenMUI follows the theme just written (tokens tok). The theme is named by the file the page loaded (the Theme file gadget) when its
 * tokens are unchanged, else by the shipped theme whose tokens it equals; a custom theme names none and OpenMUI is left alone. The
 * named theme's OpenMUI twin replaces ENV: and ENVARC:OpenMUI/global.prefs (both, as for the theme: MUI sends Use and Save through
 * GadgetsToConfig alike); a user's own ENVARC global.prefs is first kept once as global.prefs.before-up, as Install-UP-MCC does. Only
 * global.prefs is written: palette.prefs and the Workbench colours are never touched. buf is scratch of cap bytes. */
static void openmui_follow(prefs_data* d, const uint32_t* tok, char* buf, LONG cap) {
    static const char* const shipped[] = {"Dark", "Light"};
    char path[256];
    char name[32];
    char src[96];
    LONG n;
    size_t i;
    int found = 0;
    copy_string(d->file, path, sizeof path);
    if (theme_name_of(path, name, sizeof name) && theme_is(name, tok)) {
        found = 1;
    }
    for (i = 0; !found && i < sizeof shipped / sizeof shipped[0]; ++i) {
        if (theme_is(shipped[i], tok)) {
            strcpy(name, shipped[i]);
            found = 1;
        }
    }
    if (!found || strlen(name) > sizeof src - sizeof PREFS_OPENMUI_SRC - 7) {
        return;
    }
    strcpy(src, PREFS_OPENMUI_SRC);
    strcat(src, name);
    strcat(src, ".prefs");
    if (read_small_file(src, buf, cap) < 0) {
        return; /* no OpenMUI twin for this theme */
    }
    n = read_small_file(OPENMUI_ENVARC_BAK, buf, cap);
    if (n < 0) {
        n = read_small_file(OPENMUI_ENVARC_FILE, buf, cap);
        if (n >= 0) {
            write_theme_file(OPENMUI_ENVARC_DIR, OPENMUI_ENVARC_BAK, buf, (size_t)n);
        }
    }
    n = read_small_file(src, buf, cap);
    if (n < 0) {
        return;
    }
    write_theme_file(OPENMUI_ENV_DIR, OPENMUI_ENV_FILE, buf, (size_t)n);
    write_theme_file(OPENMUI_ENVARC_DIR, OPENMUI_ENVARC_FILE, buf, (size_t)n);
}

static ULONG prefs_gadgets_to_config(struct IClass* cl, Object* obj) {
    prefs_data* d = INST_DATA(cl, obj);
    txt_theme out = d->th;
    char text[4096];
    int n;
    if (page_to_tokens(d, out.token) != 0) {
        return 0; /* a malformed colour keeps the files as they were */
    }
    n = txt_theme_write(&out, text, sizeof text);
    if (n < 0) {
        return 0;
    }
    write_theme_file(PREFS_ENV_DIR, PREFS_ENV_FILE, text, (size_t)n);
    write_theme_file(PREFS_ENVARC_DIR, PREFS_ENVARC_FILE, text, (size_t)n);
    d->th = out;
    openmui_follow(d, out.token, text, (LONG)sizeof text);
    /* The library's store, shared by every process: UP_ThemeReload reads the file just written and recolours all registered objects. */
    (void)UP_ThemeReload();
    return 0;
}

static ULONG prefs_load(struct IClass* cl, Object* obj) {
    prefs_data* d = INST_DATA(cl, obj);
    char path[256];
    txt_theme t;
    copy_string(d->file, path, sizeof path);
    if (path[0] == '\0') {
        return 0;
    }
    txt_theme_default(&t);
    if (mui_theme_load(&t, NULL, path) == 1) {
        d->th = t;
        tokens_to_page(d);
    }
    return 0;
}

static ULONG prefs_reset(struct IClass* cl, Object* obj) {
    prefs_data* d = INST_DATA(cl, obj);
    txt_theme_default(&d->th);
    tokens_to_page(d);
    return 0;
}

static ULONG __attribute__((used)) up_prefs_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    switch (msg->MethodID) {
    case OM_NEW:
        return prefs_new(cl, obj, (struct opSet*)msg);
    case MUIM_Settingsgroup_ConfigToGadgets:
        return prefs_config_to_gadgets(cl, obj);
    case MUIM_Settingsgroup_GadgetsToConfig:
        return prefs_gadgets_to_config(cl, obj);
    case UPM_Prefs_Load:
        return prefs_load(cl, obj);
    case UPM_Prefs_Reset:
        return prefs_reset(cl, obj);
    default:
        break;
    }
    return DoSuperMethodA(cl, obj, msg);
}

#else
typedef int up_prefs_host_unit; /* the page class is Amiga only; the pure mapping is src/map/up_prefs_map.c */
#endif
