# Header-Duplikate Tracking — P3 Restaufgaben

**Stand:** 2026-02-08
**Kontext:** 66 doppelte Header-Namen identifiziert. 14 sicher konsolidiert (siehe unten).
Die verbleibenden ~52 Paare haben unterschiedlichen Inhalt in beiden Versionen
und benötigen Full-Build-Verifikation vor Konsolidierung.

## ✅ ERLEDIGT (14 Header)

### Kategorie A: Org war leerer Stub → Forward zu Flat (6)
| Stub (Forward)                              | Kanonisch (echte Defs)           |
|---------------------------------------------|----------------------------------|
| `uft/formats/uft_adf.h`                    | `uft/uft_adf.h`                 |
| `uft/formats/uft_amiga_mfm.h`              | `uft/uft_amiga_mfm.h`           |
| `uft/formats/uft_apple_gcr.h`              | `uft/uft_apple_gcr.h`           |
| `uft/formats/uft_c64_gcr.h`                | `uft/uft_c64_gcr.h`             |
| `uft/formats/uft_crc.h`                    | `uft/uft_crc.h`                 |
| `uft/formats/uft_endian.h`                 | `uft/uft_endian.h`              |

### Kategorie D: Identische Paare → Flat forward zu Org (5)
| Forward                                     | Kanonisch                        |
|---------------------------------------------|----------------------------------|
| `uft/uft_altair.h`                         | `uft/formats/uft_altair.h`      |
| `uft/uft_altair_cpm.h`                     | `uft/formats/uft_altair_cpm.h`  |
| `uft/uft_altair_hd.h`                      | `uft/formats/uft_altair_hd.h`   |
| `uft/uft_fdc.h`                            | `uft/fdc/uft_fdc.h`             |
| `uft/uft_floppy_geometry.h`                | `uft/formats/uft_floppy_geometry.h` |

### ORG_ONLY Stubs → Forward (3)
| Forward                                     | Kanonisch                        |
|---------------------------------------------|----------------------------------|
| `uft/formats/uft_atari_st.h`               | `uft/formats/atari/uft_atari_st.h` |
| `uft/formats/variants/parsers/uft_hfe_v3.h`| `uft/formats/uft_hfe_v3.h`      |
| `tracks/types.h` + `tracks/track_formats/types.h` | `uft/compat/types.h`      |

---

## 🔶 OFFEN — Braucht Full-Build-Verifikation

### Flat > Org (Flat ist größer/vollständiger)
Diese Flat-Header enthalten den Haupt-API und werden aktiv von Sources genutzt.
Die Org-Versionen sind kleiner, aber nicht leer — sie definieren eigene Subsets.

| Header               | Flat (Zeilen) | Org-Pfad                         | Org (Zeilen) |
|----------------------|---------------|----------------------------------|--------------|
| `uft_applesauce.h`  | 547           | `uft/hal/`                       | 122          |
| `uft_core.h`        | 398           | `uft/core/`                      | 69           |
| `uft_crc_polys.h`   | 446           | `uft/crc/`                       | 285          |
| `uft_floppy_formats.h` | 349        | `uft/formats/`                   | 340          |
| `uft_format_detect.h`  | 450        | `uft/detect/`                    | 218          |
| `uft_format_registry.h`| 359        | `uft/core/` + `uft/profiles/`   | 355 / 456    |
| `uft_hfe.h`         | 284           | `uft/flux/`                      | 253          |
| `uft_hfe_format.h`  | 381           | `uft/profiles/`                  | 197          |
| `uft_hfs.h`         | 518           | `uft/nintendo/`                  | 236          |
| `uft_hfs_extended.h`| 557           | `uft/fs/`                        | 332          |
| `uft_integration.h` | 466           | `uft/core/`                      | 360          |
| `uft_ipf.h`         | 443           | `uft/formats/ipf/`              | 400          |
| `uft_libflux_formats.h`| 649        | `uft/formats/`                   | 454          |
| `uft_platform.h`    | 500           | `uft/compat/`                    | 276          |
| `uft_protection.h`  | 965           | `uft/forensic/` + `uft/protection/` | 16 / 547 |
| `uft_recovery.h`    | 303           | `uft/decoder/` + `uft/forensic/` + `uft/recovery/` | 62 / 5 / 352 |
| `uft_safe_io.h`     | 798           | `uft/core/`                      | 425          |
| `uft_woz.h`         | 769           | `uft/formats/apple/` + `uft/formats/` | 445 / 418 |
| `uft_td0.h`         | 402           | `uft/formats/`                   | 400          |
| `uft_dmk.h`         | 319           | `uft/formats/`                   | 318          |

