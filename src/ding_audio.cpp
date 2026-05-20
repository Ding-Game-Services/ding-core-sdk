/*
 * ding_audio.cpp — Shared Audio Ring Buffer Implementation
 * Ding Game Services
 */

#include "ding_audio.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Clamp channel count to a safe range */
static u32 clamp_channels(u32 channels) {
    if (channels < 1u) return 1u;
    if (channels > DING_AUDIO_MAX_CHANNELS) return DING_AUDIO_MAX_CHANNELS;
    return channels;
}


/* ═══════════════════════════════════════════════════════════════════════════
 * PUBLIC FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

void ding_audio_init(DingAudioBuffer* buf,
                     float*           storage,
                     u32              capacity,
                     u32              channels,
                     u32              sample_rate)
{
    if (!buf || !storage) return;

    buf->buffer      = storage;
    buf->capacity    = (capacity > 0u) ? capacity : DING_AUDIO_DEFAULT_CAPACITY;
    buf->channels    = clamp_channels(channels);
    buf->sample_rate = sample_rate;
    buf->read_pos    = 0u;
    buf->write_pos   = 0u;
    buf->count       = 0u;

    /* Zero the storage so any initial reads produce silence */
    u32 total = buf->capacity * buf->channels;
    for (u32 i = 0u; i < total; i++) {
        buf->buffer[i] = 0.0f;
    }
}

void ding_audio_reset(DingAudioBuffer* buf)
{
    if (!buf || !buf->buffer) return;

    buf->read_pos  = 0u;
    buf->write_pos = 0u;
    buf->count     = 0u;

    u32 total = buf->capacity * buf->channels;
    for (u32 i = 0u; i < total; i++) {
        buf->buffer[i] = 0.0f;
    }
}

u32 ding_audio_write(DingAudioBuffer* buf,
                     const float*     src,
                     u32              frame_count)
{
    if (!buf || !buf->buffer || !src || frame_count == 0u) return 0u;

    u32 written = 0u;

    for (u32 f = 0u; f < frame_count; f++) {
        /* If full, advance read_pos to overwrite the oldest frame */
        if (buf->count == buf->capacity) {
            buf->read_pos = (buf->read_pos + 1u) % buf->capacity;
        } else {
            buf->count++;
        }

        /* Write one frame (all channels) at write_pos */
        u32 base = buf->write_pos * buf->channels;
        for (u32 c = 0u; c < buf->channels; c++) {
            buf->buffer[base + c] = src[f * buf->channels + c];
        }

        buf->write_pos = (buf->write_pos + 1u) % buf->capacity;
        written++;
    }

    return written;
}

void ding_audio_write_sample(DingAudioBuffer* buf, const float* samples)
{
    if (!buf || !buf->buffer || !samples) return;
    ding_audio_write(buf, samples, 1u);
}

u32 ding_audio_read(DingAudioBuffer* buf,
                    float*           dst,
                    u32              frame_count)
{
    if (!buf || !buf->buffer || !dst || frame_count == 0u) return 0u;

    u32 frames_read = 0u;

    for (u32 f = 0u; f < frame_count; f++) {
        if (buf->count == 0u) {
            /* Buffer dry — fill remaining dst with silence */
            u32 remaining_samples = (frame_count - f) * buf->channels;
            u32 dst_base = f * buf->channels;
            for (u32 i = 0u; i < remaining_samples; i++) {
                dst[dst_base + i] = 0.0f;
            }
            break;
        }

        /* Copy one frame into dst */
        u32 src_base = buf->read_pos * buf->channels;
        u32 dst_base = f * buf->channels;
        for (u32 c = 0u; c < buf->channels; c++) {
            dst[dst_base + c] = buf->buffer[src_base + c];
        }

        buf->read_pos = (buf->read_pos + 1u) % buf->capacity;
        buf->count--;
        frames_read++;
    }

    return frames_read;
}

u32 ding_audio_available(const DingAudioBuffer* buf)
{
    if (!buf) return 0u;
    return buf->count;
}

u32 ding_audio_free_space(const DingAudioBuffer* buf)
{
    if (!buf) return 0u;
    return buf->capacity - buf->count;
}
