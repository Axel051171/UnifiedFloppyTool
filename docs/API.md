# UnifiedFloppyTool API Documentation

**Version:** 3.8.7  
**Date:** 2026-01-13

---

## Overview

UFT provides a modular C API for floppy disk preservation and analysis.

### Core Modules

| Module | Header | Description |
|--------|--------|-------------|
| Platform | `uft_platform.h` | Cross-platform abstraction |
| Config | `uft_config_manager.h` | Configuration management |
| Recovery | `uft_recovery.h` | Data recovery algorithms |
| D64 Writer | `uft_d64_writer.h` | C64/1541 disk writing |
| WOZ Writer | `uft_woz_writer.h` | Apple II disk writing |
| Protection | `uft_protection_pipeline.h` | Copy protection handling |
| Format Detection | `uft_format_detection.h` | Auto-detection engine |
| Merge Engine | `uft_merge_engine.h` | Multi-read merging |

---

## Quick Start

```c
#include <uft/uft_platform.h>
#include <uft/uft_config_manager.h>
#include <uft/uft_recovery.h>

int main(void) {
    // Print platform info
    uft_platform_print_info();
    
    // Create config manager
    uft_config_manager_t *cfg = uft_config_create();
    int count;
    uft_config_register(cfg, uft_config_get_defaults(&count), count);
    
    // Use recovery
    // ...
    
    uft_config_destroy(cfg);
    return 0;
}
```

---

## Platform Abstraction (`uft_platform.h`)

### Platform Detection

```c
#ifdef UFT_PLATFORM_WINDOWS
    // Windows-specific code
#elif defined(UFT_PLATFORM_MACOS)
    // macOS-specific code  
#elif defined(UFT_PLATFORM_LINUX)
    // Linux-specific code
#endif
```

### Endianness

```c
uint16_t le = uft_read_le16(buffer);    // Read little-endian
uint32_t be = uft_read_be32(buffer);    // Read big-endian
uft_write_le32(buffer, value);          // Write little-endian
```

### File System

```c
bool exists = uft_file_exists("/path/to/file");
int64_t size = uft_file_size("/path/to/file");
uft_mkdir_p("/path/to/create");
uft_get_home_dir(path, sizeof(path));
uft_get_app_data_dir(path, sizeof(path), "uft");
```

### Serial Port

```c
uft_serial_config_t cfg = UFT_SERIAL_CONFIG_DEFAULT;
cfg.baud_rate = 115200;

uft_serial_t *serial = uft_serial_open("/dev/ttyUSB0", &cfg);
uft_serial_write(serial, data, size);
uft_serial_read(serial, buffer, sizeof(buffer));
uft_serial_close(serial);
```

### Timing

```c
uint64_t start = uft_time_us();
// ... operation ...
uint64_t elapsed = uft_time_us() - start;

uft_sleep_ms(100);  // Sleep 100ms
```

---

## Configuration (`uft_config_manager.h`)

### Basic Usage

```c
uft_config_manager_t *cfg = uft_config_create();

// Register default definitions
int count;
uft_config_register(cfg, uft_config_get_defaults(&count), count);

// Load from file
uft_config_load_ini(cfg, "uft.ini");

// Access values
int retries = UFT_CFG_INT(cfg, UFT_SEC_RECOVERY, UFT_KEY_MAX_RETRIES);
bool dark = UFT_CFG_BOOL(cfg, UFT_SEC_GUI, UFT_KEY_DARK_MODE);

// Set values
uft_config_set_int(cfg, UFT_SEC_RECOVERY, UFT_KEY_MAX_RETRIES, 10);

// Save
uft_config_save_ini(cfg, "uft.ini");

uft_config_destroy(cfg);
```

### Sections

| Section | Keys |
|---------|------|
| `general` | version, last_directory |
| `hardware` | device, drive_number, auto_detect |
| `recovery` | max_retries, revolutions, detect_weak_bits |
| `format` | default_format, cylinders, heads, sectors |
| `gui` | dark_mode, window_width, window_height |
| `logging` | log_level, log_file, log_to_console |
| `paths` | input_directory, output_directory |

---

## Recovery (`uft_recovery.h`)

