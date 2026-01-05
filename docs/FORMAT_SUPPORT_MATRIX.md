# UFT Format Support Matrix

**Stand:** Januar 2026  
**Auto-generiert:** Nein (manuell gepflegt)

---

## Legende

| Symbol | Bedeutung |
|--------|-----------|
| ✅ | Vollständig implementiert & getestet |
| ⚠️ | Implementiert, Tests unvollständig |
| 🔧 | In Entwicklung |
| ❌ | Nicht implementiert |
| 🔒 | Hardened Parser verfügbar |

---

## 1. Commodore

| Format | Ext | Read | Write | Convert | Tested | Hardened | Status |
|--------|-----|------|-------|---------|--------|----------|--------|
| D64 (35 Track) | .d64 | ✅ | ✅ | ✅ | ✅ | 🔒 | Production |
| D64 (40 Track) | .d64 | ✅ | ✅ | ✅ | ⚠️ | 🔒 | Production |
| D64 (42 Track) | .d64 | ✅ | ✅ | ✅ | ⚠️ | 🔒 | Production |
| D64 + Errors | .d64 | ✅ | ✅ | ✅ | ⚠️ | 🔒 | Production |
| G64 | .g64 | ✅ | ⚠️ | ✅ | ⚠️ | 🔒 | Beta |
| D71 | .d71 | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | Beta |
| D81 | .d81 | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | Beta |
| D80 | .d80 | ⚠️ | ⚠️ | ⚠️ | ❌ | ⚠️ | Alpha |
| D82 | .d82 | ⚠️ | ⚠️ | ⚠️ | ❌ | ⚠️ | Alpha |
| G71 | .g71 | ⚠️ | ❌ | ⚠️ | ❌ | ⚠️ | Alpha |
| NIB | .nib | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | Beta |
| NBZ | .nbz | ⚠️ | ⚠️ | ⚠️ | ❌ | ❌ | Alpha |

---

## 2. Amiga

| Format | Ext | Read | Write | Convert | Tested | Hardened | Status |
|--------|-----|------|-------|---------|--------|----------|--------|
| ADF (OFS) | .adf | ✅ | ✅ | ✅ | ✅ | 🔒 | Production |
| ADF (FFS) | .adf | ✅ | ✅ | ✅ | ⚠️ | 🔒 | Production |
| ADF (HD) | .adf | ✅ | ✅ | ✅ | ⚠️ | 🔒 | Beta |
| ADZ (compressed) | .adz | ⚠️ | ❌ | ⚠️ | ❌ | ❌ | Alpha |
| DMS | .dms | ❌ | ❌ | ❌ | ❌ | ❌ | Planned |

---

## 3. Flux Formats

| Format | Ext | Read | Write | Convert | Tested | Hardened | Status |
|--------|-----|------|-------|---------|--------|----------|--------|
| SCP | .scp | ✅ | ⚠️ | ✅ | ✅ | 🔒 | Production |
| KryoFlux Raw | .raw | ✅ | ❌ | ✅ | ⚠️ | ❌ | Beta |
| HFE v1 | .hfe | ✅ | ✅ | ✅ | ⚠️ | 🔒 | Production |
| HFE v3 | .hfe | ✅ | ⚠️ | ✅ | ⚠️ | 🔒 | Beta |
| IPF | .ipf | ⚠️ | ❌ | ⚠️ | ❌ | ❌ | Alpha |
| MFI | .mfi | ⚠️ | ❌ | ⚠️ | ❌ | ❌ | Alpha |

---

## 4. Apple

| Format | Ext | Read | Write | Convert | Tested | Hardened | Status |
|--------|-----|------|-------|---------|--------|----------|--------|
| WOZ 1.0 | .woz | ✅ | ⚠️ | ✅ | ⚠️ | ⚠️ | Beta |
| WOZ 2.0 | .woz | ✅ | ⚠️ | ✅ | ⚠️ | ⚠️ | Beta |
| NIB (Apple) | .nib | ✅ | ✅ | ✅ | ⚠️ | ❌ | Beta |
| DO (ProDOS) | .do | ⚠️ | ⚠️ | ⚠️ | ❌ | ❌ | Alpha |
| PO (ProDOS) | .po | ⚠️ | ⚠️ | ⚠️ | ❌ | ❌ | Alpha |
| 2MG | .2mg | ⚠️ | ⚠️ | ⚠️ | ❌ | ❌ | Alpha |

