# UFT Floppy Library v2.0

**Comprehensive Floppy Disk I/O, Encoding, and Forensics Library**

## Features

### Core I/O
- Cross-Platform: Linux, Windows, macOS
- Physical drives and image files
- FAT12 filesystem support
- LBA/CHS conversion

### Encoding Support
- **GCR (Group Coded Recording)**
  - Commodore C64/C128 5-bit GCR
  - Apple Macintosh 6+2 GCR
  - Full encode/decode with checksum
  
- **MFM (Modified Frequency Modulation)**
  - IBM PC formats (360K to 2.88M)
  - Amiga DD/HD formats
  - CRC-CCITT verification

### Disk Image Formats
- **Commodore**: D64, D71, D81, G64, NIB
- **Amiga**: ADF, ADZ, DMS
- **Apple**: DSK, PO, NIB, WOZ
- **Atari**: ATR, XFD, DCM
- **PC**: IMG, IMA, IMD, TD0, FDI
- **Flux**: SCP, KF, HFE, IPF

### Forensic Tools
- Copy protection detection
- Weak bit analysis
- Multi-revolution comparison
- CRC repair algorithms
- Detailed forensic reports

### Recovery
- Multi-pass read strategies
- Bad sector handling
- Data confidence scoring
- Sector merging/voting

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Quick Start

```c
#include "uft_floppy_io.h"
#include "uft_fat12.h"

uft_disk_init();

uft_disk_t *disk;
uft_disk_open_image("floppy.img", UFT_ACCESS_READ, &disk);

uft_fat12_t *vol;
uft_fat12_mount(disk, &vol);

uft_fat12_dir_t *dir;
uft_fat12_opendir_root(vol, &dir);

uft_fat12_entry_t entry;
while (uft_fat12_readdir(dir, &entry) == UFT_OK) {
    printf("%s  %u bytes\n", entry.name, entry.size);
}

uft_fat12_closedir(dir);
uft_fat12_unmount(vol);
uft_disk_close(disk);
uft_disk_cleanup();
```

## Supported Formats

| Type | Size | C/H/S |
|------|------|-------|
| 5.25" 360KB | 368,640 | 40/2/9 |
| 5.25" 1.2MB | 1,228,800 | 80/2/15 |
| 3.5" 720KB | 737,280 | 80/2/9 |
| 3.5" 1.44MB | 1,474,560 | 80/2/18 |
| 3.5" 2.88MB | 2,949,120 | 80/2/36 |
| Amiga DD | 901,120 | 80/2/11 |

## Source Attribution

- discdiag: Disk I/O abstraction
- lbacache: CHS↔LBA conversion (GPL-2.0)
- Fosfat: Win32 disk access (GPL-3.0)

## License

GPL-3.0-or-later
