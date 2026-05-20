/*
 * ding_md5.cpp — MD5 Hashing Implementation
 * Ding Game Services
 *
 * Based on the algorithm described in RFC 1321 (public domain).
 * No external dependencies.
 */

#include "ding_md5.h"
#include <string.h>  /* memcpy, memset */


/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL — RFC 1321 CONSTANTS AND HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Rotate x left by n bits */
#define MD5_ROL(x, n) (((x) << (n)) | ((x) >> (32u - (n))))

/* MD5 auxiliary functions (RFC 1321 section 3.4) */
#define MD5_F(b, c, d) (((b) & (c)) | (~(b) & (d)))
#define MD5_G(b, c, d) (((b) & (d)) | ((c) & ~(d)))
#define MD5_H(b, c, d) ((b) ^ (c) ^ (d))
#define MD5_I(b, c, d) ((c) ^ ((b) | ~(d)))

/* Per-round shift amounts (RFC 1321 section 3.4) */
static const u8 s_shifts[64] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
};

/*
 * Precomputed T table: T[i] = floor(abs(sin(i+1)) * 2^32)
 * (RFC 1321 section 3.4)
 */
static const u32 s_T[64] = {
    0xD76AA478u, 0xE8C7B756u, 0x242070DBu, 0xC1BDCEEEu,
    0xF57C0FAFu, 0x4787C62Au, 0xA8304613u, 0xFD469501u,
    0x698098D8u, 0x8B44F7AFu, 0xFFFF5BB1u, 0x895CD7BEu,
    0x6B901122u, 0xFD987193u, 0xA679438Eu, 0x49B40821u,
    0xF61E2562u, 0xC040B340u, 0x265E5A51u, 0xE9B6C7AAu,
    0xD62F105Du, 0x02441453u, 0xD8A1E681u, 0xE7D3FBC8u,
    0x21E1CDE6u, 0xC33707D6u, 0xF4D50D87u, 0x455A14EDu,
    0xA9E3E905u, 0xFCEFA3F8u, 0x676F02D9u, 0x8D2A4C8Au,
    0xFFFA3942u, 0x8771F681u, 0x6D9D6122u, 0xFDE5380Cu,
    0xA4BEEA44u, 0x4BDECFA9u, 0xF6BB4B60u, 0xBEBFBC70u,
    0x289B7EC6u, 0xEAA127FAu, 0xD4EF3085u, 0x04881D05u,
    0xD9D4D039u, 0xE6DB99E5u, 0x1FA27CF8u, 0xC4AC5665u,
    0xF4292244u, 0x432AFF97u, 0xAB9423A7u, 0xFC93A039u,
    0x655B59C3u, 0x8F0CCC92u, 0xFFEFF47Du, 0x85845DD1u,
    0x6FA87E4Fu, 0xFE2CE6E0u, 0xA3014314u, 0x4E0811A1u,
    0xF7537E82u, 0xBD3AF235u, 0x2AD7D2BBu, 0xEB86D391u
};


/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL — PROCESS ONE 64-BYTE BLOCK
 * ═══════════════════════════════════════════════════════════════════════════ */

static void md5_process_block(DingMD5Context* ctx, const u8* block) {
    /* Load block as 16 x u32 little-endian words */
    u32 M[16];
    for (u32 i = 0u; i < 16u; i++) {
        M[i] = ding_read_le32(block + i * 4u);
    }

    u32 a = ctx->state[0];
    u32 b = ctx->state[1];
    u32 c = ctx->state[2];
    u32 d = ctx->state[3];

    for (u32 i = 0u; i < 64u; i++) {
        u32 f, g;

        if (i < 16u) {
            f = MD5_F(b, c, d);
            g = i;
        } else if (i < 32u) {
            f = MD5_G(b, c, d);
            g = (5u * i + 1u) % 16u;
        } else if (i < 48u) {
            f = MD5_H(b, c, d);
            g = (3u * i + 5u) % 16u;
        } else {
            f = MD5_I(b, c, d);
            g = (7u * i) % 16u;
        }

        u32 temp = d;
        d = c;
        c = b;
        b = b + MD5_ROL(a + f + M[g] + s_T[i], s_shifts[i]);
        a = temp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
}


/* ═══════════════════════════════════════════════════════════════════════════
 * PUBLIC FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

void ding_md5_init(DingMD5Context* ctx) {
    if (!ctx) return;

    /* RFC 1321 section 3.3 — initial digest values */
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xEFCDAB89u;
    ctx->state[2] = 0x98BADCFEu;
    ctx->state[3] = 0x10325476u;
    ctx->count[0] = 0u;
    ctx->count[1] = 0u;
    memset(ctx->buf, 0, DING_MD5_BLOCK_SIZE);
}

