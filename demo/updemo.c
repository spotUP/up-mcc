/* UP-Demo (I685k, design 2026-11-06-mui-mcc-package.md section 10 "Final gate", item 1; section 9 Demo).
 * A plain MUI 3.8 program built against the release headers only (include/mcc/up.h, up_lib.h, the SFD-generated headers): no Selecta
 * include path, no link against sel_mcc_static (ctest mcc-demo-headers-only). It opens each of the 11 shipped classes BY NAME,
 * MUI_NewObject("UP-<Name>.mcc"), which MUI loads from MUI:Libs/mui/, puts one instance of each into one window, and prints
 *   [OK] <class> from MUI:Libs/mui (origin 0x<hex>)       the class was created
 *   [ERROR] <class> ...                                   it was not (not installed, or no origin)
 * Exit code 0 when all 11 open, 20 otherwise. The origin is the sentinel UPA_Shared_Origin: the base of the .mcc library the object's
 * class came from. A NULL origin counts as an error, because it means the class code is not coming from a library.
 * Backend: the demo uses whatever the classes pick. Without ttengine they fall back to PLAIN text; the success criterion is the same
 * (gate item 3), so there is no switch here.
 * Gate item 2 (live theme reload from UP-MCC.mcp, row I685n): the token store and the retheme list live in up-mcc.library, so a
 * Save in the prefs recolours this window. Run: UP-Demo [QUIT | AUTO | HOLD]. No argument: the window stays open until the close
 * gadget. QUIT closes it at once, AUTO after about 3 s (scripted runs), HOLD is the default plus a [RECT] listing of the widget boxes.
 * I1349: the window covers the screen and the content is one of three tiers by screen height (256, 512, 700+ lines), so it opens under MUI 3.8
 * (which refuses a window whose minimum size exceeds the screen) without a Scrollgroup; the program asks for 48 KB of stack (MUI 5 warns below 32000). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <dos/dos.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include <mcc/up.h>

struct Library* MUIMasterBase;

#define DEMO_STACK_BYTES 49152UL /* I1343: MUI 5 warns below 32000; libnix does not honour a __stack cookie in this build */
/* I1352: every failure shows a requester that names the cause and the fix, then the demo exits with rc 20 (the [ERROR] line
 * stays for the rig). muimaster is open when a MUI_Request would work, but the demo fails exactly when MUI is the problem,
 * so the intuition EasyRequest serves every case. The base is renamed: libnix would open it at start and abort without it. */
#define IntuitionBase demo_IntuitionBase
struct IntuitionBase* IntuitionBase;

/* The base is opened once by demo_main and stays open for the whole run: demo_tier and the requester both call Intuition. */
static void demo_requester(const char* text) {
    struct EasyStruct es;
    if (IntuitionBase == NULL) {
        return;
    }
    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (UBYTE*)"UP-Demo";
    es.es_TextFormat = (UBYTE*)"%s";
    es.es_GadgetFormat = (UBYTE*)"Ok";
    EasyRequest(NULL, &es, NULL, (APTR)text);
}

#define CLASS_COUNT 44 /* the rows of MCC_CLASS_LIBS in CMakeLists.txt */

static const struct MUI_PenSpec g_pen_spec = {"r3e3e3e3e,63636363,dddddddd"};
static const char* const g_names[CLASS_COUNT] = {"Button", "Text", "Panel", "Tabs", "Balance", "Slider",
                                                 "Progress", "Cycle", "Field", "Scroller", "List", "Checkmark", "Radio", "Popstring", "Poplist",
                                                 "Numericbutton", "Scale", "Colorfield", "Palette", "Image", "Menu", "Virtgroup", "Scrollgroup",
                                                 "Knob", "Levelmeter",
                                                 "Popobject", "Popasl", "Popscreen",
                                                 "Pendisplay", "Poppen", "Penadjust",
                                                 "Imagedisplay", "Framedisplay", "Popimage", "Popframe", "Imageadjust", "Frameadjust",
                                                 "Dirlist", "Volumelist", "Scrmodelist", "Menustrip",
                                                 "Textarea", "Rectangle", "Floattext"};
