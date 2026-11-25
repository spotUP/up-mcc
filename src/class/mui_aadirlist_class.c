/* UP-Dirlist, UP-Volumelist and UP-Scrmodelist. One Group subclass, three dispatchers over the same data: a UP-List (with its
 * UP-Scroller) shows an array of AllocVec'd entries through a row hook. The group answers the MUI 3.8 list ids itself (UP-List has
 * its own UPA_List_* ids), so MUI code that reads MUIA_List_Active or calls MUIM_List_GetEntry works unchanged. */

#include "mui_aadirlist_class.h"
#include "mui_aa_class.h"

#include <stdint.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <dos/dosextens.h>
#include <dos/exall.h>
#include <graphics/displayinfo.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>

#include "map/mui_aa.h"

extern struct Library* MUIMasterBase;

enum { AADL_K_DIR, AADL_K_VOL, AADL_K_MODE };

typedef struct aadl_data {
    Object* list;
    int kind;
    APTR* ent; /* Dirlist: struct FileInfoBlock*; Volumelist: "Name:"; Scrmodelist: UP_Scrmode* */
    LONG n, cap, active, dbl;
    struct Hook hook; /* UPA_List_RowHook: the entry text (h_Data is this data) */
    /* Dirlist */
    STRPTR dir, accept, reject; /* the caller's strings, kept as pointers like MUI does */
    struct Hook* filter;
    LONG drawers_only, files_only, filter_drawers, multi_dirs, reject_icons, sort_dirs, sort_high_low, sort_type;
    LONG bytes, files, drawers, status;
    char path[256];
    char row[128];
} aadl_data;

static struct MUI_CustomClass *g_dirlist_class, *g_volumelist_class, *g_scrmodelist_class;

static void aadl_clear(aadl_data* d) {
    LONG i;
    for (i = 0; i < d->n; i++) {
        FreeVec(d->ent[i]);
    }
    d->n = 0;
    d->active = -1;
}

/* Appends a copy of size bytes of src (plus a NUL when text). Returns the copy, NULL when memory runs out (the entry is skipped). */
static APTR aadl_add(aadl_data* d, const void* src, ULONG size) {
    APTR e;
    if (d->n == d->cap) {
        LONG cap = d->cap != 0 ? d->cap * 2 : 32;
        APTR* a = (APTR*)AllocVec((ULONG)cap * sizeof(APTR), MEMF_ANY);
        if (a == NULL) {
            return NULL;
        }
        if (d->ent != NULL) {
            memcpy(a, d->ent, (size_t)d->n * sizeof(APTR));
            FreeVec(d->ent);
        }
        d->ent = a, d->cap = cap;
    }
    e = AllocVec(size + 1, MEMF_CLEAR);
    if (e != NULL) {
        memcpy(e, src, size);
        d->ent[d->n++] = e;
    }
    return e;
}

static void aadl_key(const struct FileInfoBlock* f, mui_aa_dirkey* k) {
    k->name = f->fib_FileName, k->size = f->fib_Size, k->dir = f->fib_DirEntryType > 0;
    k->days = f->fib_Date.ds_Days, k->mins = f->fib_Date.ds_Minute, k->ticks = f->fib_Date.ds_Tick;
}

/* shortcut: insertion sort, fine for a few hundred entries; switch to a merge sort if huge drawers get slow */
static void aadl_sort(aadl_data* d) {
    LONG i, j;
    mui_aa_dirkey a, b;
    for (i = 1; i < d->n; i++) {
        APTR e = d->ent[i];
        aadl_key((struct FileInfoBlock*)e, &a);
        for (j = i; j > 0; j--) {
            aadl_key((struct FileInfoBlock*)d->ent[j - 1], &b);
            if (mui_aa_dir_cmp(&b, &a, (int)d->sort_type, (int)d->sort_dirs, (int)d->sort_high_low) <= 0) {
                break;
            }
            d->ent[j] = d->ent[j - 1];
        }
        d->ent[j] = e;
    }
}

static int aadl_match(STRPTR pat, const char* name) {
    char tok[256];
    return ParsePatternNoCase(pat, tok, sizeof tok) >= 0 && MatchPatternNoCase(tok, (STRPTR)name);
}

