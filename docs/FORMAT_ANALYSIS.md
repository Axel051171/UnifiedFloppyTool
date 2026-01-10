# UFT Format-Analyse - Vollständige Bestandsaufnahme

**Stand:** 2026-01-09  
**Version:** v3.7.0  
**Analysemethodik:** Automatische Code-Analyse + manuelle Verifikation

---

## PHASE 1: Bestandsaufnahme (Ist-Zustand)

### 1.1 Format-Status-Matrix

#### Legende
| Symbol | Bedeutung |
|--------|-----------|
| ✅ | Vollständig implementiert |
| ⚠️ | Teilweise implementiert |
| ❌ | Nicht implementiert |
| 📁 | Verzeichnis existiert |
| 🔧 | Nur Stubs |

---

### COMMODORE FORMATE (12 Formate)

| Format | Ext | Read | Write | Recovery | Protection | Varianten | Status |
|--------|-----|:----:|:-----:|:--------:|:----------:|-----------|--------|
| **D64** | .d64 | ✅ | ✅ | ✅ | ✅ | 35/40 Track, Errors | **Production** |
| **D71** | .d71 | ✅ | ✅ | ⚠️ | ✅ | Standard | **Production** |
| **D80** | .d80 | ⚠️ | ❌ | ❌ | ❌ | 77 Track | Basic |
| **D81** | .d81 | ✅ | ❌ | ⚠️ | ⚠️ | 3.5" | Partial |
| **D82** | .d82 | 📁 | ❌ | ❌ | ❌ | Dual 8050 | Stub |
| **G64** | .g64 | ✅ | ✅ | ✅ | ✅ | GCR Raw | **Production** |
| **G71** | .g71 | 📁 | ❌ | ❌ | ❌ | 1571 GCR | Stub |
| **NIB** | .nib | ✅ | ❌ | ⚠️ | ⚠️ | Nibble Raw | Partial |
| **P64** | .p64 | 📁 | ❌ | ❌ | ❌ | Protected | Stub |
| **P00** | .p00 | ⚠️ | ❌ | ❌ | ❌ | PC64 | Basic |
| **T64** | .t64 | 📁 | ❌ | ❌ | ❌ | Tape Archive | Stub |
| **TAP** | .tap | 📁 | ❌ | ❌ | ❌ | Tape Raw | Stub |
| **CRT** | .crt | ⚠️ | ❌ | ❌ | ❌ | Cartridge | Basic |

**Zusammenfassung Commodore:**
- ✅ Voll: 4 (D64, D71, G64, NIB)
- ⚠️ Partial: 4 (D80, D81, P00, CRT)
- ❌ Stub: 4 (D82, G71, P64, T64, TAP)

---

### AMIGA FORMATE (8 Formate)

| Format | Ext | Read | Write | Recovery | Protection | Varianten | Status |
|--------|-----|:----:|:-----:|:--------:|:----------:|-----------|--------|
| **ADF** | .adf | ✅ | ✅ | ✅ | ✅ | OFS/FFS/DCFS | **Production** |
| **ADZ** | .adz | ✅ | ✅ | ⚠️ | ✅ | Compressed | **Production** |
| **DMS** | .dms | ✅ | ⚠️ | ⚠️ | ⚠️ | DiskMasher | Partial |
| **IPF** | .ipf | ✅ | ⚠️ | ✅ | ✅ | CAPS/SPS | **Production** |
| **ExtADF** | .adf | ✅ | ❌ | ⚠️ | ⚠️ | HD 1.76MB | Partial |
| **HDF** | .hdf | 📁 | ❌ | ❌ | ❌ | Hard Disk | Stub |
| **AXDF** | .axdf | ✅ | ✅ | ✅ | ✅ | UFT Extended | **Production** |
| **Writable** | - | ✅ | ✅ | ❌ | ❌ | Greaseweazle | Driver |

**Zusammenfassung Amiga:**
- ✅ Voll: 4 (ADF, ADZ, IPF, AXDF)
- ⚠️ Partial: 2 (DMS, ExtADF)
- ❌ Stub: 2 (HDF)

---

### APPLE II/MAC FORMATE (14 Formate)