static const char* const g_cycle_entries[] = {"First", "Second", "Third", NULL};
static const char* const g_tab_captions[] = {"Songs", "Albums", "Artists", "Playlists", NULL};
static const char* const g_radio_entries[] = {"Low", "Medium", "High", NULL};
static const char* const g_menu_items[] = {"Open", "Save", "-", "Quit", NULL};
/* Menustrip sample: right mouse button over the window opens the bar; Amiga+O, S, W, X, C and V work without it */
static const UP_MenuItem g_strip_items[] = {{"Open", "O", 0, 0}, {"Save", "S", 0, 0}, {"-", NULL, 0, 0}, {"Word Wrap", "W", UPV_MenuItem_Checkit | UPV_MenuItem_Toggle, 0},
                                            {"Print", "P", UPV_MenuItem_Disabled, 0}, {NULL, NULL, 0, 0}};
/* The second title: with two, a hold-drag over the bar switches between them */
static const UP_MenuItem g_strip_edit[] = {{"Cut", "X", 0, 0}, {"Copy", "C", 0, 0}, {"Paste", "V", 0, 0}, {NULL, NULL, 0, 0}};
static ULONG g_palette[] = {0xE5484D, 0xF5A524, 0x46A758, 0x3E63DD, 0x8E4EC6, 0x889096};
static const char* const g_artists[] = {"Virgill", "Jogeir Liljedahl", "Purple Motion", "Zabutom", "Dubmood", "Chipzel"};
static const char* const g_times[] = {"3:42", "4:05", "2:58", "5:21", "3:09", "4:47"};
static UP_ListColumns g_columns = {3, {3, 2, 0}, {NULL, NULL, "0:00"}};

/* I685w run 3: three columns (title, artist, time) and a title row; the hook gets row -1 for the column titles. */
static struct Hook g_row_hook;
static int g_list_rows = 5;
static ULONG demo_cell(struct Hook* hook, Object* obj, UP_ListCell* cell) {
    static char text[40];
    static const char* const heads[] = {"Title", "Artist", "Time"};
    (void)hook;
    (void)obj;
    if (cell->row < 0) {
        return (ULONG)(uintptr_t)heads[cell->col % 3];
    }
    if (cell->col == 0) {
        snprintf(text, sizeof text, "Track %ld", (long)cell->row + 1);
        return (ULONG)(uintptr_t)text;
    }
    return (ULONG)(uintptr_t)(cell->col == 1 ? g_artists[cell->row % 6] : g_times[cell->row % 6]);
}

/* A caption line in a tone. */
static Object* label(const char* text, ULONG tone) {
    return MUI_NewObject((char*)"UP-Text.mcc", UPA_Text_Contents, (ULONG)text, UPA_Text_Tone, tone, TAG_DONE);
}

/* A vertical or horizontal group of up to 6 children (NULLs skipped). Stock groups with MUIA_FillArea FALSE, so the panel face shows. */
static Object* box(ULONG horiz, Object* a, Object* b, Object* c, Object* d, Object* e, Object* f) {
    struct TagItem t[10];
    Object* kids[6];
    int i, n = 0;
    kids[0] = a, kids[1] = b, kids[2] = c, kids[3] = d, kids[4] = e, kids[5] = f;
    t[n].ti_Tag = MUIA_Group_Horiz, t[n++].ti_Data = horiz;
    t[n].ti_Tag = MUIA_FillArea, t[n++].ti_Data = (ULONG)FALSE;
    for (i = 0; i < 6; i++) {
        if (kids[i] != NULL) {
            t[n].ti_Tag = MUIA_Group_Child, t[n++].ti_Data = (ULONG)kids[i];
        }
    }
    t[n].ti_Tag = TAG_DONE, t[n].ti_Data = 0;
    return MUI_NewObjectA((char*)MUIC_Group, t);
}

/* A raised card: a heading line, then the widgets. */
static Object* card(const char* title, Object* a, Object* b, Object* c, Object* d) {
    return MUI_NewObject((char*)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Raised, MUIA_Group_Child, (ULONG)label(title, UPV_Text_Tone_Title),
                         MUIA_Group_Child, (ULONG)box(FALSE, a, b, c, d, NULL, NULL), TAG_DONE);
}

static const char* const g_page_lines[4][2] = {{"Virgill - Past Tense", "Purple Motion - Second Reality"},
                                               {"Chipzel - Spectra", "Dubmood - Cheeky Little Buggers"},
                                               {"Zabutom", "Jogeir Liljedahl"},
                                               {"Morning mix", "Late night tracker tunes"}};

