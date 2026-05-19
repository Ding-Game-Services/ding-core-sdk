# Ding Core SDK — Coding Rules & Vision

## Overview

The Ding Core SDK is the shared foundation for all emulator cores in the Ding platform.
It defines the API contract that every core implements, shared utilities used across all
cores, and a headless test harness for verification and debugging.

The goal is a single codebase that compiles to three targets without modification:

- Native Windows binary (Hydra, desktop)
- Native Linux binary (Hydra, Steam Deck, ROG Ally on Linux)
- WebAssembly (browser-based emulators, Ding Cockpit)

---

## Licensing

**No GPL code. Ever.**

All code written for the SDK and any core must be original or sourced from MIT, BSD,
zlib, or similarly permissive licenses. GPL code cannot be used even as a reference
implementation. When in doubt, write it from scratch.

This applies to:
- CPU cores
- Audio synthesis
- Helper utilities
- Build scripts
- Anything that ends up in a compiled output

---

## Target Platforms

| Platform        | OS             | Architecture | Build Target     |
|-----------------|----------------|--------------|------------------|
| Windows desktop | Windows 10/11  | x86_64       | Native `.dll`    |
| Linux desktop   | Linux          | x86_64       | Native `.so`     |
| Steam Deck      | SteamOS(Linux) | x86_64       | Native `.so`     |
| ROG Ally        | Windows 11     | x86_64       | Native `.dll`    |
| Browser         | Any            | WASM         | `.wasm` + JS     |

ARM support is not a current requirement but should not be actively broken against.
Avoid x86-specific intrinsics in core files unless wrapped in a capability check.

---

## Repository Structure

```
ding-core-sdk/
  include/            # Public headers — the API every core implements
  src/                # Shared utility implementations
  harness/            # Headless test runner (native build only)
  tests/              # Golden snapshots and test ROM manifests
  CMakeLists.txt      # Root build definition

{platform}-core/
  sdk/                # Git submodule pointing to ding-core-sdk
  src/                # Platform-specific implementation files
  include/            # Platform-specific private headers
  CMakeLists.txt
```

Each core repo includes the SDK as a **git submodule at a pinned commit**.
SDK updates are opt-in per core. A breaking SDK change must not silently
affect cores that haven't updated.

---

## File Organization Per Core

Each C++ file maps to one clear responsibility. No exceptions.

| File            | Responsibility                              |
|-----------------|---------------------------------------------|
| `bus.cpp`       | Memory map and address decoding only        |
| `cpu.cpp`       | Primary CPU only                            |
| `vdp.cpp`       | Video/graphics processor only               |
| `apu.cpp`       | Audio processing only                       |
| `z80.cpp`       | Secondary CPU (where applicable) only       |
| `{platform}.cpp`| Orchestrator — owns the run loop            |
| `core_api.cpp`  | Emscripten/export bindings only             |

Cross-contamination between files is a bug. The CPU file must not touch audio.
The VDP file must not touch the memory map directly.

---

## Language and Type Rules

### Always use fixed-width integer types

```cpp
// Correct
uint8_t   val = 0xFF;
uint16_t  addr = 0xC000;
uint32_t  cycles = 0;
size_t    len = rom.size();

// Never
int    val = 0xFF;     // size is platform-dependent
long   addr = 0xC000;  // means different things on Windows vs Linux
```

Include `<stdint.h>` (or `<cstdint>`) in every file that uses integer types.

### All public headers must be valid C as well as C++

Wrap all declarations in `extern "C"` guards:

```c
#ifdef __cplusplus
extern "C" {
#endif

// ... declarations ...

#ifdef __cplusplus
}
#endif
```

This ensures clean symbol linkage across compilers and allows Hydra to load
cores as dynamic libraries reliably on both Windows and Linux.

### No platform-specific APIs in core files

`src/` and `include/` must use the C++ standard library only.

```cpp
// Never in a core file
#include <windows.h>
#include <unistd.h>
#include <sys/mman.h>
```

Platform-specific code (file I/O, audio output, threading) belongs in the
harness or frontend glue layer only.

---

## API Design Principles

### Cores declare capabilities — frontends adapt

Never assume a feature exists. Every capability is declared by the core
and checked by the consumer before use.

