# X-Copy Pro → UFT — Algorithm Migration Report

**Quelle:** X-Copy Professional (Amiga, 68k-Assembly), 1989–1991, Frank / H.V.
**Umfang:** ~10k Zeilen in `xcop.s` (4943), `xio.s` (4476), `depack.s`, `config.asm`
**Ziel:** Die algorithmisch relevanten Teile in UFT 3.9+ portieren
**Status:** Extern eingereichter Review 2026-04-24; Spot-Check bestätigt alle
konkret nachprüfbaren Claims (BitstreamDecoder.cpp:991-1017 Code verbatim,
uft_amiga_protection.c 367 LOC, flux_find_sync Pattern in uft_flux_decoder.c:223).

---

## TL;DR

X-Copy's Kernalgorithmen für Floppy-Reading, Track-Layout-Analyse und MFM-Decode sind
**heute noch konkurrenzfähig** — manche Sachen macht X-Copy eleganter als UFT aktuell.
Sieben Ideen sind direkt übertragbar, davon drei mit messbarem Impact. Der Rest ist
Amiga-Hardware-spezifisch und wird nicht gebraucht.

**Ein wichtiger Fund:** Der Vorschlag aus dem UFT-Performance-Review, Rolling-Shift-
Register für die Sync-Suche zu benutzen, wurde von X-Copy **1989 schon so gebaut** — mit
16-facher Rotation pro Long-Wort, Multi-Pattern-Match und Sektor-Positions-Tabelle. Das
ist Validierung, dass der Ansatz richtig ist.

---

## Copyright-Kontext (wichtig)

X-Copy Professional war kommerzielle Software. Der Source Code wurde später
inoffiziell veröffentlicht. Für UFT gilt: **Algorithmen und Ideen sind nicht
urheberrechtlich geschützt**, Code-Ausdruck schon. Die Empfehlung ist deshalb,
die X-Copy-Assembly **zu verstehen und neu in C zu schreiben** — nicht 1:1
translitterieren. Die resultierenden C-Funktionen sind dann sauberes UFT-IP.

---

## 1. Single-Pass Amiga-MFM-Decode (`decolop`)

### X-Copy-Original (`xcop.s:1002-1014`)

```asm
moveq   #$7F,D2         ; 128 Longs = 512 Bytes
lea     56(A1),A1       ; Skip header (56 Bytes vor Datenblock)
decolop move.l  512(A1),D0  ; ODD bits aus zweiter Hälfte (Offset +512)
        and.l   D3,D0       ; D3=$55555555 (MFM_MASK)
        eor.l   D0,D6       ; Running Checksum
        move.l  (A1)+,D1    ; EVEN bits aus aktueller Position
        and.l   D3,D1
        eor.l   D1,D6
        asl.l   #1,D1       ; EVEN-bits nach links schieben
        or.l    D1,D0       ; EVEN|ODD = rekombiniertes Long
        move.l  D0,(A2)+    ; Direkt in Zielpuffer
        dbf     D2,decolop
```

**Was macht das?** Amiga-MFM-Layout pro Sektor ist:
```
[32 Bytes Header] [512 Bytes alle EVEN-Bits] [512 Bytes alle ODD-Bits]
```
X-Copy greift auf die EVEN/ODD-Hälften mit einem **Offset von 512 Byte** zu. Eine
Schleife von 128 Iterationen, kein Zwischenspeicher, kein zweiter Pass.

### UFT-Aktuell (`BitstreamDecoder.cpp:991-1017`)

```cpp
static bool amiga_read_dwords(BitBuffer& bitbuf, uint32_t* pdw,
                              size_t dwords, uint32_t& checksum) {
    std::vector<uint32_t> evens;   // heap allocation
    evens.reserve(dwords);

    // First pass
    for (i = 0; i < dwords; ++i) {
        auto evenbits = bitbuf.read32();     // 32 × read1()!
        evens.push_back(evenbits);
        checksum ^= evenbits;
    }

    // Second pass
    for (auto evenbits : evens) {
        auto oddbits = bitbuf.read32();       // wieder 32 × read1()
        checksum ^= oddbits;
        uint32_t value = ((evenbits & MFM_MASK) << 1) | (oddbits & MFM_MASK);
        *pdw++ = util::betoh(value);
    }
    return !bitbuf.wrapped() || !bitbuf.tell();
}
```