static int aadl_keep(aadl_data* d, Object* obj, const struct FileInfoBlock* f) {
    int dir = f->fib_DirEntryType > 0;
    if ((dir && d->files_only) || (!dir && d->drawers_only) || (!dir && d->reject_icons && mui_aa_icon_name(f->fib_FileName))) {
        return 0;
    }
    if (!dir || d->filter_drawers) {
        if (d->accept != NULL && !aadl_match(d->accept, f->fib_FileName)) return 0;
        if (d->reject != NULL && aadl_match(d->reject, f->fib_FileName)) return 0;
    }
    if (d->filter != NULL) {
        struct ExAllData ed;
        memset(&ed, 0, sizeof ed);
        ed.ed_Name = (UBYTE*)f->fib_FileName, ed.ed_Type = f->fib_DirEntryType, ed.ed_Size = (ULONG)f->fib_Size;
        ed.ed_Prot = (ULONG)f->fib_Protection, ed.ed_Days = (ULONG)f->fib_Date.ds_Days, ed.ed_Mins = (ULONG)f->fib_Date.ds_Minute;
        ed.ed_Ticks = (ULONG)f->fib_Date.ds_Tick, ed.ed_Comment = (UBYTE*)f->fib_Comment;
        return CallHookPkt(d->filter, obj, &ed) != 0;
    }
    return 1;
}

/* shortcut: reads synchronously (MUI reads in the background); a slow volume blocks the input loop until done, make it async if that bites */
static void aadl_read_dir(Object* obj, aadl_data* d) {
    struct FileInfoBlock* f;
    BPTR lock;
    d->bytes = d->files = d->drawers = 0;
    d->status = MUIV_Dirlist_Status_Invalid;
    if (d->dir == NULL || (f = (struct FileInfoBlock*)AllocDosObject(DOS_FIB, NULL)) == NULL) {
        return;
    }
    d->status = MUIV_Dirlist_Status_Reading;
    lock = Lock(d->dir, ACCESS_READ);
    if (lock != 0 && Examine(lock, f) && f->fib_DirEntryType > 0) {
        while (ExNext(lock, f)) {
            if (aadl_keep(d, obj, f) && aadl_add(d, f, sizeof *f) != NULL) {
                if (f->fib_DirEntryType > 0) {
                    d->drawers++;
                } else {
                    d->files++, d->bytes += f->fib_Size;
                }
            }
        }
        d->status = MUIV_Dirlist_Status_Valid;
        aadl_sort(d);
    } else {
        d->status = MUIV_Dirlist_Status_Invalid;
    }
    if (lock != 0) {
        UnLock(lock);
    }
    FreeDosObject(DOS_FIB, f);
}

static void aadl_read_vol(aadl_data* d) {
    struct DosList* dl = LockDosList(LDF_VOLUMES | LDF_ASSIGNS | LDF_READ);
    char name[260];
    while ((dl = NextDosEntry(dl, LDF_VOLUMES | LDF_ASSIGNS)) != NULL) {
        const UBYTE* b = (const UBYTE*)BADDR(dl->dol_Name);
        ULONG len = b != NULL ? b[0] : 0;
        if (len != 0) {
            memcpy(name, b + 1, len);
            name[len] = ':';
            aadl_add(d, name, len + 1);
        }
    }
    UnLockDosList(LDF_VOLUMES | LDF_ASSIGNS | LDF_READ);
}

static void aadl_read_mode(aadl_data* d) {
    ULONG id = INVALID_ID;
    struct NameInfo ni;
    UP_Scrmode m;
    while ((id = NextDisplayInfo(id)) != INVALID_ID) {
        if (ModeNotAvailable(id) == 0 && GetDisplayInfoData(NULL, (UBYTE*)&ni, sizeof ni, DTAG_NAME, id) > 0 && ni.Name[0] != 0) {
            m.id = id;
            strncpy(m.name, (const char*)ni.Name, sizeof m.name - 1);
            m.name[sizeof m.name - 1] = 0;
            aadl_add(d, &m, sizeof m);
        }
    }
}

/* Reads the entries again and hands the new count to the list (no row selected afterwards, as in MUI). */
static void aadl_reread(Object* obj, aadl_data* d) {
    aadl_clear(d);
    if (d->kind == AADL_K_DIR) {
        aadl_read_dir(obj, d);
    } else if (d->kind == AADL_K_VOL) {
        aadl_read_vol(d);
    } else {
        aadl_read_mode(d);
    }
    if (d->list != NULL) {
        SetAttrs(d->list, UPA_List_Count, (ULONG)d->n, UPA_List_Active, (ULONG)-1, TAG_DONE);
    }
}

