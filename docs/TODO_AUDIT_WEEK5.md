# UFT Core TODO Audit Report

**Version:** 3.3.4  
**Date:** 2026-01-03  
**Status:** Week 5 - Core TODO Fixes Complete

## Summary

| Category | Before | After Session 1 | After Session 2 |
|----------|--------|-----------------|-----------------|
| Total TODOs | 146 | 125 | **~110** |
| Critical Core | 35 | 15 | **15** (stable) |
| Decoder | 25 | 20 | **10** |

---

## Session 1 Fixes

### 1. Amiga MFM Decoder v2 - Checksum Verification
**File:** `src/decoders/unified/uft_amiga_mfm_decoder_v2.c`
- Header checksum validation
- Data checksum validation
- Proper sector status based on CRC results
- **+70 lines**

### 2. MFM Encoder - Bounds Checking
**File:** `src/decoders/unified/uft_mfm_encoder.c`
- Buffer overflow prevention
- Size calculation before encoding
- **+15 lines**

### 3. Apple GCR Decoder
**File:** `src/decoder/uft_gcr.c`
- 6-and-2 encoding (DOS 3.3)
- 4-and-4 address field decoding
- XOR de-nibblization
- **+180 lines**

### 4. Cache Write-Back
**Files:** `src/core/uft_cache.c`, `include/uft/uft_cache.h`
- Added callback function type
- User data support
- **+27 lines**

### 5. GUI Main Window
**File:** `src/gui/uft_main_window.cpp`
- Format detection on load
- Magic byte recognition
- **+60 lines**

---

## Session 2 Fixes

### 6. Forensic Flux Decoder - Multi-Encoding Support
**File:** `src/decoder/uft_forensic_flux_decoder.c`
- FM encoding support
- GCR encoding dispatch
- **+45 lines**

### 7. MFM Decoder v2 - Encoding Implementation
**File:** `src/decoders/unified/uft_mfm_decoder_v2.c`
- Integration with uft_mfm_encoder.c
- Sector → MFM bits → flux conversion
- **+80 lines**

### 8. GCR Decoder - Complete Implementation
**File:** `src/decoders/uft_gcr_decoder.c`
- PLL-based flux-to-bits conversion
- gcr_decode_bitstream() function
- Complete GCR encoding
- Speed zone timing support
- **+270 lines**

---

## CMake Integration

**New Files:**
- `src/core/CMakeLists.txt`
- `src/decoder/CMakeLists.txt`
- `src/decoders/CMakeLists.txt`

**Updated:**
- `CMakeLists.txt` (root)
- `src/decoders/unified/CMakeLists.txt`

---

## Total Lines Added: ~822

| File | Lines | Session |
|------|-------|---------|
| uft_amiga_mfm_decoder_v2.c | +70 | 1 |
| uft_mfm_encoder.c | +15 | 1 |
| uft_gcr.c | +180 | 1 |
| uft_cache.c/h | +27 | 1 |
| uft_main_window.cpp | +60 | 1 |
| uft_decoder_registry.c | +25 | 1 |
| uft_forensic_flux_decoder.c | +45 | 2 |
| uft_mfm_decoder_v2.c | +80 | 2 |
| uft_gcr_decoder.c | +270 | 2 |
| CMakeLists (various) | +50 | 1+2 |

---

## Remaining TODOs (15)

### Hardware-Dependent
- `uft_tool_adapter.c:209` - Greaseweazle (needs hardware)

### Optimization (Low Priority)
- `uft_core.c:407` - Cache management
- `uft_memory.c:131` - Chunk allocation

### Encoding Stubs
- `uft_fm_decoder.c:882` - FM encoder
- `uft_mfm_decoder.c:422` - Old MFM encoder

### Refinements
- `uft_amiga_mfm_decoder_v3.c:136` - PLL conversion
- `uft_apple_gcr_decoder_v2.c:702` - Multi-revolution

---

## Build Verification

```bash
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

All sources properly integrated into CMake build system.
