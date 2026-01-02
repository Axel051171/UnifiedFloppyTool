# UFT Hardware Abstraction Layer & Decoder Architecture

## Gemeinsames Minimum: Hardware-Operationen

### Minimal API für alle Floppy-Controller

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    MINIMAL HARDWARE OPERATIONS                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   DRIVE CONTROL              TRACK ACCESS              DATA CAPTURE         │
│   ─────────────              ────────────              ────────────         │
│                                                                             │
│   drive_select(n)            seek(cylinder)            read_flux(revs)      │
│   ├─ Laufwerk 0-3 wählen     ├─ Absolutes Seek         ├─ Flux-Timing lesen │
│   │                          │                          │                   │
│   motor_on()                 step_in()                  read_track()        │
│   motor_off()                step_out()                 ├─ Bitstream lesen  │
│   ├─ Motorsteuerung          ├─ Relatives Seek         │                   │
│   │                          │                          write_flux(data)    │
│   head_select(h)             recalibrate()              write_track(data)   │
│   ├─ Kopf 0/1                ├─ Track 0 finden         └─ Schreiben        │
│   │                          │                                              │
│   get_rpm()                  get_cylinder()                                 │
│   └─ RPM messen              └─ Position lesen                              │
│                                                                             │
│   INDEX HANDLING             DENSITY CONTROL                                │
│   ──────────────             ───────────────                                │
│                                                                             │
│   wait_index()               set_density(hd)                                │
│   ├─ Auf Index warten        ├─ DD/HD umschalten                           │
│   │                          │                                              │
│   get_index_time_us()        get_data_rate()                                │
│   └─ Umdrehungszeit          └─ Aktuelle Datenrate                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Implementierung pro Controller

| Operation | Greaseweazle | FluxEngine | Kryoflux | FC5025 | XUM1541 |
|-----------|--------------|------------|----------|--------|---------|
| drive_select | ✓ USB cmd | ✓ USB cmd | ✓ USB cmd | ✓ USB cmd | ✓ CBM bus |
| motor_on/off | ✓ | ✓ | ✓ | ✓ | ✓ |
| head_select | ✓ | ✓ | ✓ | ✓ | ✓ (1571) |
| seek | ✓ | ✓ | ✓ | ✓ | ✓ |
| step_in/out | ✓ | ✓ | ✓ | ✓ | ✓ |
| recalibrate | ✓ | ✓ | ✓ | ✓ | ✓ |
| get_rpm | ✓ Index | ✓ Index | ✓ Index | ⚠️ Estimate | ⚠️ CBM zone |
| wait_index | ✓ HW | ✓ HW | ✓ HW | ⚠️ SW | ✗ |
| **read_flux** | ✓ | ✓ | ✓ | ✗ | ✗ |
| **read_track** | ✓ | ✓ | ⚠️ SW decode | ✓ | ✓ GCR |
| set_density | ✓ | ✓ | ✓ | ✓ | ✗ GCR only |

---

## Hardware-spezifische Limits

### Controller-Vergleich

