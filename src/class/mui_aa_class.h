#ifndef MUI_AA_CLASS_H
#define MUI_AA_CLASS_H

/* The static/shared switch of the class package (I685i, design 2026-11-06-mui-mcc-package.md section 3.2). Every Selecta_AA* class
 * file describes its class once, in an aa_class_desc: superclass name, instance size, dispatcher, and the file's class pointer.
 * The two places where the static build and the .mcc build differ are AA_CLASS_CREATE (below) and lib/classheader.c, which reads
 * the description and creates the class once in the library init. Dispatcher, draw, layout and input code are the same in both. */
#include <stdint.h>

#include <exec/types.h>
#include <libraries/mui.h>

struct aa_class_desc {
    const char* super;                 /* MUIC_Area, MUIC_Group, ... */
    ULONG size;                        /* sizeof the instance data */
    ULONG (*dispatcher)(void);         /* the asm entry stub of the class (a0 class, a2 object, a1 message) */
    struct MUI_CustomClass** slot;     /* the class file's own class pointer */
};

#ifdef UP_MCC_LIBRARY
/* .mcc build: the library init created the class with the library base as first argument and stored it in *slot. */
#define AA_CLASS_CREATE(desc) (*(desc)->slot)
/* A sibling class object: by name through MUI (the class lives in its own .mcc, so this library never links its dispatcher). */
#define AA_SIBLING(ctor, name, tags) MUI_NewObject((char*)(name), TAG_MORE, (ULONG)(uintptr_t)(tags), TAG_DONE)
#else
#define AA_SIBLING(ctor, name, tags) ctor(tags)
/* Static build: the class is created on first use with a NULL base. */
#define AA_CLASS_CREATE(desc) MUI_CreateCustomClass(NULL, (char*)(desc)->super, NULL, (desc)->size, (APTR)(uintptr_t)(desc)->dispatcher)
#endif

extern const struct aa_class_desc aabtn_class_desc, aatext_class_desc, aapanel_class_desc, aatabs_class_desc, aabal_class_desc,
    aaslider_class_desc, aaprogress_class_desc, aacycle_class_desc, aafield_class_desc, aascroller_class_desc, aalist_class_desc, aacheckmark_class_desc, aaradio_class_desc,
    aapopstring_class_desc, aapoplist_class_desc, aanumeric_class_desc, aascale_class_desc, aacolorfield_class_desc, aapalette_class_desc, aaimage_class_desc, aamenu_class_desc,
    aavirtgroup_class_desc, aascrollgroup_class_desc, aatextarea_class_desc,
    aarect_class_desc, aafloattext_class_desc;
extern const struct aa_class_desc aaknob_class_desc, aalevelmeter_class_desc,
    aapopobject_class_desc, aapopasl_class_desc, aapopscreen_class_desc;
extern const struct aa_class_desc aapendisplay_class_desc, aapoppen_class_desc, aapenadjust_class_desc; /* MUI gap D */
/* MUI gap E */
extern const struct aa_class_desc aaimagedisplay_class_desc, aaframedisplay_class_desc, aapopimage_class_desc, aapopframe_class_desc,
    aaimageadjust_class_desc, aaframeadjust_class_desc,
    aadirlist_class_desc, aavolumelist_class_desc, aascrmodelist_class_desc;
extern const struct aa_class_desc aamenustrip_class_desc; /* UP-Menustrip */

/* A class that queues MUIM_Application_PushMethod unpushes it at MUIM_DisconnectParent, the last point where the
 * application is known live (UP-Menustrip, UP-Scrollgroup); these IDs are missing from the MUI 3.8 SDK. */
#ifndef MUIM_Application_UnpushMethod
#define MUIM_Application_UnpushMethod 0x804211dd /* V20; the MUI 3.8 SDK (V19) lacks it, 3.8 ignores it */
#endif
#ifndef MUIM_ConnectParent
#define MUIM_ConnectParent 0x90421d00    /* Zune/OpenMUI Notify (MUIB_ZUNE|0x1d00); the MUI 3.8 SDK lacks it, 3.8 never sends it */
#endif
#ifndef MUIM_DisconnectParent
#define MUIM_DisconnectParent 0x90421d01 /* Zune/OpenMUI Notify (MUIB_ZUNE|0x1d01) */
#endif

#endif
