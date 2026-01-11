# 🎯 UNIVERSELLES PARAMETER-SYSTEM FÜR ALLE FORMATE

## 📅 Datum: 23. Dezember 2024

---

## ✅ **ANTWORT: JA, DAS IST MÖGLICH UND SOGAR OPTIMAL!**

Das Parameter-System kann und **sollte** auf **ALLE 237+ Formate** erweitert werden!

---

## 🏗️ **CURRENT STATE (v2.4)**

### **Was wir haben:**

```c
// formats/format_registry/cpc_dsk.h

✅ Parameter Types:
   - CPC_PARAM_INT
   - CPC_PARAM_BOOL
   - CPC_PARAM_ENUM
   - CPC_PARAM_STRING

✅ Parameter Flags:
   - CPC_PARAM_REQUIRED
   - CPC_PARAM_ADVANCED
   - CPC_PARAM_READONLY

✅ Parameter Schemas:
   - g_params_sector[]     - Standard sector parameters
   - g_params_flux[]       - Flux-specific parameters
   - g_params_nibble[]     - Nibble-specific parameters
   - g_params_raw_required[] - RAW format required parameters

✅ Formats mit Parameters:
   - 52 formats registered
   - Each has params pointer + count
```

---

## 🎯 **ZIEL: UNIVERSAL PARAMETER SYSTEM**

### **Erweitern auf:**

```
✅ ALL 237+ formats
✅ Platform-specific parameters (C64, Amiga, Apple II, etc.)
✅ Tool-specific parameters (Alcohol 120%, CloneCD, etc.)
✅ Format-specific parameters (Protection, custom geometry, etc.)
✅ Advanced/Expert parameters
✅ Conversion parameters
```

---

## 📋 **PARAMETER KATEGORIEN (ERWEITERT)**

### **1️⃣ CORE PARAMETERS (Alle Formate)**

#### **A. Geometry Parameters** - `g_params_geometry[]`

```c
static const cpc_dsk_param_desc_t g_params_geometry[] = {
    // Basic geometry
    {"cylinders", "Cylinders", CPC_PARAM_INT, 0u, 
     40ll, 1ll, 200ll, NULL, NULL,
     "Cylinders (tracks per side). 0 = auto-detect from header."},
    
    {"heads", "Heads/Sides", CPC_PARAM_INT, 0u,
     2ll, 1ll, 2ll, NULL, NULL,
     "Number of heads/sides. 0 = auto-detect."},
    
    {"sectors_per_track", "Sectors/Track", CPC_PARAM_INT, 0u,
     0ll, 1ll, 255ll, NULL, NULL,
     "Sectors per track. 0 = auto-detect; required for RAW formats."},
    
    {"sector_size", "Sector Size", CPC_PARAM_INT, 0u,
     512ll, 128ll, 8192ll, NULL, NULL,
     "Logical sector size in bytes (128/256/512/1024/2048/4096/8192)."},
    
    // Variable geometry (C64, etc.)
    {"variable_geometry", "Variable Geometry", CPC_PARAM_BOOL, CPC_PARAM_ADVANCED,
     0ll, 0ll, 1ll, NULL, NULL,
     "Enable variable sectors per track (C64 zones, Victor 9000, etc.)."},
    
    {"zone_table", "Zone Table", CPC_PARAM_STRING, CPC_PARAM_ADVANCED,
     0ll, 0ll, 0ll, NULL, "21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,19,19,19,19,19,19,19,18,18,18,18,18,18,17,17,17,17,17",
     "Comma-separated sectors per track for each zone (C64: tracks 1-17=21, 18-24=19, 25-30=18, 31-35=17)."},
};
```

---

#### **B. Encoding Parameters** - `g_params_encoding[]`

