/* FatFs R0.16 configuration for STM32F103 */
#ifndef FFCONF_H
#define FFCONF_H

#define FFCONF_DEF  80386   /* Must match FF_DEFINED in ff.h */

/* --- Function / buffer ----------------------------------------------- */
#define FF_FS_READONLY    0
#define FF_FS_MINIMIZE    0
#define FF_USE_STRFUNC    0
#define FF_USE_FIND       0
#define FF_USE_MKFS       0
#define FF_USE_FASTSEEK   0
#define FF_USE_EXPAND     0
#define FF_USE_CHMOD      0
#define FF_USE_LABEL      0
#define FF_USE_FORWARD    0
#define FF_USE_TRIM       0

/* --- Code page -------------------------------------------------------- */
#define FF_CODE_PAGE      437   /* US/English ASCII */

/* --- Long File Name --------------------------------------------------- */
/* LFN required: "STA2_2601071606.csv" = 15-char name, not 8.3 */
#define FF_USE_LFN        2     /* LFN with static working buffer */
#define FF_MAX_LFN        32
#define FF_LFN_UNICODE    0     /* 0=ANSI/OEM */
#define FF_LFN_BUF        32
#define FF_SFN_BUF        12

/* --- Drive / volume --------------------------------------------------- */
#define FF_FS_RPATH       0
#define FF_VOLUMES        1
#define FF_STR_VOLUME_ID  0
#define FF_VOLUME_STRS    "SD"
#define FF_MULTI_PARTITION  0
#define FF_MIN_SS         512
#define FF_MAX_SS         512
#define FF_LBA64          0
#define FF_FS_EXFAT       0

/* --- System ----------------------------------------------------------- */
#define FF_FS_NORTC       1     /* No RTC — use fixed timestamp */
#define FF_NORTC_MON      1
#define FF_NORTC_MDAY     1
#define FF_NORTC_YEAR     2026
#define FF_FS_NOFSINFO    3   /* skip FSINFO read/write — less risk of corruption */
#define FF_FS_LOCK        0
#define FF_FS_REENTRANT   0

#endif /* FFCONF_H */
