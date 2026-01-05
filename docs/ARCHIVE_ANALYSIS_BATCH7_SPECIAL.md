# BATCH 7: SPECIAL TOOLS ANALYSIS

## Archive Summary

| Archive | Files | Focus |
|---------|-------|-------|
| uft_scp_tool_c_super.zip | 5 | SCP Format Reader (C99) |
| omniflop_media_package.zip | 2 | Windows Hardware Interface |
| floppy_sector_parser_super.zip | 5 | IBM MFM/FM Sector Parser |
| kryoflux_3_50_linux_r4.zip | 9 | KryoFlux GUI + DTC Tool |

---

## 1. UFT SCP TOOL (C99)

**Path:** `/home/claude/batch7_special/scp_tool_c/`

### Purpose
Clean, modern SCP file reader with strict bounds checking and no hidden heap allocations.

### Key Features

**Header Structure (uft_scp.h):**
```c
#define UFT_SCP_MAX_TRACK_ENTRIES 168u

typedef enum uft_scp_rc {
    UFT_SCP_OK = 0,
    UFT_SCP_EINVAL = -1,    // Invalid parameter
    UFT_SCP_EIO = -2,       // I/O error
    UFT_SCP_EFORMAT = -3,   // Format error
    UFT_SCP_EBOUNDS = -4,   // Out of bounds
    UFT_SCP_ENOMEM = -5     // Memory error
} uft_scp_rc_t;

typedef struct uft_scp_file_header {
    uint8_t  signature[3];      // "SCP"
    uint8_t  version;
    uint8_t  disk_type;
    uint8_t  num_revs;
    uint8_t  start_track;
    uint8_t  end_track;
    uint8_t  flags;
    uint8_t  bitcell_encoding;
    uint8_t  sides;
    uint8_t  reserved;
    uint32_t checksum;
    uint32_t track_offsets[168];
} uft_scp_file_header_t;

typedef struct uft_scp_track_rev {
    uint32_t time_duration;  // Index time (25ns units)
    uint32_t data_length;    // Number of 16-bit values
    uint32_t data_offset;    // Byte offset from track block start
} uft_scp_track_rev_t;

typedef struct uft_scp_image {
    FILE *f;
    uft_scp_file_header_t hdr;
    uint32_t track_offsets[168];
    uint8_t  extended_mode;     // flags & 0x40
} uft_scp_image_t;
```

**API Functions:**
```c
int uft_scp_open(uft_scp_image_t *img, const char *path);
void uft_scp_close(uft_scp_image_t *img);
int uft_scp_get_track_info(uft_scp_image_t *img, uint8_t track_index, 
                           uft_scp_track_info_t *out);
int uft_scp_read_track_revs(uft_scp_image_t *img, uint8_t track_index,
                           uft_scp_track_rev_t *revs, size_t revs_cap,
                           uft_scp_track_header_t *out_trk_hdr);
int uft_scp_read_rev_transitions(uft_scp_image_t *img, uint8_t track_index, 
                                uint8_t rev_index,
                                uint32_t *transitions_out, size_t transitions_cap,
                                size_t *out_count, uint32_t *out_total_time);
```

### Algorithm Highlights

**Flux Delta Parsing:**
- 16-bit big-endian values
- Zero value = overflow (add 0x10000 to accumulator)
- Non-zero = add to cumulative time
- Stream-based reading in 4KB chunks

**Extended Mode Support:**
- Flag 0x40 in header enables extended mode
- Track offsets at absolute position 0x80

**CLI Tool Features:**
- `--summary`: Header overview
- `--catalog <json>`: GUI-friendly JSON export
- `--dump <csv>`: Flux transitions export
- `--max-transitions`: Buffer limit control

---

## 2. OMNIFLOP MEDIA INTERFACE

**Path:** `/home/claude/batch7_special/omniflop/`

### Purpose
Windows driver interface for OmniFlop floppy controller hardware.

### API (Windows-only)
```c
typedef struct OmniFlopMedia {
    HANDLE hMedia;
    char   drivePath[64];
    char   lastError[512];
} OmniFlopMedia;

// Core functions
bool OmniFlop_EnableExtendedFormats(const char *drivePath, bool enable, 
                                    const char accessString3[3]);
bool OmniFlop_LockMediaType(const char *drivePath, MEDIA_TYPE mediaType);
bool OmniFlop_UnlockMediaType(const char *drivePath);
bool OmniFlop_EnableReadWrite(const char *drivePath, UCHAR enable, UCHAR *previous);
bool OmniFlop_OpenForFormat(OmniFlopMedia *m, const char *drivePath, 
                           MEDIA_TYPE mediaType);
void OmniFlop_Close(OmniFlopMedia *m);
bool OmniFlop_FormatTrack(OmniFlopMedia *m, MEDIA_TYPE mediaType, 
                         DWORD cylinder, DWORD head);
```