```
╔══════════════════════════════════════════════════════════════════════════════════════════════════════════╗
║                               CONTROLLER CAPABILITIES COMPARISON                                         ║
╠═══════════════════╦══════════════╦══════════════╦══════════════╦══════════════╦══════════════════════════╣
║ Parameter         ║ Greaseweazle ║ FluxEngine   ║ Kryoflux     ║ FC5025       ║ XUM1541                  ║
╠═══════════════════╬══════════════╬══════════════╬══════════════╬══════════════╬══════════════════════════╣
║ Sample Rate       ║ 72-84 MHz    ║ 72 MHz       ║ 24.027 MHz   ║ 12 MHz       ║ ~1 MHz                   ║
║ Resolution        ║ ~14 ns       ║ ~14 ns       ║ ~42 ns       ║ ~83 ns       ║ ~1000 ns                 ║
║ Jitter            ║ <5 ns        ║ <5 ns        ║ <10 ns       ║ ~50 ns       ║ ~500 ns                  ║
╠═══════════════════╬══════════════╬══════════════╬══════════════╬══════════════╬══════════════════════════╣
║ Raw Flux          ║ ✓            ║ ✓            ║ ✓            ║ ✗            ║ ✗                        ║
║ Bitstream         ║ ✓            ║ ✓            ║ ⚠️ SW        ║ ✓            ║ ✓                        ║
║ Sector            ║ ✓            ║ ✓            ║ ⚠️ SW        ║ ✓            ║ ✓                        ║
╠═══════════════════╬══════════════╬══════════════╬══════════════╬══════════════╬══════════════════════════╣
║ HW Index          ║ ✓            ║ ✓            ║ ✓            ║ ✗            ║ ✗                        ║
║ Index Accuracy    ║ ~10 ns       ║ ~10 ns       ║ ~42 ns       ║ ~1000 ns     ║ N/A                      ║
║ Max Revolutions   ║ 20           ║ 20           ║ 50           ║ 5            ║ 1                        ║
╠═══════════════════╬══════════════╬══════════════╬══════════════╬══════════════╬══════════════════════════╣
║ Max Cylinders     ║ 84           ║ 84           ║ 86           ║ 80           ║ 42                       ║
║ Half-Tracks       ║ ✓            ║ ✓            ║ ✓            ║ ✗            ║ ✓                        ║
║ Copy Protection   ║ ✓            ║ ✓            ║ ✓            ║ ⚠️ Limited   ║ ✓ GCR-level              ║
║ Weak Bits         ║ ✓            ║ ✓            ║ ✓            ║ ✗            ║ ✗                        ║
╚═══════════════════╩══════════════╩══════════════╩══════════════╩══════════════╩══════════════════════════╝
```

### FC5025 Einschränkungen (Detail)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        FC5025 LIMITATIONS                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ⚠️ KEIN RAW FLUX CAPTURE                                                 │
│   ──────────────────────────                                                │
│   Der FC5025 kann nur BITSTREAM lesen, nicht Raw-Flux-Timing.              │
│   → Keine Weak-Bit-Erkennung                                               │
│   → Keine präzisen Timing-Analysen                                         │
│   → Eingeschränkte Kopierschutz-Unterstützung                              │
│                                                                             │
│   ⚠️ KEINE HALF-TRACKS                                                     │
│   ───────────────────────                                                   │
│   Nur volle Tracks (0, 1, 2, ...), keine 0.5, 1.5, etc.                   │
│   → Commodore-Kopierschutz nicht vollständig lesbar                        │
│   → Apple II-Schutzschemas eingeschränkt                                   │
│                                                                             │
│   ⚠️ SOFTWARE INDEX DETECTION                                              │
│   ─────────────────────────────                                             │
│   Index wird in Software erkannt, nicht in Hardware.                       │
│   → Geringere Timing-Genauigkeit (~1µs vs ~10ns)                          │
│   → Umdrehungszeit-Messung ungenauer                                       │
│                                                                             │
│   ⚠️ HÖHERER JITTER                                                        │
│   ─────────────────                                                         │
│   ~50ns Jitter vs ~5ns bei Greaseweazle                                   │
│   → Schwächere Signale schwerer zu lesen                                   │
│   → Dirty Dumps problematischer                                            │
│                                                                             │
│   ⚠️ NUR 5.25" DRIVES                                                      │
│   ──────────────────                                                        │
│   Primär für 5.25" ausgelegt, kein Apple 3.5" Support.                    │
│                                                                             │
│   CAPABILITY MODEL MAPPING:                                                 │
│   ─────────────────────────                                                 │
│   caps.can_read_flux = false                                               │
│   caps.hardware_index = false                                              │
│   caps.supports_half_tracks = false                                        │
│   caps.copy_protection_support = false (LIMITED)                           │
│   caps.weak_bit_detection = false                                          │
│   caps.sample_resolution_ns = 83.3                                         │
│   caps.jitter_ns = 50.0                                                    │
│   caps.limitations = { "No raw flux", "No half-tracks", ... }              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Drive Profile API

### Drive-Typen

