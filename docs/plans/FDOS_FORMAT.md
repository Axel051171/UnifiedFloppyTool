# Umsetzungsplan FreeDOS-FORMAT — Baum-Fassung

> **Verdaut, nicht kopiert.** Nur offene Bausteine; Erledigtes als
> Verweis. Nach MF-641 Nachtrag 3; Vorlage war ein Chat-Artefakt
> vom 2026-08-22, gegen HEAD `e97888fc` neu vermessen.

**Quelle der Anregung:** [FDOS/format](https://github.com/FDOS/format)
(vermessen: HEAD `8071c20`). Lizenz: **GPL-2.0** — Zone GRÜN,
Code portierbar mit Attribution. Der Wert liegt aber fast ganz im
**Verhalten** (Verify-Disziplin, Interleave-Regeln), nicht in
DOS-Realmode-Code.

**Universalisierungs-Befund (aus dem Chat-Artefakt, bleibt gültig):**
Vier der fünf Mechanismen sind formatübergreifend, die *Daten* nie.
Die Drei-Ebenen-Trennung (Capture / Decode / Medienprofil) trägt das
bereits — kein neuer Unterbau nötig.

## Bereits geliefert oder anderswo — nicht erneut einplanen

| Baustein | Stand |
|---|---|
| Konverter-Selbstprüfung („trägt das Ergebnis ein FS?") | ✅ MF-489 — Teil der Verify-Idee, aber für *Konversion*, nicht Formatierung |
| Interleave-Politik (Gerüst) | teilweise in `uft_media_profile` — Werte-Ausbau siehe FD-1 |
| Provenienz/Loss-Report | ✅ `src/forensic/uft_fundus_provenance.c` — Konsument der Verify-Ergebnisse (FD-2) |

## Offene Bausteine

### FD-1 — Interleave-/Skew-Daten + Policy *(S)* — Kennzahl: Profildaten belegt
Gemessene FORMAT-Werte (`floppy.c:57–150`): Standard 1:1;
**DMF Interleave 2, Extra-Large (1.68/1.72M) Interleave 3, Skew 3**.
Gemessen in UFT: **kein DMF-Profil** mit diesen Parametern.
`uft_media_profile_t.interleave` bekommt drei Ausprägungen —
**Wert / Tabelle / NONE** (Amiga-Trackkette = NONE, gültig, kein
Fehler) — plus die Apple-Unterscheidung `physical_order` vs.
`logical_map`. FORMAT-Zeilen tragen
`MEASURED_FROM_REFERENCE(FDOS-format@8071c20)`; unverifizierte Zellen
(Mac-Zonen, C64, CP/M) bleiben `[ZU VERIFIZIEREN]` und **blockieren
Release-Builds** per CI-Tor, bis ein Oracle sie belegt.

### FD-2 — Verify-Kette beim Schreiben *(M)* — Kennzahl: Tier-Hebung stützt sich darauf
Gemessen: nur eine Checkbox (`formattab.cpp:1372
onVerifyAfterWriteChanged`), keine Kette dahinter. FORMATs Lehre:
**Verifikation ausschließlich durch Rücklesen + Dekodieren, nie durch
Controller-Status** (INT13-VERIFY log auf manchen BIOSen).
`src/write/uft_write_verify.c`: Rücklesen → Profil-Decoder → Vergleich
je Sektor → bei Fehler Degradierung auf Einzelsektor mit Retry-Budget
(Default 3, Session-Setting) → Bad-Map. Encoding-agnostisch, weil je
Format ein Decoder existiert. Retries zählen als
`VERIFIED_AFTER_RETRY(n)` im Loss-Report — ein Sektor, der erst beim
dritten Versuch steht, ist eine Medium-Vorwarnung. Konsumiert von
FormatTab **und** jedem Image-auf-Disk-Pfad. Tier-3-Bench prüft den
Rücklese-Grundsatz.

### FD-3 — FS-Defektmarkierung, dreistufig *(M)* — hängt an VFS-P1 + FD-2
Optionale VFS-Op `mark_bad_sectors`:
`NATIVE` (FAT: Cluster 0xFF7 — Konstanten liegen, `uft_fat12.h`),
`ALLOC` (Amiga-Bitmap, CBM-BAM, Atari-VTOC, ProDOS: als belegt buchen,
im Manifest dokumentiert), `NONE` (kein FS → Report *ist* die
Markierung). Schließt den Kreis zur Datei-Schadenskarte
(`FLOPPYCONTROL.md`): eine Sektorliste, zwei Richtungen.

### FD-4 — Format mit Reue *(S–M)* — Kennzahl: No-Data-Loss am Schreibpfad
Als **Write-Gate-Policy** (`src/policy/uft_write_gate.c` liegt), nicht
als Formatter-Feature: vor jeder destruktiven Operation auf echtem
Medium erzwingt das Tor einen Schnell-Snapshot in den Fundus
(`reason=preformat`). Stärker als FORMATs FAT-Mirror, weil es
konserviert *was da ist*, nicht was das Ziel erwartet — deckt „PC-Format
über unerkannte Amiga-Disk" ab. Restore-Weg im Dialog.

### FD-5 — Narrow-Track-Warnung + Erase-Odd *(S)* — Kennzahl: keine; Schreibsicherheit
Trigger rein aus Profil: `media.tpi < drive.tpi` ∧ Schreiben ⇒ Warnung
(schmalere Spuren, Lesbarkeit in Zeitgenossen eingeschränkt) + Angebot
Erase-Odd-Tracks. Betrifft jede 48-in-96-Situation (auch C64-1541,
Atari-810), nicht nur PC. Lebt in Capture/Write-Settings, Sichtbarkeit
über `SeeksCylinderVia` + Write-Capability.

## Bewusst nicht übernommen
DDPT/INT13-Mechanik, Kitten-NLS, SYS-Integration (DOS-gebunden) ·
FORMATs Realmode-Codepfade (nur Verhalten zählt).

## Reihenfolge-Empfehlung
FD-1 (billig, füttert Profiltabelle) → FD-2 (Fundament, encoding-frei)
→ FD-3 (nach VFS-P1) → FD-4/FD-5 nach Gelegenheit. FD-2 ist die
inhaltliche Stütze jeder späteren Schreib-Tier-Hebung.