| Format | Ext | Read | Write | Recovery | Protection | Varianten | Status |
|--------|-----|:----:|:-----:|:--------:|:----------:|-----------|--------|
| **DO** | .do | ✅ | ⚠️ | ⚠️ | ⚠️ | DOS 3.3 | **Production** |
| **PO** | .po | ✅ | ⚠️ | ⚠️ | ⚠️ | ProDOS | **Production** |
| **NIB** | .nib | ✅ | ❌ | ⚠️ | ⚠️ | Nibble | Partial |
| **2MG** | .2mg | ✅ | ❌ | ⚠️ | ❌ | Universal | Partial |
| **WOZ** | .woz | ✅ | ⚠️ | ✅ | ✅ | Applesauce v1/v2 | **Production** |
| **A2R** | .a2r | ✅ | ❌ | ⚠️ | ⚠️ | Applesauce Raw v2 | Partial |
| **A2R3** | .a2r | ⚠️ | ❌ | ❌ | ❌ | Applesauce Raw v3 | Basic |
| **DSK** | .dsk | ✅ | ⚠️ | ⚠️ | ⚠️ | Generic | **Production** |
| **DC42** | .dc42 | ⚠️ | ❌ | ❌ | ❌ | DiskCopy 4.2 | Basic |
| **DART** | .dart | 📁 | ❌ | ❌ | ❌ | Mac Archive | Stub |
| **MOOF** | .moof | ⚠️ | ❌ | ❌ | ❌ | Applesauce Mac | Basic |
| **400K** | - | ⚠️ | ❌ | ❌ | ❌ | Mac CLV | Basic |
| **800K** | - | ⚠️ | ❌ | ❌ | ❌ | Mac CLV | Basic |
| **1.44M** | - | ✅ | ⚠️ | ⚠️ | ❌ | Mac HD | Partial |

**Zusammenfassung Apple:**
- ✅ Voll: 5 (DO, PO, WOZ, DSK)
- ⚠️ Partial: 6 (NIB, 2MG, A2R, DC42, MOOF, 400K/800K)
- ❌ Stub: 3 (A2R3, DART)

---

### ATARI ST/8-BIT FORMATE (12 Formate)

| Format | Ext | Read | Write | Recovery | Protection | Varianten | Status |
|--------|-----|:----:|:-----:|:--------:|:----------:|-----------|--------|
| **ST** | .st | ✅ | ✅ | ⚠️ | ⚠️ | Raw Sectors | **Production** |
| **MSA** | .msa | ✅ | ⚠️ | ⚠️ | ⚠️ | Compressed | **Production** |
| **STX** | .stx | ✅ | ⚠️ | ✅ | ✅ | Pasti Protected | **Production** |
| **DIM** | .dim | ⚠️ | ⚠️ | ❌ | ❌ | FastCopy | Basic |
| **ATR** | .atr | ✅ | ⚠️ | ⚠️ | ⚠️ | 8-Bit Standard | **Production** |
| **ATX** | .atx | ✅ | ⚠️ | ⚠️ | ✅ | 8-Bit Protected | **Production** |
| **XFD** | .xfd | ✅ | ❌ | ⚠️ | ❌ | XFormer | Partial |
| **DCM** | .dcm | 📁 | ❌ | ❌ | ❌ | DiskComm | Stub |
| **PRO** | .pro | 📁 | ❌ | ❌ | ❌ | APE Pro | Stub |
| **TXDF** | .txdf | ✅ | ✅ | ✅ | ✅ | UFT Extended | **Production** |

**Zusammenfassung Atari:**
- ✅ Voll: 6 (ST, MSA, STX, ATR, ATX, TXDF)
- ⚠️ Partial: 3 (DIM, XFD)
- ❌ Stub: 3 (DCM, PRO)

---

### PC/IBM FORMATE (18 Formate)