```c
typedef enum uft_drive_type {
    // 5.25" drives
    UFT_DRIVE_525_DD,       // 40 track, 360KB
    UFT_DRIVE_525_HD,       // 80 track, 1.2MB
    UFT_DRIVE_525_QD,       // 80 track, 720KB (quad density)
    
    // 3.5" drives  
    UFT_DRIVE_35_DD,        // 80 track, 720KB/880KB
    UFT_DRIVE_35_HD,        // 80 track, 1.44MB
    UFT_DRIVE_35_ED,        // 80 track, 2.88MB
    
    // Special drives
    UFT_DRIVE_CBM_1541,     // Commodore 1541 (GCR)
    UFT_DRIVE_CBM_1571,     // Commodore 1571 (double-sided)
    UFT_DRIVE_APPLE_525,    // Apple II 5.25"
    UFT_DRIVE_APPLE_35,     // Apple 3.5" (variable speed)
} uft_drive_type_t;
```

### Drive Profile Struktur

```c
typedef struct uft_drive_profile {
    uft_drive_type_t type;
    const char*      name;
    
    // Physical
    int     cylinders;          // 35, 40, 80
    int     heads;              // 1 or 2
    bool    double_step;        // Step twice per track
    
    // Timing
    double  rpm;                // 300, 360
    double  rpm_tolerance;      // ±%
    double  step_time_ms;       // Per step
    double  settle_time_ms;     // Head settle
    double  motor_spinup_ms;
    
    // Data rates (kbps)
    double  data_rate_dd;       // 250
    double  data_rate_hd;       // 500
    double  data_rate_ed;       // 1000
    
    // Bit cell (microseconds)
    double  bit_cell_dd;        // 2.0 MFM DD
    double  bit_cell_hd;        // 1.0 MFM HD
    
    // Track format
    size_t  track_length_bits;
    double  write_precomp_ns;
    
    // Special
    bool    variable_rpm;       // Apple 3.5"
    bool    half_tracks;        // CBM
    int     speed_zones;        // CBM: 4 zones
} uft_drive_profile_t;
```

### Standard-Profile

| Drive | Cyl | RPM | Data Rate | Bit Cell | Special |
|-------|-----|-----|-----------|----------|---------|
| 5.25" DD | 40 | 300 | 250 kbps | 4µs (FM), 2µs (MFM) | - |
| 5.25" HD | 80 | 360 | 500 kbps | 1µs | Write precomp |
| 3.5" DD | 80 | 300 | 250 kbps | 2µs | - |
| 3.5" HD | 80 | 300 | 500 kbps | 1µs | - |
| 3.5" ED | 80 | 300 | 1000 kbps | 0.5µs | Perpendicular |
| CBM 1541 | 35-42 | ~300 | Variable | 3.25-4.0µs | 4 Speed Zones |
| Apple II | 35-40 | 300 | 250 kbps | ~4µs | Half-tracks |

---

## Austauschformat: nibtools ↔ Flux-Hardware

### Das Problem

```
nibtools erwartet G64/NIB (GCR-Bitstream)
         ↑
         │ INKOMPATIBEL
         ↓
Greaseweazle/Kryoflux liefern SCP/raw (Flux-Timing)
```

### Die Lösung: uft_raw_track_t als Zwischenformat

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    uft_raw_track_t (Universal Bitstream)                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ HEADER                                                              │   │
│   │ • cylinder, head, is_half_track                                    │   │
│   │ • encoding (MFM/FM/GCR_CBM/GCR_APPLE)                             │   │
│   │ • nominal_bit_rate, nominal_rpm                                    │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ BITSTREAM DATA (packed, MSB first)                                 │   │
│   │ • bits[byte_count] - die eigentlichen Bits                         │   │
│   │ • bit_count - Gesamtzahl Bits                                      │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ OPTIONAL: TIMING (per-bit, in ns)                                  │   │
│   │ • timing[bit_count] - Zeit jedes Bit-Cells                         │   │
│   │ → Ermöglicht Flux-Synthese                                         │   │
│   │ → Erhält Timing-Variationen für Kopierschutz                       │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ OPTIONAL: WEAK BITS (Maske)                                        │   │
│   │ • weak_mask[byte_count] - 1 = unsicheres Bit                       │   │
│   │ → Für Kopierschutz-Erkennung                                       │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │ OPTIONAL: INDEX POSITIONS                                          │   │
│   │ • index_positions[] - Bit-Offsets der Index-Holes                  │   │
│   │ • index_count - Anzahl Revolutionen                                │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Konvertierungspfade

