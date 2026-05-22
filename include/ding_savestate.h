/*
 * ding_savestate.h — Shared Save State Serialization
 * Ding Game Services
 *
 * Defines the .ding binary save state format and the reader/writer
 * utilities all cores use to serialize and restore emulator state.
 *
 * The .ding format is a simple sequential binary container:
 *
 *   [ DingSaveHeader ]
 *   [ block 0 header ][ block 0 data ]
 *   [ block 1 header ][ block 1 data ]
 *   ...
 *   [ DING_BLOCK_END marker ]
 *
 * Each block represents one named region of state (CPU registers, WRAM,
 * VRAM, APU state, etc.). Cores write blocks in any order. The reader
 * locates blocks by name so forward/backward compatibility is possible —
 * unknown blocks are skipped, missing blocks leave state untouched.
 *
 * Rules:
 *   - Valid C99 and C++
 *   - No platform-specific includes
 *   - No dynamic allocation — all buffers are caller-owned
 *   - All multi-byte values stored little-endian in the file
 */

#ifndef DING_SAVESTATE_H
#define DING_SAVESTATE_H

#include "ding_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/* ═══════════════════════════════════════════════════════════════════════════
 * FORMAT CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Magic number — first 4 bytes of every .ding file: ASCII "DING" */
#define DING_SAVE_MAGIC         0x44494E47u

/* Current format version — increment minor on additive changes,
   major on breaking changes */
#define DING_SAVE_VERSION_MAJOR 1u
#define DING_SAVE_VERSION_MINOR 0u

/* Maximum length of a block name including null terminator */
#define DING_SAVE_BLOCK_NAME_LEN 32u

/* Maximum length of the platform name string in the file header */
#define DING_SAVE_PLATFORM_LEN   32u

/* Sentinel block ID written at the end of a save state */
#define DING_BLOCK_END          0xFFFFFFFFu


/* ═══════════════════════════════════════════════════════════════════════════
 * RETURN CODES
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    DING_SS_OK               =  0,
    DING_SS_ERR_BAD_MAGIC    = -1,  /* not a .ding file                       */
    DING_SS_ERR_BAD_VERSION  = -2,  /* incompatible format version            */
    DING_SS_ERR_BAD_CHECKSUM = -3,  /* data corruption detected               */
    DING_SS_ERR_OVERFLOW     = -4,  /* write would exceed buffer bounds       */
    DING_SS_ERR_TRUNCATED    = -5,  /* file ended before DING_BLOCK_END       */
    DING_SS_ERR_NULL         = -6,  /* null pointer passed                    */
} DingSaveResult;


/* ═══════════════════════════════════════════════════════════════════════════
 * FILE LAYOUT STRUCTS
 * These map directly onto the binary file — field order and sizes matter.
 * All multi-byte fields are little-endian.
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * DingSaveHeader
 * The first bytes of every .ding file.
 * Total size: 56 bytes.
 */
typedef struct {
    u32  magic;                              /* DING_SAVE_MAGIC                */
    u8   version_major;                      /* DING_SAVE_VERSION_MAJOR        */
    u8   version_minor;                      /* DING_SAVE_VERSION_MINOR        */
    u8   _reserved[2];                       /* padding — must be zero         */
    char platform[DING_SAVE_PLATFORM_LEN];   /* e.g. "Sega Genesis"            */
    u32  block_count;                        /* number of data blocks          */
    u32  total_size;                         /* total file size in bytes       */
u32  checksum;                           /* CRC32 of all data after header */
    u8   rom_md5[16];                        /* MD5 of the loaded ROM — used to verify
                                               this save state matches the current game */
} DingSaveHeader;

/*
 * DingSaveBlockHeader
 * Precedes each block's data in the file.
 * Total size: 40 bytes.
 */
typedef struct {
    u32  block_id;                           /* sequential index, 0-based      */
    char name[DING_SAVE_BLOCK_NAME_LEN];     /* e.g. "CPU", "WRAM", "VDP"      */
    u32  data_size;                          /* size of block data in bytes    */
} DingSaveBlockHeader;


