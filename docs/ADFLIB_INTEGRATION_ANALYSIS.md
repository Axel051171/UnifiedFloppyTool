# ADFlib Integration Analysis für UnifiedFloppyTool

## Was ist ADFlib?

The ADFlib is a free, portable and open implementation of the Amiga filesystem. The library is written in portable C (currently C99).

**Wichtige Merkmale**:
- Seit den späten 1990ern entwickelt (Laurent Clévy)
- Cross-Platform: Linux, Windows (MSVC, CygWin, MSYS2), macOS
- CI-getestet auf GitHub Actions
- Lizenz: LGPL-2.1 / GPL-2.0
- Version: 0.10.5 (September 2025)

---

## Aktuelle UFT Amiga-Implementierung

| Komponente | Zeilen | Status |
|------------|--------|--------|
| `uft_adf_pipeline.c` | 703 | ✅ Read/Write |
| `adf_loader.c` | 106 | ✅ Basic |
| `adf_writer.c` | 133 | ✅ Basic |
| `amigaffs.cc` | 960 | ⚠️ FluxEngine-Port |
| **Total** | ~1.900 | Partiell |

**Vorhandene Features**:
- ADF DD (880 KB) und HD (1.76 MB) lesen/schreiben
- OFS/FFS Erkennung
- MFM Track Decoding
- Basic Sector Parser

**Fehlende Features**:
- ❌ HDF (Hard Disk Files) mit Partitionen
- ❌ Rigid Disk Block (RDB) Parsing
- ❌ File/Directory Operations (nur raw blocks)
- ❌ Dircache Support
- ❌ Deleted File Recovery
- ❌ Block Allocation Bitmap Rebuild
- ❌ Native Device Access

---

## ADFlib Feature-Matrix

### 3-Ebenen-Architektur

```
┌─────────────────────────────────────────────┐
│  Level 3: Filesystem                        │
│  Files, Directories, Links, Permissions     │
├─────────────────────────────────────────────┤
│  Level 2: Volume                            │
│  Partitions, Format, Block Allocation       │
├─────────────────────────────────────────────┤
│  Level 1: Device                            │
│  ADF, HDF, Native Disks, Ramdisk           │
└─────────────────────────────────────────────┘
```

### Unterstützte Formate

| Format | Read | Write | UFT Status |
|--------|------|-------|------------|
| ADF DD (880 KB) | ✅ | ✅ | ✅ Vorhanden |
| ADF HD (1.76 MB) | ✅ | ✅ | ✅ Vorhanden |
| ADF Non-Standard | ✅ | ✅ | ❌ Fehlt |
| HDF Unpartitioned | ✅ | ✅ | ⚠️ Basic |
| HDF mit RDB | ✅ | ⚠️ | ❌ Fehlt |
| Native Devices | ✅ | ⚠️ | ❌ Fehlt |

### Filesystem-Features

| Feature | ADFlib | UFT |
|---------|--------|-----|
| OFS (Original File System) | ✅ | ✅ |
| FFS (Fast File System) | ✅ | ✅ |
| International Mode | ✅ | ❌ |
| Directory Cache | ✅ | ❌ |
| Hard Links | ✅ | ❌ |
| Soft Links | ✅ | ❌ |
| File Comments | ✅ | ❌ |
| File Permissions | ✅ | ❌ |
| Deleted File Recovery | ✅ | ❌ |
| Bitmap Rebuild | ✅ | ❌ |

---

## Konkrete Integrationspunkte

### 1. HDF mit Rigid Disk Block (Priorität: P1)

**Problem**: UFT kann keine partitionierten Amiga-Festplatten lesen.

