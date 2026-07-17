/* Minimal ffunicode.c for FatFs R0.16
   Our filenames use only ASCII (0x20-0x7E): STA2_YYMMDDHHMM.csv
   and folder names YYYYMMDD — no non-ASCII characters ever appear.
   Full CP437 tables not needed. */

#include "ff.h"

/* Unicode code point → OEM byte (CP437).
   For pure ASCII the mapping is identity. */
WCHAR ff_uni2oem(DWORD uni, WORD cp) {
    (void)cp;
    if (uni >= 0x20u && uni <= 0x7Eu) return (WCHAR)uni;
    return 0; /* un-representable → null */
}

/* OEM byte → Unicode code point.
   For pure ASCII the mapping is identity. */
WCHAR ff_oem2uni(WCHAR oem, WORD cp) {
    (void)cp;
    if (oem >= 0x20u && oem <= 0x7Eu) return (WCHAR)oem;
    return 0;
}

/* Unicode code point → uppercase.
   Handles A-Z and a-z only; enough for our filenames. */
DWORD ff_wtoupper(DWORD uni) {
    if (uni >= (DWORD)'a' && uni <= (DWORD)'z')
        return uni - 0x20u;
    return uni;
}
