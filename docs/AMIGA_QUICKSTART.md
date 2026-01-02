# 🎮 Amiga Support - Quick Start Guide

## UnifiedFloppyTool v2.6.1 - Amiga Edition

---

## 🚀 **Quick Start**

### **1. Build with Amiga Support**

```bash
mkdir build && cd build
cmake ..
make

# You now have:
# - libflux_core.a (with AmigaDOS filesystem)
# - libflux_format.a (with ADF loader)
# - example_amiga_adf (example program)
```

### **2. Test with an ADF File**

```bash
# Run the example
./example_amiga_adf path/to/workbench.adf

# Expected output:
# ✅ Valid ADF file detected
# ✅ ADF loaded: 80 cylinders, 2 heads
# ✅ Filesystem: OFS (Old File System)
# Root directory contents:
#   [DIR ] C                              ...
#   [DIR ] Devs                          ...
#   [DIR ] Libs                          ...
#   [FILE] Startup-Sequence             ...
```

---

## 📚 **API Overview**

### **AmigaDOS Filesystem API**

```c
#include "filesystems/amigados_filesystem.h"

// Open filesystem
amigados_fs_t *fs = amigados_open(&disk);

// Detect filesystem type
amigados_type_t type = amigados_detect_type(&disk);
// Returns: AMIGADOS_OFS, AMIGADOS_FFS, AMIGADOS_OFS_INTL, AMIGADOS_FFS_INTL

// List directory
void callback(void *user_data, const amigados_file_info_t *info) {
    printf("%s: %u bytes\n", info->filename, info->size);
}
amigados_list_directory(fs, "/", callback, NULL);

// Read file
amigados_file_info_t info;
if (amigados_read_file(fs, "S/Startup-Sequence", &info) == 0) {
    printf("File size: %u bytes\n", info.size);
}

// Close filesystem
amigados_close(fs);
```

### **ADF Format API**

```c
#include "flux_format/adf.h"

// Probe (detect) ADF file
int confidence = adf_probe("disk.adf");
if (confidence > 0) {
    // Is an ADF file
}

// Read ADF
ufm_disk_t disk;
ufm_disk_init(&disk);
adf_read("disk.adf", &disk);

// ... use disk ...

// Write ADF
adf_write("output.adf", &disk);

ufm_disk_free(&disk);
```

---

## 🎯 **Supported Features**

### **AmigaDOS Filesystem** ✅

- ✅ **OFS (Old File System)** - Original Amiga filesystem
- ✅ **FFS (Fast File System)** - Faster variant
- ✅ **International Variants** - Support for international character sets
- ✅ **File Operations**
  - Read file metadata (name, size, protection, date)
  - Directory listing (recursive)
  - File type detection
- ✅ **Date/Time** - Full Amiga date format support (days since 1978-01-01)
- ✅ **Protection Bits** - Read Amiga protection flags

### **ADF Format** ✅

- ✅ **Standard ADF** - 880KB double-density disks (most common)
- ✅ **Read/Write** - Full bidirectional support
- ✅ **Format Detection** - Automatic ADF recognition
- ✅ **Compatibility** - Works with WinUAE, FS-UAE, etc.

### **Not Yet Supported** ⚠️

- ⚠️ **HD ADF** (1.76MB) - Not tested, may work
- ⚠️ **ADZ** - Compressed ADF (gzip)
- ⚠️ **DMS** - Disk Masher System
- ⚠️ **IPF** - Interchangeable Preservation Format
- ⚠️ **Extended ADF** - Non-standard variants

---

## 🔧 **Code Examples**

### **Example 1: List All Files Recursively**

```c
#include "filesystems/amigados_filesystem.h"

void list_recursive(amigados_fs_t *fs, const char *path, int depth)
{
    // Print indentation
    for (int i = 0; i < depth; i++) printf("  ");
    printf("%s/\n", path);
    
    // List callback
    void callback(void *user_data, const amigados_file_info_t *info) {
        for (int i = 0; i < depth + 1; i++) printf("  ");
        
        if (info->is_directory) {
            printf("[DIR ] %s\n", info->filename);
            // Recurse into directory
            char subpath[256];
            snprintf(subpath, sizeof(subpath), "%s/%s", path, info->filename);
            list_recursive(fs, subpath, depth + 1);
        } else {
            printf("[FILE] %s (%u bytes)\n", info->filename, info->size);
        }
    }
    
    amigados_list_directory(fs, path, callback, NULL);
}

// Usage:
list_recursive(fs, "/", 0);
```

### **Example 2: Extract File to Host Filesystem**