/* One page per tab: a page group whose active page follows UPA_Tabs_Active (I685w run 5). */
static Object* tab_pages(Object* tabs) {
    Object* pg[4];
    Object* grp;
    int i;
    for (i = 0; i < 4; i++) {
        pg[i] = box(FALSE, label(g_page_lines[i][0], UPV_Text_Tone_Normal), label(g_page_lines[i][1], UPV_Text_Tone_Muted), NULL, NULL, NULL, NULL);
    }
    grp = MUI_NewObject((char*)MUIC_Group, MUIA_Group_PageMode, (ULONG)TRUE, MUIA_Group_Child, (ULONG)pg[0], MUIA_Group_Child, (ULONG)pg[1], MUIA_Group_Child, (ULONG)pg[2],
                        MUIA_Group_Child, (ULONG)pg[3], TAG_DONE);
    if (grp != NULL) {
        DoMethod(tabs, MUIM_Notify, UPA_Tabs_Active, MUIV_EveryTime, (ULONG)grp, 3UL, MUIM_Set, MUIA_Group_ActivePage, (ULONG)MUIV_TriggerValue);
    }
    return grp;
}

/* One instance of class `index` with the tags that class needs to be valid on its own; every class is created by file name. */
static Object* make(int index) {
    char path[32];
    snprintf(path, sizeof path, "UP-%s.mcc", g_names[index]);
    switch (index) {
    case 0:
        return MUI_NewObject(path, UPA_Button_Caption, (ULONG) "Play", UPA_Button_Icon, (ULONG) "play", UPA_Button_Role, (ULONG)UPV_Button_Role_Accent, TAG_DONE);
    case 1:
        return MUI_NewObject(path, UPA_Text_Contents, (ULONG) "Welcome back. Every control below is a UP class loaded by name.", UPA_Text_Tone, (ULONG)UPV_Text_Tone_Muted, TAG_DONE);
    case 2:
        return MUI_NewObject(path, UPA_Panel_Role, (ULONG)UPV_Panel_Role_Surface, MUIA_Group_Child, (ULONG)label("Panel: a Surface role card", UPV_Text_Tone_Normal),
                             MUIA_Group_Child, (ULONG)label("It paints its face in the theme token.", UPV_Text_Tone_Muted), TAG_DONE);
    case 3:
        return MUI_NewObject(path, UPA_Tabs_Captions, (ULONG)g_tab_captions, TAG_DONE);
    case 4:
        return MUI_NewObject(path, TAG_DONE);
    case 5:
        return MUI_NewObject(path, MUIA_Numeric_Min, 0UL, MUIA_Numeric_Max, 100UL, MUIA_Numeric_Value, 40UL, TAG_DONE);
    case 6:
        return MUI_NewObject(path, UPA_Progress_Value, 62UL, UPA_Progress_Max, 100UL, TAG_DONE);
    case 7:
        return MUI_NewObject(path, UPA_Cycle_Entries, (ULONG)g_cycle_entries, TAG_DONE);
    case 8:
        return MUI_NewObject(path, UPA_Field_Placeholder, (ULONG) "Search songs, albums, artists", TAG_DONE);
    case 9:
        return MUI_NewObject(path, UPA_Scroller_Entries, 100UL, UPA_Scroller_Visible, 10UL, TAG_DONE);
    case 11:
        return MUI_NewObject(path, UPA_Checkmark_Label, (ULONG) "Shuffle on", TAG_DONE);
    case 12:
        return MUI_NewObject(path, UPA_Radio_Entries, (ULONG)g_radio_entries, UPA_Radio_Active, 1UL, TAG_DONE);
    case 13:
        return MUI_NewObject(path, TAG_DONE);
    case 14:
        return MUI_NewObject(path, UPA_Poplist_Entries, (ULONG)g_cycle_entries, UPA_Poplist_Active, 0UL, TAG_DONE);
    case 15:
        return MUI_NewObject(path, UPA_Numeric_Min, 0UL, UPA_Numeric_Max, 10UL, UPA_Numeric_Value, 4UL, TAG_DONE);
    case 16:
        return MUI_NewObject(path, UPA_Scale_Min, 0UL, UPA_Scale_Max, 100UL, UPA_Scale_Ticks, 11UL, TAG_DONE);
    case 17:
        return MUI_NewObject(path, UPA_Colorfield_RGB, 0x3E63DDUL, TAG_DONE);
    case 18:
        return MUI_NewObject(path, UPA_Palette_Entries, (ULONG)g_palette, UPA_Palette_Count, 6UL, UPA_Palette_Columns, 6UL, TAG_DONE);
    case 19:
        return MUI_NewObject(path, TAG_DONE);
    case 20:
        return MUI_NewObject(path, UPA_Menu_Title, (ULONG) "File", UPA_Menu_Items, (ULONG)g_menu_items, TAG_DONE);
    case 21:
        return MUI_NewObject(path, MUIA_Group_Child, (ULONG)label("Virtgroup: contents at their own size", UPV_Text_Tone_Normal), TAG_DONE);
    case 22:
        return MUI_NewObject(path, MUIA_Group_Child, (ULONG)label("Scrollgroup: scroller shows when taller", UPV_Text_Tone_Normal), TAG_DONE);
    case 23:
        return MUI_NewObject(path, MUIA_Numeric_Min, 0UL, MUIA_Numeric_Max, 100UL, MUIA_Numeric_Value, 65UL, TAG_DONE);
    case 24:
        return MUI_NewObject(path, MUIA_Numeric_Min, 0UL, MUIA_Numeric_Max, 100UL, MUIA_Numeric_Value, 30UL, MUIA_Levelmeter_Label, (ULONG) "Level", TAG_DONE);
    case 25: /* Popobject: a text card as the popup object (MUI 3.8 MUIA_Popobject_Object) */
        return MUI_NewObject(path, MUIA_Popobject_Object,
                             (ULONG)MUI_NewObject((char*)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Popup, UPA_Panel_Popup, (ULONG)TRUE,
                                                  MUIA_Group_Child, (ULONG)label("Popobject: any object as popup", UPV_Text_Tone_Normal), TAG_DONE),
                             TAG_DONE);
    case 26:
        return MUI_NewObject(path, MUIA_Popasl_Type, 0UL /* ASL_FileRequest */, TAG_DONE);
    case 27:
        return MUI_NewObject(path, TAG_DONE);
    case 28:
    case 29:
    case 30:
        return MUI_NewObject(path, MUIA_Pendisplay_Spec, (ULONG)&g_pen_spec, TAG_DONE);
    case 31: /* MUI gap E */
    case 33:
    case 35:
        return MUI_NewObject(path, MUIA_Imagedisplay_Spec, (ULONG) "2:r3E3E3E3E,63636363,DDDDDDDD", TAG_DONE);
    case 32:
    case 34:
    case 36:
        return MUI_NewObject(path, MUIA_Framedisplay_Spec, (ULONG) "910202", TAG_DONE);
    case 37:
        return MUI_NewObject(path, MUIA_Dirlist_Directory, (ULONG) "SYS:", MUIA_Dirlist_RejectIcons, (ULONG)TRUE, TAG_DONE);
    case 38:
    case 39:
        return MUI_NewObject(path, TAG_DONE);
    case 40: /* Menustrip: invisible; two UP-Menu children with the Menuitem model, so a drag can switch titles */
        return MUI_NewObject(path, MUIA_Family_Child,
                             (ULONG)MUI_NewObject((char*)"UP-Menu.mcc", UPA_Menu_Title, (ULONG) "Project", UPA_Menu_Entries, (ULONG)g_strip_items, TAG_DONE),
                             MUIA_Family_Child,
                             (ULONG)MUI_NewObject((char*)"UP-Menu.mcc", UPA_Menu_Title, (ULONG) "Edit", UPA_Menu_Entries, (ULONG)g_strip_edit, TAG_DONE), TAG_DONE);
    case 41: /* Textarea: plain, no own tags needed */
        return MUI_NewObject(path, TAG_DONE);
    case 42:
        return MUI_NewObject(path, MUIA_Rectangle_HBar, (ULONG)TRUE, TAG_DONE);
    case 43:
        return MUI_NewObject(path, MUIA_Floattext_Text, (ULONG) "Floattext: read-only text that wraps at the word.", TAG_DONE);
    default:
        g_row_hook.h_Entry = (__typeof__(g_row_hook.h_Entry))(void (*)(void))HookEntry;
        g_row_hook.h_SubEntry = (__typeof__(g_row_hook.h_SubEntry))(void (*)(void))demo_cell;
        return MUI_NewObject(path, UPA_List_Count, 20UL, UPA_List_CellHook, (ULONG)(uintptr_t)&g_row_hook, UPA_List_Columns, (ULONG)(uintptr_t)&g_columns,
                             UPA_List_Title, (ULONG)TRUE, UPA_List_MultiSelect, (ULONG)UPV_List_MultiSelect_Shifted, UPA_List_MaxRows, (ULONG)(ULONG)g_list_rows, TAG_DONE);
    }
}

