/*
 * ding_audio.h — Shared Audio Ring Buffer
 * Ding Game Services
 *
 * Defines the audio ring buffer used by all Ding cores to deliver samples
 * to the frontend. The core writes samples after each frame. The frontend
 * reads them at its own pace via ding_core.h's ding_read_audio_samples().
 *
 * Neither side blocks the other. If the buffer fills up, new samples
 * overwrite the oldest ones. If the buffer runs dry, the frontend gets
 * silence. Both are preferable to stalling emulation.
 *
 * Samples are 32-bit floats in the range [-1.0, 1.0].
 * Channels are interleaved: [L, R, L, R, ...] for stereo.
 * One "sample frame" = one sample per channel.
 *
 * Rules:
 *   - Valid C99 and C++
 *   - No platform-specific includes
 *   - No dynamic allocation — buffer memory is caller-owned
 */

#ifndef DING_AUDIO_H
#define DING_AUDIO_H

#include "ding_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Default ring buffer capacity in sample frames.
 * At 44100hz stereo this is roughly 93ms of audio — enough headroom for
 * a frame of emulation without risking overflow under normal conditions.
 * Cores or frontends with different requirements can pass a custom size
 * to ding_audio_init().
 */
#define DING_AUDIO_DEFAULT_CAPACITY 4096u

/* Maximum supported channel count */
#define DING_AUDIO_MAX_CHANNELS 8u


/* ═══════════════════════════════════════════════════════════════════════════
 * STRUCT
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * DingAudioBuffer
 * The ring buffer instance. One per core.
 *
 * Do not read or write fields directly — use the functions below.
 * The buffer pointer and capacity are set once at init and never change.
 * read_pos and write_pos advance modulo (capacity * channels).
 */
typedef struct {
    float*   buffer;        /* caller-owned sample storage                    */
    u32      capacity;      /* total sample frames the buffer can hold        */
    u32      channels;      /* number of channels (1 = mono, 2 = stereo etc.) */
    u32      sample_rate;   /* samples per second, for frontend reference     */
    u32      read_pos;      /* next frame index to read  (in sample frames)   */
    u32      write_pos;     /* next frame index to write (in sample frames)   */
    u32      count;         /* number of sample frames currently available    */
} DingAudioBuffer;


/* ═══════════════════════════════════════════════════════════════════════════
 * FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * ding_audio_init
 * Initialize a DingAudioBuffer.
 *
 * buf         — the DingAudioBuffer to initialize
 * storage     — caller-owned float array; must be at least
 *               (capacity * channels) floats in size
 * capacity    — number of sample frames the buffer can hold;
 *               pass DING_AUDIO_DEFAULT_CAPACITY if unsure
 * channels    — number of audio channels (1 or 2 for most systems)
 * sample_rate — output sample rate in Hz (e.g. 44100)
 */
void ding_audio_init(DingAudioBuffer* buf,
                     float*           storage,
                     u32              capacity,
                     u32              channels,
                     u32              sample_rate);

/*
 * ding_audio_reset
 * Clear all pending samples and reset read/write positions.
 * Call on core reset or when switching games.
 */
void ding_audio_reset(DingAudioBuffer* buf);

/*
 * ding_audio_write
 * Write sample frames from src into the ring buffer.
 * src must contain (frame_count * channels) interleaved floats.
 * If the buffer is full, oldest samples are overwritten.
 * Returns the number of sample frames actually written.
 */
u32 ding_audio_write(DingAudioBuffer* buf,
                     const float*     src,
                     u32              frame_count);

/*
 * ding_audio_write_sample
 * Write a single sample frame (all channels at once).
 * samples must point to exactly buf->channels floats.
 * Convenience wrapper around ding_audio_write for per-sample synthesis loops.
 */
void ding_audio_write_sample(DingAudioBuffer* buf, const float* samples);

/*
 * ding_audio_read
 * Read up to frame_count sample frames into dst.
 * dst must be at least (frame_count * channels) floats in size.
 * If fewer frames are available, the remainder of dst is filled with silence.
 * Returns the number of sample frames actually read from the buffer
 * (not counting silence padding).
 */
u32 ding_audio_read(DingAudioBuffer* buf,
                    float*           dst,
                    u32              frame_count);

/*
 * ding_audio_available
 * Returns the number of sample frames currently available to read.
 */
u32 ding_audio_available(const DingAudioBuffer* buf);

/*
 * ding_audio_free_space
 * Returns the number of additional sample frames that can be written
 * before the buffer is full.
 */
u32 ding_audio_free_space(const DingAudioBuffer* buf);


#ifdef __cplusplus
}
#endif

#endif /* DING_AUDIO_H */