### Multi-Revolution Analysis

```c
const uint8_t *revolutions[5];  // 5 revolution captures
uint8_t consensus[8192];
uint8_t weak_mask[8192];

uint32_t weak_count = uft_analyze_revolutions(
    revolutions, 5, bit_count,
    consensus, weak_mask, NULL);

printf("Found %u weak bits\n", weak_count);
```

### CRC Recovery

```c
// Single-bit correction
int fixed_bit;
if (uft_fix_crc_single_bit(data, size, expected_crc, &fixed_bit)) {
    printf("Fixed bit %d\n", fixed_bit);
}

// Weak-bit based correction
if (uft_fix_crc_weak_bits(data, size, weak_mask, expected_crc)) {
    printf("CRC fixed using weak bits\n");
}
```

### Error Mapping

```c
uft_error_map_t *map = uft_error_map_create(1024);

// Add entries
uft_error_entry_t entry = {
    .track = 5,
    .head = 0,
    .sector = 3,
    .status = UFT_REC_CRC_ERROR,
    .weak_bits = 8
};
uft_error_map_add(map, &entry);

// Generate report
char report[4096];
uft_error_map_report(map, report, sizeof(report));
printf("%s", report);

uft_error_map_free(map);
```

---

## D64 Writer (`uft_d64_writer.h`)

### Basic Usage

```c
d64_writer_config_t cfg = D64_WRITER_CONFIG_DEFAULT;
cfg.disk_id[0] = 'A';
cfg.disk_id[1] = 'B';

d64_writer_t *writer = d64_writer_create(&cfg);

// Write from sector data (683 sectors * 256 bytes)
uint8_t output[200000];
size_t output_size;
d64_writer_write(writer, sector_data, 683, output, &output_size);

d64_writer_destroy(writer);
```

### Track Information

```c
int sectors = d64_sectors_per_track(1);   // 21 for track 1-17
d64_speed_zone_t zone = d64_track_zone(1); // Zone 0
double bit_time = d64_zone_bit_time(zone); // 4.0 us
```

---

## WOZ Writer (`uft_woz_writer.h`)

### Basic Usage

```c
woz_writer_config_t cfg = WOZ_WRITER_CONFIG_DEFAULT;
cfg.disk_type = WOZ_DISK_525;
cfg.track_count = 35;

woz_writer_t *writer = woz_writer_create(&cfg);

// Add tracks
for (int t = 0; t < 35; t++) {
    woz_track_data_t track = {
        .track_number = t,
        .bit_data = track_bits[t],
        .bit_count = track_bit_counts[t]
    };
    woz_writer_add_track(writer, &track);
}

woz_writer_write(writer, "output.woz");
woz_writer_destroy(writer);
```

### DSK to WOZ Conversion

```c
uint8_t bit_data[8192];
size_t bit_count;

woz_from_dsk_track(sector_data, track, true, bit_data, &bit_count);
```

---

## Build Integration

### CMake

```cmake
find_package(UFT REQUIRED)
target_link_libraries(myapp PRIVATE uft::uft)
```

### pkg-config

```bash
pkg-config --cflags --libs uft
```

---

## Thread Safety

- `uft_config_manager_t`: NOT thread-safe (use external locking)
- `uft_serial_t`: NOT thread-safe (one handle per thread)
- Platform functions: Generally thread-safe
- Recovery functions: Thread-safe (no global state)

---

## Error Handling

Most functions return:
- `0` on success
- `-1` on error
- `NULL` for failed allocations

Check `errno` for system-level errors.

---

## Memory Management

All `_create()` functions have corresponding `_destroy()` functions:

```c
uft_config_manager_t *cfg = uft_config_create();
// ... use ...
uft_config_destroy(cfg);

d64_writer_t *writer = d64_writer_create(&config);
// ... use ...
d64_writer_destroy(writer);
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 3.8.7 | 2026-01-13 | Config Manager, Recovery APIs |
| 3.8.6 | 2026-01-13 | Cross-Platform, D64/WOZ Writers |
| 3.8.5 | 2026-01-12 | GUI Consolidation, Protection Pipeline |

---

*"Bei uns geht kein Bit verloren"*