```c
static const cpc_dsk_param_desc_t g_params_encoding[] = {
    {"encoding", "Encoding", CPC_PARAM_ENUM, 0u,
     2ll, 0ll, 0ll, "fm|mfm|gcr|apple_gcr|m2fm|amiga_gcr", NULL,
     "On-disk line encoding: FM (single density), MFM (double density), GCR (C64/Apple), M2FM (Amiga)."},
    
    {"data_rate_kbps", "Data Rate (kbps)", CPC_PARAM_INT, 0u,
     250ll, 50ll, 1000ll, NULL, NULL,
     "Nominal data rate. Common: 250 (DD), 300 (HD), 500 (HD), 1000 (ED)."},
    
    {"rpm", "RPM", CPC_PARAM_INT, 0u,
     300ll, 150ll, 600ll, NULL, NULL,
     "Rotation speed. 300 for 3.5\"/5.25\" DD, 360 for some 5.25\" HD, 600 for ED."},
    
    {"bitcell_ns", "Bitcell (ns)", CPC_PARAM_INT, CPC_PARAM_ADVANCED,
     2000ll, 500ll, 8000ll, NULL, NULL,
     "Nominal bitcell width in nanoseconds for timing analysis."},
};
```

---

#### **C. Track Layout Parameters** - `g_params_track_layout[]`

```c
static const cpc_dsk_param_desc_t g_params_track_layout[] = {
    {"interleave", "Interleave", CPC_PARAM_INT, 0u,
     1ll, 1ll, 32ll, NULL, NULL,
     "Sector interleave factor. 1 = sequential (1,2,3...), 2 = every other (1,3,5,2,4,6...)."},
    
    {"skew", "Track Skew", CPC_PARAM_INT, CPC_PARAM_ADVANCED,
     0ll, 0ll, 32ll, NULL, NULL,
     "Sector offset between adjacent tracks to optimize seek time."},
    
    {"first_sector_id", "First Sector ID", CPC_PARAM_INT, 0u,
     1ll, 0ll, 255ll, NULL, NULL,
     "ID of first sector. Most formats: 1, C64: 0, some IBM: 0 or 1."},
    
    {"gap3_length", "GAP3 Length", CPC_PARAM_INT, CPC_PARAM_ADVANCED,
     84ll, 0ll, 255ll, NULL, NULL,
     "Gap between sectors in bytes. Depends on format (IBM: 84, Amiga: 0)."},
    
    {"gap4_length", "GAP4 Fill Length", CPC_PARAM_INT, CPC_PARAM_ADVANCED,
     0ll, 0ll, 1024ll, NULL, NULL,
     "Gap at end of track in bytes. 0 = fill to end of track."},
    
    {"fill_byte", "Fill Byte", CPC_PARAM_INT, CPC_PARAM_ADVANCED,
     0xF6ll, 0ll, 255ll, NULL, NULL,
     "Byte value for gap filling (IBM: 0xF6, Amiga: 0x00)."},
};
```

---

### **2️⃣ FLUX-SPECIFIC PARAMETERS** - `g_params_flux[]`

```c
static const cpc_dsk_param_desc_t g_params_flux[] = {
    // Timing
    {"timebase_ns", "Timebase (ns)", CPC_PARAM_INT, 0u,
     25ll, 1ll, 1000ll, NULL, NULL,
     "Flux tick timebase in nanoseconds. SCP: 25ns, KryoFlux: 40MHz = 25ns."},
    
    {"index_mode", "Index Mode", CPC_PARAM_ENUM, 0u,
     0ll, 0ll, 0ll, "header|reconstruct|ignore", NULL,
     "Index pulse handling: from header, reconstruct from gaps, or ignore."},
    
    // Quality/Preservation
    {"preserve_jitter", "Preserve Jitter", CPC_PARAM_BOOL, 0u,
     1ll, 0ll, 1ll, NULL, NULL,
     "Keep per-sample timing variance; do NOT smooth/normalize."},
    
    {"preserve_weak", "Preserve Weak Bits", CPC_PARAM_BOOL, 0u,
     1ll, 0ll, 1ll, NULL, NULL,
     "Keep weak/ambiguous bit regions as metadata instead of collapsing."},
    
    // PLL/Decoding
    {"pll_strength", "PLL Strength", CPC_PARAM_INT, 0u,
     5ll, 0ll, 10ll, NULL, NULL,
     "PLL loop gain for bitcell quantization. 0=off, higher=stronger locking."},
    
    {"pll_phase_adjust", "PLL Phase Adjust", CPC_PARAM_BOOL, CPC_PARAM_ADVANCED,
     1ll, 0ll, 1ll, NULL, NULL,
     "Enable phase adjustment in PLL for better timing recovery."},
    
    // Multi-revolution
    {"revolutions", "Revolutions", CPC_PARAM_INT, 0u,
     5ll, 1ll, 20ll, NULL, NULL,
     "Number of revolutions to capture/analyze for multi-rev consensus."},
    
    {"revolution_mode", "Revolution Mode", CPC_PARAM_ENUM, 0u,
     0ll, 0ll, 0ll, "first|best|consensus|all", NULL,
     "How to use multiple revolutions: first only, best quality, consensus, or keep all."},
};
```