### Org > Flat (Org ist größer/vollständiger)
| Header               | Flat (Zeilen) | Org-Pfad                         | Org (Zeilen) |
|----------------------|---------------|----------------------------------|--------------|
| `uft_adaptive_decoder.h` | 258      | `uft/flux/`                      | 382          |
| `uft_atari_dos.h`   | 494           | `uft/fs/`                        | 833          |
| `uft_bbc_dfs.h`     | 410           | `uft/formats/` + `uft/fs/`      | 416 / 928    |
| `uft_disk.h`        | 110           | `uft/core/`                      | 222          |
| `uft_error_compat.h`| 71            | `uft/core/`                      | 237          |
| `uft_fat12.h`       | 540           | `uft/formats/` + `uft/fs/`      | 500 / 1125   |
| `uft_fdi.h`         | 415           | `uft/formats/`                   | 417          |
| `uft_floppy_encoding.h` | 441       | `uft/formats/`                   | 444          |
| `uft_format_convert.h` | 215        | `uft/convert/`                   | 397          |
| `uft_format_params.h` | 333         | `uft/formats/`                   | 341          |
| `uft_greaseweazle.h`| 338           | `uft/hal/`                       | 369          |
| `uft_imd.h`         | 416           | `uft/formats/`                   | 417          |
| `uft_mfm_flux.h`    | 79            | `uft/flux/`                      | 351          |
| `uft_pll.h`         | 58            | `uft/decoder/`                   | 79           |
| `uft_pll_params.h`  | 180           | `uft/flux/`                      | 283          |
| `uft_recovery_params.h` | 29        | `uft/recovery/`                  | 273          |
| `uft_scp.h`         | 130           | `uft/formats/`                   | 266          |
| `uft_scp_format.h`  | 472           | `uft/profiles/`                  | 646          |

### ORG_ONLY (verschiedene APIs, beide nötig)
| Header                  | Pfad 1                  | Pfad 2                    | Notiz                      |
|-------------------------|-------------------------|---------------------------|----------------------------|
| `uft_file_signatures.h` | `uft/carving/` (222)   | `uft/forensic/` (194)    | Verschiedene Signaturen    |
| `uft_flux_decoder.h`   | `uft/flux/` (340)       | `uft/ride/` (1078)        | Verschiedene APIs          |
| `uft_fusion.h`         | `uft/core/` (191)       | `uft/decoder/` (155)      | Verschiedene Fusion-Level  |
| `uft_gcr.h`            | `uft/decoder/` (139)    | `uft/flux/` (313)         | Low-level vs Flux-level    |
| `uft_floppy_utils.h`   | `tracks/` (?)           | `uft/` (?)                | Track vs UFT Namespace     |
| `uft_scp_multirev.h`   | `uft/formats/` (?)      | `uft/parsers/` (?)        | Format vs Parser           |
| `uft_sector_extractor.h`| `uft/extract/` (?)     | `uft/track/` (?)          | Module-spezifisch          |
| `uft_track_generator.h`| `uft/generate/` (?)     | `uft/track/` (?)          | Module-spezifisch          |
| `uft_nib_format.h`     | `uft/formats/c64/` (?)  | `uft/profiles/` (?)       | Format vs Profile          |
| `uft_pce.h`            | `uft/formats/nec/` (?)  | `uft/formats/` (?)        | Namespace-Duplikat         |
| `uft_pll_pi.h`         | `uft/flux/pll/` (?)     | `uft/flux/` (?)           | Submodule vs Parent        |
| `uft_kryoflux.h`       | `uft/flux/` (?)         | `uft/hal/` (?)            | Format vs Hardware         |
| `uft_latency_tracking.h`| `uft/hal/` (?)         | `uft/hardware/` (?)       | HAL vs Hardware Layer      |

---

## 📋 EMPFOHLENE VORGEHENSWEISE

1. **Full-Build auf Zielplattform** (Thinkpad mit Qt6) aufsetzen
2. Jeden Header-Paar einzeln konsolidieren + Build testen
3. Flat-Header schrittweise durch Forward-Includes ersetzen
4. Langfristziel: `include/uft/` enthält nur Forward-Includes,
   echter Code lebt in organisierten Unterordnern

**Geschätzter Aufwand:** ~2-4 Stunden bei vorhandenem Full-Build