static const char* aadl_row(struct Hook* hook, Object* obj, UP_ListCell* cell) {
    aadl_data* d = (aadl_data*)hook->h_Data;
    APTR e;
    (void)obj;
    if (cell->row < 0 || cell->row >= d->n) {
        return "";
    }
    e = d->ent[cell->row];
    if (d->kind == AADL_K_VOL) {
        return (const char*)e;
    }
    if (d->kind == AADL_K_MODE) {
        return ((UP_Scrmode*)e)->name;
    }
    if (((struct FileInfoBlock*)e)->fib_DirEntryType <= 0) {
        return ((struct FileInfoBlock*)e)->fib_FileName;
    }
    /* shortcut: one column, drawers marked by a suffix; MUI shows a size column with "Drawer", add UPA_List_Columns if asked */
    strcpy(d->row, ((struct FileInfoBlock*)e)->fib_FileName);
    strcat(d->row, " (Drawer)");
    return d->row;
}

static LONG aadl_mode_row(aadl_data* d, ULONG id) {
    LONG i;
    for (i = 0; i < d->n; i++) {
        if (((UP_Scrmode*)d->ent[i])->id == id) return i;
    }
    return -1;
}

static void aadl_pick(aadl_data* d, LONG v) {
    if (v < 0 || v >= d->n) v = -1; /* MUIV_List_Active_Off and friends: no row */
    d->active = v;
    if (d->list != NULL) {
        SetAttrs(d->list, MUIA_NoNotify, TRUE, UPA_List_Active, (ULONG)v, TAG_DONE);
    }
}

/* Takes the ids this class owns from tags; returns TRUE when a Dirlist id changed what is read. */
static int aadl_tags(Object* obj, aadl_data* d, struct TagItem* tags) {
    struct TagItem* t;
    int reread = 0;
    while ((t = NextTagItem(&tags)) != NULL) {
        LONG v = (LONG)t->ti_Data;
        switch (t->ti_Tag) {
        case UPA_Scrmodelist_ModeID:
            aadl_pick(d, aadl_mode_row(d, t->ti_Data));
            break;
        case MUIA_List_Active:
            aadl_pick(d, v);
            break;
        case MUIA_Listview_DoubleClick: d->dbl = v; break;
        case MUIA_Dirlist_Directory: d->dir = (STRPTR)t->ti_Data, reread = 1; break;
        case MUIA_Dirlist_AcceptPattern: d->accept = (STRPTR)t->ti_Data, reread = 1; break;
        case MUIA_Dirlist_RejectPattern: d->reject = (STRPTR)t->ti_Data, reread = 1; break;
        case MUIA_Dirlist_FilterHook: d->filter = (struct Hook*)t->ti_Data, reread = 1; break;
        case MUIA_Dirlist_DrawersOnly: d->drawers_only = v, reread = 1; break;
        case MUIA_Dirlist_FilesOnly: d->files_only = v, reread = 1; break;
        case MUIA_Dirlist_FilterDrawers: d->filter_drawers = v, reread = 1; break;
        case MUIA_Dirlist_MultiSelDirs: d->multi_dirs = v; break;
        case MUIA_Dirlist_RejectIcons: d->reject_icons = v, reread = 1; break;
        case MUIA_Dirlist_SortDirs: d->sort_dirs = v, reread = 1; break;
        case MUIA_Dirlist_SortHighLow: d->sort_high_low = v, reread = 1; break;
        case MUIA_Dirlist_SortType: d->sort_type = v, reread = 1; break;
        }
    }
    (void)obj;
    return reread && d->kind == AADL_K_DIR;
}

