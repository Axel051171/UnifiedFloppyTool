# 💎 X-Copy Integration - Quick Start Guide

## UnifiedFloppyTool v2.6.2 - X-Copy Edition

---

## 🎯 **What is This?**

This version integrates the **legendary X-Copy Professional error detection system**,
converted from original 68000 assembler source code to modern C.

**X-Copy** was THE standard disk copying tool for Amiga, used by millions.
Its error detection system is **professional-grade** and **battle-tested**.

---

## 🚀 **Quick Start**

### **1. Build**

```bash
mkdir build && cd build
cmake ..
make

# You now have:
# - libflux_core.a (with X-Copy error system!)
# - example_xcopy_errors (demo program)
```

### **2. Run Demo**

```bash
./example_xcopy_errors

# Output shows:
# - Error detection on simulated tracks
# - Copy protection detection
# - UFM flag mapping
# - Statistics collection
```

---

## 📚 **API Overview**

### **Error Detection**

```c
#include "xcopy_errors.h"

// Analyze a track
uint8_t *track_data = ...;  // RAW track data
size_t track_length = ...;

xcopy_track_error_t error;
int result = xcopy_analyze_track(track_data, track_length, &error);

// Check error code
if (error.error_code == XCOPY_ERR_LONG_TRACK) {
    printf("Copy protection detected: Long track!\n");
}

// Check protection flag
if (error.is_protected) {
    printf("This track has copy protection\n");
}
```

### **UFM Integration**

```c
// Convert X-Copy error to UFM flags
uint32_t cp_flags = xcopy_error_to_ufm_flags(error.error_code);

// Set in UFM track
ufm_track_t *track = ...;
track->cp_flags |= cp_flags;
```

### **Statistics**

```c
// Collect statistics for entire disk
xcopy_error_stats_t stats;
xcopy_stats_init(&stats);

for (int cyl = 0; cyl < 80; cyl++) {
    for (int head = 0; head < 2; head++) {
        xcopy_track_error_t error;
        // ... analyze track ...
        xcopy_stats_add(&stats, &error);
    }
}

// Print summary
xcopy_stats_print(&stats);
```

---

## 🎯 **Error Codes**

### **The 8 X-Copy Error Types:**

| Code | Type | Meaning | Protection Indicator |
|------|------|---------|---------------------|
| **0** | NONE | No error | ✅ Normal |
| **1** | SECTOR_COUNT | ≠11 sectors | ⚠️ Possible |
| **2** | NO_SYNC | Missing sync mark | ⚠️ Common |
| **3** | GAP_SYNC | Bad gap timing | ⚠️ Timing issue |
| **4** | HEADER_CRC | Header checksum | ✅ Data error |
| **5** | HEADER_FORMAT | Format error | ⚠️ Protection |
| **6** | DATA_CRC | Data checksum | ✅ Data error |
| **7** | **LONG_TRACK** | **>Standard length** | ⭐ **PROTECTION!** |
| **8** | VERIFY | Write verify fail | ✅ Media error |

### **Error Messages**

```c
// English
const char *msg = xcopy_error_message(XCOPY_ERR_LONG_TRACK);
// Returns: "7: Long track!"

// German
const char *msg_de = xcopy_error_message_de(XCOPY_ERR_LONG_TRACK);
// Returns: "7: Langer Track!"
```

---

## 💎 **Copy Protection Detection**

### **Error Code 7 = Classic Protection**

```c
// Long track detection
if (error.error_code == XCOPY_ERR_LONG_TRACK) {
    // This is a CLASSIC Amiga copy protection technique!
    // Track is longer than standard 11 sectors
    printf("COPY PROTECTION: Long track detected!\n");
    printf("Track length: %u bytes (expected ~%u)\n",
           error.track_length, error.expected_length);
}
```

### **Helper Function**

```c
// Check if error indicates protection
if (xcopy_is_protection(error.error_code)) {
    printf("This error suggests copy protection\n");
}

// Returns true for:
// - XCOPY_ERR_LONG_TRACK (most common)
// - XCOPY_ERR_SECTOR_COUNT
// - XCOPY_ERR_GAP_SYNC
```