### IOCTL Operations
- `IOCTL_OMNIFLOP_ENABLE_EXTENDED_FORMATS`
- `IOCTL_OMNIFLOP_DISABLE_EXTENDED_FORMATS`
- `IOCTL_OMNIFLOP_SELECT_MEDIA_TYPE`
- `IOCTL_OMNIFLOP_LOCK_MEDIA_TYPE`
- `IOCTL_OMNIFLOP_UNLOCK_MEDIA_TYPE`
- `IOCTL_OMNIFLOP_ENABLE_READ_WRITE`
- `FSCTL_LOCK_VOLUME`
- `FSCTL_DISMOUNT_VOLUME`
- `IOCTL_DISK_FORMAT_TRACKS`

### Integration Notes
- Requires OmniFlop SDK headers
- Windows-specific (HANDLE, DeviceIoControl)
- Can be abstracted for cross-platform hardware layer

---

## 3. FLOPPY SECTOR PARSER

**Path:** `/home/claude/batch7_special/sector_parser/`

### Purpose
Parse IBM-style FM/MFM sector structures from demodulated byte streams.

### Status Flags
```c
typedef enum fps_status_flags {
    FPS_OK                     = 0u,
    FPS_WARN_CRC_ID_BAD        = 1u << 0,  // ID field CRC error
    FPS_WARN_CRC_DATA_BAD      = 1u << 1,  // Data field CRC error
    FPS_WARN_MISSING_DATA      = 1u << 2,  // No DAM found after IDAM
    FPS_WARN_DUPLICATE_ID      = 1u << 3,  // Same C/H/R/N found twice
    FPS_WARN_SIZE_MISMATCH     = 1u << 4,  // N field invalid
    FPS_WARN_TRUNCATED_RECORD  = 1u << 5,  // Track ended mid-record
    FPS_WARN_WEAK_SYNC         = 1u << 6,  // Sync without mark mask
    FPS_WARN_UNUSUAL_MARK      = 1u << 7,  // Non-standard address mark
} fps_status_flags_t;
```

### Data Structures
```c
typedef struct fps_id_fields {
    uint8_t cyl;   // C - Cylinder
    uint8_t head;  // H - Head
    uint8_t sec;   // R - Sector
    uint8_t sizeN; // N - Size code (2^N * 128 bytes)
} fps_id_fields_t;

typedef struct fps_sector {
    fps_id_record_t   idrec;
    fps_data_record_t datarec;
    uint8_t *data;          // Caller-provided buffer
    uint16_t data_capacity;
} fps_sector_t;

typedef struct fps_config {
    fps_encoding_t encoding;      // FPS_ENC_MFM or FPS_ENC_FM
    const uint8_t *mark_mask;     // Optional address mark mask
    size_t mark_mask_len;
    uint16_t max_sectors;
    uint16_t max_search_gap;      // Bytes after ID to search for DAM
    uint8_t require_mark_mask;    // Strict sync validation
} fps_config_t;
```

### CRC-16 CCITT Implementation
```c
uint16_t fps_crc16_ccitt(const uint8_t *buf, size_t len, uint16_t init) {
    uint16_t crc = init;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) 
                                  : (uint16_t)(crc << 1);
        }
    }
    return crc;
}
```

### Sync Detection
- **MFM**: `0xA1 0xA1 0xA1` (with missing clock)
- **FM**: `0x00 0x00 0x00`
- **IDAM**: `0xFE`
- **DAM**: `0xFB` (normal) or `0xF8` (deleted)

### API
```c
uint16_t fps_expected_length_from_N(uint8_t sizeN);
uint16_t fps_crc16_ccitt(const uint8_t *buf, size_t len, uint16_t init);
int fps_parse_track(const fps_config_t *cfg,
                    const uint8_t *stream, size_t stream_len,
                    fps_sector_t *sectors, size_t sectors_cap,
                    fps_result_t *out);
```

---

## 4. KRYOFLUX GUI ANALYSIS

**Path:** `/home/claude/batch7_special/kryoflux_gui/`

### Application Structure