Für einen 512-Byte-Sektor:
- 128 × `read32()` für evens = 4096 × `read1()`
- 128 `push_back` (vector-Wachstum)
- 128 × `read32()` für odds = 4096 × `read1()`
- Gesamt: **8192 Bit-Einzel-Reads + 1 heap alloc**

### UFT-Portierung

Der Trick von X-Copy funktioniert nur, wenn der MFM-Buffer als `uint32_t*`-Array
vorliegt (nicht als Bitstream mit arbitrary Offset). Da UFT den MFM-Track bereits
byte-aligned im Speicher hat (nach `flux_to_bitstream`), geht das:

```c
/**
 * Port von X-Copys decolop: single-pass EVEN/ODD MFM decode.
 * Voraussetzung: mfm_buf zeigt auf einen ausgerichteten 32-Byte-Header,
 * gefolgt von dwords*4 Bytes EVEN-Bits und dwords*4 Bytes ODD-Bits.
 */
static bool amiga_decode_dwords_xc(const uint8_t *mfm_buf,
                                    uint32_t *out, size_t dwords,
                                    uint32_t *inout_checksum) {
    const uint32_t *evens = (const uint32_t *)(mfm_buf);
    const uint32_t *odds  = (const uint32_t *)(mfm_buf + dwords * 4);
    uint32_t checksum = *inout_checksum;

    for (size_t i = 0; i < dwords; i++) {
        uint32_t e = __builtin_bswap32(evens[i]);  /* Big-endian auf Disk */
        uint32_t o = __builtin_bswap32(odds[i]);
        checksum ^= e;
        checksum ^= o;
        out[i] = __builtin_bswap32(((e & MFM_MASK) << 1) | (o & MFM_MASK));
    }

    *inout_checksum = checksum;
    return true;
}
```

**Voraussetzungen:**
- Caller muss sicherstellen, dass `mfm_buf` aligned + groß genug ist (`32 + 8*dwords`)
- Klarer API-Contract: kein `BitBuffer`, direkter Byte-Pointer

**Effort:** ~40 LOC + Call-Site-Anpassung in `scan_bitstream_amiga` · **Impact:** ~2×
schneller für Amiga-Standard-Decode, null heap alloc pro Sektor

**Achtung:** Das ist eine **zusätzliche** Code-Pfad, keine Ersetzung — der bestehende
`amiga_read_dwords` wird weiter gebraucht für Fälle, in denen nur der Bitstream
verfügbar ist (nicht-aligned nach Sync-Detection).

---

## 2. Multi-Pattern-Sync mit 16× Rotation

### X-Copy-Original (`xcop.s:2176-2205`, Funktion `Analyse`)

```asm
        move.w  #$9521,D2       ; Variante 1 (shifted by -1 bit)
        move.w  #$A245,D3       ; Variante 2 (shifted by -2 bits)
        move.w  #$A89A,D4       ; Variante 3
        move.w  #$448A,D5       ; Alt-Sync (A1/A1 mit Fehler)
        move.w  #$4489,D6       ; Standard Amiga MFM Sync

        move.w  (A2)+,D0
nextword swap   D0              ; High- und Low-Word tauschen
        move.w  (A2)+,D0
        swap    D0              ; D0 = { old_word, new_word }
        moveq   #$0F,D7         ; 16 Rotationen
1$      cmp.w   D0,D1           ; Check Haupt-Sync
        beq.s   syncfind
        cmp.w   D0,D2           ; Check Varianten
        beq.s   syncfind
        cmp.w   D0,D3
        beq.s   syncfind
        cmp.w   D0,D4
        beq.s   syncfind
        cmp.w   D0,D5
        beq.s   syncfind
        cmp.w   D0,D6
        beq.s   syncfind
        rol.l   #1,D0           ; Rotation: alle 16 Bit-Alignments testen
        dbf     D7,1$
        cmp.l   A1,A2
        blt     nextword
```

