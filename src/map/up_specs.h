#ifndef UP_SPECS_H
#define UP_SPECS_H
/* MUI gap E: the MUI image and frame spec strings, pure (host test up-specs). Used by UP-Imagedisplay/Popimage/Imageadjust and
 * UP-Framedisplay/Popframe/Frameadjust.
 * Image spec "<d>:<value>", d one digit: 0 pattern, 1 vector image, 2 pen spec (rRRRRRRRR,GGGGGGGG,BBBBBBBB / pN / mN), 3 image class,
 * 4 brush file, 5 picture file, 6 preset (MUII_ number), 7 gradient.
 * Frame spec "TSLRTB": T frame type, S state (0 normal, 1 recessed), then the inner spacing left, right, top, bottom; one hex digit each. */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UP_IMSPEC_LEN 64 /* struct MUI_ImageSpec buffer */
#define UP_FRSPEC_LEN 8  /* struct MUI_FrameSpec buffer */

/* Kind digit 0..7, or -1 when spec is not "<d>:". *tail = the value after the colon. *rgb = 0xRRGGBB of an "2:r..." pen spec
 * (top byte of each 32-bit gun) with *has_rgb 1; otherwise *has_rgb 0 and *rgb untouched. */
int up_imspec_parse(const char* spec, const char** tail, uint32_t* rgb, int* has_rgb);
/* 1 when muimaster's MUIC_Image draws this kind (0 pattern, 1 vector, 3 class, 4 brush, 5 picture, 6 preset), 0 for 2 pen, 7 gradient
 * (not in MUI 3.8) and bad specs, which UP-Imagedisplay fills itself. */
int up_imspec_by_mui(int kind);
/* "<d>:<value>" into out (UP_IMSPEC_LEN), cut to fit, NUL terminated. */
void up_imspec_format(int kind, const char* value, char* out);
/* Frame spec digits into v[6] (type, state, left, right, top, bottom); missing or bad digits read 0. */
void up_frspec_parse(const char* spec, int v[6]);
/* v[6] (each clamped to 0..15, state to 0..1) into out (UP_FRSPEC_LEN). */
void up_frspec_format(const int v[6], char* out);

#ifdef __cplusplus
}
#endif
#endif
