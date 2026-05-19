/*
 * ding_core.h — Universal Emulator Core API
 * Ding Game Services
 *
 * This header defines the contract every Ding emulator core must implement.
 * It is the sole interface between a core and all consumers: Hydra, Cockpit,
 * Ding Engine, and the headless test harness.
 *
 * Rules:
 *   - This header must be valid C (C99) as well as C++
 *   - No platform-specific includes or types
 *   - Fixed-width integer types only (stdint.h)
 *   - All function exports use C linkage
 *
 * Targets:
 *   - Native Windows (.dll)
 *   - Native Linux   (.so)
 *   - WebAssembly    (.wasm via Emscripten)
 */

#ifndef DING_CORE_H
#define DING_CORE_H

#include "ding_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/* ═══════════════════════════════════════════════════════════════════════════
 * VERSION
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DING_CORE_API_VERSION_MAJOR 1
#define DING_CORE_API_VERSION_MINOR 0
#define DING_CORE_API_VERSION_PATCH 0


/* ═══════════════════════════════════════════════════════════════════════════
 * ENUMERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Pixel format of the framebuffer the core outputs */
typedef enum {
    DING_PIXFMT_RGBA8   = 0,   /* 8 bits per channel, RGBA — most common     */
    DING_PIXFMT_RGB565  = 1,   /* 16-bit packed, used by some older systems   */
    DING_PIXFMT_XRGB8   = 2,   /* 32-bit, alpha ignored                       */
    DING_PIXFMT_YUV420  = 3,   /* planar YUV, for FMV-heavy systems           */
} DingPixelFormat;

/* How the core exposes memory regions to consumers */
typedef enum {
    DING_MEM_DIRECT  = 0,      /* ptr field is valid — fast direct access     */
    DING_MEM_MANAGED = 1,      /* use read8/write8 functions instead          */
    DING_MEM_OPAQUE  = 2,      /* not accessible (e.g. GPU-internal memory)   */
} DingMemoryAccess;

/* Input descriptor types */
typedef enum {
    DING_INPUT_BUTTON = 0,     /* digital on/off                              */
    DING_INPUT_AXIS   = 1,     /* analog axis, -32768 to 32767                */
    DING_INPUT_MOTION = 2,     /* motion/gyro sensor                          */
} DingInputType;

/* Save state serialization method */
typedef enum {
    DING_SAVE_UNSUPPORTED  = 0, /* core does not support save states          */
    DING_SAVE_FULL         = 1, /* full state serialized to blob (8/16-bit)   */
    DING_SAVE_DELTA        = 2, /* delta encoding (future, larger systems)    */
    DING_SAVE_CHECKPOINT   = 3, /* core manages its own state internally      */
} DingSaveMethod;

/* ROM / disc identity method
 * All methods produce an MD5 hash — the difference is what bytes are fed in.
 * FULL:    MD5 of the entire ROM file (most cartridge systems)
 * STRIPPED: MD5 after removing a known copier/format header (NES, SNES, PCE, Lynx etc.)
 * CUSTOM:  MD5 of platform-specific extracted content (disc systems, NDS, GameCube etc.)
 *          The exact extraction recipe is implemented per-core. */
typedef enum {
    DING_ID_MD5_FULL     = 0,
    DING_ID_MD5_STRIPPED = 1,
    DING_ID_MD5_CUSTOM   = 2,
} DingIdentityMethod;

/* Disc image container format */
typedef enum {
    DING_DISC_BINCUE = 0,      /* BIN + CUE sheet                            */
    DING_DISC_ISO    = 1,      /* single ISO image                           */
    DING_DISC_CHD    = 2,      /* MAME compressed disc (CHD)                 */
    DING_DISC_XISO   = 3,      /* Xbox ISO format                            */
} DingDiscFormat;

/* Return codes */
typedef enum {
    DING_OK               =  0,
    DING_ERR_GENERIC      = -1,
    DING_ERR_BAD_ROM      = -2, /* ROM failed validation                      */
    DING_ERR_BIOS_MISSING = -3, /* required BIOS not supplied                 */
    DING_ERR_BIOS_INVALID = -4, /* BIOS supplied but MD5 mismatch             */
    DING_ERR_BAD_STATE    = -5, /* save state data is corrupt or incompatible */
    DING_ERR_NO_DISC      = -6, /* disc operation attempted with no disc      */
} DingResult;