```
                          uft_raw_track_t
                                │
        ┌───────────────────────┼───────────────────────┐
        │                       │                       │
        ▼                       ▼                       ▼
   ┌─────────┐            ┌─────────┐            ┌─────────┐
   │   SCP   │            │   G64   │            │   HFE   │
   │Kryoflux │            │   NIB   │            │         │
   └─────────┘            └─────────┘            └─────────┘
        │                       │                       │
        │                       ▼                       │
        │                  ┌─────────┐                  │
        │                  │nibtools │                  │
        │                  │ adftools│                  │
        │                  └─────────┘                  │
        │                       │                       │
        ▼                       ▼                       ▼
   ┌─────────────────────────────────────────────────────┐
   │                   UFT Pipeline                      │
   │                                                     │
   │  SCP → [PLL Decode] → uft_raw_track_t → [G64 Write] → nibtools
   │                                                     │
   │  nibtools output → [G64 Read] → uft_raw_track_t → [SCP Write] → Hardware
   │                                                     │
   └─────────────────────────────────────────────────────┘
```

---

## Minimale Decoder Adapter APIs

### 1. PLL Decoder (Flux → Bitstream)

```c
// Initialisierung
uft_pll_state_t* pll = uft_pll_create(&(uft_pll_params_t){
    .nominal_bit_cell_ns = 2000,    // 2µs für MFM DD
    .pll_bandwidth = 0.05,          // 5% tracking
    .clock_tolerance = 0.10,        // 10% Toleranz
    .detect_weak_bits = true,
});

// Dekodierung
uft_raw_track_t track;
uft_raw_track_init(&track);
uft_pll_decode(pll, flux_samples, sample_count, 
               72.0,  // MHz sample rate
               &track);

// Statistiken
uft_pll_stats_t stats;
uft_pll_get_stats(pll, &stats);
printf("Jitter: %.1f ns\n", stats.jitter_ns);
```

### 2. Sync Decoder (Bitstream → Sectors)

```c
// Sync-Decoder für MFM erstellen
uft_sync_decoder_t* sync = uft_sync_create(UFT_ENC_MFM);

// Sektoren finden
uft_sector_header_t headers[20];
int count = uft_sync_find_sectors(sync, &track, headers, 20);

// Sektor-Daten dekodieren
for (int i = 0; i < count; i++) {
    uft_sector_data_t data;
    uft_sync_decode_sector(sync, &track, &headers[i], &data);
    
    if (data.crc_valid) {
        // Sektor OK
    }
}
```

### 3. Encoder (Sectors → Bitstream)

```c
// Encoder erstellen
uft_encoder_t* enc = uft_encoder_create(UFT_ENC_GCR_CBM);

// Track-Format setzen (CBM 1541)
uft_encoder_set_format(enc, &(uft_track_format_t){
    .sectors_per_track = 21,    // Zone 1
    .sector_size = 256,
    .interleave = 4,
    .gap1_bytes = 5,
    .gap2_bytes = 9,
    .gap3_bytes = 9,
    .fill_byte = 0x55,
});

// Sektoren enkodieren
uft_raw_track_t output;
uft_encoder_encode_track(enc, cylinder, head, sector_data, 21, &output);
```

### 4. High-Level Decoder Adapter

```c
// Komplette Pipeline: Flux → Sectors
uft_decoder_adapter_t* dec = uft_decoder_adapter_create(UFT_ENC_MFM);

uft_sector_data_t sectors[20];
int sector_count;

uft_decoder_adapter_flux_to_sectors(
    dec,
    flux_samples, sample_count,
    72.0,  // MHz
    sectors, &sector_count, 20
);

// Oder: Flux → Bitstream (für G64/NIB)
uft_raw_track_t bitstream;
uft_decoder_adapter_flux_to_bitstream(
    dec,
    flux_samples, sample_count,
    72.0,
    &bitstream
);

// bitstream kann jetzt als G64/NIB gespeichert werden
```

