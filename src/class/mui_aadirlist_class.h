/* UP-Dirlist, UP-Volumelist and UP-Scrmodelist (MUIC_Dirlist / MUIC_Volumelist / MUIC_Scrmodelist equivalents). A horizontal Group
 * around a UP-List and its UP-Scroller; the entries are a directory, the mounted volumes and assigns, or the display modes. All three
 * classes live in mui_aadirlist_class.c. Amiga only; the directory sort order is in map/mui_aa.c (mui_aa_dir_cmp). */
#ifndef MUI_AADIRLIST_CLASS_H
#define MUI_AADIRLIST_CLASS_H

#include <mcc/up.h>

void mui_aadirlist_class_free(void);

#endif