/* ═══════════════════════════════════════════════════════════════════════════
 * STRUCTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * DingCoreInfo
 * Returned by ding_get_core_info(). Describes the core itself.
 */
typedef struct {
    const char* core_name;         /* "Genesis Core"                          */
    const char* platform_name;     /* "Sega Genesis / Mega Drive"             */
    const char* version;           /* "1.0.0"                                 */
    uint32_t    api_version_major; /* must match DING_CORE_API_VERSION_MAJOR  */
    uint32_t    api_version_minor;
} DingCoreInfo;

/*
 * DingVideoInfo
 * Returned by ding_get_video_info(). Describes framebuffer properties.
 * Frontend allocates canvas at max_width x max_height.
 * Core outputs at base_width x base_height each frame (may vary if dynamic).
 */
typedef struct {
    uint32_t        base_width;    /* native output resolution                */
    uint32_t        base_height;
    uint32_t        max_width;     /* maximum possible output (allocate this) */
    uint32_t        max_height;
    DingPixelFormat format;
    uint8_t         dynamic;       /* 1 if resolution can change mid-game     */
} DingVideoInfo;

/*
 * DingAudioInfo
 * Returned by ding_get_audio_info(). Describes audio output properties.
 */
typedef struct {
    uint32_t sample_rate;          /* e.g. 44100, 48000                       */
    uint8_t  channels;             /* 1 = mono, 2 = stereo, 6 = 5.1          */
} DingAudioInfo;

/*
 * DingMemoryRegion
 * Describes one addressable memory region the core exposes.
 * Consumers call ding_get_memory_region() for each region index.
 *
 * If access == DING_MEM_DIRECT: use ptr for fast reads/writes.
 * If access == DING_MEM_MANAGED: use read8/write8 functions.
 * If access == DING_MEM_OPAQUE: region is not externally accessible.
 *
 * Consumers must always check access before using ptr or function pointers.
 */
typedef struct {
    const char*      name;         /* "WRAM", "VRAM", "Z80 RAM", "SPU RAM"   */
    uint32_t         base_addr;    /* base address in the core's address space */
    size_t           size;         /* size in bytes                           */
    DingMemoryAccess access;

    /* Direct access — valid only when access == DING_MEM_DIRECT */
    uint8_t*         ptr;

    /* Managed access — valid only when access == DING_MEM_MANAGED */
    uint8_t  (*read8) (uint32_t addr);
    void     (*write8)(uint32_t addr, uint8_t val);

    uint8_t          writable;     /* 1 if consumers may write to this region */
} DingMemoryRegion;

/*
 * DingInputDescriptor
 * Describes one input on one controller port.
 * The core declares all its inputs at init. The frontend maps physical
 * inputs to descriptor indices. No hardcoded button layouts anywhere.
 */
typedef struct {
    const char*  name;             /* "D-Pad Up", "Left Trigger", "Gyro X"   */
    DingInputType type;
    uint8_t      port;             /* controller port index (0-based)         */
    uint8_t      index;            /* input index within the port             */
} DingInputDescriptor;

/*
 * DingBiosDescriptor
 * Describes one BIOS or firmware file the core requires or can use.
 * The frontend locates files and supplies them via ding_load_bios().
 * required == 0 means the core can run without it (enhanced if present).
 */
typedef struct {
    const char* name;              /* "PlayStation BIOS (NTSC-U)"             */
    const char* filename;          /* expected filename the user must provide  */
    uint32_t    size;              /* expected file size in bytes              */
    uint8_t     md5[16];           /* expected MD5 — core verifies on load    */
    uint8_t     required;          /* 1 = hard requirement, 0 = optional       */
    uint8_t     loaded;            /* core sets 1 after successful load        */
} DingBiosDescriptor;