```
com.kryoflux.ui/
├── KryoFluxApp.class           # Main application
├── iface/
│   ├── KryoFluxUI.class        # Main UI class
│   ├── Calibration.class       # Drive calibration
│   ├── StreamPlot.class        # Flux visualization
│   ├── Summary.class           # Track summary
│   ├── TrackState.class        # Track status tracking
│   │
│   ├── component/
│   │   ├── grid/
│   │   │   ├── Grid.class      # ★ Track grid component
│   │   │   ├── Grid$Cell.class
│   │   │   ├── CellFocused.class
│   │   │   └── RangeSelected.class
│   │   │
│   │   ├── digit/
│   │   │   ├── Digit.class     # 7-segment digit
│   │   │   └── Digits.class
│   │   │
│   │   ├── plot/
│   │   │   ├── Plot.class      # Base plot
│   │   │   ├── HistogramPlot.class
│   │   │   ├── ScatterPlot.class
│   │   │   └── DensityPlot.class
│   │   │
│   │   ├── Plotter.class       # Unified plotter
│   │   ├── LED.class           # Status LED
│   │   ├── LEDGroup.class      # LED array
│   │   ├── Counter.class       # Numeric counter
│   │   ├── TrackData.class     # Track info table
│   │   ├── TrackDataBasic.class
│   │   ├── TrackDataAdvanced.class
│   │   ├── TrackSideCounter.class
│   │   └── Tracks.class
│   │
│   ├── settings/
│   │   ├── SettingsDialog.class
│   │   ├── AdvancedSettings.class
│   │   ├── OutputSettings.class
│   │   ├── ImageProfileSelection.class
│   │   ├── ImageProfileEditor.class
│   │   └── ImageProfileManager.class
│   │
│   ├── swing/
│   │   ├── GroupPanel.class    # Layout helper
│   │   ├── CardPanel.class
│   │   ├── FileChooser.class
│   │   └── MessagePanel.class
│   │
│   └── util/
│       ├── Form.class          # Form builder
│       ├── Icons.class
│       ├── Sounds.class
│       └── AntiAliasing.class
│
├── params/                     # Parameter handling
│   ├── ParamsGlobal.class
│   ├── ParamsImageLocal.class
│   └── ImageProfile.class
│
├── domain/                     # Data models
│   ├── Track.class
│   ├── TrackResult.class
│   ├── Format.class
│   └── Metrics.class
│
└── event/                      # Event system
    ├── DeviceEvent.class
    ├── TrackEvent.class
    └── StatusMessage.class
```

### GUI Layout Concept

```
┌──────────────────────────────────────────────────────────────────┐
│ File   View   Drive   Help                                       │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────────────────┐  ┌────────────────────────┐│
│  │          TRACK GRID             │  │     CONTROL PANEL      ││
│  │  ┌──┬──┬──┬──┬──┬──┬──┬──┬──┐  │  │  ┌────┐   ┌────────┐  ││
│  │  │00│01│02│03│04│05│06│07│..│  │  │  │ ▶  │   │ Motor  │  ││
│  │  ├──┼──┼──┼──┼──┼──┼──┼──┼──┤  │  │  │    │   │ Stream │  ││
│  │  │  │  │  │  │  │  │  │  │  │  │  │  │Start│   │ Error  │  ││
│  │  └──┴──┴──┴──┴──┴──┴──┴──┴──┘  │  │  └────┘   └────────┘  ││
│  │        Side 0  |  Side 1       │  │                        ││
│  │                                 │  │  [Output Selection ▼]  ││
│  └─────────────────────────────────┘  │  [Enter name...     ]  ││
│                                        └────────────────────────┘│
│  ┌──────────────────────────────────────────────────────────────┐│
│  │ Track │ Advanced │ Histogram │ Scatter │ Density │           ││
│  ├──────────────────────────────────────────────────────────────┤│
│  │  Track:     42                                               ││
│  │  Format:    Amiga DD                                         ││
│  │  Result:    OK                                               ││
│  │  Sectors:   11/11                                            ││
│  │  RPM:       300.2                                            ││
│  │  Transfer:  125000 B/s                                       ││
│  └──────────────────────────────────────────────────────────────┘│
│                                                                  │
├──────────────────────────────────────────────────────────────────┤
│ Status: Ready                              Density: DD           │
└──────────────────────────────────────────────────────────────────┘
```

### Track Grid Cell States
```
+--------+----------+------------------+
| Color  | State    | Description      |
+--------+----------+------------------+
| Green  | Good     | All sectors OK   |
| Red    | Bad      | CRC/Read errors  |
| Gray   | Unknown  | Not yet read     |
| Yellow | Modified | Changed sectors  |
+--------+----------+------------------+
```

### Information Tabs

**Basic Tab:**
- Track number
- Logical Track
- Format (detected)
- Result (OK/Bad/Missing)
- Sectors (found/expected)
- RPM
- Transfer rate (Bytes/s)

**Advanced Tab:**
- Flux Reversals count
- Drift (µs)
- Base timing (µs)
- Bands (µs) - timing histogram peaks

### Plotter Styles
1. **Histogram** - Pulse timing distribution
2. **Scatter** - Pulse timing vs position
3. **Density** - Flux density visualization
4. **HeatMap** - 2D timing distribution

### Settings Dialog Structure

**Image Profiles Tab:**
- Profile list with +/-/Copy/Update buttons
- Profile name
- Image type dropdown
- Extension field
- Track range (Start/End)
- Side mode (Side0/Side1/Both)
- Sector size
- Sector count (Any/Exactly/AtMost)
- Track distance
- Target RPM
- Flippy mode checkbox
- Other params text field

