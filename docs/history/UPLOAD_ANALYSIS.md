# Upload-Analyse: Was können wir lernen?

## 1. libdiscferret-1.5 (DiscFerret Library)

**Typ:** USB Hardware-Treiber für DiscFerret Flux-Capture-Hardware  
**Lizenz:** Apache 2.0  
**Sprache:** C mit libusb

### Relevanz für UFT: ⭐ NIEDRIG

Die Library ist hauptsächlich USB-Kommunikationscode für die DiscFerret-Hardware.
- Wenig wiederverwendbarer Code für unser Software-Projekt
- Hardware-spezifische Register und FPGA-Microcode
- Könnte als HAL-Inspiration dienen für Hardware-Abstraktion

### Nützlich:
- `bitswap()` Funktion (Bit-Reihenfolge umkehren)
- Error-Code-Enum Pattern

---

## 2. floppy-decode-master (Haskell MFM Decoder)

**Typ:** MFM/FM Decoder in Haskell  
**Lizenz:** MIT  
**Sprache:** Haskell

### Relevanz für UFT: ⭐⭐⭐ MITTEL-HOCH

Sehr saubere, funktionale Implementierung eines MFM-Decoders!

### Interessante Algorithmen:

```haskell
-- 1. Hysterese-basierte Pulserkennung
transitionTimes :: Double -> Double -> [Double] -> [Int]
transitionTimes lowHyst highHyst = tt 0 False
  where
    tt time True (x : xs) | x < lowHyst = time : tt 0 False xs
    tt time True (x : xs) | otherwise = tt (time + 1) True xs
    ...

-- 2. Bit-Cell Quantisierung
toBaseBitRate :: Int -> [Int] -> [Int]
toBaseBitRate bitRate = map (\x -> (x + bitRate `div` 2) `div` bitRate)

-- 3. MFM Clock-Bit Entfernung
unMFM :: [Bool] -> [Bool]
unMFM (x:_:xs) = x : unMFM xs
unMFM xs = xs

-- 4. MFM Clock-Bit Insertion
addMFM :: [Bool] -> [Bool]
addMFM (False : rest@(False : _)) = False : True : addMFM rest
addMFM (x : rest) = x : False : addMFM rest
```

### Pattern-Definitionen (IBM MFM):
```haskell
-- Special sync bytes with non-standard clocking
xa1 = bits "100010010001001"  -- A1 with missing clock
xc2 = bits "101001000100100"  -- C2 with missing clock

-- Address marks
iam  = glue [xc2, xc2, xc2, xfc]  -- Index Address Mark
idam = glue [xa1, xa1, xa1, xfe]  -- ID Address Mark
damF8 = glue [xa1, xa1, xa1, xf8] -- Deleted Data Address Mark
damFB = glue [xa1, xa1, xa1, xfb] -- Data Address Mark
```

### Empfehlung: 
- Hysterese-Algorithmus könnte PLL verbessern
- Pattern-Definitionen validieren unsere IBM-MFM Implementation
- Saubere Trennung von Bitstream → Bytes → Sektoren

---

## 3. m2f2extract (CD-ROM Mode2 Form2 Extractor)

**Typ:** CD-ROM M2F2 Datei-Extraktor  
**Lizenz:** GPL  
**Sprache:** Delphi/Pascal

### Relevanz für UFT: ⭐ NIEDRIG

CD-ROM ist außerhalb unseres Floppy-Fokus, aber:

### Interessant:
- CRC-Implementierungen (CRC16 CCITT, CRC32 PKZIP)
- Optimierte x86-Assembler CRC-Routinen
- RIFF/CDXA Header-Parsing

### CRC16 CCITT (X25):
```pascal
Polynom: x^16 + x^12 + x^5 + 1
Polynom: $1021
Reversed: False

function UpdateCRC16(Init: Word; Buffer: Pointer; count: Longint): Word;
begin
  crc := Init;
  while Count > 0 do
  begin
    crc := (crc shl 8) xor CRC16Tab[(crc shr 8) xor PByte(Buffer)^];
    ...
  end;
end;
```

---

## Zusammenfassung

| Projekt | Relevanz | Aktion |
|---------|----------|--------|
| libdiscferret | ⭐ | Keine direkte Übernahme |
| floppy-decode | ⭐⭐⭐ | Algorithmen als Referenz nutzen |
| m2f2extract | ⭐ | CRC-Code vergleichen |

### Konkrete Verbesserungen für UFT:

1. **Hysterese-basierte Pulserkennung** für robustere Flux-Dekodierung
2. **Pattern-Validierung** unserer MFM Address Marks gegen Haskell-Code
3. **MFM addMFM/unMFM** als Referenz für Clock-Bit-Handling

Die floppy-decode Patterns sind konsistent mit unserer Implementation ✓