```c
// Wrong: assumes every core has a single flat WRAM
uint8_t ding_mem_read8(uint32_t addr);

// Right: core declares what memory regions exist
uint32_t ding_get_memory_region_count();
void     ding_get_memory_region(uint32_t index, DingMemoryRegion* out);
```

### All structs must have nullable/optional fields from day one

Even if a current core doesn't use a field, leave the door open for cores
that will. A NULL pointer or zero value costs nothing. Retrofitting a field
later breaks the API for every existing consumer.

### The lifecycle functions are fixed and identical across all cores forever

```c
void    ding_init();
void    ding_destroy();
void    ding_reset();
bool    ding_load_rom(const uint8_t* data, size_t len);
bool    ding_load_disc(DingDiscImage* disc);
void    ding_run_frame();
```

These never change. Everything variable is handled through info structs and
descriptors.

---

## Memory Regions

Memory access must support both small and large address spaces to scale from
8-bit systems to PS2/Xbox 360 and beyond.

```c
typedef struct {
    const char* name;                            // "WRAM", "VRAM", "SPU RAM"
    uint32_t    base_addr;
    size_t      size;
    uint8_t*    ptr;                             // NULL if managed access only
    uint8_t   (*read8) (uint32_t addr);          // used when ptr is NULL
    void      (*write8)(uint32_t addr, uint8_t val);
    uint8_t     writable;
    uint8_t     direct;                          // 1 if ptr is valid
} DingMemoryRegion;
```

Consumers must always check `direct` before using `ptr`:

```c
if (region.direct && region.ptr) {
    // fast path: direct pointer access
} else if (region.read8) {
    // managed path: use read function
} else {
    // region not accessible (e.g. GPU-side memory on advanced hardware)
}
```

---

## Video

Framebuffer dimensions are declared by the core, not assumed by the frontend.

```c
typedef struct {
    uint32_t base_width;
    uint32_t base_height;
    uint32_t max_width;      // frontend allocates canvas at this size
    uint32_t max_height;
    uint8_t  format;         // RGBA8, RGB565, YUV — see DingPixelFormat enum
    uint8_t  dynamic;        // 1 if resolution can change mid-game
} DingVideoInfo;
```

The frontend allocates at `max_width` x `max_height` and renders whatever
the core actually outputs each frame. A PC Engine and an Xbox 360 core both
fill this struct — just with very different values.

---

## Audio

Sample rate and channel count are declared by the core, not hardcoded.

```c
typedef struct {
    uint32_t sample_rate;
    uint8_t  channels;
} DingAudioInfo;
```

Audio is delivered via a ring buffer the frontend polls each frame.
The core writes samples. The frontend reads them. Neither blocks the other.

---

## Input

Input layout is declared by the core via descriptors. No hardcoded button layouts.

```c
typedef struct {
    const char* name;     // "D-Pad Up", "Left Trigger", "Gyro X"
    uint8_t     type;     // DING_INPUT_BUTTON, DING_INPUT_AXIS, DING_INPUT_MOTION
    uint8_t     index;    // used by frontend to map physical input
} DingInputDescriptor;
```

The core declares its descriptors at init. The frontend maps physical inputs
to descriptor indices. A 2-button PC Engine pad and a full Xbox 360 controller
are both handled by the same mechanism.

---

## BIOS and Firmware

Cores that require a BIOS or firmware declare their requirements at init.
The project never distributes BIOS files. The frontend is responsible for
locating and supplying them.

```c
typedef struct {
    const char* name;         // "PlayStation BIOS", "Sega CD BIOS (US)"
    const char* filename;     // filename the user must provide
    uint32_t    size;         // expected file size in bytes
    uint8_t     md5[16];      // MD5 of the correct file for verification
    uint8_t     required;     // 1 = hard requirement, 0 = optional enhancement
    uint8_t     loaded;       // core sets this to 1 after successful load
} DingBiosDescriptor;
```

BIOS files live in a predictable location the frontend manages:
```
/ding-data/bios/{platform}/
```

The core verifies the MD5 of any supplied BIOS before accepting it.
A wrong BIOS version is rejected with a clear error, not silently accepted.

---

## ROM Identification

MD5 hashing works for cartridge-based systems. It is not sufficient for
disc-based systems where the same game can be ripped into multiple formats.

