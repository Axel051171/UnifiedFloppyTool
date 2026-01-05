# BOOTBLOCK QUICKSTART GUIDE 🔍💾

## 📅 UnifiedFloppyTool v2.6.3 - Bootblock Edition

---

## 🎯 **Quick Overview**

The **Bootblock Detection System** provides:
- 2,988 bootblock signatures
- 422 VIRUS signatures ⚠️
- 126 X-Copy bootblock signatures
- Pattern matching (~1ms detection)
- CRC32 verification
- Automatic virus detection

---

## 🚀 **Quick Start**

### **1. Build the Example:**

```bash
cd UnifiedFloppyTool_v2.6.3_Bootblock_Edition
mkdir build && cd build
cmake ..
make
```

### **2. Scan an ADF File:**

```bash
./examples/example_bootblock_scanner disk.adf
```

### **3. Scan Multiple Files:**

```bash
./examples/example_bootblock_scanner *.adf
./examples/example_bootblock_scanner /path/to/collection/*.adf
```

---

## 📊 **Example Output**

### **Clean Disk:**

```
═══════════════════════════════════════════════════════════
  Scanning: lemmings.adf
═══════════════════════════════════════════════════════════

  DOS Type:      OFS
  Checksum:      0x370482A1 (✅ VALID)
  CRC32:         0xA5B3C7D2

  ✅ BOOTBLOCK DETECTED!

  Name:          Psygnosis Loader
  Category:      Loader
  Bootable:      Yes
  Has Data:      No
  Kickstart:     KS1.2+

  Detection:     Pattern Match
```

### **Virus Detected:**

```
═══════════════════════════════════════════════════════════
  Scanning: suspicious.adf
═══════════════════════════════════════════════════════════

  DOS Type:      OFS
  Checksum:      0x12345678 (❌ INVALID)
  CRC32:         0x492A98FC

  ✅ BOOTBLOCK DETECTED!

  Name:          16-Bit Crew Virus
  Category:      VIRUS ⚠️⚠️⚠️ VIRUS!
  Bootable:      True
  Kickstart:     KS1.3

  Detection:     Pattern Match CRC32 Match

  Notes: This is a known boot sector virus. Remove immediately!
  URL:   http://amiga.nvg.org/amiga/VirusEncyclopedia/ae000016.php

  ⚠️⚠️⚠️ WARNING: VIRUS DETECTED! ⚠️⚠️⚠️
  This disk contains a known virus!
  DO NOT boot this disk on real hardware!
```

### **X-Copy Bootblock:**

```
  ✅ BOOTBLOCK DETECTED!

  Name:          X-Copy Professional v2.2
  Category:      X-Copy 💾 (X-Copy synergy!)
  Bootable:      Yes
  Detection:     Pattern Match
```

### **Demoscene Intro:**

```
  ✅ BOOTBLOCK DETECTED!

  Name:          Fairlight Intro
  Category:      Intro 🎨
  Demoscene:     Yes
  Bootable:      Yes
```

---

## 💻 **API Usage**

### **Basic Detection:**

```c
#include "bootblock_db.h"

// Initialize database (once)
bb_db_init(NULL);  // Uses default brainfile.xml

// Read bootblock from ADF
uint8_t bootblock[BOOTBLOCK_SIZE];
FILE *f = fopen("disk.adf", "rb");
fread(bootblock, 1, BOOTBLOCK_SIZE, f);
fclose(f);

// Detect bootblock
bb_detection_result_t result;
bb_detect(bootblock, &result);

if (result.detected) {
    printf("Name: %s\n", result.signature.name);
    printf("Category: %s\n", bb_category_name(result.signature.category));
    
    if (bb_is_virus(result.signature.category)) {
        printf("⚠️ VIRUS!\n");
    }
}

// Cleanup
bb_db_free();
```

### **Checksum Verification:**

```c
// Verify bootblock checksum
bool is_valid;
bb_verify_checksum(bootblock, &is_valid, false);

if (!is_valid) {
    printf("Invalid checksum!\n");
    
    // Fix it (optional)
    bb_verify_checksum(bootblock, &is_valid, true);
    printf("Checksum fixed!\n");
}
```

### **Statistics Collection:**

```c
bb_scan_stats_t stats;
bb_stats_init(&stats);

// Scan multiple disks
for (int i = 0; i < num_disks; i++) {
    bb_detection_result_t result;
    bb_detect(bootblocks[i], &result);
    bb_stats_add(&stats, &result);
}

// Print summary
bb_stats_print(&stats);
// Output:
//   Total disks scanned:  50
//   Detected bootblocks:  48
//   Viruses found:        12 ⚠️
//   X-Copy bootblocks:    5
//   Demoscene intros:     8
```

---

## 🎯 **Perfect Synergies**

### **X-Copy + Bootblock Detection:**

```c
// Step 1: X-Copy detects track anomaly (v2.6.2)
xcopy_track_error_t xcopy_err;
xcopy_analyze_track(track_data, length, &xcopy_err);

if (xcopy_err.error_code == XCOPY_ERR_LONG_TRACK) {
    printf("X-Copy: Long track detected\n");
    
    // Step 2: Bootblock identifies WHICH protection (v2.6.3)
    bb_detection_result_t bb;
    bb_detect(bootblock, &bb);
    
    if (bb.detected) {
        printf("Protection: %s\n", bb.signature.name);
        // → "Rob Northen Copylock"
        // → "Speedlock"
        // → etc.
    }
}

// PERFECT SYNERGY! 💎
```

