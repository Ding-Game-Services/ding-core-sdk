/*
 * ding_md5.h — MD5 Hashing
 * Ding Game Services
 *
 * Provides MD5 hashing for ROM identification across all Ding cores.
 * Based on the algorithm described in RFC 1321. Original algorithm
 * is public domain.
 *
 * Usage — single buffer:
 *   u8 hash[16];
 *   ding_md5(rom_data, rom_size, hash);
 *
 * Usage — streaming (large or non-contiguous data):
 *   DingMD5Context ctx;
 *   ding_md5_init(&ctx);
 *   ding_md5_update(&ctx, chunk_a, size_a);
 *   ding_md5_update(&ctx, chunk_b, size_b);
 *   ding_md5_finish(&ctx, hash);
 *
 * Usage — hex string output:
 *   char hex[33];
 *   ding_md5_to_hex(hash, hex);  // "A1B2C3..." uppercase, null terminated
 *
 * Rules:
 *   - Valid C99 and C++
 *   - No platform-specific includes
 *   - No dynamic allocation
 */

#ifndef DING_MD5_H
#define DING_MD5_H

#include "ding_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* MD5 always produces 16 bytes */
#define DING_MD5_SIZE 16u

/* Hex string length including null terminator */
#define DING_MD5_HEX_LEN 33u

/* Internal block size in bytes */
#define DING_MD5_BLOCK_SIZE 64u


/* ═══════════════════════════════════════════════════════════════════════════
 * CONTEXT
 * Holds incremental state for streaming hashing.
 * Do not read or write fields directly.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    u32  state[4];                    /* ABCD digest registers                */
    u32  count[2];                    /* bit count, low and high word         */
    u8   buf[DING_MD5_BLOCK_SIZE];    /* input buffer for partial blocks      */
} DingMD5Context;


/* ═══════════════════════════════════════════════════════════════════════════
 * FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * ding_md5
 * Hash a single contiguous buffer in one call.
 * out must be exactly DING_MD5_SIZE (16) bytes.
 */
void ding_md5(const void* data, size_t size, u8 out[DING_MD5_SIZE]);

/*
 * ding_md5_init
 * Initialize a streaming MD5 context.
 */
void ding_md5_init(DingMD5Context* ctx);

/*
 * ding_md5_update
 * Feed data into an initialized context.
 * May be called any number of times with any chunk sizes.
 */
void ding_md5_update(DingMD5Context* ctx, const void* data, size_t size);

/*
 * ding_md5_finish
 * Finalize the hash and write 16 bytes into out.
 * The context is invalidated after this call — do not reuse it.
 * out must be exactly DING_MD5_SIZE (16) bytes.
 */
void ding_md5_finish(DingMD5Context* ctx, u8 out[DING_MD5_SIZE]);

/*
 * ding_md5_to_hex
 * Convert a 16-byte MD5 hash to a 32-character uppercase hex string.
 * hex must be at least DING_MD5_HEX_LEN (33) bytes — includes null terminator.
 * e.g. {0xA1, 0xB2, ...} -> "A1B2..."
 */
void ding_md5_to_hex(const u8 hash[DING_MD5_SIZE], char hex[DING_MD5_HEX_LEN]);

/*
 * ding_md5_compare
 * Compare two MD5 hashes.
 * Returns 1 if equal, 0 if not.
 */
u8 ding_md5_compare(const u8 a[DING_MD5_SIZE], const u8 b[DING_MD5_SIZE]);


#ifdef __cplusplus
}
#endif

#endif /* DING_MD5_H */