/* super gets every tag too, so notifies on MUIA_List_Active and MUIA_Listview_DoubleClick fire */
static ULONG aadl_set(struct IClass* cl, Object* obj, struct opSet* msg) {
    aadl_data* d = INST_DATA(cl, obj);
    if (aadl_tags(obj, d, msg->ops_AttrList)) {
        aadl_reread(obj, d);
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aadl_new(struct IClass* cl, Object* obj, struct opSet* msg, int kind) {
    struct TagItem gt[] = {{MUIA_Group_Horiz, TRUE}, {MUIA_Group_Spacing, 0}, {TAG_MORE, (ULONG)(uintptr_t)msg->ops_AttrList}};
    struct TagItem tl[5];
    struct opSet sup = *msg;
    aadl_data* d;
    Object *list = NULL, *group;
    sup.ops_AttrList = gt;
    obj = (Object*)DoSuperMethodA(cl, obj, (Msg)&sup);
    if (obj == NULL) {
        return 0;
    }
    d = INST_DATA(cl, obj);
    memset(d, 0, sizeof *d);
    d->kind = kind, d->active = -1;
    d->hook.h_Entry = (__typeof__(d->hook.h_Entry))(void (*)(void))HookEntry;
    d->hook.h_SubEntry = (__typeof__(d->hook.h_SubEntry))(void (*)(void))aadl_row;
    d->hook.h_Data = d;
    tl[0].ti_Tag = UPA_List_RowHook, tl[0].ti_Data = (ULONG)(uintptr_t)&d->hook;
    tl[1].ti_Tag = UPA_List_DoublePick, tl[1].ti_Data = TRUE;
    tl[2].ti_Tag = UPA_List_Count, tl[2].ti_Data = 0;
#ifdef UP_MCC_LIBRARY
    /* .mcc build: this library holds only its own class slots, so the list and its scroller are made by name */
    {
        Object* sc = MUI_NewObject((char*)"UP-Scroller.mcc", MUIA_Frame, MUIV_Frame_None, TAG_DONE);
        tl[3].ti_Tag = UPA_List_Scroller, tl[3].ti_Data = (ULONG)(uintptr_t)sc;
        tl[4].ti_Tag = TAG_DONE, tl[4].ti_Data = 0;
        group = list = sc != NULL ? MUI_NewObjectA((char*)"UP-List.mcc", tl) : NULL;
        if (list == NULL) {
            if (sc != NULL) MUI_DisposeObject(sc);
        } else {
            DoMethod(obj, OM_ADDMEMBER, (ULONG)list);
            DoMethod(obj, OM_ADDMEMBER, (ULONG)sc);
            DoMethod(sc, MUIM_Notify, UPA_Scroller_First, MUIV_EveryTime, (ULONG)list, 3, MUIM_Set, UPA_List_First, MUIV_TriggerValue);
        }
    }
#else
    tl[3].ti_Tag = TAG_DONE, tl[3].ti_Data = 0;
    group = UP_NewList(tl, &list);
    if (group != NULL) {
        DoMethod(obj, OM_ADDMEMBER, (ULONG)group);
    }
#endif
    if (group == NULL) {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }
    d->list = list;
    DoMethod(list, MUIM_Notify, UPA_List_Active, MUIV_EveryTime, (ULONG)obj, 3, MUIM_Set, MUIA_List_Active, MUIV_TriggerValue);
    DoMethod(list, MUIM_Notify, UPA_List_Picked, MUIV_EveryTime, (ULONG)obj, 3, MUIM_Set, MUIA_Listview_DoubleClick, TRUE);
    /* Dirlist reads only once a directory is given; the other two read now */
    if (aadl_tags(obj, d, msg->ops_AttrList) || kind != AADL_K_DIR) {
        aadl_reread(obj, d);
        aadl_tags(obj, d, msg->ops_AttrList); /* an initial MUIA_List_Active / ModeID picks its row in the fresh entries */
    }
    return (ULONG)obj;
}

static ULONG aadl_get(struct IClass* cl, Object* obj, struct opGet* msg) {
    aadl_data* d = INST_DATA(cl, obj);
    ULONG* s = msg->opg_Storage;
    switch (msg->opg_AttrID) {
    case MUIA_List_Active: *s = (ULONG)d->active; return TRUE;
    case MUIA_List_Entries: *s = (ULONG)d->n; return TRUE;
    case MUIA_Listview_DoubleClick: *s = (ULONG)d->dbl; return TRUE;
    case UPA_Scrmodelist_ModeID: *s = d->kind == AADL_K_MODE && d->active >= 0 ? ((UP_Scrmode*)d->ent[d->active])->id : INVALID_ID; return TRUE;
    }
    if (d->kind == AADL_K_DIR) {
        switch (msg->opg_AttrID) {
        case MUIA_Dirlist_Directory: *s = (ULONG)(uintptr_t)d->dir; return TRUE;
        case MUIA_Dirlist_NumBytes: *s = (ULONG)d->bytes; return TRUE;
        case MUIA_Dirlist_NumFiles: *s = (ULONG)d->files; return TRUE;
        case MUIA_Dirlist_NumDrawers: *s = (ULONG)d->drawers; return TRUE;
        case MUIA_Dirlist_Status: *s = (ULONG)d->status; return TRUE;
        case MUIA_Dirlist_Path:
            *s = 0;
            if (d->active >= 0 && d->dir != NULL && strlen(d->dir) < sizeof d->path) {
                strcpy(d->path, d->dir);
                if (AddPart(d->path, ((struct FileInfoBlock*)d->ent[d->active])->fib_FileName, sizeof d->path)) {
                    *s = (ULONG)(uintptr_t)d->path;
                }
            }
            return TRUE;
        }
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG aadl_dispatch_c(struct IClass* cl, Object* obj, Msg msg, int kind) {
    aadl_data* d;
    switch (msg->MethodID) {
    case OM_NEW:
        return aadl_new(cl, obj, (struct opSet*)msg, kind);
    case OM_DISPOSE:
        d = INST_DATA(cl, obj);
        aadl_clear(d);
        if (d->ent != NULL) FreeVec(d->ent);
        d->ent = NULL;
        break;
    case OM_SET:
        return aadl_set(cl, obj, (struct opSet*)msg);
    case OM_GET:
        return aadl_get(cl, obj, (struct opGet*)msg);
    case MUIM_List_GetEntry: {
        struct MUIP_List_GetEntry* m = (struct MUIP_List_GetEntry*)msg;
        LONG pos;
        d = INST_DATA(cl, obj);
        pos = m->pos == MUIV_List_GetEntry_Active ? d->active : m->pos;
        *m->entry = pos >= 0 && pos < d->n ? d->ent[pos] : NULL;
        return (ULONG)(uintptr_t)*m->entry;
    }
    case MUIM_Dirlist_ReRead:
        if (kind == AADL_K_DIR) {
            aadl_reread(obj, INST_DATA(cl, obj));
        }
        return 0;
    }
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG __attribute__((used)) aadirlist_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return aadl_dispatch_c(cl, obj, msg, AADL_K_DIR);
}
static ULONG __attribute__((used)) aavolumelist_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return aadl_dispatch_c(cl, obj, msg, AADL_K_VOL);
}
static ULONG __attribute__((used)) aascrmodelist_dispatch_c(struct IClass* cl, Object* obj, Msg msg) {
    return aadl_dispatch_c(cl, obj, msg, AADL_K_MODE);
}

extern ULONG aadirlist_dispatch(void);
extern ULONG aavolumelist_dispatch(void);
extern ULONG aascrmodelist_dispatch(void);
const struct aa_class_desc aadirlist_class_desc = {MUIC_Group, sizeof(aadl_data), aadirlist_dispatch, &g_dirlist_class};
const struct aa_class_desc aavolumelist_class_desc = {MUIC_Group, sizeof(aadl_data), aavolumelist_dispatch, &g_volumelist_class};
const struct aa_class_desc aascrmodelist_class_desc = {MUIC_Group, sizeof(aadl_data), aascrmodelist_dispatch, &g_scrmodelist_class};
__asm__(".text\n.even\n.globl _aadirlist_dispatch\n_aadirlist_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aadirlist_dispatch_c\n\tlea 12(sp),sp\n\trts\n"
        ".globl _aavolumelist_dispatch\n_aavolumelist_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aavolumelist_dispatch_c\n\tlea 12(sp),sp\n\trts\n"
        ".globl _aascrmodelist_dispatch\n_aascrmodelist_dispatch:\n"
        "\tmove.l a1,-(sp)\n\tmove.l a2,-(sp)\n\tmove.l a0,-(sp)\n"
        "\tjbsr _aascrmodelist_dispatch_c\n\tlea 12(sp),sp\n\trts\n");

static Object* aadl_make(struct MUI_CustomClass** slot, const struct aa_class_desc* desc, const struct TagItem* tags) {
    if (*slot == NULL) {
        *slot = AA_CLASS_CREATE(desc);
    }
    if (*slot == NULL) {
        return NULL;
    }
    return NewObjectA((*slot)->mcc_Class, NULL, (struct TagItem*)tags);
}

Object* UP_NewDirlist(const struct TagItem* tags) {
    return aadl_make(&g_dirlist_class, &aadirlist_class_desc, tags);
}

Object* UP_NewVolumelist(const struct TagItem* tags) {
    return aadl_make(&g_volumelist_class, &aavolumelist_class_desc, tags);
}

Object* UP_NewScrmodelist(const struct TagItem* tags) {
    return aadl_make(&g_scrmodelist_class, &aascrmodelist_class_desc, tags);
}

void mui_aadirlist_class_free(void) {
    struct MUI_CustomClass** slot[3] = {&g_dirlist_class, &g_volumelist_class, &g_scrmodelist_class};
    int i;
    for (i = 0; i < 3; i++) {
        if (*slot[i] != NULL) {
            MUI_DeleteCustomClass(*slot[i]);
            *slot[i] = NULL;
        }
    }
}