/* ═══════════════════════════════════════════════════════════════════════════
 * WRITER
 * Cores use the writer to serialize state into a caller-owned buffer.
 * Usage:
 *   1. ding_save_writer_init()
 *   2. ding_save_write_block() for each region of state
 *   3. ding_save_writer_finish() to seal the file and get final size
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct DingSaveWriter {
    u8*    buf;
    size_t buf_size;
    size_t pos;
    u32    block_count;
    char   platform[DING_SAVE_PLATFORM_LEN];
    u8     valid;
} DingSaveWriter;

/*
 * ding_save_writer_init
 * Initialize a writer targeting buf.
 * platform should match DingCoreInfo.platform_name (e.g. "Sega Genesis").
 * buf must remain valid until ding_save_writer_finish() returns.
 */
DingSaveResult ding_save_writer_init(DingSaveWriter* w,
                                     u8*             buf,
                                     size_t          buf_size,
                                     const char*     platform);

/*
 * ding_save_write_block
 * Serialize one named block of state into the buffer.
 * name   — short identifier, max DING_SAVE_BLOCK_NAME_LEN-1 chars
 *          e.g. "CPU", "WRAM", "VDP_REGS", "APU"
 * data   — pointer to the state data to serialize
 * size   — size of data in bytes
 *
 * If a previous write caused an overflow, this becomes a no-op.
 */
DingSaveResult ding_save_write_block(DingSaveWriter* w,
                                     const char*     name,
                                     const void*     data,
                                     size_t          size);

/*
 * ding_save_writer_finish
 * Write the end marker, finalize the header (checksum, block count,
 * total size), and seal the buffer.
 * out_size is set to the total number of bytes written.
 * Returns DING_SS_OK on success.
 */
DingSaveResult ding_save_writer_finish(DingSaveWriter* w, size_t* out_size);


/* ═══════════════════════════════════════════════════════════════════════════
 * READER
 * Cores use the reader to restore state from a .ding buffer.
 * Usage:
 *   1. ding_save_reader_init() — validates header and checksum
 *   2. ding_save_read_block()  — call for each block you want to restore;
 *                                blocks not found in the file are skipped
 *                                gracefully
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const u8* buf;        /* input buffer                                     */
    size_t    buf_size;   /* total size of buf                                */
    size_t    pos;        /* current read position                            */
    u32       block_count;
    u8        valid;      /* 0 if header validation failed                    */
} DingSaveReader;

/*
 * ding_save_reader_init
 * Initialize a reader from a .ding buffer.
 * Validates magic, version, and checksum before returning.
 * Returns DING_SS_OK if the buffer is a valid .ding file.
 */
DingSaveResult ding_save_reader_init(DingSaveReader* r,
                                     const u8*       buf,
                                     size_t          buf_size);

/*
 * ding_save_read_block
 * Find a block by name and copy its data into dst.
 * dst must be at least dst_size bytes.
 * If the block is not found, dst is left untouched and DING_SS_OK is returned
 * — missing blocks are not an error (forward compatibility).
 * If the block is larger than dst_size, only dst_size bytes are copied.
 * out_size (optional) is set to the actual block data size in the file.
 */
DingSaveResult ding_save_read_block(DingSaveReader* r,
                                    const char*     name,
                                    void*           dst,
                                    size_t          dst_size,
                                    size_t*         out_size);


/* ═══════════════════════════════════════════════════════════════════════════
 * CHECKSUM
 * CRC32 used for file integrity checks.
 * Also available to cores that want to checksum individual blocks.
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * ding_crc32
 * Compute CRC32 of data. Call with crc=0 to start a new checksum.
 * Chain calls for non-contiguous data:
 *   u32 crc = ding_crc32(0, block_a, size_a);
 *   crc     = ding_crc32(crc, block_b, size_b);
 */
u32 ding_crc32(u32 crc, const void* data, size_t size);


#ifdef __cplusplus
}
#endif

#endif /* DING_SAVESTATE_H */