| Format | Ext | Read | Write | Recovery | Protection | Varianten | Status |
|--------|-----|:----:|:-----:|:--------:|:----------:|-----------|--------|
| **IMG** | .img | ✅ | ⚠️ | ⚠️ | ❌ | 160K-2.88M | **Production** |
| **IMA** | .ima | ✅ | ⚠️ | ⚠️ | ❌ | WinImage | **Production** |
| **IMD** | .imd | ✅ | ✅ | ⚠️ | ⚠️ | ImageDisk | **Production** |
| **TD0** | .td0 | ✅ | ✅ | ⚠️ | ⚠️ | TeleDisk | **Production** |
| **DMK** | .dmk | ✅ | ⚠️ | ⚠️ | ⚠️ | TRS-80/CoCo | **Production** |
| **DSK** | .dsk | ✅ | ⚠️ | ⚠️ | ⚠️ | Generic | **Production** |
| **FDI** | .fdi | ✅ | ⚠️ | ⚠️ | ⚠️ | UKV | **Production** |
| **CQM** | .cqm | ✅ | ❌ | ❌ | ❌ | CopyQM | Partial |
| **DMF** | .dmf | ⚠️ | ❌ | ❌ | ❌ | MS DMF | Basic |
| **XDF** | .xdf | ⚠️ | ❌ | ❌ | ❌ | IBM XDF | Basic |
| **PXDF** | .pxdf | ✅ | ✅ | ✅ | ✅ | UFT Extended | **Production** |
| **BPB** | - | ✅ | ⚠️ | ❌ | ❌ | Boot Sector | Support |
| **FAT12** | - | ✅ | ⚠️ | ⚠️ | ❌ | Filesystem | Support |
| **FAT16** | - | ⚠️ | ❌ | ❌ | ❌ | Filesystem | Basic |
| **CPM** | - | ✅ | ❌ | ❌ | ❌ | CP/M 2.2/3.0 | Partial |
| **EDSK** | .dsk | ✅ | ⚠️ | ⚠️ | ⚠️ | Extended DSK | **Production** |

**Zusammenfassung PC:**
- ✅ Voll: 10 (IMG, IMA, IMD, TD0, DMK, DSK, FDI, EDSK, PXDF)
- ⚠️ Partial: 5 (CQM, DMF, XDF, FAT16, CPM)
- ❌ Stub: 3

---

### FLUX-FORMATE (14 Formate)

| Format | Ext | Read | Write | Recovery | Protection | Varianten | Status |
|--------|-----|:----:|:-----:|:--------:|:----------:|-----------|--------|
| **SCP** | .scp | ✅ | ⚠️ | ✅ | ✅ | SuperCard Pro | **Production** |
| **HFE** | .hfe | ✅ | ✅ | ⚠️ | ⚠️ | HxC v1/v3 | **Production** |
| **RAW** | .raw | ✅ | ✅ | ⚠️ | ⚠️ | Greaseweazle | **Production** |
| **KF** | .raw | ✅ | ❌ | ⚠️ | ⚠️ | KryoFlux Stream | Partial |
| **MFM** | - | ✅ | ⚠️ | ⚠️ | ⚠️ | Encoding | Support |
| **FM** | - | ✅ | ⚠️ | ⚠️ | ❌ | Encoding | Support |
| **GCR** | - | ✅ | ❌ | ⚠️ | ⚠️ | Encoding | Support |
| **DFI** | .dfi | ⚠️ | ❌ | ❌ | ❌ | DiscFerret | Basic |
| **MFI** | .mfi | ⚠️ | ❌ | ❌ | ❌ | MAME | Basic |
| **PDI** | .pdi | ⚠️ | ❌ | ❌ | ❌ | Pauline | Basic |
| **CTR** | .ctr | ❌ | ❌ | ❌ | ❌ | Catweasel | Missing |
| **FLX** | .flx | ❌ | ❌ | ❌ | ❌ | Generic Flux | Missing |
| **86F** | .86f | ⚠️ | ❌ | ❌ | ❌ | 86Box | Basic |
| **LDBS** | .ldbs | ❌ | ❌ | ❌ | ❌ | LibDsk | Missing |

**Zusammenfassung Flux:**
- ✅ Voll: 3 (SCP, HFE, RAW)
- ⚠️ Partial: 8 (KF, MFM, FM, GCR, DFI, MFI, PDI, 86F)
- ❌ Missing: 3 (CTR, FLX, LDBS)

---

### SPECTRUM/CPC FORMATE (12 Formate)