**ADFlib Lösung**:
```c
#include <adflib.h>

// HDF mit Partitionen öffnen
struct AdfDevice *dev = adfDevOpen("disk.hdf", ADF_ACCESS_MODE_READONLY);
adfDevMount(dev);  // Liest RDB, erkennt Partitionen

// Anzahl Volumes (Partitionen)
int numVolumes = dev->nVol;

// Erste Partition mounten
struct AdfVolume *vol = adfVolMount(dev, 0, ADF_ACCESS_MODE_READONLY);

// Filesystem-Typ
printf("FS: %s\n", vol->dosType == 0 ? "OFS" : "FFS");

adfVolUnMount(vol);
adfDevUnMount(dev);
adfDevClose(dev);
```

### 2. File/Directory Operations (Priorität: P1)

**Problem**: UFT kann nur raw blocks lesen, keine Dateien extrahieren.

**ADFlib Lösung**:
```c
// Verzeichnis wechseln
adfChangeDir(vol, "Devs");

// Verzeichnisinhalt auflisten
struct AdfList *list = adfGetDirEnt(vol, vol->curDirPtr);
while (list) {
    struct AdfEntry *entry = (struct AdfEntry*)list->content;
    printf("%s (%d bytes)\n", entry->name, entry->size);
    list = list->next;
}
adfFreeDirList(list);

// Datei lesen
struct AdfFile *file = adfFileOpen(vol, "startup-sequence", ADF_FILE_MODE_READ);
char buf[256];
while (!adfFileAtEOF(file)) {
    int n = adfFileRead(file, 256, buf);
    // ... process buf ...
}
adfFileClose(file);
```

### 3. Deleted File Recovery (Priorität: P2)

**Problem**: UFT kann gelöschte Dateien nicht wiederherstellen.

**ADFlib Tool**: `adfsalvage`
```bash
# Gelöschte Einträge auflisten
adfsalvage -l disk.adf

# Datei wiederherstellen
adfsalvage -u "deleted_file.txt" disk.adf
```

**API**:
```c
// Gelöschte Einträge scannen
struct AdfList *deleted = adfGetDelEnt(vol);
// ... iterate and recover ...
```

### 4. Block Allocation Bitmap (Priorität: P2)

**Problem**: Korrupte ADFs mit falscher Bitmap.

**ADFlib Tool**: `adfbitmap`
```bash
# Bitmap anzeigen
adfbitmap -d disk.adf

# Bitmap rebuilden
adfbitmap -r disk.adf
```

**API**:
```c
// Bitmap rebuilden
adfReconstructBitmap(vol);
```

### 5. Native Device Access (Priorität: P3)

**Problem**: UFT kann keine echten Amiga-Disketten/Festplatten lesen.

**ADFlib Lösung**:
```c
// Native Driver aktivieren
adfAddDeviceDriver(&adfDeviceDriverNative);

// Linux: Echte Diskette lesen
struct AdfDevice *dev = adfDevOpen("/dev/fd0", ADF_ACCESS_MODE_READONLY);

// Windows: Physische Disk
struct AdfDevice *dev = adfDevOpen("|H5", ADF_ACCESS_MODE_READONLY);
```

---

## Integration in UFT

### Option A: ADFlib als externe Dependency

**CMakeLists.txt**:
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(ADFLIB REQUIRED adflib)

target_include_directories(UFT PRIVATE ${ADFLIB_INCLUDE_DIRS})
target_link_libraries(UFT PRIVATE ${ADFLIB_LIBRARIES})
```

**Vorteile**:
- Bewährte, getestete Implementation
- Aktive Maintenance
- Umfangreiche Features

**Nachteile**:
- Externe Dependency
- LGPL/GPL Lizenz beachten
- Teilweise Redundanz mit UFT-Code

### Option B: Selektive Code-Übernahme

**Relevante ADFlib-Module**:
```
src/adf_dev.c       - Device-Handling
src/adf_vol.c       - Volume-Handling  
src/adf_file.c      - File Operations
src/adf_dir.c       - Directory Operations
src/adf_bitmap.c    - Block Allocation
src/adf_salv.c      - Deleted File Recovery
src/adf_hd.c        - Hard Disk / RDB
```

**UFT-Anpassungen**:
- Namespace: `adf_*` → `uft_adf_*`
- Error Handling: An UFT-System anpassen
- Callbacks: An UFT-Logging anpassen

### Option C: Wrapper-Schicht

```c
// uft_adflib_wrapper.h
#ifndef UFT_ADFLIB_WRAPPER_H
#define UFT_ADFLIB_WRAPPER_H