**Was macht das?** Statt nur nach `0x4489` zu suchen, sucht es nach **allen
bit-rotationierten Varianten** eines Sync-Patterns. Das ist die Antwort auf
Kopier-geschützte Amiga-Disketten, die mit absichtlich verrutschten Syncs arbeiten
(z.B. Rob Northen, Copylock).

Die `rol.l #1,D0` + `dbf D7,1$` Struktur testet das Pattern gegen 16 mögliche
Bit-Positionen in einem einzigen Pass über den Puffer. **Exakt das Rolling-Shift-
Register-Muster aus Fix #1 für `flux_find_sync`.**

### UFT-Portierung in `uft_amiga_protection.c` oder neu `uft_xc_sync_scan.c`

```c
typedef struct {
    uint16_t patterns[8];   /* up to 8 pattern variants */
    int n_patterns;
} xc_sync_set_t;

/**
 * Scans a bitstream (as uint8_t array, MSB-first) for any of `n_patterns`
 * 16-bit patterns. Returns bit-offsets into positions[] up to max_results.
 * Single pass, O(bit_count).
 */
static int xc_scan_multi_sync(const uint8_t *bits, size_t bit_count,
                               const xc_sync_set_t *set,
                               size_t *positions, int max_positions) {
    if (bit_count < 32) return 0;

    /* Prime window with first 32 bits */
    uint32_t window = 0;
    size_t pos = 0;
    for (int i = 0; i < 32 && pos < bit_count; i++, pos++) {
        window = (window << 1) | ((bits[pos >> 3] >> (7 - (pos & 7))) & 1);
    }

    int found = 0;
    while (pos < bit_count && found < max_positions) {
        /* Extract low 16 bits and check against patterns */
        uint16_t candidate = (uint16_t)window;
        for (int p = 0; p < set->n_patterns; p++) {
            if (candidate == set->patterns[p]) {
                positions[found++] = pos - 16;
                break;  /* only count once per position */
            }
        }
        if (found >= max_positions) break;

        /* Shift in next bit */
        window = (window << 1) | ((bits[pos >> 3] >> (7 - (pos & 7))) & 1);
        pos++;
    }
    return found;
}

/* Example: Amiga copy-protection sync variants, as used by X-Copy */
static const xc_sync_set_t xc_amiga_protection_syncs = {
    .patterns = { 0x4489, 0x4891, 0x9122, 0x2244,
                  0x4488, 0x8912, 0x1224, 0x2448 },
    .n_patterns = 8
};

/* Standard Amiga MFM */
static const xc_sync_set_t xc_amiga_standard_syncs = {
    .patterns = { 0x4489 },
    .n_patterns = 1
};
```

**Wo einbauen:**
- `src/protection/uft_amiga_protection.c` — aktuell 367 LOC, hat Detection-Logik
  aber keinen solchen Multi-Pattern-Scanner
- Nutzbar für Copylock, Rob Northen, und unbekannte Schemes mit verschobenen Syncs

**Effort:** ~50 LOC · **Impact:** Neue Detection-Fähigkeit, +1-2% für Standard-Decode

---

## 3. GAP-Detection über Sektorlängen-Histogramm

### X-Copy-Original (`xcop.s:2214-2261`, Funktionen `nex_len` / `nex_cmp` / `gapsearch`)

Die Logik ist:

1. Sammle alle Sync-Positionen in `syncpos[]`-Tabelle (max. 24).
2. Berechne Sektor-Längen als Differenzen: `lentab[i] = syncpos[i+1] - syncpos[i]`.
3. Baue Histogramm der Längen mit ±32-Byte-Toleranz: `coutab[j] = {länge, vorkommen}`.
4. Finde die **am seltensten vorkommende Länge** → das ist der GAP.
5. Der Sektor mit dieser Länge ist der Track-GAP (zwischen letztem und erstem Sektor).