---

## 5. Atari

| Format | Ext | Read | Write | Convert | Tested | Hardened | Status |
|--------|-----|------|-------|---------|--------|----------|--------|
| ATR | .atr | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | Beta |
| ATX | .atx | ⚠️ | ❌ | ⚠️ | ❌ | ❌ | Alpha |
| ST | .st | ✅ | ✅ | ✅ | ⚠️ | ❌ | Beta |
| MSA | .msa | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | Beta |
| STX | .stx | ⚠️ | ❌ | ⚠️ | ❌ | ❌ | Alpha |

---

## 6. IBM PC / DOS

| Format | Ext | Read | Write | Convert | Tested | Hardened | Status |
|--------|-----|------|-------|---------|--------|----------|--------|
| IMG (FAT12) | .img | ✅ | ✅ | ✅ | ⚠️ | ❌ | Beta |
| IMA | .ima | ✅ | ✅ | ✅ | ⚠️ | ❌ | Beta |
| IMD | .imd | ✅ | ⚠️ | ✅ | ⚠️ | ⚠️ | Beta |
| TD0 | .td0 | ⚠️ | ❌ | ⚠️ | ❌ | ❌ | Alpha |
| DMF | .dmf | ⚠️ | ⚠️ | ⚠️ | ❌ | ❌ | Alpha |

---

## 7. Amstrad / Spectrum

| Format | Ext | Read | Write | Convert | Tested | Hardened | Status |
|--------|-----|------|-------|---------|--------|----------|--------|
| DSK (CPC) | .dsk | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | Beta |
| EDSK | .dsk | ✅ | ⚠️ | ✅ | ⚠️ | ⚠️ | Beta |
| TRD | .trd | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | Beta |
| SCL | .scl | ⚠️ | ⚠️ | ⚠️ | ❌ | ❌ | Alpha |

---

## 8. Japanese (PC-98, etc.)

| Format | Ext | Read | Write | Convert | Tested | Hardened | Status |
|--------|-----|------|-------|---------|--------|----------|--------|
| D88 | .d88 | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | Beta |
| FDI | .fdi | ⚠️ | ⚠️ | ⚠️ | ❌ | ⚠️ | Alpha |
| NFD | .nfd | ⚠️ | ❌ | ⚠️ | ❌ | ❌ | Alpha |
| HDM | .hdm | ⚠️ | ⚠️ | ⚠️ | ❌ | ❌ | Alpha |
| DIM | .dim | ⚠️ | ⚠️ | ⚠️ | ❌ | ❌ | Alpha |

---

## 9. BBC / Acorn

| Format | Ext | Read | Write | Convert | Tested | Hardened | Status |
|--------|-----|------|-------|---------|--------|----------|--------|
| SSD | .ssd | ✅ | ✅ | ✅ | ⚠️ | ⚠️ | Beta |
| DSD | .dsd | ✅ | ✅ | ✅ | ⚠️ | ❌ | Beta |
| ADF (BBC) | .adf | ⚠️ | ⚠️ | ⚠️ | ❌ | ❌ | Alpha |

---

## 10. Other

| Format | Ext | Read | Write | Convert | Tested | Hardened | Status |
|--------|-----|------|-------|---------|--------|----------|--------|
| CQM | .cqm | ✅ | ❌ | ✅ | ⚠️ | ⚠️ | Beta |
| SAP | .sap | ⚠️ | ⚠️ | ⚠️ | ❌ | ❌ | Alpha |
| SAD | .sad | ⚠️ | ⚠️ | ⚠️ | ❌ | ⚠️ | Alpha |
| DMK | .dmk | ✅ | ⚠️ | ✅ | ⚠️ | ⚠️ | Beta |

---

## Statistik

| Status | Anzahl |
|--------|--------|
| Production | 10 |
| Beta | 28 |
| Alpha | 25 |
| Planned | 2 |
| **Total** | **65** |

| Feature | Anzahl |
|---------|--------|
| Hardened Parser | 12 |
| Vollständig getestet | 5 |
| Write-Support | 45 |

---

**DOKUMENT ENDE**