| Format | Ext | Read | Write | Recovery | Protection | Varianten | Status |
|--------|-----|:----:|:-----:|:--------:|:----------:|-----------|--------|
| **TRD** | .trd | ✅ | ❌ | ⚠️ | ⚠️ | TR-DOS | Partial |
| **SCL** | .scl | ✅ | ❌ | ❌ | ❌ | Compressed | Partial |
| **DSK** | .dsk | ✅ | ⚠️ | ⚠️ | ⚠️ | CPC/Spectrum | **Production** |
| **UDI** | .udi | ⚠️ | ❌ | ❌ | ❌ | Ultra Disk | Basic |
| **FDI** | .fdi | ✅ | ⚠️ | ⚠️ | ⚠️ | Full Disk | **Production** |
| **OPD** | .opd | ⚠️ | ❌ | ❌ | ❌ | Opus Discovery | Basic |
| **MGT** | .mgt | ⚠️ | ❌ | ❌ | ❌ | Miles Gordon | Basic |
| **SAD** | .sad | ⚠️ | ❌ | ❌ | ❌ | SAM Coupé | Basic |
| **TZX** | .tzx | ⚠️ | ❌ | ❌ | ❌ | Tape Extended | Basic |
| **TAP** | .tap | ⚠️ | ❌ | ❌ | ❌ | Tape Basic | Basic |
| **SSD** | .ssd | ⚠️ | ❌ | ❌ | ❌ | BBC Single | Basic |
| **ZXDF** | .zxdf | ✅ | ✅ | ✅ | ✅ | UFT Extended | **Production** |

**Zusammenfassung Spectrum/CPC:**
- ✅ Voll: 3 (DSK, FDI, ZXDF)
- ⚠️ Partial: 9 (TRD, SCL, UDI, OPD, MGT, SAD, TZX, TAP, SSD)

---

### JAPANISCHE FORMATE (10 Formate)

| Format | Ext | Read | Write | Recovery | Protection | Varianten | Status |
|--------|-----|:----:|:-----:|:--------:|:----------:|-----------|--------|
| **D88** | .d88 | ✅ | ✅ | ⚠️ | ⚠️ | PC-88/98 | **Production** |
| **D77** | .d77 | ⚠️ | ❌ | ❌ | ❌ | FM-7/77 | Basic |
| **DIM** | .dim | ⚠️ | ⚠️ | ❌ | ❌ | X68000 | Basic |
| **XDF** | .xdf | ⚠️ | ❌ | ❌ | ❌ | X68000 78T | Basic |
| **NFD** | .nfd | ⚠️ | ❌ | ❌ | ❌ | PC-98 r0/r1 | Basic |
| **HDM** | .hdm | ⚠️ | ❌ | ❌ | ❌ | PC-98 HD | Basic |
| **FDD** | .fdd | ⚠️ | ❌ | ❌ | ❌ | PC-98 | Basic |
| **DCU** | .dcu | ⚠️ | ❌ | ❌ | ❌ | DiskCopy | Basic |
| **DCP** | .dcp | ⚠️ | ❌ | ❌ | ❌ | DiskCopy Pro | Basic |
| **2DD** | - | ⚠️ | ❌ | ❌ | ❌ | 720KB 360rpm | Basic |

**Zusammenfassung Japan:**
- ✅ Voll: 1 (D88)
- ⚠️ Partial: 9 (D77, DIM, XDF, NFD, HDM, FDD, DCU, DCP, 2DD)

---

### BBC/ACORN FORMATE (8 Formate)

| Format | Ext | Read | Write | Recovery | Protection | Varianten | Status |
|--------|-----|:----:|:-----:|:--------:|:----------:|-----------|--------|
| **ADFS** | .adf | ❌ | ❌ | ❌ | ❌ | Archimedes | **Missing** |
| **DFS** | .ssd | ⚠️ | ❌ | ❌ | ❌ | BBC DFS | Basic |
| **SSD** | .ssd | ⚠️ | ❌ | ❌ | ❌ | Single-Sided | Basic |
| **DSD** | .dsd | ⚠️ | ❌ | ❌ | ❌ | Double-Sided | Basic |
| **ADL** | .adl | ⚠️ | ❌ | ❌ | ❌ | Large ADFS | Basic |
| **ADM** | .adm | ❌ | ❌ | ❌ | ❌ | Medium ADFS | **Missing** |
| **ADS** | .ads | ❌ | ❌ | ❌ | ❌ | Small ADFS | **Missing** |
| **UEF** | .uef | 📁 | ❌ | ❌ | ❌ | Tape | Stub |