```asm
; coutab wird aufgebaut
nex_len  move.w  (A0)+,D1         ; naechste Sektorlaenge
         beq.s   gapsearch        ; TABEND
         lea     coutab-4(A2),A2
nex_cmp  addq.l  #4,A2
         move.w  (A2),D2
         beq.s   new_entry        ; neuer Eintrag
         sub.w   D1,D2
         bpl.s   1$
         neg.w   D2
1$       cmp.w   #$20,D2          ; Toleranz ±32 Bytes
         bgt     nex_cmp
         move.w  D1,(A2)
         addq.w  #1,2(A2)         ; Vorkommen++
         bra     nex_len

gapsearch
         moveq   #$7f,D2          ; D2 = MAX
1$       move.l  (A0)+,D1
         beq.s   gap_fou
         cmp.w   D2,D1
         bgt.s   1$               ; letzten kleinen nehmen!!
         move.w  D1,D2            ; Minimum gefunden
         swap    D1
         move.w  D1,D5            ; Laenge des Minimums
         bra.s   1$
```

**Warum ist das schlau?** Standard-Amiga-Track hat 11 gleichlange Sektoren → Histogramm
hat **einen dicken Peak und einen Mini-Peak** (der GAP). Die 11 normalen Sektoren
haben Länge ~544 Bytes, der GAP ist variabel (vielleicht 60-80 Bytes). Mit
±32-Byte-Toleranz clustert das Histogramm robust trotz Spindle-Drift.

### UFT-Portierung in `src/analysis/` oder `src/algorithms/format/`

```c
/**
 * Port of X-Copy's gap-detection algorithm.
 * Given a sequence of sync positions (in bits), classifies each inter-sync
 * gap by length and returns the one that occurs least often — this is the
 * actual track gap.
 *
 * @return Index in syncs[] of the sync that marks the start of the gap,
 *         or -1 if no gap detected (single large sector).
 */
int xc_find_track_gap(const size_t *syncs, int n_syncs,
                      uint32_t tolerance_bits /* ~256 for ±32 bytes */) {
    if (n_syncs < 2) return -1;

    /* Length of each sector-gap between syncs */
    uint32_t lens[64];
    if (n_syncs > 64) n_syncs = 64;
    for (int i = 0; i < n_syncs - 1; i++) {
        lens[i] = (uint32_t)(syncs[i+1] - syncs[i]);
    }
    int n_lens = n_syncs - 1;

    /* Histogram with tolerance clustering */
    struct { uint32_t len; int count; } hist[16] = {0};
    int n_hist = 0;
    for (int i = 0; i < n_lens; i++) {
        int matched = 0;
        for (int h = 0; h < n_hist; h++) {
            uint32_t d = (lens[i] > hist[h].len) ?
                         lens[i] - hist[h].len : hist[h].len - lens[i];
            if (d <= tolerance_bits) {
                hist[h].count++;
                matched = 1;
                break;
            }
        }
        if (!matched && n_hist < 16) {
            hist[n_hist].len = lens[i];
            hist[n_hist].count = 1;
            n_hist++;
        }
    }

    /* Find least common length → that's the gap */
    int min_count = INT_MAX;
    uint32_t gap_len = 0;
    for (int h = 0; h < n_hist; h++) {
        if (hist[h].count < min_count) {
            min_count = hist[h].count;
            gap_len = hist[h].len;
        }
    }

    /* Find which sync starts a gap-length interval */
    for (int i = 0; i < n_lens; i++) {
        uint32_t d = (lens[i] > gap_len) ? lens[i] - gap_len : gap_len - lens[i];
        if (d <= tolerance_bits) return i;
    }
    return -1;
}
```

**Wo einbauen:**
- Neues File `src/analysis/uft_track_layout.c` (oder ähnlich)
- Nutzbar vor dem Decode, um Track-Start zu finden (wichtig für saubere Write-Backs)
- Komplementär zum existierenden `flux_detect_encoding` — zusätzliche Track-Struktur-Info