### **AmigaDOS + Bootblock Analysis:**

```c
// Complete disk analysis
adf_disk_t adf;
adf_open("disk.adf", &adf);

// Filesystem
amigados_rootblock_t root;
amigados_read_root_block(&adf, &root);
printf("Filesystem: %s\n", root.dos_type == 0 ? "OFS" : "FFS");

// Bootblock
bb_detection_result_t bb;
bb_detect(adf.bootblock, &bb);

if (bb.detected) {
    printf("Bootblock: %s\n", bb.signature.name);
    printf("Virus: %s\n", bb_is_virus(bb.signature.category) ? "YES ⚠️" : "NO ✅");
}
```

---

## 📚 **Categories**

```c
BB_CAT_VIRUS        // Virus ⚠️
BB_CAT_XCOPY        // X-Copy 💾
BB_CAT_DEMOSCENE    // Demoscene 🎨
BB_CAT_INTRO        // Intro
BB_CAT_LOADER       // Loader
BB_CAT_UTILITY      // Utility
BB_CAT_BOOTLOADER   // Bootloader
BB_CAT_GAME         // Game
BB_CAT_CUSTOM       // Custom
BB_CAT_PASSWORD     // Password
BB_CAT_VIRUS_FAKE   // Fake Virus
BB_CAT_SCENE        // Scene
```

---

## 🔍 **Database Information**

### **brainfile.xml:**
```
Size:                648 KB
Total Signatures:    2,988
Virus Signatures:    422 (14.1%)
X-Copy Signatures:   126 (4.2%)
Demoscene:           472 (15.8%)
Loaders:             333
Utilities:           1,036
```

### **Detection Methods:**

1. **Pattern Matching** (Fast - ~1ms)
   - Offset,value pairs
   - Example: byte 471=104, byte 481=114
   
2. **CRC32 Verification** (Exact - ~5ms)
   - Standard CRC32 (polynomial 0xEDB88320)
   - Exact match required

---

## ⚡ **Performance**

```
Database Load:       ~200ms (one-time)
Pattern Match:       ~1ms per disk
CRC32 Verify:        ~5ms per disk
Memory Usage:        ~2MB (database)
```

---

## 🎯 **Use Cases**

### **1. Virus Scanning**
```bash
# Scan entire collection
./example_bootblock_scanner ~/amiga_disks/*.adf > scan_report.txt

# Check for viruses
grep "VIRUS" scan_report.txt
```

### **2. Copy Protection Identification**
```bash
# Identify protection on game
./example_bootblock_scanner game.adf

# Combine with X-Copy
./example_xcopy_errors game.adf
./example_bootblock_scanner game.adf
```

### **3. Demoscene Archive**
```bash
# Identify demo intros
./example_bootblock_scanner demos/*.adf | grep "Intro\|Demoscene"
```

---

## 📖 **API Reference**

### **Initialization:**
```c
int bb_db_init(const char *xml_path);  // NULL = default "brainfile.xml"
void bb_db_free(void);
int bb_db_get_stats(int *total, int *viruses, int *xcopy);
```

### **Detection:**
```c
int bb_detect(const uint8_t bootblock[1024], bb_detection_result_t *result);
int bb_verify_checksum(uint8_t bootblock[1024], bool *valid, bool fix);
uint32_t bb_crc32(const uint8_t bootblock[1024]);
```

### **Utilities:**
```c
const char* bb_category_name(bootblock_category_t cat);
bool bb_is_virus(bootblock_category_t cat);
```

### **Statistics:**
```c
void bb_stats_init(bb_scan_stats_t *stats);
void bb_stats_add(bb_scan_stats_t *stats, const bb_detection_result_t *result);
void bb_stats_print(const bb_scan_stats_t *stats);
```

---

## ✅ **Tips & Tricks**

### **Tip 1: Batch Scanning**
```bash
# Create scan script
#!/bin/bash
for file in "$@"; do
    ./example_bootblock_scanner "$file"
done

# Use it
./scan_all.sh /amiga/games/*.adf
```

### **Tip 2: Find Viruses**
```bash
# Extract viruses only
./example_bootblock_scanner *.adf | grep -A 5 "VIRUS DETECTED"
```

### **Tip 3: X-Copy Synergy**
```bash
# Complete protection analysis
for file in *.adf; do
    echo "=== $file ==="
    ./example_xcopy_errors "$file"
    ./example_bootblock_scanner "$file"
done
```

---

## 🏆 **Credits**

- **AmigaBootBlockReader v6.0** by Jason and Jordan Smith
- **Database:** Community-maintained (2,988 signatures)
- **Integration:** UnifiedFloppyTool v2.6.3

---

## 📚 **See Also**

- `CHANGELOG_v2.6.3.md` - Complete v2.6.3 changes
- `XCOPY_QUICKSTART.md` - X-Copy integration guide
- `AMIGA_QUICKSTART.md` - AmigaDOS/ADF guide
- `brainfile.xml` - Full bootblock database

---

**v2.6.3 - The ULTIMATE Amiga Forensics Tool!** 💾🔍🎮

*2,988 signatures + X-Copy + AmigaDOS = PERFECT preservation!*
