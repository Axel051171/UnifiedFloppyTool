# Gutachten: sergev/mfmdisk

> Gemessen 2026-08-30 gegen HEAD `42f560badf` (2018-02-17 — seit acht
> Jahren ruhend).
> Messdatei: `tools/uft-scout/work/mfmdisk.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `bd2d5616`).
> Auftrag: Block 4 (MF-692), Andockstelle **Fluss-Dekodierung**.

## Kategorie

**Fundus** — mit einem klaren Lizenz-Formfall (Datei-Header ≠
Repo-Lizenz) und einer kleinen Verhaltens-Referenz für BK-0010.

## 1. Was es ist

Kleines C-Werkzeug (2 204 Zeilen `src/`, `wc -l`) für „MFM-Images" von
Floppy-Emulatoren (vak.ru-Megadrive-Projekt, README). Vier Aktionen
(`src/main.c:37-42`): info / extract / create / dump. Eingaben: eigenes
Halbbit-MFM-Format (`src/mfm.h:38-48`: `halfbit`-Leser, 80/160 Spuren,
9–11 Sektoren × 512 B) oder **SCP** (`src/scp.c`, 402 Z., mit
Revolutions-Auswahl `main.c:66-67`). Drei Decoder: IBM PC
(`src/ibmpc.c`), Amiga (`src/amiga.c`), **BK-0010**
(`main.c:64` `--bk`).

## 2. Abgleich gegen das Inventar

Abfrage zitiert:

* `"mfm": vorhanden: true, treffer: ["dsk_mfm", "mfm", "mfm_native",
  "uft_mfm"]` — MFM-Codec und MFM-Formate liegen.
* `"scp": vorhanden: true, tier: "T1b"` — SCP-Leser belegt.
* `"bk0010": vorhanden: true, treffer: ["uft_bk0010"], tier: null` —
  `src/formats/soviet/uft_bk0010.c` existiert, ist aber **nicht
  tier-geführt** (kein Eintrag in den 88 Tier-Zeilen).

Damit ist nichts an mfmdisk eine Lücke: SCP-Lesen, MFM-Decode,
IBM/Amiga/BK-Formate sind vorhanden. Ob mfmdisks Halbbit-„.mfm" mit
unserem `mfm_native`/`dsk_mfm` identisch ist, wurde **nicht** geprüft
(vermutlich nein — Halbbit-Rohspur vs. HxC-artige Container sind
verschiedene Dinge); ein neues Plugin dafür fiele ohnehin unter das
Moratorium. UNGEKLÄRT, ohne Handlungsdruck.

## 3. Der eine brauchbare Rest: BK-0010-MFM als Zweithand

`uft_bk0010.c` ist nicht tier-geführt und damit in der Klasse
„ungeprüft ohne Tier-Zeile". mfmdisk ist eine der wenigen offenen
Quellen, die BK-0010-Spuren **aus MFM-Bitströmen** dekodieren — als
Verhaltens-Referenz (welche Sync-Marken, welche Prüfsumme, welche
Sektorordnung) für eine spätere Hebung brauchbar. Das ist
Verifikationsarbeit, von der Einfrier-Regel erlaubt; der regelkonforme
Weg wäre: mfmdisk als benannte Referenz im Header, Rotbeweis zuerst.
Kein offener Plan-Baustein verlangt BK-0010 → Fundus.

## 4. Lizenz — der Formfall

* `COPYING` = **GPL-2.0** (Messung: `kennung: GPL-2.0, zone: GRUEN`).
* Die Datei-Köpfe sagen etwas anderes: `src/mfm.h:1-27` und
  `src/main.c:1-27` tragen wörtlich die **BSD-3-Clause**-Bedingungen
  („Redistribution and use … 3. The name of the author may not be
  used to endorse…").

Nach `playbook/lizenzmatrix.md` ist „Datei-Header ≠ Repo-Lizenz"
**Zone PRÜFEN — immer Eigentümer-Vorlage**. Anmerkung zur Vorlage:
beide Kennungen stehen einzeln in der GRÜN-Zeile; jede Auflösung des
Widerspruchs landet in GRÜN. Die Einordnung trifft trotzdem der
Eigentümer, nicht dieses Gutachten (Regel 8).

**Attribution:** mfmdisk, Copyright 2008-2018 Serge Vakulenko,
BSD-3-Clause-Header unter GPL-2.0-COPYING. Kein Code übernommen.

## 5. Bewegte Kennzahl

**Keine.** `bk0010` steht in keiner T3-Zeile (tier: null), also senkt
eine Hebung die T3-Zahl nicht; Wandlungspfade bewegt nichts ohne
Format-Arbeit unter Moratorium. **Fundus, nicht Auftrag.**

## 6. Einhängepunkt

Keiner nötig. Falls BK-0010 je tier-geführt wird:
`docs/VERIFICATION_TIERS.md` + `docs/VERIFICATION_PLAN.md`
§ Einfrier-Regel.

## 7. Oracle-Kandidat

Nein. Acht Jahre ruhend, autotools-Bau, Halbbit-Eigenformat als
Drehscheibe — als Oracle wäre jede Aussage erst durch zwei
Konvertierungen gelaufen.

## 8. Beschaffungsliste

Nichts. (Gegen `inv["korpus"]`, 24 Einträge: kein BK-0010-Abbild liegt
— das würde erst relevant, wenn jemand die Hebung will.)

## 9. Aufwandsklasse

**S** (Fundus-Notiz).

## UNGEKLÄRT

* Verhältnis mfmdisk-„.mfm" (Halbbit) zu UFT `mfm_native`/`dsk_mfm`.
* Ob `uft_bk0010.c` Sektor- oder Bitstrom-Ebene implementiert (nur
  Kopf gelesen, nicht der Pfad) — für die Fundus-Notiz unerheblich.