**Effort:** ~60 LOC · **Impact:** Verbesserte Nibble/RawCopy-Detection, bessere
Track-Start-Ausrichtung beim Schreiben

---

## 4. Reverse-Scan für Track-Länge (`getracklen`)

### X-Copy-Original (`xcop.s:2090-2100`)

```asm
getracklen:
        move.w  #RLEN-2,D0        ; Read LEN - 2
        lea     RLEN*2(A3),A1     ; Ende des Puffers (2 Revs)
1$      tst.w   -(A1)             ; Scan RÜCKWÄRTS nach Nicht-Null
        dbne    D0,1$             ; bis gefunden oder Ende
        addq.l  #2,A1
        move.l  A1,D0
        sub.l   A3,D0             ; Länge in Bytes = 2×Tracklänge
        lsr.w   #1,D0             ; durch 2 → Mitte = echte Tracklänge
        and.w   #$FFFE,D0         ; gerade, wegen LEA
        rts
```

**Was macht das?** Der DMA-Capture liest 2 Umdrehungen in einen Puffer. Der echte
Track ist irgendwo drin — aber wo genau endet er? X-Copy löst das **von hinten**:
scannt rückwärts, bis das erste Nicht-Null-Word kommt. Das markiert das Track-Ende
des 2. Revs. Division durch 2 → Tracklänge.

Einfach, robust, O(N).

### UFT-Portierung

UFT hat aktuell in `flux_to_bitstream` keine vergleichbare Heuristik. Der Bit-Count
wird durch den Input-Flux bestimmt, nicht durch tatsächliche Track-Struktur-Erkennung.

```c
/**
 * Measures actual track length in a multi-revolution MFM buffer by
 * scanning backwards from end for the last non-zero word.
 * Port of X-Copy's getracklen.
 *
 * @param buf      MFM buffer (uint16_t-array, little- or big-endian agnostic)
 * @param buf_len  Total buffer length in words (should cover ≥1.5 revolutions)
 * @param n_revs   Number of revolutions captured (typically 2)
 * @return Track length in words, or 0 on error
 */
size_t xc_measure_track_length_words(const uint16_t *buf, size_t buf_len,
                                      int n_revs) {
    if (n_revs < 2 || buf_len < 100) return 0;

    /* Scan backwards for last non-zero word */
    size_t i = buf_len;
    while (i > 0 && buf[i - 1] == 0) i--;
    if (i == 0) return 0;

    /* Split by number of revs → track length */
    size_t track_words = i / n_revs;
    return track_words & ~((size_t)1);  /* align to even words */
}
```

**Wo einbauen:**
- `src/flux/uft_flux_decoder.c` — optionaler Helper für HAL-Layer
- Besonders nützlich für Greaseweazle/SCP-Captures mit mehreren Revolutions

**Effort:** ~15 LOC · **Impact:** Bessere Track-Boundary-Erkennung, kein direkter Speedup

---

## 5. MFM-Grenzkorrektur (`clockbits` + `grenz`)

### X-Copy-Original (`xcop.s:1134-1148`)

```asm
clockbits:
        andi.l  #$55555555,D0    ; Data-Bits isolieren
        move.l  D0,D2
        eori.l  #$55555555,D2    ; Invertierte Data-Bits
        move.l  D2,D1
        lsl.l   #1,D2
        lsr.l   #1,D1
        bset.l  #31,D1
        and.l   D2,D1            ; Clock-Bits berechnet
        or.l    D1,D0
        btst    #0,-1(A1)        ; Vorheriges Bit prüfen!
        beq.s   1$
        bclr.l  #31,D0           ; Grenze korrigieren
1$      move.l  D0,(A1)+
        rts

grenz   move.b  (A1),D0          ; Grenzkorrektur zwischen Longs
        btst.b  #0,-1(A1)
        bne.s   1$
        btst.l  #6,D0
        bne.s   3$
        bset    #7,D0
        bra.s   2$
1$      bclr    #7,D0
2$      move.b  D0,(A1)
3$      rts
```