---

### **3️⃣ PLATFORM-SPECIFIC PARAMETERS**

#### **A. C64 Parameters** - `g_params_c64[]`

```c
static const cpc_dsk_param_desc_t g_params_c64[] = {
    {"c64_zones", "Use C64 Zones", CPC_PARAM_BOOL, 0u,
     1ll, 0ll, 1ll, NULL, NULL,
     "Use C64 variable zone geometry (21/19/18/17 sectors)."},
    
    {"c64_error_bytes", "Error Bytes", CPC_PARAM_BOOL, 0u,
     1ll, 0ll, 1ll, NULL, NULL,
     "Include 35-byte error table at end of D64 (for copy protection)."},
    
    {"c64_track_alignment", "Track Alignment", CPC_PARAM_ENUM, 0u,
     0ll, 0ll, 0ll, "standard|nondos", NULL,
     "Track alignment: standard DOS or non-DOS for protections."},
    
    {"c64_gcr_sync", "GCR Sync Length", CPC_PARAM_INT, CPC_PARAM_ADVANCED,
     10ll, 5ll, 20ll, NULL, NULL,
     "Number of 0xFF sync bytes before sector header (usually 10)."},
    
    {"c64_speed_zones", "Speed Zones", CPC_PARAM_BOOL, CPC_PARAM_ADVANCED,
     0ll, 0ll, 1ll, NULL, NULL,
     "Use different rotation speeds per zone (advanced copy protection)."},
};
```

---

#### **B. Amiga Parameters** - `g_params_amiga[]`

```c
static const cpc_dsk_param_desc_t g_params_amiga[] = {
    {"amiga_bootblock", "Bootblock Type", CPC_PARAM_ENUM, 0u,
     0ll, 0ll, 0ll, "dos|ndos|custom", NULL,
     "Bootblock type: standard DOS, non-DOS, or custom."},
    
    {"amiga_filesystem", "Filesystem", CPC_PARAM_ENUM, 0u,
     0ll, 0ll, 0ll, "ofs|ffs|intl_ofs|intl_ffs|cache_ofs|cache_ffs", NULL,
     "AmigaDOS filesystem: OFS (Old), FFS (Fast), with international/cache variants."},
    
    {"amiga_sectors_per_track", "Sectors/Track", CPC_PARAM_INT, 0u,
     11ll, 10ll, 22ll, NULL, NULL,
     "Amiga sectors per track. Standard: 11 (DD), 22 (HD)."},
    
    {"amiga_track_gap", "Track Gap", CPC_PARAM_INT, CPC_PARAM_ADVANCED,
     0ll, 0ll, 1024ll, NULL, NULL,
     "Gap between sectors in Amiga MFM (usually 0, filled by controller)."},
    
    {"amiga_verify_checksums", "Verify Checksums", CPC_PARAM_BOOL, 0u,
     1ll, 0ll, 1ll, NULL, NULL,
     "Verify header/data checksums when reading (detect corruption)."},
};
```

---

#### **C. Apple II Parameters** - `g_params_apple2[]`