/* I1349: an unused sample object leaves the tree before the window exists; its slot is NULL afterwards. */
static void demo_drop(Object** obj, int i) {
    if (obj[i] != NULL) {
        MUI_DisposeObject(obj[i]);
        obj[i] = NULL;
    }
}

/* I1349: 0 under 400 screen lines, 1 under 700, else 2. */
static int demo_tier(void) {
    struct Screen* scr = LockPubScreen(NULL);
    int h = 256;
    if (scr != NULL) {
        h = scr->Height;
        UnlockPubScreen(NULL, scr);
    }
    return h < 400 ? 0 : h < 700 ? 1 : 2;
}

static int demo_body(int argc, char** argv) {
    Object* obj[CLASS_COUNT];
    Object *app = NULL, *win, *root, *content = NULL;
    ULONG sigs = 0, is_open = 0;
    int quit_now = argc > 1 && argv[1] != NULL && strcmp(argv[1], "QUIT") == 0;
    int hold = argc > 1 && argv[1] != NULL && strcmp(argv[1], "HOLD") == 0;
    int auto_close = argc > 1 && argv[1] != NULL && strcmp(argv[1], "AUTO") == 0;
    int i, ok = 0, ticks = 0;
    int tier = demo_tier();
    int mode = argc > 2 && argv[1] != NULL && strcmp(argv[1], "CASE") == 0 ? atoi(argv[2]) : 0; /* RIGCHECK: 1 menustrip, 2 imagedisplay, 3 textarea, 4 Popasl/Popscreen/Popobject, 5 Framedisplay/Popimage/Popframe/Imageadjust/Frameadjust, 6 Pendisplay/Poppen/Penadjust, 7 Dirlist, 8 Volumelist, 9 Scrmodelist, 10 Floattext */

    memset(obj, 0, sizeof obj);
    MUIMasterBase = OpenLibrary((CONST_STRPTR) "muimaster.library", 19);
    if (MUIMasterBase == NULL) {
        printf("[ERROR] muimaster.library 19\n");
        demo_requester("muimaster.library 19 (MUI 3.8) is not installed.\nInstall MUI 3.8, then start UP-Demo again.");
        return 20;
    }
    g_list_rows = tier == 2 ? 5 : 3;
    for (i = 0; i < CLASS_COUNT; i++) {
        ULONG origin = 0;
        obj[i] = make(i);
        if (obj[i] == NULL) {
            printf("[ERROR] %s (MUI_NewObject UP-%s.mcc returned NULL)\n", g_names[i], g_names[i]);
            continue;
        }
        GetAttr(UPA_Shared_Origin, obj[i], &origin);
        if (origin == 0) {
            printf("[ERROR] %s (no origin: the class code is not from a library)\n", g_names[i]);
            continue;
        }
        printf("[OK] %s from MUI:Libs/mui (origin 0x%08lx)\n", g_names[i], (unsigned long)origin);
        ok++;
    }
    if (ok != CLASS_COUNT) {
        char msg[300];
        size_t n;
        snprintf(msg, sizeof msg, "These classes did not open:");
        for (i = 0; i < CLASS_COUNT; i++) {
            n = strlen(msg);
            if (obj[i] == NULL && n < sizeof msg - 16) {
                snprintf(msg + n, sizeof msg - n, " UP-%s", g_names[i]);
            }
        }
        n = strlen(msg);
        snprintf(msg + n, sizeof msg - n, "\nInstall the UP-MCC classes (UP-<name>.mcc) in MUI:Libs/mui and up-mcc.library in LIBS:, then start again.");
        demo_requester(msg);
    }
    if (ok == CLASS_COUNT) {
        {   /* I1349: the window covers the screen and the content is chosen to fit it, no scrolling: three tiers by screen height */
            Object* ctl;
            Object* pbk;
            ctl = mode != 0 ? NULL : card("Controls", /* CASE mode builds its own tree; an unattached card would never be disposed */
                       box(TRUE, obj[0], MUI_NewObject((char*)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Shuffle", UPA_Button_Icon, (ULONG) "shuffle", TAG_DONE),
                           MUI_NewObject((char*)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Queue", UPA_Button_Role, (ULONG)UPV_Button_Role_Ghost, TAG_DONE),
                           MUI_NewObject((char*)"UP-Button.mcc", UPA_Button_Role, (ULONG)UPV_Button_Role_Toggle, TAG_DONE), label("Repeat", UPV_Text_Tone_Normal), NULL),
                       box(FALSE, tier == 0 ? NULL : label("Search", UPV_Text_Tone_Muted), obj[8], obj[7], NULL, NULL, NULL), NULL, NULL);
            pbk = mode != 0 ? NULL : card("Playback", tier == 0 ? NULL : label("Volume 40", UPV_Text_Tone_Muted), obj[5], tier == 0 ? NULL : label("Elapsed 2:05 of 3:22", UPV_Text_Tone_Muted),
                       box(FALSE, obj[6], tier == 0 ? NULL : label("Scroll position", UPV_Text_Tone_Muted), obj[9], NULL, NULL, NULL));
            if (mode != 0) {
                Object* a = NULL;
                Object* b = NULL;
                Object* c = NULL;
                Object* d = NULL;
                Object* e = NULL;
                Object* f = NULL;
                if (mode == 2) {
                    a = MUI_NewObject((char*)"UP-Imagedisplay.mcc", MUIA_Imagedisplay_Spec, (ULONG) "6:3", TAG_DONE);
                    b = MUI_NewObject((char*)"UP-Imagedisplay.mcc", MUIA_Imagedisplay_Spec, (ULONG) "0:128", TAG_DONE);
                    c = MUI_NewObject((char*)"UP-Imagedisplay.mcc", MUIA_Imagedisplay_Spec, (ULONG) "1:4", TAG_DONE);
                    d = MUI_NewObject((char*)"UP-Imagedisplay.mcc", MUIA_Imagedisplay_Spec, (ULONG) "5:RAM:pic.iff", TAG_DONE);
                    e = MUI_NewObject((char*)"UP-Imagedisplay.mcc", MUIA_Imagedisplay_Spec, (ULONG) "2:r3E3E3E3E,63636363,DDDDDDDD", TAG_DONE);
                    f = obj[33];
                } else if (mode == 3) {
                    a = MUI_NewObject((char*)"UP-Textarea.mcc", UPA_TextArea_Text, (ULONG) "alpha beta gamma\nsecond line of words\nthird line here", TAG_DONE);
                    if (a == NULL) a = obj[41];
                } else if (mode == 4) {
                    a = obj[26];
                    b = obj[27];
                    c = obj[25];
                } else if (mode == 5) {
                    a = obj[32];
                    b = obj[33];
                    c = obj[34];
                    d = obj[35];
                    e = obj[36];
                } else if (mode == 6) {
                    a = obj[28];
                    b = obj[29];
                    c = obj[30];
                } else if (mode == 7) {
                    a = obj[37];
                } else if (mode == 8) {
                    a = obj[38];
                } else if (mode == 9) {
                    a = obj[39];
                } else if (mode == 10) {
                    a = obj[43];
                } else {
                    a = label("Right mouse button: menu bar", UPV_Text_Tone_Normal);
                    b = obj[40];
                }
                if (argc > 3 && atoi(argv[3]) > 0 && atoi(argv[3]) < CLASS_COUNT) { /* CASE <n> <index>: that one class alone (bisect) */
                    a = obj[atoi(argv[3])];
                    b = c = d = e = f = NULL;
                }
                root = MUI_NewObject((char*)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Panel, MUIA_Group_Child, (ULONG)label("UP-Demo case", UPV_Text_Tone_Heading),
                                     MUIA_Group_Child, (ULONG)box(TRUE, a, b, c, d, e, f), TAG_DONE);
                for (i = 0; i < CLASS_COUNT; i++) { /* the app disposes only its tree: the samples left out go now, or their classes stay open */
                    if (obj[i] != a && obj[i] != b && obj[i] != c && obj[i] != d && obj[i] != e && obj[i] != f) {
                        demo_drop(obj, i);
                    }
                }
            } else if (tier == 2) {
                Object* more = card("More controls", box(TRUE, obj[11], obj[12], NULL, NULL, NULL, NULL), box(TRUE, obj[13], obj[14], obj[20], NULL, NULL, NULL),
                                    box(TRUE, obj[15], obj[16], obj[17], obj[18], obj[19], NULL), box(FALSE, box(TRUE, obj[21], obj[22], obj[23], obj[24], NULL, NULL), box(TRUE, obj[25], obj[26], obj[27], NULL, NULL, NULL), NULL, NULL, NULL, NULL));
                Object* left = card("Library", box(FALSE, obj[3], tab_pages(obj[3]), NULL, NULL, NULL, NULL), label("128 songs in your library", UPV_Text_Tone_Muted), obj[2], obj[10]);
                root = MUI_NewObject((char*)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Panel, MUIA_Group_Child, (ULONG)label("UP-Demo", UPV_Text_Tone_Heading),
                                     MUIA_Group_Child, (ULONG)obj[1], MUIA_Group_Child, (ULONG)ctl,
                                     MUIA_Group_Child, (ULONG)box(TRUE, left, obj[4], pbk, NULL, NULL, NULL),
                                     MUIA_Group_Child, (ULONG)more, TAG_DONE);
            } else if (tier == 1) { /* 512 lines: no welcome line, no Surface panel; controls and playback side by side over a three-row library */
                Object* lib = card("Library", obj[3], obj[10], NULL, NULL);
                for (i = 11; i < CLASS_COUNT; i++) {
                    demo_drop(obj, i);
                }
                demo_drop(obj, 1);
                demo_drop(obj, 2);
                root = MUI_NewObject((char*)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Panel, MUIA_Group_Child, (ULONG)label("UP-Demo", UPV_Text_Tone_Heading),
                                     MUIA_Group_Child, (ULONG)box(TRUE, ctl, obj[4], pbk, NULL, NULL, NULL), MUIA_Group_Child, (ULONG)lib, TAG_DONE);
            } else { /* 256 lines: heading, controls and playback side by side, the tabs; no list, no Surface panel, no welcome line */
                for (i = 11; i < CLASS_COUNT; i++) {
                    demo_drop(obj, i);
                }
                demo_drop(obj, 1);
                demo_drop(obj, 2);
                demo_drop(obj, 10);
                root = MUI_NewObject((char*)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Panel, MUIA_Group_Child, (ULONG)label("UP-Demo", UPV_Text_Tone_Heading),
                                     MUIA_Group_Child, (ULONG)box(TRUE, ctl, obj[4], pbk, NULL, NULL, NULL), MUIA_Group_Child, (ULONG)obj[3], TAG_DONE);
            }
        }
        if (root != NULL) {
            content = root;
        }
        win = root == NULL ? NULL
                           : MUI_NewObject((char*)MUIC_Window, MUIA_Window_Title, (ULONG) "UP-Demo", MUIA_Window_ID, 0x55504431, MUIA_Window_Width, (ULONG)MUIV_Window_Width_Screen(100),
                                           MUIA_Window_Height, (ULONG)MUIV_Window_Height_Screen(100),
                                           MUIA_Window_RootObject, (ULONG)root, TAG_DONE);
        if (win != NULL) {
            app = MUI_NewObject((char*)MUIC_Application, MUIA_Application_Title, (ULONG) "UP-Demo", MUIA_Application_Base,
                                (ULONG) "UPDEMO", MUIA_Application_Window, (ULONG)win, TAG_DONE);
        }
        if (app == NULL) {
            printf("[ERROR] window or application could not be created\n");
            demo_requester("The window could not be created.\nFree some memory and check that the MUI classes in MUI:Libs/mui are installed, then start again.");
            ok = 0;
            if (win != NULL) {
                MUI_DisposeObject(win);
            } else if (root != NULL) {
                MUI_DisposeObject(root);
            }
        } else {
            DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)app, 2UL, MUIM_Application_ReturnID,
                     (ULONG)MUIV_Application_ReturnID_Quit);
            SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
            GetAttr(MUIA_Window_Open, win, &is_open);
            if (!is_open) {
                printf("[ERROR] window did not open\n");
                demo_requester("The window did not open.\nUse a screen of at least 640x256 pixels (Screenmode preferences), free some memory, "
                               "and check that ttengine.library is in LIBS: and the MUI classes are in MUI:Libs/mui, then start again.");
                ok = 0;
            }
            if (is_open && (hold || auto_close)) { /* I685w: the rig census reads the widget boxes from here, in screen pixels */
                ULONG wptr = 0;
                GetAttr(MUIA_Window_Window, win, &wptr);
                Delay(50);
                if (wptr != 0) {
                    struct Window* w = (struct Window*)wptr;
                    for (i = 0; i < CLASS_COUNT; i++) {
                        ULONG l = 0, t = 0, ww = 0, hh = 0;
                        if (obj[i] == NULL) {
                            continue;
                        }
                        GetAttr(MUIA_LeftEdge, obj[i], &l);
                        GetAttr(MUIA_TopEdge, obj[i], &t);
                        GetAttr(MUIA_Width, obj[i], &ww);
                        GetAttr(MUIA_Height, obj[i], &hh);
                        printf("[RECT] %s %ld %ld %ld %ld\n", g_names[i], (long)(w->LeftEdge + l), (long)(w->TopEdge + t), (long)ww, (long)hh);
                    }
                    {
                        ULONG ch = 0, cw = 0;
                        GetAttr(MUIA_Width, content, &cw);
                        GetAttr(MUIA_Height, content, &ch);
                        printf("[SIZE] content %ld x %ld on a window of %ld x %ld\n", (long)cw, (long)ch, (long)w->Width, (long)w->Height);
                    }
                    {
                        struct Task* me = FindTask(NULL);
                        printf("[STACK] %lu bytes\n", (unsigned long)((char*)me->tc_SPUpper - (char*)me->tc_SPLower));
                    }
                    printf("[RECT] Window %ld %ld %ld %ld\n", (long)w->LeftEdge, (long)w->TopEdge, (long)w->Width, (long)w->Height);
                    fflush(stdout);
                }
            }
            while (!quit_now && (!auto_close || ticks < 30) && is_open) { /* until the close gadget; AUTO: about 3 s */
                ULONG id = DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);
                if (id == (ULONG)MUIV_Application_ReturnID_Quit || (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) != 0) { /* close gadget or Ctrl-C from a script; the break is consumed so the exit flush does not abort */
                    break;
                }
                Delay(5);
                ticks++;
            }
            MUI_DisposeObject(app); /* the application owns window, group and the class objects */
        }
    } else {
        for (i = 0; i < CLASS_COUNT; i++) {
            if (obj[i] != NULL) {
                MUI_DisposeObject(obj[i]);
            }
        }
    }
    CloseLibrary(MUIMasterBase);
    return ok == CLASS_COUNT ? 0 : 20;
}