**Was macht das?** MFM hat die Regel: **nie zwei Clock-Bits in Folge, nie zwei
Data-Bits in Folge**. An Long-Grenzen kann diese Regel verletzt werden, wenn das
letzte Bit des vorherigen Longs und das erste Bit des nächsten Longs nicht
zueinander passen. `clockbits` generiert den MFM-Stream und korrigiert die
Grenze indem es Bit 31 des neuen Longs je nach Zustand von Bit 0 des vorherigen
Bytes löscht/setzt.

Das ist **absolut korrekte MFM-Clock-Rekonstruktion** — ohne das würde ein
geschriebener Track mit Sync-Fehlern vom Floppy-Controller abgelehnt.

### UFT-Relevanz

UFT hat einen Write-Path für Amiga (`uft_amiga_caps.c`, `uft_amiga_protection_full.c`),
aber es lohnt sich, dies zu vergleichen:

```c
/* In uft_amiga_*.c: suche nach clock-bit generation / MFM encoding */
grep -n "clock" src/protection/uft_amiga*.c
```

**Aktion:** Vergleiche UFTs MFM-Encoder mit diesem Algorithmus. Falls die
Grenzkorrektur fehlt oder unvollständig ist → direkter Bug-Fix. Falls sie bereits
vorhanden ist → Validierung.

**Effort:** ~30 LOC Review + ggf. Fix · **Impact:** Korrektheit beim Schreiben von
Amiga-Tracks, besonders für Protection-Writes

---

## 6. syncpos-Tabelle (Single-Pass Sync Collection)

Das ist **die Bestätigung von Fix #5 aus dem UFT-Performance-Review**. X-Copy macht's so:

```asm
        moveq   #24,D6          ; Platz für maximal 24 Sektoren
store   move.l  A2,(A0)+        ; Pointer auf Sync speichern
        add.l   #$100,A2        ; minimale Sektorlänge ($100 MFM-Bytes)
        ; ... Sync-Suche ...
        beq     store
        ; ...
getlast move.l  A2,(A0)+        ; letzten SYNC speichern
        clr.l   (A0)            ; ENDE-MARKER
```

Die Tabelle wird **einmal** gefüllt, danach arbeiten alle weiteren Analysen
(GAP-Detection, Sektor-Längen) auf dieser Tabelle — nicht nochmal auf dem
Rohpuffer.

UFT's Decoder-Loops (`flux_decode_mfm` etc.) rufen `flux_find_sync` in jedem
Iteration neu auf. Das ist **exakt der O(N·S)-Code** aus Fix #5.

X-Copy's Design ist der Beweis, dass der Ansatz seit 1989 funktioniert.

---

## 7. 2-Umdrehungen-Index-Capture (NibbleRead)

### X-Copy-Original (`xcop.s:1989-2045`)

```asm
nib_write
        move.w  #$7FFF,$9E(A6)    ; ADKCON clear
        move.w  #$9100,$9E(A6)    ; MFM mode
        move.w  #$3002,$9C(A6)    ; clear INDEX+DSKBLK
        move.l  A3,$20(A6)        ; DMA pointer
        move.b  #$90,$BFDD00      ; Index-IRQ ON → DMA startet bei Index
        or.b    #$10,I_enable     ; System-Mask
        StartTimer 917500         ; Timeout für 2 Revs
wdma    tst.b   flag              ; flag wird von ISR cleared
        beq.s   ok_nibRW
        TestTimer wdma            ; Timeout-Check
```

**Was macht das?** Das ist die klassische Index-synchronisierte Capture-Strategie:
1. DMA-Buffer vorbereiten
2. Index-Interrupt aktivieren
3. Bei nächstem Index → DMA startet automatisch
4. Warten bis DMA-Done-Flag gesetzt oder Timeout
5. Puffer enthält jetzt exakt 2 Revolutions ab Index