**Zusammenfassung BBC/Acorn:**
- ✅ Voll: 0
- ⚠️ Partial: 4 (DFS, SSD, DSD, ADL)
- ❌ Missing: 4 (ADFS, ADM, ADS, UEF)

---

### DEC/MINICOMPUTER FORMATE (6 Formate)

| Format | Ext | Read | Write | Recovery | Protection | Varianten | Status |
|--------|-----|:----:|:-----:|:--------:|:----------:|-----------|--------|
| **RX01** | - | ⚠️ | ❌ | ❌ | ❌ | 8" SS/SD | Basic |
| **RX02** | - | ⚠️ | ❌ | ❌ | ❌ | 8" SS/DD | Basic |
| **RX50** | - | ❌ | ❌ | ❌ | ❌ | Rainbow | **Missing** |
| **RT-11** | - | ❌ | ❌ | ❌ | ❌ | RT-11 FS | **Missing** |
| **RSX** | - | ❌ | ❌ | ❌ | ❌ | RSX-11 FS | **Missing** |
| **RSTS** | - | ❌ | ❌ | ❌ | ❌ | RSTS/E FS | **Missing** |

**Zusammenfassung DEC:**
- ✅ Voll: 0
- ⚠️ Partial: 2 (RX01, RX02)
- ❌ Missing: 4 (RX50, RT-11, RSX, RSTS)

---

### SYNTHESIZER/SAMPLER FORMATE (10 Formate)

| Format | Ext | Read | Write | Recovery | Protection | Varianten | Status |
|--------|-----|:----:|:-----:|:--------:|:----------:|-----------|--------|
| **EDE** | .ede | ❌ | ❌ | ❌ | ❌ | Ensoniq EPS | **Missing** |
| **EDA** | .eda | ❌ | ❌ | ❌ | ❌ | Ensoniq ASR | **Missing** |
| **EMX** | .emx | ❌ | ❌ | ❌ | ❌ | E-mu Emax | **Missing** |
| **EM1** | .em1 | ❌ | ❌ | ❌ | ❌ | E-mu | **Missing** |
| **OUT** | .out | ❌ | ❌ | ❌ | ❌ | Roland S-50 | **Missing** |
| **W30** | .w30 | ❌ | ❌ | ❌ | ❌ | Roland W-30 | **Missing** |
| **AKP** | .akp | ❌ | ❌ | ❌ | ❌ | Akai S1000 | **Missing** |
| **DSS** | .dss | ❌ | ❌ | ❌ | ❌ | Korg DSS-1 | **Missing** |
| **QD** | .qd | ❌ | ❌ | ❌ | ❌ | Akai S612 | **Missing** |
| **Prophet** | - | ❌ | ❌ | ❌ | ❌ | Prophet 2000 | **Missing** |

**Zusammenfassung Synthesizer:**
- ✅ Voll: 0
- ⚠️ Partial: 0
- ❌ Missing: 10

---

### WEITERE/SONSTIGE FORMATE (20+ Formate)

| Format | Ext | Read | Write | Recovery | Protection | Varianten | Status |
|--------|-----|:----:|:-----:|:--------:|:----------:|-----------|--------|
| **FDS** | .fds | ⚠️ | ❌ | ❌ | ❌ | Famicom | Basic |
| **LIF** | .lif | ⚠️ | ❌ | ❌ | ❌ | HP LIF | Basic |
| **SAP** | .sap | ⚠️ | ❌ | ❌ | ❌ | Thomson | Basic |
| **JV3** | .jv3 | ⚠️ | ❌ | ❌ | ❌ | TRS-80 | Basic |
| **JVC** | .jvc | ⚠️ | ❌ | ❌ | ❌ | TRS-80 | Basic |
| **QDOS** | .qdos | ⚠️ | ❌ | ❌ | ❌ | Sinclair QL | Basic |
| **MBD** | .mbd | ⚠️ | ❌ | ❌ | ❌ | Generic | Basic |
| **S24** | .s24 | ⚠️ | ❌ | ❌ | ❌ | Sega System 24 | Basic |
| **Brother** | - | ⚠️ | ❌ | ❌ | ❌ | Word Processor | Basic |
| **Victor** | - | ⚠️ | ❌ | ❌ | ❌ | Victor 9000 | Basic |
| **Micropolis** | - | 📁 | ❌ | ❌ | ❌ | Micropolis | Stub |
| **Northstar** | - | 📁 | ❌ | ❌ | ❌ | North Star | Stub |
| **TI-99** | - | 📁 | ❌ | ❌ | ❌ | TI-99/4A | Stub |
| **Dragon** | - | 📁 | ❌ | ❌ | ❌ | Dragon 32/64 | Stub |
| **MSX** | - | 📁 | ❌ | ❌ | ❌ | MSX | Stub |

