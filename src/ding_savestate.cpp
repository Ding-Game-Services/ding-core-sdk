/*
 * ding_savestate.cpp — Shared Save State Serialization Implementation
 * Ding Game Services
 */

#include "ding_savestate.h"
#include <string.h>  /* memcpy, memset, strncpy, strncmp */


/* ═══════════════════════════════════════════════════════════════════════════
 * CRC32
 * Standard CRC32 using the IEEE 802.3 polynomial (0xEDB88320).
 * Table is generated once on first use.
 * ═══════════════════════════════════════════════════════════════════════════ */

static u32 s_crc_table[256];
static u8  s_crc_table_ready = 0;

static void crc32_build_table(void) {
    for (u32 i = 0u; i < 256u; i++) {
        u32 c = i;
        for (u32 j = 0u; j < 8u; j++) {
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        s_crc_table[i] = c;
    }
    s_crc_table_ready = 1;
}

u32 ding_crc32(u32 crc, const void* data, size_t size) {
    if (!s_crc_table_ready) crc32_build_table();
    if (!data || size == 0u) return crc;

    const u8* p = (const u8*)data;
    crc = ~crc;
    for (size_t i = 0u; i < size; i++) {
        crc = s_crc_table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
    }
    return ~crc;
}


/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Write bytes into writer buffer, advancing pos. Returns 0 on overflow. */
static u8 writer_push(DingSaveWriter* w, const void* src, size_t size) {
    if (!w->valid) return 0;
    if (w->pos + size > w->buf_size) {
        w->valid = 0;
        return 0;
    }
    memcpy(w->buf + w->pos, src, size);
    w->pos += size;
    return 1;
}

/* Write a u32 little-endian into the writer buffer */
static u8 writer_push_u32(DingSaveWriter* w, u32 val) {
    u8 tmp[4];
    ding_write_le32(tmp, val);
    return writer_push(w, tmp, 4u);
}

/* Read a u32 little-endian from a byte buffer at offset */
static u32 read_u32_le(const u8* buf, size_t offset) {
    return ding_read_le32(buf + offset);
}


/* ═══════════════════════════════════════════════════════════════════════════
 * WRITER IMPLEMENTATION
 * ═══════════════════════════════════════════════════════════════════════════ */

DingSaveResult ding_save_writer_init(DingSaveWriter* w,
                                     u8*             buf,
                                     size_t          buf_size,
                                     const char*     platform)
{
    if (!w || !buf || !platform) return DING_SS_ERR_NULL;

    memset(w, 0, sizeof(DingSaveWriter));
    w->buf      = buf;
    w->buf_size = buf_size;
    w->valid    = 1;

    strncpy(w->platform, platform, DING_SAVE_PLATFORM_LEN - 1u);
    w->platform[DING_SAVE_PLATFORM_LEN - 1u] = '\0';

    /* Reserve space for the file header — filled in by finish() */
    w->pos = sizeof(DingSaveHeader);
    if (w->pos > buf_size) {
        w->valid = 0;
        return DING_SS_ERR_OVERFLOW;
    }

    /* Zero the header region for now */
    memset(buf, 0, sizeof(DingSaveHeader));

    return DING_SS_OK;
}

DingSaveResult ding_save_write_block(DingSaveWriter* w,
                                     const char*     name,
                                     const void*     data,
                                     size_t          size)
{
    if (!w || !name || !data) return DING_SS_ERR_NULL;
    if (!w->valid)            return DING_SS_ERR_OVERFLOW;

    /* Write block header */
    DingSaveBlockHeader bh;
    memset(&bh, 0, sizeof(bh));
    bh.block_id  = w->block_count;
    bh.data_size = (u32)size;
    strncpy(bh.name, name, DING_SAVE_BLOCK_NAME_LEN - 1u);
    bh.name[DING_SAVE_BLOCK_NAME_LEN - 1u] = '\0';

    /* Write block_id as u32 LE */
    if (!writer_push_u32(w, bh.block_id))  return DING_SS_ERR_OVERFLOW;
    /* Write name field */
    if (!writer_push(w, bh.name, DING_SAVE_BLOCK_NAME_LEN)) return DING_SS_ERR_OVERFLOW;
    /* Write data_size as u32 LE */
    if (!writer_push_u32(w, bh.data_size)) return DING_SS_ERR_OVERFLOW;

    /* Write block data */
    if (!writer_push(w, data, size)) return DING_SS_ERR_OVERFLOW;

    w->block_count++;
    return DING_SS_OK;
}

DingSaveResult ding_save_writer_finish(DingSaveWriter* w, size_t* out_size) {
    if (!w || !out_size) return DING_SS_ERR_NULL;
    if (!w->valid)       return DING_SS_ERR_OVERFLOW;

    /* Write end marker */
    if (!writer_push_u32(w, DING_BLOCK_END)) return DING_SS_ERR_OVERFLOW;

    /* Compute CRC32 over everything after the header */
    size_t data_start = sizeof(DingSaveHeader);
    size_t data_size  = w->pos - data_start;
    u32 checksum = ding_crc32(0u, w->buf + data_start, data_size);

    /* Fill in the file header */
    DingSaveHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic         = DING_SAVE_MAGIC;
    hdr.version_major = (u8)DING_SAVE_VERSION_MAJOR;
    hdr.version_minor = (u8)DING_SAVE_VERSION_MINOR;
    hdr.block_count   = w->block_count;
    hdr.total_size    = (u32)w->pos;
    hdr.checksum      = checksum;
    strncpy(hdr.platform, w->platform, DING_SAVE_PLATFORM_LEN - 1u);
    hdr.platform[DING_SAVE_PLATFORM_LEN - 1u] = '\0';

    /* Write header fields manually as LE bytes */
    u8* h = w->buf;
    ding_write_le32(h,      hdr.magic);
    h[4] = hdr.version_major;
    h[5] = hdr.version_minor;
    h[6] = 0u;  /* reserved */
    h[7] = 0u;  /* reserved */
    memcpy(h + 8,  hdr.platform, DING_SAVE_PLATFORM_LEN);
    ding_write_le32(h + 40, hdr.block_count);
    ding_write_le32(h + 44, hdr.total_size);
    ding_write_le32(h + 48, hdr.checksum);

    *out_size = w->pos;
    return DING_SS_OK;
}


/* ═══════════════════════════════════════════════════════════════════════════
 * READER IMPLEMENTATION
 * ═══════════════════════════════════════════════════════════════════════════ */

DingSaveResult ding_save_reader_init(DingSaveReader* r,
                                     const u8*       buf,
                                     size_t          buf_size)
{
    if (!r || !buf) return DING_SS_ERR_NULL;

    memset(r, 0, sizeof(DingSaveReader));

    if (buf_size < sizeof(DingSaveHeader)) return DING_SS_ERR_TRUNCATED;

    /* Validate magic */
    u32 magic = read_u32_le(buf, 0u);
    if (magic != DING_SAVE_MAGIC) return DING_SS_ERR_BAD_MAGIC;

    /* Validate version — major must match exactly */
    u8 ver_major = buf[4];
    if (ver_major != (u8)DING_SAVE_VERSION_MAJOR) return DING_SS_ERR_BAD_VERSION;

    /* Read header fields */
    u32 block_count = read_u32_le(buf, 40u);
    u32 total_size  = read_u32_le(buf, 44u);
    u32 stored_crc  = read_u32_le(buf, 48u);

    if (total_size > buf_size) return DING_SS_ERR_TRUNCATED;

    /* Validate checksum over all data after the header */
    size_t data_start = sizeof(DingSaveHeader);
    size_t data_size  = total_size - data_start;
    u32 computed_crc  = ding_crc32(0u, buf + data_start, data_size);
    if (computed_crc != stored_crc) return DING_SS_ERR_BAD_CHECKSUM;

    r->buf         = buf;
    r->buf_size    = total_size;
    r->pos         = data_start;
    r->block_count = block_count;
    r->valid       = 1;

    return DING_SS_OK;
}

DingSaveResult ding_save_read_block(DingSaveReader* r,
                                    const char*     name,
                                    void*           dst,
                                    size_t          dst_size,
                                    size_t*         out_size)
{
    if (!r || !name || !dst) return DING_SS_ERR_NULL;
    if (!r->valid)           return DING_SS_ERR_BAD_MAGIC;

    /* Scan blocks from the start of the data section each time.
     * Simple linear search — block counts are small for 8/16-bit systems. */
    size_t scan = sizeof(DingSaveHeader);

    while (scan + 4u + DING_SAVE_BLOCK_NAME_LEN + 4u <= r->buf_size) {
        u32 block_id = read_u32_le(r->buf, scan);

        /* End marker — block not found, leave dst untouched */
        if (block_id == DING_BLOCK_END) break;

        const char* block_name = (const char*)(r->buf + scan + 4u);
        u32 data_size = read_u32_le(r->buf, scan + 4u + DING_SAVE_BLOCK_NAME_LEN);

        size_t data_offset = scan + 4u + DING_SAVE_BLOCK_NAME_LEN + 4u;

        if (strncmp(block_name, name, DING_SAVE_BLOCK_NAME_LEN) == 0) {
            /* Found — copy into dst */
            size_t copy_size = (data_size < dst_size) ? data_size : dst_size;
            memcpy(dst, r->buf + data_offset, copy_size);
            if (out_size) *out_size = data_size;
            return DING_SS_OK;
        }

        /* Advance past this block */
        scan = data_offset + data_size;
    }

    /* Block not found — not an error, just return with dst untouched */
    if (out_size) *out_size = 0u;
    return DING_SS_OK;
}