void ding_md5_update(DingMD5Context* ctx, const void* data, size_t size) {
    if (!ctx || !data || size == 0u) return;

    const u8* src = (const u8*)data;

    /* How many bytes are already in the partial block buffer */
    u32 buf_used = (ctx->count[0] >> 3) & (DING_MD5_BLOCK_SIZE - 1u);

    /* Update bit count */
    u32 bits_low = ctx->count[0] + (u32)(size << 3);
    if (bits_low < ctx->count[0]) {
        ctx->count[1]++;  /* carry */
    }
    ctx->count[0]  = bits_low;
    ctx->count[1] += (u32)(size >> 29);

    u32 buf_space = DING_MD5_BLOCK_SIZE - buf_used;

    size_t offset = 0u;

    /* If we can fill and process the partial block, do so first */
    if (size >= buf_space) {
        memcpy(ctx->buf + buf_used, src, buf_space);
        md5_process_block(ctx, ctx->buf);
        offset   += buf_space;
        buf_used  = 0u;

        /* Process remaining full blocks directly from src */
        while (offset + DING_MD5_BLOCK_SIZE <= size) {
            md5_process_block(ctx, src + offset);
            offset += DING_MD5_BLOCK_SIZE;
        }
    }

    /* Buffer any remaining bytes */
    if (offset < size) {
        memcpy(ctx->buf + buf_used, src + offset, size - offset);
    }
}

void ding_md5_finish(DingMD5Context* ctx, u8 out[DING_MD5_SIZE]) {
    if (!ctx || !out) return;

    /* Padding: append 0x80 then zeros until 56 bytes mod 64 */
    static const u8 padding[DING_MD5_BLOCK_SIZE] = { 0x80u };

    /* Save bit count before padding changes it */
    u8 bit_count[8];
    ding_write_le32(bit_count,     ctx->count[0]);
    ding_write_le32(bit_count + 4, ctx->count[1]);

    /* Pad to 56 bytes mod 64 */
    u32 buf_used    = (ctx->count[0] >> 3) & (DING_MD5_BLOCK_SIZE - 1u);
    u32 pad_len     = (buf_used < 56u) ? (56u - buf_used) : (120u - buf_used);
    ding_md5_update(ctx, padding, pad_len);

    /* Append original bit count as 64-bit LE */
    ding_md5_update(ctx, bit_count, 8u);

    /* Write digest as little-endian u32s */
    for (u32 i = 0u; i < 4u; i++) {
        ding_write_le32(out + i * 4u, ctx->state[i]);
    }

    /* Scrub context */
    memset(ctx, 0, sizeof(DingMD5Context));
}

void ding_md5(const void* data, size_t size, u8 out[DING_MD5_SIZE]) {
    DingMD5Context ctx;
    ding_md5_init(&ctx);
    ding_md5_update(&ctx, data, size);
    ding_md5_finish(&ctx, out);
}

void ding_md5_to_hex(const u8 hash[DING_MD5_SIZE], char hex[DING_MD5_HEX_LEN]) {
    if (!hash || !hex) return;

    static const char digits[] = "0123456789ABCDEF";
    for (u32 i = 0u; i < DING_MD5_SIZE; i++) {
        hex[i * 2u]      = digits[(hash[i] >> 4) & 0x0Fu];
        hex[i * 2u + 1u] = digits[ hash[i]       & 0x0Fu];
    }
    hex[DING_MD5_HEX_LEN - 1u] = '\0';
}

u8 ding_md5_compare(const u8 a[DING_MD5_SIZE], const u8 b[DING_MD5_SIZE]) {
    if (!a || !b) return 0u;
    for (u32 i = 0u; i < DING_MD5_SIZE; i++) {
        if (a[i] != b[i]) return 0u;
    }
    return 1u;
}