---

## PHASE 1 ZUSAMMENFASSUNG

### Gesamtstatistik

| Kategorie | Voll | Partial | Stub/Missing | Total |
|-----------|:----:|:-------:|:------------:|:-----:|
| Commodore | 4 | 4 | 4 | 12 |
| Amiga | 4 | 2 | 2 | 8 |
| Apple | 5 | 6 | 3 | 14 |
| Atari | 6 | 3 | 3 | 12 |
| PC/IBM | 10 | 5 | 3 | 18 |
| Flux | 3 | 8 | 3 | 14 |
| Spectrum/CPC | 3 | 9 | 0 | 12 |
| Japan | 1 | 9 | 0 | 10 |
| BBC/Acorn | 0 | 4 | 4 | 8 |
| DEC | 0 | 2 | 4 | 6 |
| Synthesizer | 0 | 0 | 10 | 10 |
| Sonstige | 0 | 10 | 5 | 15 |
| **TOTAL** | **36** | **62** | **41** | **139** |

### Prozentuale Abdeckung

```
Vollständig:    36/139 = 26%
Partial:        62/139 = 45%
Stub/Missing:   41/139 = 29%

Nutzbar (Voll+Partial): 98/139 = 71%
```

---

## PHASE 2: Fehlende Formate (Priorisiert)

### 🔴 P0 - Kritisch Fehlend (Weit verbreitet, oft angefragt)

| Format | System | Relevanzscore | Aufwand | Begründung |
|--------|--------|:-------------:|---------|------------|
| **ADFS** | BBC/Acorn | 95 | M | UK Retro-Szene, Archimedes wichtig |
| **DC42** | Macintosh | 90 | S | DiskCopy 4.2 sehr verbreitet |
| **Mac 400K/800K** | Macintosh | 85 | L | CLV historisch wichtig |
| **TRD Write** | Spectrum | 80 | S | Nur Lesen vorhanden |
| **SCL Write** | Spectrum | 75 | S | Nur Lesen vorhanden |

### 🟠 P1 - Wichtig Fehlend (Spezialisiert, aktive Szene)

| Format | System | Relevanzscore | Aufwand | Begründung |
|--------|--------|:-------------:|---------|------------|
| **DIM (X68K)** | Sharp X68000 | 85 | M | Japan aktive Szene |
| **NFD r1** | NEC PC-98 | 80 | M | r0 vorhanden, r1 fehlt |
| **D77** | FM-7/77 | 70 | S | Japan Homecomputer |
| **A2R3** | Apple | 65 | M | Neueste Applesauce Version |
| **Brother** | Word Proc | 60 | M | Nische aber aktiv |

### 🟡 P2 - Nische Fehlend (Sammler, Archive)

| Format | System | Relevanzscore | Aufwand | Begründung |
|--------|--------|:-------------:|---------|------------|
| **EDE/EDA** | Ensoniq | 50 | M | Musik-Szene |
| **EMX** | E-mu | 45 | M | Musik-Szene |
| **RX50** | DEC | 40 | L | Historisch |
| **Victor 9000** | Victor | 35 | L | GCR spezial |
| **Micropolis** | CP/M | 30 | L | Sehr alt |

### 🟢 P3 - Optional (Sehr selten)