```c
static const cpc_dsk_param_desc_t g_params_apple2[] = {
    {"apple2_dos_version", "DOS Version", CPC_PARAM_ENUM, 0u,
     0ll, 0ll, 0ll, "dos33|dos32|prodos", NULL,
     "Apple II DOS version: DOS 3.3, DOS 3.2, or ProDOS."},
    
    {"apple2_sectors_per_track", "Sectors/Track", CPC_PARAM_INT, 0u,
     16ll, 13ll, 32ll, NULL, NULL,
     "DOS 3.2: 13, DOS 3.3/ProDOS: 16, ProDOS HD: 32."},
    
    {"apple2_sector_skew", "Sector Skew", CPC_PARAM_STRING, 0u,
     0ll, 0ll, 0ll, NULL, "0,13,11,9,7,5,3,1,14,12,10,8,6,4,2,15",
     "DOS 3.3 sector skew table (physical → logical mapping)."},
    
    {"apple2_gcr_encoding", "GCR Encoding", CPC_PARAM_ENUM, 0u,
     0ll, 0ll, 0ll, "6_and_2|5_and_3", NULL,
     "GCR encoding: 6-and-2 (DOS 3.3+) or 5-and-3 (DOS 3.2)."},
    
    {"apple2_volume_number", "Volume Number", CPC_PARAM_INT, 0u,
     254ll, 0ll, 255ll, NULL, NULL,
     "Volume number stored in sector headers (usually 254)."},
};
```

---

#### **D. Atari 8-bit Parameters** - `g_params_atari8[]`

```c
static const cpc_dsk_param_desc_t g_params_atari8[] = {
    {"atari_density", "Density", CPC_PARAM_ENUM, 0u,
     0ll, 0ll, 0ll, "sd|ed|dd|qd", NULL,
     "Disk density: SD (90K), ED (130K), DD (180K), QD (360K)."},
    
    {"atari_sector_size", "Sector Size", CPC_PARAM_INT, 0u,
     128ll, 128ll, 256ll, NULL, NULL,
     "Sector size: 128 bytes (SD) or 256 bytes (DD)."},
    
    {"atari_dos_version", "DOS Version", CPC_PARAM_ENUM, 0u,
     0ll, 0ll, 0ll, "dos2|dos25|dos3|mydos|spartados", NULL,
     "Atari DOS version: DOS 2.0/2.5/3, MyDOS, or SpartaDOS."},
    
    {"atari_boot_sectors", "Boot Sectors", CPC_PARAM_INT, CPC_PARAM_ADVANCED,
     3ll, 0ll, 128ll, NULL, NULL,
     "Number of boot sectors (usually 3, some formats vary)."},
};
```

---

### **4️⃣ TOOL-SPECIFIC PARAMETERS**

#### **A. Alcohol 120% Parameters** - `g_params_alcohol[]`

```c
static const cpc_dsk_param_desc_t g_params_alcohol[] = {
    {"alcohol_datatype", "Data Type", CPC_PARAM_ENUM, 0u,
     0ll, 0ll, 0ll, "mode1|mode2|audio|mixed", NULL,
     "CD/DVD data type for Alcohol 120% images (MDF/MDS)."},
    
    {"alcohol_subchannel", "Subchannel Data", CPC_PARAM_BOOL, 0u,
     1ll, 0ll, 1ll, NULL, NULL,
     "Include subchannel data (for copy protection, CD+G, etc.)."},
    
    {"alcohol_raw_mode", "RAW Mode", CPC_PARAM_BOOL, CPC_PARAM_ADVANCED,
     0ll, 0ll, 1ll, NULL, NULL,
     "Use RAW sector mode (2352 bytes/sector instead of 2048)."},
    
    {"alcohol_dpm", "DPM Protection", CPC_PARAM_BOOL, 0u,
     0ll, 0ll, 1ll, NULL, NULL,
     "Enable Data Position Measurement for protection scanning."},
};
```

---

#### **B. CloneCD Parameters** - `g_params_clonecd[]`