**Warum 2 Revs?** Weil man sicher **eine vollständige Revolution** haben will und
der genaue Start-Phasen des Heads zum Index unbekannt ist. Mit 2 Revs garantiert
man mindestens 1 voll im Puffer.

### UFT/UFI-Relevanz

Das ist **direkt relevant für UFI-Firmware** (STM32H723 + CM5). Der Ansatz ist:
- Capture-Buffer für ~2 Revs sizen (DD: ~100k transitions, HD: ~200k)
- Index-Pulse als DMA-Trigger nutzen (STM32 Timer + DMA-Trigger)
- Software kann danach einen sauberen vollen Rev extrahieren

Das existiert in UFI wahrscheinlich schon in irgendeiner Form — aber X-Copy's
exakte Sequenz und Timing-Überlegungen (917500 Timer-Ticks = ca. 460ms für 2 Revs
bei 300 RPM) sind wertvoller Referenzwert für Timeout-Berechnung.

**Effort:** architektonisch · **Impact:** Firmware-Korrektheit

---

## Was NICHT portiert werden sollte

1. **`depack.s` (PowerPacker decrunch)** — UFT braucht keinen PP-Unpacker. Wenn
   doch: moderne Implementierungen existieren in C (amiga-propacker, etc.).

2. **Alle `$DFF0xx`- und `$BFDxx`-Register-Zugriffe** — Amiga-Custom-Chip + CIA.
   Für UFT irrelevant; UFI hat eigene HAL.

3. **Copperlist-GUI und Blitter-Operationen** (`xio.s`) — komplett Amiga-
   Display-Chip. UFT hat Qt6.

4. **Novirus-Bootblock-Code** — historisch interessant, funktional obsolet.

5. **`$4489`-spezifische Handcoded-Vergleiche** — UFT macht das schon portabler
   mit `sync_mask` in `BitstreamDecoder.cpp`.

6. **`FindTask`/`FindName`/AmigaDOS-Calls** — Ex-Exec-Bibliotheks-Zeug, UFT
   braucht das nicht.

---

## Empfohlene Reihenfolge

1. **#6 syncpos-Validierung** — einfach bestätigen, dass Fix #5 aus dem UFT-Review
   der richtige Weg ist. Kein Code, nur Architektur-Entscheidung.
2. **#1 Single-Pass Amiga-Decode** — klarster Einzel-Speedup, ~40 LOC. ~2× für
   Amiga-Standard.
3. **#5 MFM-Grenzkorrektur Review** — Korrektheits-Check auf dem Write-Path. Wenn
   der schon richtig ist, skip; wenn nicht, bugfix.
4. **#3 GAP-Detection** — neues Feature, fits in `src/analysis/`.
5. **#2 Multi-Pattern-Sync** — erweiterte Protection-Detection.
6. **#4 Reverse-Track-Length** — Helper-Funktion für HAL.
7. **#7 NibbleRead** — UFI-Firmware-Thema, separater Kontext.

---

## Historische Bemerkung

X-Copy Pro war 1990 **das** Referenz-Tool für Amiga-Cracker und Disk-Preservation.
Die Tatsache, dass UFT 2026 immer noch auf Algorithmen von 1989 zurückgreifen kann,
ist kein Schwäche-Signal — es ist die Bestätigung, dass diese Probleme (Sync-
Suche, Track-Struktur-Erkennung, GAP-Analyse) **seit 35 Jahren dieselben** sind.
Was sich geändert hat: 68000 @ 7MHz vs. i7 @ 4GHz, 512KB RAM vs. 32GB, Assembly
vs. C++. Die Kernalgorithmen sind stabil.

Frank (der primäre Autor) hat in X-Copy viele Patterns verbaut, die später in
FluxEngine, Greaseweazle-Tools und HxC Floppy Emulator auftauchen sollten. UFT
erbt implizit von dieser Linie — und kann aus dem Original noch 2-3 echte
Verbesserungen extrahieren.