### Cartridge systems
MD5 of the full ROM binary. Stored and compared in uppercase hex.

### Disc-based systems
Use the **serial number** embedded in the disc header. This is stable across
BIN/CUE, ISO, CHD, and other container formats because it comes from the disc
content itself.

```c
typedef struct {
    uint8_t  method;       // DING_ID_MD5, DING_ID_SERIAL, DING_ID_DISC_ID
    uint8_t  hash[20];     // MD5 or SHA1 (SHA1 sized for future use)
    char     serial[32];   // "SCUS-94163", "MK-4407", etc.
    char     disc_id[64];  // internal disc identifier where applicable
} DingRomIdentity;
```

---

## Disc Images

Cores never parse disc image formats directly. The frontend provides an
abstraction and the core reads through it. This means format support
(BIN/CUE, ISO, CHD, XISO) can be added to the frontend without touching
any core.

```c
typedef struct {
    uint8_t  format;          // DING_DISC_BINCUE, DING_DISC_ISO, DING_DISC_CHD
    uint8_t  track_count;
    uint32_t (*read_sector)(uint32_t lba, uint8_t* buf, uint32_t sector_size);
    void*    userdata;        // opaque handle passed back to read_sector
} DingDiscImage;
```

Multi-disc games use an explicit swap handshake:

```c
uint8_t ding_is_disc_swap_pending();   // core signals tray is open
void    ding_swap_disc(DingDiscImage* disc);
```

The frontend waits for the swap pending signal before calling swap.

---

## Save States

Save state capability is declared by the core. Not all systems can support
full serialization.

```c
typedef struct {
    uint8_t  method;       // DING_SAVE_FULL, DING_SAVE_DELTA, DING_SAVE_CHECKPOINT
    size_t   max_size;     // hint: 0 = unknown or very large
    uint8_t  supported;
} DingSaveStateInfo;
```

- `DING_SAVE_FULL` — entire state serialized to a binary blob (8/16-bit systems)
- `DING_SAVE_DELTA` — only changed state serialized (future, larger systems)
- `DING_SAVE_CHECKPOINT` — core manages its own state, frontend triggers it

Save state files use the `.ding` binary format defined in `ding_savestate.h`.

---

## The Test Harness

The harness is a native-only (non-WASM) headless runner for verification and
debugging. It is the first tier of diagnostics, sitting below Cockpit and the
frontend diagnostic panels.

```
Tier 1 — Harness        pure C++, no browser, automated verification
Tier 2 — Cockpit        browser-based, live memory inspection during play
Tier 3 — Frontend HUD   in-page CPU/IRQ/frame diagnostics during play
```

The harness must be able to:
- Load a ROM or disc image from disk
- Run N frames headlessly
- Dump WRAM, VRAM, and register state to stdout or file
- Compare output against a golden snapshot file
- Accept a CPU instruction log and diff against a reference trace

The harness builds with a standard native compiler. It must never require
Emscripten to build. If the harness build breaks, something has leaked
browser-specific code into the core.

---

## Build System

CMake 3.20 or higher. No other build system.

Each core has its own `CMakeLists.txt`. The SDK has its own `CMakeLists.txt`.
Cores include the SDK via the submodule — they do not copy SDK files.

Two build configurations must always work:

```bash
# Native (harness + shared library)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# WASM (browser target)
cmake -B build-wasm -DCMAKE_TOOLCHAIN_FILE=path/to/Emscripten.cmake
cmake --build build-wasm
```

If either configuration breaks, the breakage must be fixed before new
feature work continues.

---

## What Belongs Where — Quick Reference

| Code type                        | Lives in              |
|----------------------------------|-----------------------|
| Emulation logic                  | `{platform}-core/src/`|
| Shared API header                | `ding-core-sdk/include/`|
| Shared utilities (MD5, audio)    | `ding-core-sdk/src/`  |
| Emscripten bindings              | `core_api.cpp` only   |
| File I/O, audio output           | Harness or frontend   |
| Platform-specific OS calls       | Harness or frontend   |
| DOM, Web Audio, BroadcastChannel | Frontend HTML only    |
| Ding Engine achievement logic    | `ding-engine.js` only |

---

*This document is the source of truth for SDK and core development decisions.
Update it when a new architectural decision is made, not after the fact.*