---

## 🔧 **UFM Flag Mapping**

### **X-Copy Error → UFM Flags:**

```c
// Maps to UFM copy-protection flags
uint32_t flags = xcopy_error_to_ufm_flags(error_code);

// Mapping:
XCOPY_ERR_LONG_TRACK    → UFM_CP_LONGTRACK
XCOPY_ERR_SECTOR_COUNT  → UFM_CP_LONGTRACK
XCOPY_ERR_NO_SYNC       → UFM_CP_SYNC_ANOMALY
XCOPY_ERR_GAP_SYNC      → UFM_CP_SYNC_ANOMALY
XCOPY_ERR_HEADER_CRC    → UFM_CP_BAD_CRC
XCOPY_ERR_DATA_CRC      → UFM_CP_BAD_CRC
XCOPY_ERR_HEADER_FMT    → UFM_CP_NONSTD_GAP
```

---

## 📖 **Complete Example**

```c
#include "flux_core.h"
#include "xcopy_errors.h"

void analyze_amiga_disk(ufm_disk_t *disk)
{
    xcopy_error_stats_t stats;
    xcopy_stats_init(&stats);
    
    printf("Analyzing disk for copy protection...\n");
    
    for (int cyl = 0; cyl < disk->cyls; cyl++) {
        for (int head = 0; head < disk->heads; head++) {
            ufm_track_t *track = ufm_disk_track(disk, cyl, head);
            
            // Analyze first revolution
            if (track->rev_count > 0) {
                xcopy_track_error_t error;
                
                // Simple analysis (track length check)
                error.track_length = track->revs[0].count;
                error.expected_length = 11 * 512 + 800;
                
                if (error.track_length > 13000) {
                    error.error_code = XCOPY_ERR_LONG_TRACK;
                    error.is_protected = true;
                    
                    printf("Track %d/%d: COPY PROTECTION!\n", 
                           cyl, head);
                } else {
                    error.error_code = XCOPY_ERR_NONE;
                    error.is_protected = false;
                }
                
                // Set UFM flags
                track->cp_flags |= xcopy_error_to_ufm_flags(
                    error.error_code
                );
                
                // Collect statistics
                xcopy_stats_add(&stats, &error);
            }
        }
    }
    
    printf("\nDisk Analysis Complete:\n");
    xcopy_stats_print(&stats);
}
```

---

## 🎓 **Technical Details**

### **Original Source:**

- **File:** xio.s (4,476 lines 68000 assembler)
- **Location:** docs/xcopy/xio.s
- **Origin:** X-Copy Professional (released 2012)

### **Converted Code:**

- **Header:** libflux_core/include/xcopy_errors.h (178 lines)
- **Implementation:** libflux_core/src/xcopy_errors.c (196 lines)
- **Total:** 374 lines production C code

### **Key Features:**

- ✅ Professional error taxonomy (8 types)
- ✅ Bilingual support (English/German)
- ✅ Copy protection detection
- ✅ Statistics collection
- ✅ UFM integration
- ✅ Production ready

---

## 📚 **Additional Resources**

### **Original X-Copy:**

- **xio.s:** Complete original source (docs/xcopy/)
- **Documentation:** See XCOPY_SOURCE_CODE_ANALYSIS.md
- **Integration:** See XCOPY_INTEGRATION_COMPLETE.md

### **UFM Integration:**

- **flux_core.h:** UFM data structures
- **Copy-protection flags:** ufm_cp_flags_t

---

## 🏆 **Credits**

**X-Copy Professional:**
- Legendary Amiga disk copying software
- Professional error detection system
- Used by millions of Amiga users worldwide

**Conversion:**
- Original: 68000 assembler (4,476 lines)
- Modern: C17 (374 lines)
- Integration: UnifiedFloppyTool v2.6.2

**Source Release:**
- English Amiga Board (2012)
- Preserved for historical significance
- Integrated for modern disk preservation

---

**UnifiedFloppyTool v2.6.2** - Legendary code, preserved! 💎🎮

*Protokoll-Satz: "Wir bewahren Information – wir entscheiden nicht vorschnell, was wichtig ist."*