---

## Warum gemeinsames Zwischenformat statt N×M Konverter?

### Das N×M Problem

```
OHNE Zwischenformat: N×M Konverter nötig

  SCP ──────┬──────► G64
            ├──────► HFE
            ├──────► D64
            └──────► NIB
            
  Kryoflux ─┬──────► G64
            ├──────► HFE
            ├──────► D64
            └──────► NIB
            
  HFE ──────┬──────► G64
            ├──────► SCP
            ├──────► D64
            └──────► NIB

= 4 Formate × 4 Formate = 16 Konverter-Pfade (minus Identität)
= Exponentielles Wachstum bei neuen Formaten!
```

### Die Lösung: Hub-Format

```
MIT uft_raw_track_t als Hub: N+M Konverter

  SCP ──────┐
            │
  Kryoflux ─┤                       ┌──────► G64
            │                       │
  HFE ──────┼───► uft_raw_track_t ──┼──────► HFE
            │          (Hub)        │
  G64 ──────┤                       ├──────► D64
            │                       │
  NIB ──────┘                       └──────► NIB

= 5 Reader + 4 Writer = 9 Module statt 20!
= Lineares Wachstum bei neuen Formaten
```

### Vorteile des Hub-Formats

| Aspekt | Ohne Hub | Mit Hub |
|--------|----------|---------|
| Neue Format-Integration | N neue Konverter | 1 Reader + 1 Writer |
| Code-Duplizierung | Hoch (PLL in jedem Konverter) | Niedrig (PLL einmal) |
| Testbarkeit | N×M Tests | N+M Tests |
| Debugging | Komplex | Isolierte Module |
| Timing-Erhaltung | Inkonsistent | Einheitlich |
| Weak-Bit-Handling | Pro Konverter | Zentral |

---

## Zusammenfassung der APIs

### Minimale APIs für Decoder Adapter

```c
// 1. PLL (Flux → Bits)
uft_pll_state_t* uft_pll_create(const uft_pll_params_t* params);
uft_error_t uft_pll_decode(uft_pll_state_t*, const uint32_t* flux, 
                           size_t count, double mhz, uft_raw_track_t* out);

// 2. Sync (Bits → Sectors)  
uft_sync_decoder_t* uft_sync_create(uft_encoding_t enc);
int uft_sync_find_sectors(uft_sync_decoder_t*, const uft_raw_track_t*,
                          uft_sector_header_t*, int max);
uft_error_t uft_sync_decode_sector(uft_sync_decoder_t*, const uft_raw_track_t*,
                                   const uft_sector_header_t*, uft_sector_data_t*);

// 3. Encoder (Sectors → Bits)
uft_encoder_t* uft_encoder_create(uft_encoding_t enc);
uft_error_t uft_encoder_encode_track(uft_encoder_t*, int cyl, int head,
                                     const uint8_t** sectors, int count,
                                     uft_raw_track_t* out);

// 4. High-Level Adapter
uft_decoder_adapter_t* uft_decoder_adapter_create(uft_encoding_t);
uft_error_t uft_decoder_adapter_flux_to_sectors(...);
uft_error_t uft_decoder_adapter_flux_to_bitstream(...);
uft_error_t uft_decoder_adapter_bitstream_to_sectors(...);
uft_error_t uft_decoder_adapter_sectors_to_bitstream(...);
```

### Integration mit externen Tools

```c
// SCP einlesen → nibtools-kompatibles G64 erzeugen
uft_io_source_t* src;
uft_io_open_source("disk.scp", &src);

uft_io_sink_t* sink;
uft_io_create_sink("disk.g64", UFT_FORMAT_G64, 42, 1, &sink);

// Automatische Konvertierung: SCP → [Decode] → uft_raw_track_t → G64
uft_io_copy(src, sink, progress_cb, NULL);

// Jetzt kann nibtools das G64 verarbeiten
system("nibwrite -D8 disk.g64");
```
