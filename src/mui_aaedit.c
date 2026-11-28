/* Shared parts of the UP-Field and UP-TextArea own editors; see mui_aaedit.h. Moved out of mui_aatextarea_class.c (2026-11-25) so the
 * field reuses them instead of a copy. */
#include "mui_aaedit.h"

#include <string.h>

#include <devices/clipboard.h>
#include <devices/inputevent.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include "mui_gfxbase.h"
#define KeymapBase up_mcc_aaedit_KeymapBase /* own base symbol: the library is opened per use, not by libnix */
#include <proto/keymap.h>

#include "mui_ftxt.h"

struct Library* KeymapBase;

int aaedit_map_key(const struct IntuiMessage* im) {
    struct InputEvent ie;
    UBYTE c[4];
    int n;
    if (im->Class != IDCMP_RAWKEY || (im->Code & IECODE_UP_PREFIX)) {
        return -1;
    }
    if ((KeymapBase = OpenLibrary((CONST_STRPTR) "keymap.library", 37)) == NULL) {
        return -1;
    }
    memset(&ie, 0, sizeof ie);
    ie.ie_Class = IECLASS_RAWKEY, ie.ie_Code = im->Code;
    ie.ie_Qualifier = im->Qualifier & ~(IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND);
    ie.ie_EventAddress = (APTR)*((ULONG*)im->IAddress); /* dead-key context, as Intuition and betterstring pass it */
    n = (int)MapRawKey(&ie, (STRPTR)c, sizeof c, NULL);
    CloseLibrary(KeymapBase);
    KeymapBase = NULL;
    return n == 1 ? (int)c[0] : -1;
}

/* One request on a private port per call. */
static struct IOClipReq* aaedit_clip_open(struct MsgPort** port) {
    struct IOClipReq* io;
    *port = CreateMsgPort();
    if (*port == NULL) {
        return NULL;
    }
    io = (struct IOClipReq*)CreateIORequest(*port, sizeof(struct IOClipReq));
    if (io != NULL && OpenDevice((STRPTR) "clipboard.device", 0, (struct IORequest*)io, 0) != 0) {
        DeleteIORequest((struct IORequest*)io);
        io = NULL;
    }
    if (io == NULL) {
        DeleteMsgPort(*port);
    }
    return io;
}

static void aaedit_clip_close(struct IOClipReq* io, struct MsgPort* port) {
    CloseDevice((struct IORequest*)io);
    DeleteIORequest((struct IORequest*)io);
    DeleteMsgPort(port);
}

void aaedit_clip_put(const char* s, int n) {
    struct MsgPort* port;
    struct IOClipReq* io;
    unsigned char* b;
    int total = ftxt_size(n);
    if (n <= 0 || (b = (unsigned char*)AllocVec((ULONG)total, MEMF_ANY)) == NULL) {
        return;
    }
    ftxt_encode(s, n, b, total);
    io = aaedit_clip_open(&port);
    if (io != NULL) {
        io->io_Data = (STRPTR)b;
        io->io_Length = (ULONG)total;
        io->io_Offset = 0;
        io->io_ClipID = 0;
        io->io_Command = CMD_WRITE;
        DoIO((struct IORequest*)io);
        io->io_Command = CMD_UPDATE;
        DoIO((struct IORequest*)io);
        aaedit_clip_close(io, port);
    }
    FreeVec(b);
}

/* At most cap*2+64 raw bytes are kept; the rest is read and thrown away because a clip read must run to its end to be released. */
int aaedit_clip_get(char* out, int cap) {
    struct MsgPort* port;
    struct IOClipReq* io;
    unsigned char *b, chunk[256];
    int raw = cap * 2 + 64, have = 0, n = 0;
    if (cap < 2 || (b = (unsigned char*)AllocVec((ULONG)raw, MEMF_ANY)) == NULL) {
        return 0;
    }
    io = aaedit_clip_open(&port);
    if (io != NULL) {
        io->io_Offset = 0;
        io->io_ClipID = 0;
        for (;;) {
            io->io_Command = CMD_READ;
            io->io_Data = (STRPTR)chunk;
            io->io_Length = sizeof chunk;
            if (DoIO((struct IORequest*)io) != 0 || io->io_Actual == 0) {
                break;
            }
            if (have < raw) {
                int take = (int)io->io_Actual < raw - have ? (int)io->io_Actual : raw - have;
                memcpy(b + have, chunk, (size_t)take);
                have += take;
            }
        }
        aaedit_clip_close(io, port);
        n = ftxt_decode(b, have, out, cap);
    }
    FreeVec(b);
    return n < 0 ? 0 : n;
}

/* Each pixel once before the text: erasing the box, filling it in the outline colour and then in the face made every keystroke
 * flash the parent and ring colours across the whole box. The ring leaves the face as a cross of two holes, so its rounded
 * corners still sit on the ring. */
void aaedit_frame(aa_ctx* aa, struct RastPort* rp, int l, int t, int w, int h, const mui_aa_check_op_t* ops, int n) {
    int k;
    if (n > 0) {
        aa_round_corners(aa, rp, l, t, w, h, ops[0].radius, aa->bg_rgb);
    }
    for (k = 0; k < n; k++) {
        int x = l + ops[k].inset, y = t + ops[k].inset, bw = w - 2 * ops[k].inset, bh = h - 2 * ops[k].inset;
        if (k + 1 < n) {
            int fx = l + ops[k + 1].inset, fy = t + ops[k + 1].inset, fw = w - 2 * ops[k + 1].inset, fh = h - 2 * ops[k + 1].inset, r = ops[k + 1].radius;
            int holes[8];
            holes[0] = fx, holes[1] = fy + r, holes[2] = fw, holes[3] = fh - 2 * r;
            holes[4] = fx + r, holes[5] = fy, holes[6] = fw - 2 * r, holes[7] = fh;
            aa_round_fill_holes(aa, rp, x, y, bw, bh, ops[k].radius, ops[k].rgb, holes, 2);
        } else {
            aa_round_fill(aa, rp, x, y, bw, bh, ops[k].radius, ops[k].rgb);
        }
    }
    aa_low_outline(aa, rp, l, t, w, h, n > 0 ? ops[0].radius : 0);
}
