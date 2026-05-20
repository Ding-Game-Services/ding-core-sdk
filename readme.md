# Ding Core SDK — Documentation

## What Is This?

The Ding Core SDK is the shared foundation every Ding emulator core is built on.
It lives in its own repo (`ding-core-sdk`) and is included in each core repo as a
git submodule at `sdk/`. It does three things:

1. Defines the universal API every core must implement (`ding_core.h`)
2. Provides shared utilities every core will need (MD5, audio, save states, types)
3. Provides a headless test harness for verifying core behavior without a browser

Nothing in this SDK is platform-specific. It compiles identically whether the target
is a Windows `.dll`, a Linux `.so`, or a WebAssembly `.wasm` via Emscripten.

---

## File Reference

### `include/ding_types.h`

The primitive vocabulary the entire SDK and every core is written in. This is the
first file included by everything else.

**What it provides:**

- **Type aliases** — `u8`, `u16`, `u32`, `u64`, `s8`, `s16`, `s32`, `s64` as
  shorthand for the fixed-width stdint types. Never use `int`, `long`, or `short`
  for emulation values — their sizes are platform-dependent.

- **Boolean** — `ding_bool`, `DING_TRUE`, `DING_FALSE`. Consistent across C and C++.

- **Bit manipulation macros** — `BIT(n)`, `BIT_TEST`, `BIT_SET`, `BIT_CLEAR`,
  `BIT_TOGGLE`, `MASK(n)`, `BITS(x, p, n)`. Used constantly in CPU and VDP code.

- **Byte/word extraction** — `HI_BYTE`, `LO_BYTE`, `HI_WORD`, `LO_WORD`,
  `MAKE_U16`, `MAKE_U32`.

- **Endianness helpers** — `ding_read_be16/32`, `ding_read_le16/32`,
  `ding_write_be16/32`, `ding_write_le16/32`. Read and write multi-byte values
  from raw byte buffers correctly regardless of host platform byte order.
  Essential for 68000 (big-endian) and Z80/x86 (little-endian) emulation.

- **Utility macros** — `DING_MIN`, `DING_MAX`, `DING_CLAMP`, `DING_ARRAY_SIZE`,
  `DING_UNUSED`.

- **Size constants** — CD sector sizes (`DING_SECTOR_SIZE_RAW/DATA/AUDIO`),
  ROM size hints per generation (`DING_ROM_SIZE_MAX_8BIT/16BIT/32BIT`).

---

### `include/ding_core.h`

The universal API contract. Every Ding core implements exactly these functions and
structs. Consumers (Hydra, Cockpit, Ding Engine, the test harness) talk to every
core through this interface without any platform-specific knowledge.

**Key design principle:** cores declare their capabilities, consumers adapt. Nothing
is hardcoded or assumed.

#### Enumerations

| Enum | Purpose |
|---|---|
| `DingPixelFormat` | Framebuffer pixel layout (RGBA8, RGB565, etc.) |
| `DingMemoryAccess` | How a memory region is accessible (direct/managed/opaque) |
| `DingInputType` | Button, analog axis, or motion sensor |
| `DingSaveMethod` | Full blob, delta, checkpoint, or unsupported |
| `DingIdentityMethod` | How ROM hash is computed (full MD5, stripped MD5, custom MD5) |
| `DingDiscFormat` | Disc image container (BIN/CUE, ISO, CHD, XISO) |
| `DingResult` | Return codes for operations that can fail |

#### Structs

| Struct | Purpose |
|---|---|
| `DingCoreInfo` | Core name, platform name, version |
| `DingVideoInfo` | Framebuffer dimensions, pixel format, whether resolution is dynamic |
| `DingAudioInfo` | Sample rate and channel count |
| `DingMemoryRegion` | One named region of addressable memory with direct or managed access |
| `DingInputDescriptor` | One input on one controller port — name, type, port, index |
| `DingBiosDescriptor` | One BIOS/firmware requirement — filename, size, MD5, required flag |
| `DingRomIdentity` | MD5 hash of the loaded ROM plus optional serial/disc metadata |
| `DingDiscImage` | Disc abstraction with a `read_sector` function pointer |
| `DingSaveStateInfo` | Save state capability — method and max size |

#### Function Groups

**Lifecycle** — fixed forever, identical across all cores:
```
ding_init / ding_destroy / ding_reset / ding_run_frame
```

**Loading:**
```
ding_load_rom       — cartridge: raw byte array
ding_load_disc      — disc: DingDiscImage abstraction
ding_load_bios      — supply a BIOS file by index
ding_swap_disc      — multi-disc swap (after swap pending signal)
```

**Capability queries** — call after loading a ROM:
```
ding_get_core_info / ding_get_video_info / ding_get_audio_info
ding_get_rom_identity / ding_get_savestate_info
ding_get_memory_region_count / ding_get_memory_region
ding_get_bios_count / ding_get_bios_descriptor
ding_get_input_descriptor_count / ding_get_input_descriptor
```