**Advanced Tab:**
- Drive selection (Drive 0/1)
- Side selection
- Retries count
- Revolutions count
- Physical Track Zero position
- Calibration Override section
- Max Track overrides per drive
- DTC global params

**Output Tab:**
- Output format selection
- Path configuration

### Localization Keys (for UFT translation)

```properties
# Main sections
section.tracks=Tracks
section.info=Information
section.control=Control

# LEDs
led.motor=Motor
led.stream=Stream
led.error=Error

# Controls
control.start=Start
control.stop=Stop

# Info tabs
infotab.track=Track
infotab.advanced=Advanced
infotab.histogram=Histogram
infotab.scatter=Scatter
infotab.density=Density

# Track fields
track=Track
logical-track=Logical Track
format=Format
result=Result
sectors=Sectors
rpm=RPM
transfer=Transfer (Bytes/s)

# Advanced fields
flux-reversals=Flux Reversals
drift=Drift (us)
base=Base (us)
bands=Bands (us)

# Status flags (for error display)
P=Generic protection present
N=Sector not in image
X=Decoding stopped (protection)
H=Hidden data in header
I=Non-standard format/ID
T=Wrong track number
S=Wrong side number
B=Sector out of range
L=Non-standard sector length
Z=Illegal offset
C=Unchecked checksum
```

---

## 5. INTEGRATION RECOMMENDATIONS

### For UFT Qt GUI

**1. Track Grid Widget (Highest Priority)**
```cpp
class TrackGridWidget : public QWidget {
    Q_OBJECT
public:
    enum CellState { Unknown, Good, Bad, Modified };
    
    void setCellState(int track, int side, CellState state);
    void setSelection(int startTrack, int endTrack, int side);
    void clearSelection();
    
signals:
    void cellClicked(int track, int side);
    void rangeSelected(int startTrack, int endTrack, int side);
    void cellHovered(int track, int side);
};
```

**2. Information Panel with Tabs**
```cpp
class TrackInfoPanel : public QTabWidget {
    Q_OBJECT
public:
    void updateTrackInfo(const TrackInfo& info);
    void updateAdvancedInfo(const AdvancedTrackInfo& info);
    void updateHistogram(const QVector<double>& data);
    void updateScatterPlot(const QVector<QPointF>& points);
};
```

**3. Control Panel**
```cpp
class ControlPanel : public QWidget {
    Q_OBJECT
public:
    void setMotorLED(bool on);
    void setStreamLED(bool on);
    void setErrorLED(bool on);
    
signals:
    void startClicked();
    void stopClicked();
    void outputSelected(const QString& format);
};
```

**4. Settings Dialog**
```cpp
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    // Tabs
    QWidget* createProfilesTab();
    QWidget* createAdvancedTab();
    QWidget* createOutputTab();
    
    // Profile management
    void addProfile(const ImageProfile& profile);
    void removeProfile(int index);
    ImageProfile getProfile(int index) const;
};
```

### File Structure for Qt Implementation

```
src/gui/
├── mainwindow.cpp
├── mainwindow.h
├── widgets/
│   ├── trackgridwidget.cpp
│   ├── trackgridwidget.h
│   ├── trackinfoPanel.cpp
│   ├── trackinfoPanel.h
│   ├── controlpanel.cpp
│   ├── controlpanel.h
│   ├── ledwidget.cpp
│   ├── ledwidget.h
│   ├── plotterwidget.cpp
│   └── plotterwidget.h
├── dialogs/
│   ├── settingsdialog.cpp
│   ├── settingsdialog.h
│   ├── calibrationdialog.cpp
│   └── calibrationdialog.h
└── resources/
    ├── translations/
    │   ├── uft_de.ts
    │   └── uft_en.ts
    └── icons/
```

---

## 6. CODE ADOPTION SUMMARY

| Source | Adopt | Adapt | Notes |
|--------|-------|-------|-------|
| SCP Tool | ✓ | - | Direct integration, excellent code quality |
| Sector Parser | ✓ | - | Clean IBM parser with status flags |
| OmniFlop | - | ✓ | Abstract for cross-platform hardware layer |
| KryoFlux GUI | - | ✓ | Layout concept only, rebuild in Qt |

### Immediate Actions

1. **Integrate SCP Tool** - Copy `uft_scp.h`/`uft_scp.c` directly
2. **Integrate Sector Parser** - Copy `floppy_sector_parser.h`/`.c` 
3. **Create Qt Track Grid** - Based on KryoFlux grid concept
4. **Implement Info Tabs** - Basic/Advanced/Plots
5. **Add LED Status** - Motor/Stream/Error indicators
6. **Create Settings Dialog** - Profile management + advanced settings