/* libnix read()/write() call __chkabort, which turns a pending Ctrl-C into exit(20) inside printf and skips the
 * dispose below (the classes leak). This empty one keeps Ctrl-C pending until the main loop ends the demo cleanly. */
void __chkabort(void) {
}

static int demo_main(int argc, char** argv) {
    int rc;
    IntuitionBase = (struct IntuitionBase*)OpenLibrary((CONST_STRPTR) "intuition.library", 36);
    if (IntuitionBase == NULL) {
        return 20;
    }
    rc = demo_body(argc, argv);
    CloseLibrary((struct Library*)IntuitionBase);
    IntuitionBase = NULL;
    return rc;
}

/* Run demo_main on a stack of DEMO_STACK_BYTES when the process stack is smaller (I1343). Same technique as
 * apps/common/amiga_start.c (StackSwap, both swaps in one asm block so no compiler code runs on the wrong stack);
 * repeated here because the demo is headers-only and must not link Selecta code. */
static struct StackSwapStruct g_sss;
static int g_argc, g_rc;
static char** g_argv;

static void run_body(void) {
    g_rc = demo_main(g_argc, g_argv);
}

static void __attribute__((noinline)) run_swapped(void) {
    __asm__ volatile("move.l %0,%%a0\n\t"
                     "move.l %1,%%a6\n\t"
                     "jsr -732(%%a6)\n\t"
                     "move.l %2,%%a1\n\t"
                     "jsr (%%a1)\n\t"
                     "move.l %0,%%a0\n\t"
                     "move.l %1,%%a6\n\t"
                     "jsr -732(%%a6)\n\t"
                     :
                     : "i"(&g_sss), "m"(SysBase), "i"(run_body)
                     : "d0", "d1", "a0", "a1", "a6", "cc", "memory");
}

int main(int argc, char** argv) {
    struct Task* me = FindTask(NULL);
    unsigned long have = (unsigned long)me->tc_SPUpper - (unsigned long)me->tc_SPLower;
    UBYTE* mem;

    if (have >= DEMO_STACK_BYTES) {
        return demo_main(argc, argv);
    }
    mem = (UBYTE*)AllocMem(DEMO_STACK_BYTES, MEMF_PUBLIC | MEMF_CLEAR);
    if (mem == NULL) {
        return demo_main(argc, argv);
    }
    g_argc = argc;
    g_argv = argv;
    g_sss.stk_Lower = mem;
    g_sss.stk_Upper = (ULONG)(mem + DEMO_STACK_BYTES);
    g_sss.stk_Pointer = (APTR)(mem + DEMO_STACK_BYTES);
    run_swapped();
    FreeMem(mem, DEMO_STACK_BYTES);
    return g_rc;
}