```c
static const cpc_dsk_param_desc_t g_params_clonecd[] = {
    {"clonecd_regenerate", "Regenerate Sectors", CPC_PARAM_BOOL, 0u,
     0ll, 0ll, 1ll, NULL, NULL,
     "Regenerate error sectors instead of raw copy."},
    
    {"clonecd_intelligent", "Intelligent Bad Sector Scanner", CPC_PARAM_BOOL, 0u,
     1ll, 0ll, 1ll, NULL, NULL,
     "Use intelligent scanning for copy-protected sectors."},
    
    {"clonecd_fast_error_skip", "Fast Error Skip", CPC_PARAM_BOOL, 0u,
     0ll, 0ll, 1ll, NULL, NULL,
     "Skip unreadable sectors quickly (faster but less thorough)."},
};
```

---

#### **C. KryoFlux Parameters** - `g_params_kryoflux[]`

```c
static const cpc_dsk_param_desc_t g_params_kryoflux[] = {
    {"kryoflux_stream_format", "Stream Format", CPC_PARAM_ENUM, 0u,
     0ll, 0ll, 0ll, "stream|oricstream|ipfstream", NULL,
     "KryoFlux stream format variant."},
    
    {"kryoflux_index_signal", "Index Signal", CPC_PARAM_ENUM, 0u,
     0ll, 0ll, 0ll, "auto|forced|ignored", NULL,
     "How to handle index pulse: auto-detect, force present, or ignore."},
    
    {"kryoflux_cell_size", "Cell Size", CPC_PARAM_ENUM, CPC_PARAM_ADVANCED,
     0ll, 0ll, 0ll, "auto|1us|2us|4us|8us", NULL,
     "Sample cell size (usually auto-detect from flux transitions)."},
};
```

---

### **5️⃣ PROTECTION-SPECIFIC PARAMETERS**

#### **A. Copy Protection Parameters** - `g_params_protection[]`

```c
static const cpc_dsk_param_desc_t g_params_protection[] = {
    {"detect_protection", "Detect Protection", CPC_PARAM_BOOL, 0u,
     1ll, 0ll, 1ll, NULL, NULL,
     "Automatically detect copy protection schemes."},
    
    {"protection_scheme", "Protection Scheme", CPC_PARAM_ENUM, CPC_PARAM_ADVANCED,
     0ll, 0ll, 0ll, "none|rapidlok|robnorthen|tages|v-max|softlock|copylock|custom", NULL,
     "Known protection scheme to handle specially."},
    
    {"preserve_bad_sectors", "Preserve Bad Sectors", CPC_PARAM_BOOL, 0u,
     1ll, 0ll, 1ll, NULL, NULL,
     "Keep intentional bad sectors (part of protection)."},
    
    {"preserve_weak_bits", "Preserve Weak Bits", CPC_PARAM_BOOL, 0u,
     1ll, 0ll, 1ll, NULL, NULL,
     "Keep weak/fuzzy bits (protection technique)."},
    
    {"preserve_timing", "Preserve Timing", CPC_PARAM_BOOL, 0u,
     1ll, 0ll, 1ll, NULL, NULL,
     "Preserve exact timing information (for timing-based protection)."},
    
    {"track_length_tolerance", "Track Length Tolerance (%)", CPC_PARAM_INT, CPC_PARAM_ADVANCED,
     5ll, 0ll, 50ll, NULL, NULL,
     "Allow track length variance (some protections use non-standard track lengths)."},
};
```

---

### **6️⃣ CONVERSION-SPECIFIC PARAMETERS**

#### **A. Conversion Parameters** - `g_params_conversion[]`