| Format | System | Relevanzscore | Aufwand | Begründung |
|--------|--------|:-------------:|---------|------------|
| **Roland OUT** | Sampler | 25 | M | Nische |
| **Akai AKP** | Sampler | 25 | M | Nische |
| **Korg DSS** | Sampler | 20 | M | Nische |
| **Prophet** | Sampler | 15 | L | Sehr selten |

---

## PHASE 3: Versions- & Variantenanalyse

### Kritische Varianten mit Inkompatibilitäten

| Format | Varianten | Unterschiede | Status |
|--------|-----------|--------------|--------|
| **D64** | 35T, 40T, Errors | Track-Anzahl, Error-Block | ✅ Alle |
| **G64** | v1, v2 | Track-Längen | ✅ Alle |
| **ADF** | OFS, FFS, DCFS, Int | Filesystem, Block-Size | ✅ Alle |
| **WOZ** | v1, v2 | Chunk-Format | ✅ Beide |
| **A2R** | v2, v3 | Capture-Format | ⚠️ v2 nur |
| **HFE** | v1, v3 | Encoding | ✅ Beide |
| **D88** | Standard, Extended | Header-Size | ⚠️ Standard nur |
| **NFD** | r0, r1 | Header-Format | ⚠️ r0 nur |
| **STX** | v1, v2, v3 | Pasti Revision | ✅ Alle |
| **DSK** | Standard, Extended | Sector-Info | ✅ Beide |

### Kopierschutz-Varianten

| Plattform | Schutz-Typen | Implementiert | Status |
|-----------|--------------|:-------------:|--------|
| **Amiga** | Copylock, PDOS, Custom MFM | ✅ 7 | Komplett |
| **C64** | V-MAX!, RapidLok, Vorpal, GMA | ✅ 5 | Teilweise |
| **Atari ST** | Copylock, Macrodos, Fuzzy | ✅ 5 | Komplett |
| **Apple II** | Nibble Count, Half-Track | ⚠️ 1 | Teilweise |
| **PC** | DMF, XDF, Fat Track | ⚠️ 2 | Teilweise |

---

## PHASE 4: Timeline

### Zeitliche Einordnung aller Formate

#### VERGANGENHEIT (Legacy, abgeschlossen)
- 8-Zoll Formate (RX01, RX02, IBM 3740)
- CP/M Varianten (Kaypro, Osborne)
- Frühe Apple (13-Sector)
- DEC RT-11, RSX-11

#### GEGENWART (Aktiv genutzt)
- Commodore D64/G64 (Emulation, neue Spiele)
- Amiga ADF/IPF (Demoszene, Preservation)
- Atari ST/STX (Preservation)
- Apple WOZ/A2R (Applesauce Community)
- PC IMG/IMD (Archive, Emulation)
- Flux SCP/HFE/RAW (Hardware-Tools)

#### ZUKUNFT (Preservation, Meta-Container)
- XDF Familie (AXDF, DXDF, PXDF, TXDF, ZXDF, MXDF)
- Applesauce A2R3
- MAME MFI Evolution
- Cloud-basierte Archive

---

### Entwicklungs-Timeline

```
2025 Q1-Q2: v3.8.0 - v3.9.0 (Format-Erweiterung)
├── P0: ADFS, DC42, TRD/SCL Write
├── P1: DIM, NFD r1, D77
└── Protection: Apple II Erweiterung

2025 Q3: v4.0.0 MAJOR
├── Mac 400K/800K CLV
├── A2R3 Support
├── GUI 2.0
└── Plugin-System

2025 Q4: v4.1.0 - v4.2.0 (Nische)
├── P2: Synthesizer (EDE, EMX)
├── P2: DEC (RX50)
└── P2: Weitere Legacy

2026: v5.0.0 (Completion)
├── Alle bekannten Formate
├── 100% Varianten-Support
└── Cloud-Integration
```

---

## PHASE 5: TODO-Integration

### Neue TODOs aus Analyse

#### P0 - Blocker/Kritisch

```
### P0-10: ADFS Loader implementieren
**Format:** Acorn ADFS (Archimedes)
**Aufwand:** M (2-3 Tage)
**Akzeptanz:** 
- [ ] Read Standard ADFS
- [ ] Read Big ADFS (D/E/F)
- [ ] Auto-Detect
**Test:** Sample-Images aus Stairway to Hell

### P0-11: DC42 Loader implementieren
**Format:** DiskCopy 4.2 (Macintosh)
**Aufwand:** S (1 Tag)
**Akzeptanz:**
- [ ] Read 400K/800K/1.44M
- [ ] Checksum-Validierung
- [ ] Tag-Data Support
**Test:** Sample-Images aus Macintosh Garden
```