/*
 * DingRomIdentity
 * Filled by ding_get_rom_identity() after a ROM or disc is loaded.
 * Cartridge systems use MD5/SHA1. Disc systems use serial or disc ID.
 */
typedef struct {
    DingIdentityMethod method;
    uint8_t            hash[16];   /* MD5 result — always 16 bytes            */
    /* serial and disc_id are metadata only — for display and human readability.
     * They are NOT used for identity matching. hash is the identity. */
    char               serial[32]; /* "SCUS-94163", "MK-4407", etc.           */
    char               disc_id[64];
} DingRomIdentity;

/*
 * DingDiscImage
 * An abstraction over disc image formats passed to ding_load_disc().
 * The core reads sectors through the read_sector function pointer.
 * The core never parses BIN/CUE, ISO, CHD etc. directly — that is the
 * frontend's responsibility.
 *
 * read_sector returns number of bytes read, 0 on failure.
 */
typedef struct {
    DingDiscFormat format;
    uint8_t        track_count;
    uint32_t       (*read_sector)(uint32_t lba,
                                  uint8_t* buf,
                                  uint32_t sector_size,
                                  void*    userdata);
    void*          userdata;       /* opaque handle passed back to read_sector */
} DingDiscImage;

/*
 * DingSaveStateInfo
 * Returned by ding_get_savestate_info(). Describes save state capability.
 * Consumers check supported before attempting any save/load operation.
 */
typedef struct {
    DingSaveMethod method;
    size_t         max_size;       /* hint: 0 = unknown or very large          */
    uint8_t        supported;      /* 1 if save states are available at all    */
} DingSaveStateInfo;


/* ═══════════════════════════════════════════════════════════════════════════
 * LIFECYCLE — fixed, identical across all cores, never changes
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Initialize the core. Must be called before anything else. */
void        ding_init();

/* Destroy the core and free all resources. */
void        ding_destroy();

/* Reset the core to power-on state. ROM and BIOS remain loaded. */
void        ding_reset();

/* Run exactly one video frame. */
void        ding_run_frame();


/* ═══════════════════════════════════════════════════════════════════════════
 * ROM AND DISC LOADING
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Load a cartridge ROM from a byte array.
 * Returns DING_OK on success.
 * data must remain valid for the lifetime of the loaded ROM.
 */
DingResult  ding_load_rom(const uint8_t* data, size_t len);

/*
 * Load a disc image via the DingDiscImage abstraction.
 * Returns DING_OK on success.
 * The disc struct must remain valid for the lifetime of the loaded disc.
 */
DingResult  ding_load_disc(DingDiscImage* disc);

/*
 * Supply a BIOS file to the core.
 * Call for each DingBiosDescriptor the core reports.
 * Core verifies MD5 before accepting.
 * Returns DING_OK if accepted, DING_ERR_BIOS_INVALID if MD5 mismatch.
 */
DingResult  ding_load_bios(uint32_t bios_index,
                           const uint8_t* data,
                           size_t len);

/*
 * Multi-disc: check if the core is signalling a disc swap (tray open).
 * Returns 1 when it is safe to call ding_swap_disc().
 */
uint8_t     ding_is_disc_swap_pending();

/* Swap the current disc. Only call when ding_is_disc_swap_pending() == 1. */
void        ding_swap_disc(DingDiscImage* disc);


/* ═══════════════════════════════════════════════════════════════════════════
 * CAPABILITY QUERIES
 * All return const pointers to core-owned data. Do not free them.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* General core information */
const DingCoreInfo*      ding_get_core_info();

/* Video properties — query once after loading a ROM */
const DingVideoInfo*     ding_get_video_info();

/* Audio properties — query once after loading a ROM */
const DingAudioInfo*     ding_get_audio_info();

/* ROM identity — query after loading a ROM or disc */
const DingRomIdentity*   ding_get_rom_identity();

/* Save state capability */
const DingSaveStateInfo* ding_get_savestate_info();

/* Number of memory regions the core exposes */
uint32_t    ding_get_memory_region_count();

/* Fill out a DingMemoryRegion for the given index */
void        ding_get_memory_region(uint32_t index, DingMemoryRegion* out);

