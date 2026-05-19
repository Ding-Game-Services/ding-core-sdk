/*
 * ding_types.h — Shared Primitive Types and Macros
 * Ding Game Services
 *
 * This header defines the fundamental vocabulary used across all Ding cores
 * and SDK utilities. It must be included before any other Ding header.
 *
 * Rules:
 *   - This header must be valid C (C99) as well as C++
 *   - No platform-specific includes
 *   - No function declarations — types and macros only
 *   - Every core and SDK utility includes this file, nothing else should
 *     redefine these types
 */

#ifndef DING_TYPES_H
#define DING_TYPES_H

#include <stdint.h>
#include <stddef.h>


/* ═══════════════════════════════════════════════════════════════════════════
 * INTEGER TYPE ALIASES
 * Shorthand for fixed-width types. Use these throughout all core code.
 * Never use int, long, short, or char for emulation values.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;

typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;


/* ═══════════════════════════════════════════════════════════════════════════
 * BOOLEAN
 * C99 does not guarantee a bool type without stdbool.h.
 * Use ding_bool throughout core code for consistency.
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifndef __cplusplus
  #include <stdbool.h>
#endif

typedef u8 ding_bool;
#define DING_TRUE  ((ding_bool)1)
#define DING_FALSE ((ding_bool)0)


/* ═══════════════════════════════════════════════════════════════════════════
 * BIT MANIPULATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Single bit mask — BIT(0) = 0x01, BIT(7) = 0x80 */
#define BIT(n)              (1u << (n))

/* Test if bit n is set in value x */
#define BIT_TEST(x, n)      (((x) >> (n)) & 1u)

/* Set bit n in value x */
#define BIT_SET(x, n)       ((x) |  (1u << (n)))

/* Clear bit n in value x */
#define BIT_CLEAR(x, n)     ((x) & ~(1u << (n)))

/* Toggle bit n in value x */
#define BIT_TOGGLE(x, n)    ((x) ^  (1u << (n)))

/* Produce a mask of n consecutive bits starting at bit 0 — MASK(4) = 0x0F */
#define MASK(n)             ((1u << (n)) - 1u)

/* Extract n bits from value x starting at bit position p */
#define BITS(x, p, n)       (((x) >> (p)) & MASK(n))


/* ═══════════════════════════════════════════════════════════════════════════
 * BYTE / WORD EXTRACTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/* High and low bytes of a 16-bit value */
#define HI_BYTE(x)          ((u8)(((u16)(x) >> 8) & 0xFF))
#define LO_BYTE(x)          ((u8)((u16)(x) & 0xFF))

/* High and low 16-bit words of a 32-bit value */
#define HI_WORD(x)          ((u16)(((u32)(x) >> 16) & 0xFFFF))
#define LO_WORD(x)          ((u16)((u32)(x) & 0xFFFF))

/* Build a 16-bit value from two bytes — hi byte first */
#define MAKE_U16(hi, lo)    ((u16)(((u16)(u8)(hi) << 8) | (u8)(lo)))

/* Build a 32-bit value from four bytes — most significant first */
#define MAKE_U32(b3, b2, b1, b0) \
    ((u32)(((u32)(u8)(b3) << 24) | \
           ((u32)(u8)(b2) << 16) | \
           ((u32)(u8)(b1) <<  8) | \
            (u32)(u8)(b0)))


/* ═══════════════════════════════════════════════════════════════════════════
 * ENDIANNESS HELPERS
 * Emulated systems use both big-endian (68000, PowerPC) and
 * little-endian (Z80, x86, ARM) byte ordering.
 * These read multi-byte values from a raw byte buffer regardless of
 * host platform endianness.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Read a big-endian u16 from a byte pointer */
static inline u16 ding_read_be16(const u8* p) {
    return (u16)((u16)p[0] << 8 | p[1]);
}

/* Read a big-endian u32 from a byte pointer */
static inline u32 ding_read_be32(const u8* p) {
    return (u32)((u32)p[0] << 24 | (u32)p[1] << 16 |
                 (u32)p[2] <<  8 | p[3]);
}

/* Read a little-endian u16 from a byte pointer */
static inline u16 ding_read_le16(const u8* p) {
    return (u16)((u16)p[1] << 8 | p[0]);
}

/* Read a little-endian u32 from a byte pointer */
static inline u32 ding_read_le32(const u8* p) {
    return (u32)((u32)p[3] << 24 | (u32)p[2] << 16 |
                 (u32)p[1] <<  8 | p[0]);
}

/* Write a big-endian u16 to a byte pointer */
static inline void ding_write_be16(u8* p, u16 v) {
    p[0] = (u8)(v >> 8);
    p[1] = (u8)(v);
}

/* Write a big-endian u32 to a byte pointer */
static inline void ding_write_be32(u8* p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >>  8);
    p[3] = (u8)(v);
}

/* Write a little-endian u16 to a byte pointer */
static inline void ding_write_le16(u8* p, u16 v) {
    p[0] = (u8)(v);
    p[1] = (u8)(v >> 8);
}

/* Write a little-endian u32 to a byte pointer */
static inline void ding_write_le32(u8* p, u32 v) {
    p[0] = (u8)(v);
    p[1] = (u8)(v >>  8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * CLAMPING AND RANGE
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DING_MIN(a, b)      ((a) < (b) ? (a) : (b))
#define DING_MAX(a, b)      ((a) > (b) ? (a) : (b))
#define DING_CLAMP(x, lo, hi) (DING_MIN(DING_MAX((x), (lo)), (hi)))


/* ═══════════════════════════════════════════════════════════════════════════
 * ARRAY SIZE
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DING_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))


/* ═══════════════════════════════════════════════════════════════════════════
 * UNUSED PARAMETER SUPPRESSION
 * Silences compiler warnings for intentionally unused parameters,
 * common in stub implementations and platform shims.
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DING_UNUSED(x) ((void)(x))


/* ═══════════════════════════════════════════════════════════════════════════
 * DISC AND ROM SIZE CONSTANTS
 * Reference values for allocation hints and validation.
 * These are not hard limits — cores may exceed them for unusual media.
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DING_SECTOR_SIZE_RAW     2352u   /* raw CD sector including sync/header */
#define DING_SECTOR_SIZE_DATA    2048u   /* ISO 9660 data payload per sector    */
#define DING_SECTOR_SIZE_AUDIO   2352u   /* CDDA audio sector                   */

#define DING_ROM_SIZE_MAX_8BIT   (512u  * 1024u)          /* 512 KB  */
#define DING_ROM_SIZE_MAX_16BIT  (8u    * 1024u * 1024u)  /* 8 MB    */
#define DING_ROM_SIZE_MAX_32BIT  (128u  * 1024u * 1024u)  /* 128 MB  */


#endif /* DING_TYPES_H */