```c
int extract_file(amigados_fs_t *fs, const char *amiga_path, const char *host_path)
{
    // Read file info
    amigados_file_info_t info;
    if (amigados_read_file(fs, amiga_path, &info) != 0) {
        fprintf(stderr, "File not found: %s\n", amiga_path);
        return -1;
    }
    
    // Allocate buffer
    uint8_t *buffer = malloc(info.size);
    if (!buffer) {
        return -1;
    }
    
    // Read file data (implementation depends on your AmigaDOS API)
    // This is a simplified example
    amigados_read_file_data(fs, amiga_path, buffer, info.size);
    
    // Write to host file
    FILE *fp = fopen(host_path, "wb");
    if (fp) {
        fwrite(buffer, 1, info.size, fp);
        fclose(fp);
    }
    
    free(buffer);
    return 0;
}

// Usage:
extract_file(fs, "C/Dir", "./dir_command");
```

### **Example 3: Convert Directory to ADF**

```c
int create_adf_from_directory(const char *host_dir, const char *adf_file)
{
    // Create empty disk
    ufm_disk_t disk;
    ufm_disk_init(&disk);
    
    // Set up Amiga geometry (standard 880KB)
    disk.cyls = 80;
    disk.heads = 2;
    // ... allocate tracks, initialize filesystem ...
    
    // Scan host directory and add files
    // (Implementation depends on your requirements)
    
    // Write ADF
    int result = adf_write(adf_file, &disk);
    
    ufm_disk_free(&disk);
    return result;
}
```

---

## 📖 **File Information Structure**

```c
typedef struct {
    char filename[32];      // Amiga filename (max 31 chars + null)
    uint32_t size;          // File size in bytes
    bool is_directory;      // true if directory, false if file
    uint32_t protection;    // Protection bits (RWED etc.)
    uint32_t days;          // Days since 1978-01-01
    uint32_t mins;          // Minutes since midnight
    uint32_t ticks;         // Ticks (1/50th second)
} amigados_file_info_t;
```

### **Protection Bits:**

AmigaDOS uses different protection bits than Unix:
- **D** - Delete protected
- **E** - Execute
- **W** - Write
- **R** - Read
- **A** - Archive (backup bit)
- **P** - Pure (re-entrant)
- **S** - Script
- **H** - Hold (not yet implemented in AmigaDOS 1.x)

---

## 🎓 **Technical Details**

### **Filesystem Detection:**

The library automatically detects filesystem type by reading:
1. **Boot block** (sector 0-1)
2. **Root block** (sector 880)
3. **Filesystem version** from root block
4. **International mode** flag

### **Disk Geometry:**

Standard Amiga floppy:
- **Cylinders:** 80 (tracks 0-79)
- **Heads:** 2 (sides 0-1)
- **Sectors per track:** 11
- **Bytes per sector:** 512
- **Total capacity:** 880KB (901,120 bytes)

### **Encoding:**

- Amiga uses **Amiga MFM** encoding (not IBM MFM!)
- Different sync marks and sector headers
- Checksums on headers and data
- Interleave factor varies by track

---

## ⚙️ **Build Options**

```bash
# Standard build (includes Amiga support)
cmake ..
make

# Build with examples
cmake -DBUILD_EXAMPLES=ON ..
make

# Build with tests
cmake -DBUILD_TESTS=ON ..
make

# Build everything
cmake -DBUILD_GUI=ON \
      -DBUILD_CLI=ON \
      -DBUILD_TESTS=ON \
      -DBUILD_EXAMPLES=ON \
      ..
make
```

---

## 🐛 **Troubleshooting**

### **"Invalid ADF file" error:**

- Check file size (should be 901,120 bytes for standard DD)
- Verify file is not corrupted
- Try with a known-good ADF (e.g., Workbench disk)

### **"Filesystem not recognized" error:**

- File may not have valid boot block
- Disk may not be formatted
- Disk may use non-standard format

### **Missing files in directory listing:**

- Check if using OFS or FFS (different directory structures)
- Verify filesystem is not corrupted
- Check if disk is full (can affect directory chains)

---

## 📚 **Additional Resources**

### **Amiga Disk Formats:**
- OFS/FFS Documentation: AmigaDOS Technical Reference Manual
- ADF Specification: Industry standard (WinUAE documentation)

### **Tools for Testing:**
- **WinUAE** - Amiga emulator (Windows)
- **FS-UAE** - Cross-platform Amiga emulator
- **adf-tools** - Command-line ADF utilities

### **Sample ADF Files:**
- **Workbench 1.3** - Classic Amiga OS
- **Demo disks** - Public domain Amiga software
- **Game demos** - Test copy protection handling

---

## 🏆 **Credits**

**AmigaDOS Filesystem:**
- Based on **Keir Fraser's amiga-stuff** implementation
- Adapted for UnifiedFloppyTool architecture
- Thanks to the Amiga preservation community

**ADF Format:**
- Based on industry-standard ADF specification
- Compatible with major Amiga emulators

---

**UnifiedFloppyTool v2.6.1** - Amiga Edition 🎮

*Preserve your Amiga history!*