/* Number of BIOS files this core requires or can use */
uint32_t    ding_get_bios_count();

/* Fill out a DingBiosDescriptor for the given index */
void        ding_get_bios_descriptor(uint32_t index, DingBiosDescriptor* out);

/* Total number of input descriptors across all ports */
uint32_t    ding_get_input_descriptor_count();

/* Fill out a DingInputDescriptor for the given index */
void        ding_get_input_descriptor(uint32_t index, DingInputDescriptor* out);


/* ═══════════════════════════════════════════════════════════════════════════
 * VIDEO OUTPUT
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Returns a pointer to the current framebuffer.
 * Valid after ding_run_frame(). Size is base_width * base_height * bytes_per_pixel.
 * The pointer is core-owned — do not free it.
 * If dynamic == 1 in DingVideoInfo, query ding_get_current_dimensions() each frame.
 */
const uint8_t* ding_get_framebuffer();

/*
 * For cores with dynamic == 1: returns the actual output dimensions this frame.
 * For fixed-resolution cores these always equal base_width / base_height.
 */
void        ding_get_current_dimensions(uint32_t* width, uint32_t* height);


/* ═══════════════════════════════════════════════════════════════════════════
 * AUDIO OUTPUT
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Returns the number of sample frames available in the audio ring buffer.
 * One sample frame = one sample per channel (e.g. stereo = 2 floats).
 */
uint32_t    ding_get_audio_sample_count();

/*
 * Copy available audio samples into buf.
 * buf must be at least (count * channels) floats.
 * Returns actual number of sample frames written.
 * Samples are interleaved: [L, R, L, R, ...] for stereo.
 */
uint32_t    ding_read_audio_samples(float* buf, uint32_t count);


/* ═══════════════════════════════════════════════════════════════════════════
 * INPUT
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Set the state of a digital button input.
 * port and index match the DingInputDescriptor.
 * pressed: 1 = held, 0 = released.
 */
void        ding_set_button(uint8_t port, uint8_t index, uint8_t pressed);

/*
 * Set the value of an analog axis input.
 * value range: -32768 to 32767.
 */
void        ding_set_axis(uint8_t port, uint8_t index, int16_t value);


/* ═══════════════════════════════════════════════════════════════════════════
 * SAVE STATES
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Serialize current state into buf.
 * buf must be at least DingSaveStateInfo.max_size bytes.
 * Returns number of bytes written, 0 on failure.
 * Only valid when DingSaveStateInfo.method == DING_SAVE_FULL.
 */
size_t      ding_save_state(uint8_t* buf, size_t buf_size);

/*
 * Restore state from buf.
 * Returns DING_OK on success, DING_ERR_BAD_STATE if data is corrupt
 * or was produced by an incompatible core version.
 */
DingResult  ding_load_state(const uint8_t* buf, size_t len);


/* ═══════════════════════════════════════════════════════════════════════════
 * REGION / SYSTEM CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Set the hardware region before loading a ROM.
 * region: "NTSC", "PAL", "NTSC-J"
 * Not all cores honour this — check DingCoreInfo for platform specifics.
 */
void        ding_set_region(const char* region);


/* ═══════════════════════════════════════════════════════════════════════════
 * DIAGNOSTICS (optional — cores may return NULL / no-op)
 * Used by Cockpit and the test harness.
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Write a human-readable diagnostic dump of current CPU state into buf.
 * Returns number of bytes written. buf is null-terminated.
 */
size_t      ding_diag_cpu_state(char* buf, size_t buf_size);

/*
 * Write a human-readable diagnostic dump of current VDP/GPU state into buf.
 */
size_t      ding_diag_video_state(char* buf, size_t buf_size);

/*
 * Returns 1 if the core has encountered an unrecoverable error this frame.
 * Consumers should call ding_diag_last_error() to retrieve the message.
 */
uint8_t     ding_has_error();

/*
 * Returns a null-terminated string describing the last error.
 * Returns NULL if no error. Core-owned — do not free.
 */
const char* ding_diag_last_error();


#ifdef __cplusplus
}
#endif

#endif /* DING_CORE_H */