**Video:**
```
ding_get_framebuffer          — pointer to current frame's pixel data
ding_get_current_dimensions   — actual output size this frame (dynamic cores)
```

**Audio:**
```
ding_get_audio_sample_count   — frames available in ring buffer
ding_read_audio_samples       — pull samples out into a float buffer
```

**Input:**
```
ding_set_button   — digital button state
ding_set_axis     — analog axis value
```

**Save states:**
```
ding_save_state   — serialize to caller-owned buffer
ding_load_state   — restore from buffer
```

**Diagnostics** (optional — cores may no-op these):
```
ding_diag_cpu_state    — human-readable CPU register dump
ding_diag_video_state  — human-readable VDP/GPU state dump
ding_has_error         — check for unrecoverable error this frame
ding_diag_last_error   — retrieve error message string
```

#### Memory Region Access Pattern

Always check `access` before touching a region:

```c
DingMemoryRegion region;
ding_get_memory_region(0, &region);

if (region.access == DING_MEM_DIRECT && region.ptr) {
    // fast path — read directly from pointer
    u8 val = region.ptr[addr - region.base_addr];
} else if (region.access == DING_MEM_MANAGED && region.read8) {
    // managed path — use function (large address spaces, PS2 etc.)
    u8 val = region.read8(addr);
} else {
    // DING_MEM_OPAQUE — not externally accessible
}
```

#### ROM Identification

All systems use MD5. The method determines which bytes are hashed:

| Method | When used | What gets hashed |
|---|---|---|
| `DING_ID_MD5_FULL` | Most cartridge systems | Entire ROM file |
| `DING_ID_MD5_STRIPPED` | NES, SNES, PCE, Lynx | ROM after removing copier/format header |
| `DING_ID_MD5_CUSTOM` | Disc systems, NDS, GCN | Platform-specific content extraction |

Disc systems extract meaningful content from the disc (executable, volume header, etc.)
rather than hashing the container file. The exact recipe is per-core.
The `serial` and `disc_id` fields in `DingRomIdentity` are display metadata only —
the hash is always the identity.

#### BIOS Handling

Cores that need a BIOS declare requirements via `DingBiosDescriptor`. The project
never distributes BIOS files. The frontend locates them and supplies them via
`ding_load_bios()`. The core verifies the MD5 of any supplied BIOS before accepting it.

Expected BIOS location managed by the frontend:
```
/ding-data/bios/{platform}/
```

#### Disc Images

Cores never parse disc formats directly. The frontend provides a `DingDiscImage`
with a `read_sector` function pointer. This means BIN/CUE, ISO, CHD, and XISO support
can be added to the frontend without modifying any core.

Multi-disc games use an explicit handshake:
```c
// Core signals it's safe to swap (tray open event)
if (ding_is_disc_swap_pending()) {
    ding_swap_disc(&new_disc);
}
```

---

### `include/ding_audio.h` + `src/ding_audio.cpp`

A lock-free ring buffer for delivering audio samples from the core to the frontend.

**How it works:**
- The core writes samples during synthesis (after `ding_run_frame`)
- The frontend reads samples at its own pace (Web Audio callback, SDL callback, etc.)
- Neither side blocks the other
- Buffer overflow: oldest samples are silently overwritten
- Buffer underrun: frontend receives silence padding

**Samples** are 32-bit floats in the range `[-1.0, 1.0]`. Channels are interleaved:
`[L, R, L, R, ...]` for stereo.

**No dynamic allocation** — the caller provides the backing float array at init.
The `DingAudioBuffer` struct holds only state; storage is external.

**Key functions:**

| Function | Purpose |
|---|---|
| `ding_audio_init` | Initialize with caller-owned storage, capacity, channels, sample rate |
| `ding_audio_reset` | Clear buffer on core reset or game switch |
| `ding_audio_write` | Core writes a batch of sample frames |
| `ding_audio_write_sample` | Core writes one sample frame (per-sample synthesis loops) |
| `ding_audio_read` | Frontend reads sample frames, silence-padded if buffer is dry |
| `ding_audio_available` | How many frames are ready to read |
| `ding_audio_free_space` | How many frames can be written before overflow |

**Typical usage in a core:**
```c
// At init
float storage[DING_AUDIO_DEFAULT_CAPACITY * 2];  // stereo
ding_audio_init(&audio_buf, storage, DING_AUDIO_DEFAULT_CAPACITY, 2, 44100);

// During synthesis (called per sample or per batch)
float samples[2] = { left, right };
ding_audio_write_sample(&audio_buf, samples);
```

---

### `include/ding_savestate.h` + `src/ding_savestate.cpp`

Serialization and deserialization for the `.ding` binary save state format.

#### File Format

```
[ DingSaveHeader  ]   — magic, version, platform, ROM MD5, CRC32, block count
[ Block 0 header  ]   — block ID, name, data size
[ Block 0 data    ]   — raw bytes
[ Block 1 header  ]
[ Block 1 data    ]
...
[ End marker      ]   — 0xFFFFFFFF
```

All multi-byte values are stored little-endian.