#### P1 - Wichtig

```
### P1-10: TRD/SCL Write-Support
**Beschreibung:** Nur Read vorhanden
**Aufwand:** S (0.5 Tage)
**Akzeptanz:** Write + Round-trip Test

### P1-11: DIM (X68000) vollständig
**Beschreibung:** Nur Basic Read
**Aufwand:** M (2 Tage)
**Akzeptanz:** Read + Write + Varianten

### P1-12: NFD r1 Support
**Beschreibung:** r0 vorhanden, r1 fehlt
**Aufwand:** S (1 Tag)
**Akzeptanz:** r0 + r1 Read/Detect

### P1-13: A2R v3 Support
**Beschreibung:** v2 vorhanden, v3 fehlt
**Aufwand:** M (2 Tage)
**Akzeptanz:** v2 + v3 Read
```

#### P2 - Architektur/Erweiterung

```
### P2-20: Mac CLV Support (400K/800K)
**Beschreibung:** Variable Speed Encoding
**Aufwand:** L (5+ Tage)
**Akzeptanz:** 
- [ ] CLV Decoder
- [ ] Zone-Mapping
- [ ] GCR-Apple Support
**Abhängigkeit:** Hardware mit CLV-Fähigkeit

### P2-21: Synthesizer Format Familie
**Beschreibung:** EDE/EDA/EMX/OUT
**Aufwand:** L (1 Woche)
**Akzeptanz:** Mind. 2 Formate vollständig
```

---

### Deduplizierte TODO.md

Die folgenden Einträge wurden mit bestehenden zusammengeführt:

| Alt | Neu | Aktion |
|-----|-----|--------|
| P2-5: Format Write-Support | P1-10 TRD/SCL | Spezifiziert |
| - | P0-10 ADFS | Neu hinzugefügt |
| - | P0-11 DC42 | Neu hinzugefügt |
| - | P1-11 DIM | Neu hinzugefügt |
| - | P1-12 NFD r1 | Neu hinzugefügt |
| - | P1-13 A2R v3 | Neu hinzugefügt |
| - | P2-20 Mac CLV | Neu hinzugefügt |
| - | P2-21 Synthesizer | Neu hinzugefügt |

---

## FAZIT: Zukunftsfähigkeit

### Stärken
1. **Kernformate solide:** D64, ADF, WOZ, SCP, HFE = Production-Ready
2. **Protection-Support gut:** Amiga/Atari ST/C64 abgedeckt
3. **XDF Container:** Zukunftssichere Architektur
4. **HAL komplett:** Alle wichtigen Hardware-Controller

### Schwächen
1. **BBC/Acorn fehlt komplett:** ADFS = große Lücke
2. **Japan unvollständig:** DIM, NFD r1 fehlen
3. **Write-Support lückenhaft:** Viele nur Read
4. **Synthesizer = 0:** Komplette Nische fehlt

### Empfehlung

```
PRIORITÄT 1 (sofort): ADFS, DC42, TRD/SCL Write
PRIORITÄT 2 (Q1): DIM, NFD r1, D77
PRIORITÄT 3 (Q2): Mac CLV, A2R3
PRIORITÄT 4 (Q3+): Synthesizer, DEC Legacy

Geschätzte Zeit bis 90% Abdeckung: 6 Monate
Geschätzte Zeit bis 100% Abdeckung: 12 Monate
```

### Antwort auf: "Sind wir formatseitig zukunftsfähig?"

**JA, mit Einschränkungen.**

- ✅ Architektur erlaubt Erweiterung (Plugin, XDF)
- ✅ Kernformate stabil und vollständig
- ⚠️ BBC/Acorn = kritische Lücke für UK-Markt
- ⚠️ Japan = wichtig für Nischen-Community
- ❌ Synthesizer = komplett fehlend (Nische)

**Gesamtbewertung: 7/10** - Gute Basis, klare Erweiterungspfade definiert.