```c
static const cpc_dsk_param_desc_t g_params_conversion[] = {
    {"conversion_quality", "Conversion Quality", CPC_PARAM_ENUM, 0u,
     1ll, 0ll, 0ll, "fast|normal|best|archival", NULL,
     "Quality/speed tradeoff: fast, normal, best (slower), or archival (preserve everything)."},
    
    {"auto_geometry", "Auto-detect Geometry", CPC_PARAM_BOOL, 0u,
     1ll, 0ll, 1ll, NULL, NULL,
     "Automatically detect cylinders/heads/sectors from source."},
    
    {"verify_after_conversion", "Verify After Conversion", CPC_PARAM_BOOL, 0u,
     0ll, 0ll, 1ll, NULL, NULL,
     "Verify converted image matches source (slower but safer)."},
    
    {"compression_level", "Compression Level", CPC_PARAM_INT, 0u,
     6ll, 0ll, 9ll, NULL, NULL,
     "Compression level for formats that support it (0=none, 9=maximum)."},
    
    {"preserve_metadata", "Preserve Metadata", CPC_PARAM_BOOL, 0u,
     1ll, 0ll, 1ll, NULL, NULL,
     "Preserve all metadata (timestamps, comments, protection info)."},
};
```

---

## 🗂️ **FORMAT-SPECIFIC PARAMETER MAPPING**

### **Wie Parameter zu Formaten zugeordnet werden:**

```c
// In cpc_dsk.c - UPDATED

// Combine multiple parameter sets
static const cpc_dsk_param_desc_t* g_params_d64[] = {
    // Include standard geometry
    g_params_geometry,
    // Include C64-specific
    g_params_c64,
    // Include encoding
    g_params_encoding,
    // Include protection (optional)
    g_params_protection,
};

// Format definition
{
    CPC_DSK_FMT_D64,
    "Commodore 1541 D64 image",
    "d64",
    CPC_DSK_KIND_SECTOR_IMAGE,
    CPC_DSK_ENC_GCR,
    g_params_d64,                    // Combined parameter set
    sizeof(g_params_d64) / sizeof(g_params_d64[0])
}
```

---

## 📊 **COMPLETE PARAMETER SET ALLOCATION**

### **Format → Parameter Sets Mapping:**

| Format | Geometry | Encoding | Track | Flux | Platform | Tool | Protection |
|--------|----------|----------|-------|------|----------|------|------------|
| **ADF** | ✅ | ✅ | ✅ | ❌ | Amiga | ❌ | ✅ |
| **D64** | ✅ | ✅ | ✅ | ❌ | C64 | ❌ | ✅ |
| **IMG** | ✅ | ✅ | ✅ | ❌ | IBM | ❌ | ❌ |
| **SCP** | ✅ | ✅ | ❌ | ✅ | ❌ | KryoFlux | ✅ |
| **ATR** | ✅ | ✅ | ✅ | ❌ | Atari8 | ❌ | ❌ |
| **NIB** | ✅ | ✅ | ✅ | ❌ | Apple2 | ❌ | ✅ |
| **WOZ** | ✅ | ✅ | ✅ | ✅ | Apple2 | ❌ | ✅ |
| **MDF** | ❌ | ❌ | ❌ | ❌ | ❌ | Alcohol | ❌ |
| **IPF** | ✅ | ✅ | ✅ | ✅ | Multi | ❌ | ✅ |

---

## 💻 **IMPLEMENTATION PLAN**

### **Phase 1: Extend Core Parameters (Week 1)**

```bash
# Update cpc_dsk.c

1. Add new parameter sets:
   touch formats/format_registry/params_geometry_extended.c
   touch formats/format_registry/params_track_layout.c
   touch formats/format_registry/params_protection.c
   touch formats/format_registry/params_conversion.c

2. Implement parameter sets:
   static const cpc_dsk_param_desc_t g_params_geometry_extended[] = { ... };
   static const cpc_dsk_param_desc_t g_params_track_layout[] = { ... };
   static const cpc_dsk_param_desc_t g_params_protection[] = { ... };
   static const cpc_dsk_param_desc_t g_params_conversion[] = { ... };

3. Test with existing formats:
   ./test_param_system
```

---

### **Phase 2: Add Platform-Specific (Week 2)**