#include "uft_disk_image.h"

#ifdef UFT_USE_ADFLIB
#include <adflib.h>

typedef struct {
    struct AdfDevice *dev;
    struct AdfVolume *vol;
} uft_adf_context_t;

// High-Level API
uft_adf_context_t* uft_adf_open(const char *path);
void uft_adf_close(uft_adf_context_t *ctx);

// Volume Info
int uft_adf_get_volume_count(uft_adf_context_t *ctx);
const char* uft_adf_get_volume_name(uft_adf_context_t *ctx, int vol);
int uft_adf_get_fs_type(uft_adf_context_t *ctx, int vol);

// Directory Operations
char** uft_adf_list_dir(uft_adf_context_t *ctx, const char *path);
int uft_adf_extract_file(uft_adf_context_t *ctx, const char *src, const char *dst);

// Recovery
char** uft_adf_list_deleted(uft_adf_context_t *ctx);
int uft_adf_recover_file(uft_adf_context_t *ctx, const char *name);

// Repair
int uft_adf_rebuild_bitmap(uft_adf_context_t *ctx);

#endif // UFT_USE_ADFLIB
#endif // UFT_ADFLIB_WRAPPER_H
```

---

## Vergleich: UFT vs. ADFlib

| Aspekt | UFT Aktuell | ADFlib |
|--------|-------------|--------|
| Raw Block I/O | ✅ | ✅ |
| MFM Decoding | ✅ | ❌ (erwartet decoded) |
| Flux-Level | ✅ | ❌ |
| Filesystem-Level | ❌ | ✅ |
| HDF Partitionen | ❌ | ✅ |
| File Extraction | ❌ | ✅ |
| Deleted Recovery | ❌ | ✅ |
| Protection Detection | ✅ | ❌ |
| Forensik | ✅ | ⚠️ |

**Fazit**: ADFlib und UFT ergänzen sich:
- **UFT**: Low-Level (Flux, MFM, Protection)
- **ADFlib**: High-Level (Filesystem, Files, Directories)

---

## Empfehlung

### Phase 1: Wrapper-Integration (P1)
- [ ] ADFlib als optionale Dependency
- [ ] Wrapper für File/Directory Operations
- [ ] HDF mit RDB Support

### Phase 2: GUI-Integration (P2)
- [ ] File Browser für ADF/HDF
- [ ] Extract/Add Files Dialog
- [ ] Deleted File Recovery UI

### Phase 3: Forensik-Erweiterung (P3)
- [ ] Bitmap Analysis View
- [ ] Filesystem Consistency Check
- [ ] Cross-Reference mit UFT Protection Detection

---

## Command-Line Tools von ADFlib

| Tool | Funktion | UFT-Äquivalent |
|------|----------|----------------|
| `unadf` | Dateien extrahieren | ❌ Fehlt |
| `adfimgcreate` | Leere Images erstellen | ⚠️ Basic |
| `adfformat` | Filesystem formatieren | ❌ Fehlt |
| `adfinfo` | Metadata anzeigen | ⚠️ Basic |
| `adfbitmap` | Bitmap analysieren | ❌ Fehlt |
| `adfsalvage` | Deleted Recovery | ❌ Fehlt |

**Empfehlung**: Diese Tools als Referenz für UFT CLI verwenden.

---

## Fazit

**ADFlib ist sehr wertvoll für UFT**, besonders für:
- HDF mit Partitionen (Rigid Disk Block)
- Filesystem-Level Operations (Files, Directories)
- Deleted File Recovery
- Block Allocation Bitmap Repair

**Integration**: Option C (Wrapper) empfohlen:
- ADFlib als optionale Dependency
- UFT bleibt unabhängig ohne ADFlib
- Volle Power bei aktiviertem ADFlib