**CRC32** covers all data after the file header — detects file corruption before
any state is restored.

**ROM MD5** in the header ties the save state to a specific game — prevents loading
a save from one game into another.

#### Compatibility

Blocks are identified by name, not position. The reader searches for blocks by name
and skips any it doesn't recognize. This means:

- Older save states load in newer core versions (new blocks are simply absent)
- Newer save states load in older core versions (unknown blocks are skipped)
- Block order in the file does not matter

#### Writer Usage

```c
u8 buf[MAX_SAVE_SIZE];
size_t save_size;
DingSaveWriter w;

ding_save_writer_init(&w, buf, sizeof(buf), "Sega Genesis");
ding_save_write_block(&w, "CPU",      &cpu_state,  sizeof(cpu_state));
ding_save_write_block(&w, "WRAM",     wram,         WRAM_SIZE);
ding_save_write_block(&w, "VDP_REGS", &vdp_state,  sizeof(vdp_state));
ding_save_write_block(&w, "APU",      &apu_state,  sizeof(apu_state));
ding_save_writer_finish(&w, &save_size);
// buf[0..save_size] is the complete .ding file
```

#### Reader Usage

```c
DingSaveReader r;
if (ding_save_reader_init(&r, buf, buf_size) != DING_SS_OK) {
    // bad magic, wrong version, or corrupted data — abort
    return;
}
ding_save_read_block(&r, "CPU",      &cpu_state,  sizeof(cpu_state),  NULL);
ding_save_read_block(&r, "WRAM",     wram,         WRAM_SIZE,          NULL);
ding_save_read_block(&r, "VDP_REGS", &vdp_state,  sizeof(vdp_state),  NULL);
ding_save_read_block(&r, "APU",      &apu_state,  sizeof(apu_state),  NULL);
// Missing blocks leave destination untouched — not an error
```

---

### `include/ding_md5.h` + `src/ding_md5.cpp`

MD5 hashing for ROM identification. Based on RFC 1321 (public domain algorithm).
No external dependencies.

**Three usage modes:**

**Single buffer** — for cartridge ROMs where the whole file is in memory:
```c
u8 hash[16];
ding_md5(rom_data, rom_size, hash);
```

**Streaming** — for disc systems where content is assembled from multiple reads:
```c
DingMD5Context ctx;
ding_md5_init(&ctx);
ding_md5_update(&ctx, chunk_a, size_a);
ding_md5_update(&ctx, chunk_b, size_b);
ding_md5_finish(&ctx, hash);
```

**Hex output** — for display and database comparison:
```c
char hex[33];
ding_md5_to_hex(hash, hex);
// hex is now e.g. "A1B2C3D4E5F6..."  (uppercase, null-terminated)
```

Output is always uppercase to match the format used in the game database.

`ding_md5_compare(a, b)` compares two hashes without converting to strings.

---

## How the SDK Fits Into the Bigger Picture

```
ding-core-sdk/          Shared foundation — this repo
  └── used by:
        genesis-core/   First C++ core, test subject for the pipeline
        nes-core/       Future
        gameboy-core/   Future
        snes-core/      Future
        gba-core/       Future

Each core compiles to:
  {platform}-core.wasm  ← consumed by browser frontends and Cockpit
  {platform}-core.dll   ← consumed by Hydra on Windows
  {platform}-core.so    ← consumed by Hydra on Linux / Steam Deck

All consumers talk to cores through ding_core.h only.
No consumer has platform-specific emulation knowledge.
```

### Diagnostic Tiers

```
Tier 1 — Headless harness (SDK, native build only)
         Automated. No browser needed. Golden file comparisons.
         "Did 1000 frames produce correct WRAM values?"

Tier 2 — Cockpit (browser)
         Interactive. Live memory inspection during play.
         "Let me watch this RAM address while the game runs."

Tier 3 — Frontend diagnostic HUD
         In-page. CPU state, IRQ flags, frame timing overlaid on gameplay.
         "Something looks wrong, let me see what the CPU is doing."
```

---

## Building

Requires CMake 3.20 or higher. Two configurations must always work:

**Native (harness + shared library):**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**WebAssembly:**
```bash
cmake -B build-wasm -DCMAKE_TOOLCHAIN_FILE=path/to/Emscripten.cmake
cmake --build build-wasm
```

If either configuration breaks, fix it before continuing feature work.

---

## Adding a New Core

1. Create a new repo under `Ding-Game-Services` (e.g. `nes-core`)
2. Add `ding-core-sdk` as a submodule at `sdk/`
3. Create `src/`, `include/`, `CMakeLists.txt`
4. Implement every function declared in `sdk/include/ding_core.h`
5. Use `DingAudioBuffer` for all audio output
6. Use `DingSaveWriter`/`DingSaveReader` for all save state serialization
7. Use `ding_md5` for ROM identification
8. Implement `ding_diag_cpu_state` and `ding_diag_video_state` for Cockpit support
9. No GPL code, no platform-specific code in `src/` or `include/`

See `DING_CORE_SDK_RULES.md` for the full coding standard.