```bash
# Create platform parameter modules

mkdir -p formats/format_registry/platforms/

1. C64 parameters:
   touch formats/format_registry/platforms/c64_params.c
   static const cpc_dsk_param_desc_t g_params_c64[] = { ... };

2. Amiga parameters:
   touch formats/format_registry/platforms/amiga_params.c
   static const cpc_dsk_param_desc_t g_params_amiga[] = { ... };

3. Apple II parameters:
   touch formats/format_registry/platforms/apple2_params.c
   static const cpc_dsk_param_desc_t g_params_apple2[] = { ... };

4. Atari 8-bit parameters:
   touch formats/format_registry/platforms/atari8_params.c
   static const cpc_dsk_param_desc_t g_params_atari8[] = { ... };

5. IBM PC parameters:
   touch formats/format_registry/platforms/ibm_params.c
   static const cpc_dsk_param_desc_t g_params_ibm[] = { ... };
```

---

### **Phase 3: Tool-Specific Parameters (Week 3)**

```bash
mkdir -p formats/format_registry/tools/

1. Alcohol 120%:
   touch formats/format_registry/tools/alcohol_params.c

2. CloneCD:
   touch formats/format_registry/tools/clonecd_params.c

3. KryoFlux:
   touch formats/format_registry/tools/kryoflux_params.c

4. DiscFerret:
   touch formats/format_registry/tools/discferret_params.c

5. Greaseweazle:
   touch formats/format_registry/tools/greaseweazle_params.c
```

---

### **Phase 4: Apply to ALL Formats (Week 4)**

```bash
# Update format definitions for all 237 formats

# Script to auto-generate parameter assignments
./scripts/generate_format_params.py

# Input: Format list + platform + encoding
# Output: Optimized parameter set assignments

Example:
  ADF → {geometry, encoding, track_layout, amiga, protection}
  D64 → {geometry, encoding, track_layout, c64, protection}
  IMG → {geometry, encoding, track_layout, ibm}
  SCP → {geometry, encoding, flux, protection, conversion}
```

---

## 🎨 **GUI INTEGRATION**

### **How GUI will use parameters:**

```cpp
// In FormatPresetManager or similar

// Get format parameters
const cpc_dsk_format_desc_t *desc = cpc_dsk_desc(CPC_DSK_FMT_D64);

// Build UI dynamically
for (size_t i = 0; i < desc->params_count; i++) {
    const cpc_dsk_param_desc_t *p = &desc->params[i];
    
    // Check if advanced
    bool is_advanced = (p->flags & CPC_PARAM_ADVANCED) != 0;
    bool is_required = (p->flags & CPC_PARAM_REQUIRED) != 0;
    
    // Create UI widget based on type
    switch (p->type) {
        case CPC_PARAM_INT:
            // Create QSpinBox with min/max/default
            auto *spin = new QSpinBox();
            spin->setRange(p->min_i, p->max_i);
            spin->setValue(p->def_i);
            spin->setToolTip(p->help);
            if (is_advanced) {
                // Add to "Advanced" tab
                advancedLayout->addWidget(new QLabel(p->label), row, 0);
                advancedLayout->addWidget(spin, row, 1);
            } else {
                // Add to main UI
                mainLayout->addWidget(new QLabel(p->label), row, 0);
                mainLayout->addWidget(spin, row, 1);
            }
            break;
            
        case CPC_PARAM_BOOL:
            // Create QCheckBox
            auto *check = new QCheckBox(p->label);
            check->setChecked(p->def_i != 0);
            check->setToolTip(p->help);
            break;
            
        case CPC_PARAM_ENUM:
            // Create QComboBox
            auto *combo = new QComboBox();
            QStringList values = QString(p->enum_values).split('|');
            combo->addItems(values);
            combo->setCurrentIndex(p->def_i);
            combo->setToolTip(p->help);
            break;
            
        case CPC_PARAM_STRING:
            // Create QLineEdit
            auto *edit = new QLineEdit(p->def_s);
            edit->setToolTip(p->help);
            break;
    }
}

// Required parameters get red label
if (is_required) {
    label->setStyleSheet("QLabel { color: red; font-weight: bold; }");
}
```

---

## 📚 **PARAMETER PRESETS (BONUS)**

### **Pre-defined configurations:**

```c
// formats/format_registry/presets.c

typedef struct cpc_dsk_preset {
    const char *name;
    cpc_dsk_format_id_t format;
    // Parameter overrides
    struct {
        const char *key;
        int64_t value_i;
        const char *value_s;
    } overrides[16];
} cpc_dsk_preset_t;

static const cpc_dsk_preset_t g_presets[] = {
    // C64 presets
    {
        "C64 Standard (35 tracks, variable zones)",
        CPC_DSK_FMT_D64,
        {
            {"cylinders", 35, NULL},
            {"c64_zones", 1, NULL},
            {"c64_error_bytes", 1, NULL},
            {NULL, 0, NULL}
        }
    },
    {
        "C64 Extended (40 tracks)",
        CPC_DSK_FMT_D64,
        {
            {"cylinders", 40, NULL},
            {"c64_zones", 1, NULL},
            {NULL, 0, NULL}
        }
    },
    
    // Amiga presets
    {
        "Amiga Standard (OFS, 11 sectors)",
        CPC_DSK_FMT_ADF,
        {
            {"cylinders", 80, NULL},
            {"amiga_sectors_per_track", 11, NULL},
            {"amiga_filesystem", 0, "ofs"},  // OFS
            {NULL, 0, NULL}
        }
    },
    {
        "Amiga HD (FFS, 22 sectors)",
        CPC_DSK_FMT_ADF,
        {
            {"cylinders", 80, NULL},
            {"amiga_sectors_per_track", 22, NULL},
            {"amiga_filesystem", 1, "ffs"},  // FFS
            {NULL, 0, NULL}
        }
    },
    
    // Apple II presets
    {
        "Apple II DOS 3.3 (16 sectors)",
        CPC_DSK_FMT_DO,
        {
            {"cylinders", 35, NULL},
            {"apple2_dos_version", 0, "dos33"},
            {"apple2_sectors_per_track", 16, NULL},
            {NULL, 0, NULL}
        }
    },
    {
        "Apple II DOS 3.2 (13 sectors)",
        CPC_DSK_FMT_DO,
        {
            {"cylinders", 35, NULL},
            {"apple2_dos_version", 1, "dos32"},
            {"apple2_sectors_per_track", 13, NULL},
            {NULL, 0, NULL}
        }
    },
};
```

---

## ✅ **ZUSAMMENFASSUNG**

### **KANN MAN DAS AUF ALLE FORMATE ANWENDEN?**

**ANTWORT: JA! ✅**

```
✅ Alle 237+ Formate bekommen Parameter-Sets
✅ Kombinierbar (Geometry + Platform + Tool + Protection)
✅ GUI baut sich automatisch auf
✅ Presets für gängige Konfigurationen
✅ Required/Optional/Advanced Flags
✅ Validation durch min/max/enum
✅ Erweiterbar ohne Code-Änderungen
```

---

### **VORTEILE:**

1. **Einheitlich:** Alle Formate nutzen gleiche Parameter-API
2. **Flexibel:** Kombinierbare Parameter-Sets
3. **Automatisch:** GUI generiert sich selbst
4. **Erweiterbar:** Neue Parameter ohne Core-Änderungen
5. **Validierung:** Type-safe mit Bounds-Checking
6. **Dokumentiert:** Help-Text pro Parameter
7. **Presets:** Vordefinierte Konfigurationen

---

### **IMPLEMENTATION:**

```
Phase 1: Core Extended       - 1 Woche
Phase 2: Platform-Specific   - 1 Woche
Phase 3: Tool-Specific       - 1 Woche
Phase 4: Apply to ALL        - 1 Woche
───────────────────────────────────────
TOTAL:                        4 Wochen
```

---

### **RESULT:**

```
237+ Formats × Dynamic Parameters = 
UNIVERSAL CONFIGURATION SYSTEM! 🎉
```

**READY TO IMPLEMENT!** 🚀
