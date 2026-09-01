# Gutachten: Nextor (Konamiman/Nextor) — Lizenzlage und FORMAT-Bootsektor

**Scout-Zyklus:** 2026-09-01 · **Auftrag:** drei gezielte Fragen (Lizenz wörtlich,
Bootsektor-Inhalt von FORMAT, mitgelieferte Abbilder) · **Nur Dokument, kein Code.**

**Untersuchter Stand:** Klon `c49c43226bd6259f5da4bbf6dffb6a2314dfd795`
(origin/master, 2026-09-01, shallow), Arbeitskopie
`C:\Users\Axel\Github\xcopy\_scout\Nextor\` (außerhalb des UFT-Baums).
Zusätzlich vermessen: Release-Artefakt `nextor.dsk` aus
`tools-disk-image-and-zip-v1.1` (737 280 Bytes, SHA-256
`35a131e872d73b9db45324f980a1db6419ef714486ed90ee0089ce1bdf8be600`), Kopie
`C:\Users\Axel\Github\xcopy\_scout\nextor_tools.dsk`.

**Kategorie:** Daten/Oracle (Ground-Truth-Erzeuger) · **Kanal (MF-695):**
Oracle + Daten/Fixture + Spec, ausdrücklich **kein Port** ·
**Bewegte Kennzahl (MF-640):** ungeprüfte Formate (T3) **runter** —
`msx_disk` T3 → T1b (cross-tool), plus Härtung von `uft_fat12` (FS-T1,
bisher „geprüft gegen den eigenen Erzeuger") ·
**Einhängepunkt:** `docs/OPEN_ITEMS.md` **P3-13** (dort seit MF-779/780
geführt; dieses Gutachten beantwortet die dort benannte Vorfrage
„Bootsektor: ja (was)?").

**Inventar-Abfrage (zitiert, `inventar.py query`, 2026-09-01):**
`"msx": vorhanden: true, treffer: [dmf_msx, dsk_msx, msx, msx_disk, uft_msx]` ·
`"fat12": vorhanden: true, treffer: [uft_fat12]` ·
`"bootsektor"/"nextor": abgedeckt: false` (Fähigkeitsfrage — von Hand geprüft:
der Baum-Kontext ist in P3-13/MF-780 bereits gemessen, hier nicht neu erhoben).
Kein Eintrag zu Nextor/MSX in `data/known_negatives.json` (gefiltert, 0 Treffer).

---

## Frage 1 — Lizenzlage (Bericht, kein Urteil; MF-679)

**Es gibt genau eine Lizenzdatei:** `LICENSE.md` an der Repo-Wurzel, 15 Zeilen.
Gemessen: `find . -iname '*license*' -o -iname '*copying*'` liefert nur
`./LICENSE.md`. **Keine abweichenden Lizenzen** für `docs/`, `sdk/` oder
`source/`-Unterverzeichnisse (Vendoring-Prüfung: 0 weitere Dateien;
`DOS2-PIS.TXT`/`DOS2-FCS.TXT` tragen keinerlei Copyright-Vermerk — grep 0
Treffer).

### Wortlaut (LICENSE.md, vollständig die operativen Teile)

> MSX-DOS is (c) 2018 The MSX Licensing Corporation
> Nextor is (c) 2018 Nestor Soriano Vilchez
>
> Nextor is a fork of MSX-DOS and as such it makes extensive use of the
> MSX-DOS source code. The MSX Licensing Corporation authorizes this usage
> under the following terms:
>
> Permission is hereby granted, free of charge, to any person obtaining a
> copy of this software and associated documentation files (the "Software"),
> to deal in the Software without restriction, including without limitation
> the rights to use, copy, modify, merge, publish and/or distribute the
> Software, and to permit persons to whom the Software is furnished to do
> so, subject to the following conditions:
>
> - The above copyright notice and this permission notice shall be included
>   in all copies or substantial portions of the Software.
>
> - Commercial usage of the Software is not allowed without explicit
>   permission from the copyright holders. "Commercial usage" means selling
>   copies of the Software, either in source code form or in binary form.
>
> - Producing and distributing hardware that includes the Software in ROM
>   (or in an equivalent built-in storage media) is allowed as long as no
>   fee is charged for the Software itself. […]
>
> - Derivative works are not allowed without explicit permission from the
>   copyright holders. "Derivative works" means independent projects that
>   are created as forks of the original source code for the Software.

(dazu der übliche AS-IS-Gewährleistungsausschluss, hier gekürzt.)

### Feststellungen ohne Urteil

* Der Rahmen ist MIT-ähnlich („use, copy, modify, merge, publish and/or
  distribute"), aber mit **zwei einschränkenden Klauseln**: kein Verkauf
  („Commercial usage"), keine Fork-Projekte („Derivative works") ohne
  Erlaubnis. Die Lizenz definiert „Software" ausdrücklich als „source code
  form or … binary form" — **Binärdateien fallen unter dieselben Bedingungen,
  eine gesonderte Binär- oder Doku-Lizenz existiert nicht.** „associated
  documentation files" stehen im Grant; ob das die `docs/`-Handbücher meint,
  ist Auslegung → UNGEKLÄRT-Liste.
* Historische Attributionen in Quelltexten, z. B.
  `source/commandcom/cli.mac:12`: „Based on COMMAND.COM 2.31, copyright
  (1986) IS Systems Ltd, and on the COMMAND2.COM 2.40/2.41 additions by
  Fokke Post, whose source code was released as freeware in 2003."
* **Matrix-Konsequenz (Bericht):** `playbook/lizenzmatrix.md` führt
  „NC/ND-Klauseln" in Zone **ROT** und zugleich die Meta-Regel „jede Lizenz,
  die nicht in der Tabelle steht → **PRÜFEN**". In **beiden** Lesarten gilt
  laut Matrix: Code portieren ❌ · Konzept-Nachbau nur aus Doku/Blackbox ✅ ·
  **Oracle-Binary ✅ falls legal beziehbar**. Der geplante Weg (Emulator
  laufen lassen, Doku lesen) bleibt also in jeder Zone offen — die
  Zonen-Entscheidung selbst liegt beim Eigentümer.

---

## Frage 2 — was FORMAT in den Bootsektor schreibt

**Kurzantwort: ja, ausführbarer Code — 154 Bytes Nextor-Bootcode, wörtlich
aus Nextors Quelltext, bei jedem regulären Format im DOS-2-Modus.**

### Beleg aus der Dokumentation

* User Manual (`docs/Nextor_3.0_User_Manual.md`, Z. 259): „Except when
  running in MSX-DOS 1 mode, an MSX-DOS 2 boot sector is generated on the
  disk after it is formatted."
* Programmers Reference (`docs/Nextor_3.0_Programmers_Reference.md`, §2.7
  `_FORMAT (67h)`, Z. 210): „When the disk is actually formatted (choice
  1..9) in MSX-DOS 2 mode, an MSX-DOS 2 boot sector will be generated once
  the physical format completes."
* DOS2-FCS.TXT (§3.83, Z. ~2379–2390): der Bootsektor enthält neben den
  Parametern eine Volume-Id **und ein „boot program"** — Wahl FEh
  aktualisiert nur die Parameter, „the volume id does not overwrite the
  boot program". (FEh/FFh formatieren nicht; sie schreiben einem
  vorhandenen Datenträger einen neuen Bootsektor.)

### Beleg aus der Quelle (Tatsachenfeststellung)

* `source/kernel/bank2/bootsect.mac` (82 Zeilen), Kopfkommentar wörtlich:
  „MSX-DOS 2.20 style boot sector code for FAT12 disks: the ‚VOL_ID' block
  followed by the boot code that loads MSXDOS.SYS […] This is what the
  built-in FORMAT command puts at offset 1Eh of the boot sector, right
  after the disk parameters […]; the rest of the sector, up to byte 511,
  is zeroed."
* Der Block wird in `source/kernel/bank2/val.mac` als `NEW_BOOT`
  eingebunden (Z. 530–539, `NEW_BOOT_SZ equ $-NEW_BOOT`) und von
  `SET_BOOT_EX` (Z. 417 ff.) beim Formatieren an Offset 1Eh des
  Sektorpuffers kopiert; der Rest des Sektors wird genullt (Z. 213–221).
* **Gemessen am Release-Artefakt** `nextor.dsk` (das denselben Block
  standalone assembliert trägt, siehe Frage 3): belegte Bytes im
  Bootsektor reichen von Offset **0x1E bis 0xB7 = 154 Bytes**, Rest bis
  0x1FF null. Enthaltene Strings: `VOL_ID`, `Boot error`,
  `Press any key for retry`, `MSXDOS  SYS`. Media-Descriptor an Offset
  0x15: **0xF9** (720K DS). OEM-Name `NEXTOR3 `.

### Herkunft und Konsequenz

Der Code stammt **aus Nextors Quelltext** (`bootsect.mac`, Teil des
Kernels unter `LICENSE.md`) — es ist **kein** generischer „not a bootable
disk"-Stub, sondern ein Lader für `MSXDOS.SYS` mit Rückfall auf Disk
BASIC. **Ein von Nextors FORMAT (Wahl 1..9, DOS-2-Modus) erzeugtes Abbild
enthält damit 154 Bytes wörtlichen Nextor-Objektcodes** und ist in diesem
Bereich eine Kopie von „Software" im Sinn der Lizenz. UFTs bestehende
Politik (Abbilder nur unter `tests/corpus/`, gitignored; ins Repo nur das
Manifest) fängt das ab — der Befund gehört ins Manifest-Feld
„fremder Code: **ja** (154 B Nextor-Bootsektor, Offset 0x1E–0xB7,
Bedingungen aus LICENSE.md)".

### Gibt es einen Weg ohne Bootcode?

* **Wahl FDh** (PR §2.7, Z. 204): schreibt einen „standard boot sector" —
  nur erweiterter BPB-Block (0x29-Signatur, Volume-Id, Name,
  `FAT12`/`FAT16`-Mark), **kein Code**; in der Quelle: `NEW_BOOT_STD`
  (26 Datenbytes, `val.mac` Z. ~522–528) plus Nullung des alten
  2-Byte-Sprungs an Offset 0x1E. **Aber:** FDh formatiert nicht (setzt
  gültiges FAT12/16 voraus), und `val.mac` (Kommentar bei `do_boot_sec`):
  „we do NOT clear the rest of the boot sector" — nach einem vorherigen
  DOS-2-Format blieben Code-Reste (0x3E–0xB7) stehen. **Kein sauberer
  „null Nextor-Bytes"-Weg.**
* **Wahl FCh** (Z. 206): wie FDh, aber bei FAT12 wird doch der
  MSX-DOS-2-Bootsektor (mit Code) geschrieben.
* **DOS-1-Modus** (PR Z. 218): nach dem Format bleibt der vom **Treiber**
  geschriebene MSX-DOS-1-Bootsektor — der stammt dann aus dem DiskROM der
  emulierten Maschine, also von einer **anderen Hand mit anderem
  Rechteinhaber** (Maschinen-ROM). Rechtelage dort: UNGEKLÄRT, nicht
  untersucht.
* **Fazit:** eine von Nextor frisch formatierte Diskette trägt praktisch
  immer die 154 Nextor-Bytes. Ob eine Korpus-Fixture sie behalten darf
  (Manifest-Vermerk) oder ob eine dokumentierte Nachbehandlung (Bytes
  0x1E–0xB7 nullen, als Transformation im Manifest ausgewiesen) gewünscht
  ist, ist eine Stufe-4-/Eigentümer-Entscheidung — für die FAT12-Wahrheit
  (FAT, Verzeichnis, Cluster-Kette) sind diese Bytes ohne Belang.

---

## Frage 3 — mitgelieferte Diskettenabbilder

* **Im Repo-Baum: keine.** `find . -iname '*.dsk' -o -iname '*.img'` → 0
  Treffer.
* **Im Release `tools-disk-image-and-zip-v1.1`: ja** — Asset `nextor.dsk`
  (720K, DS, FAT12; oben vermessen). Inhalt laut
  `source/tools/Makefile` Z. 125–133: `NEXTOR.SYS`, `NEXTORJ.SYS`, alle
  gebauten `.COM`-Tools inkl. `COMMAND3.COM`, `README.TXT`,
  `HELP`-Verzeichnis. **Keine eigene Lizenz** — es besteht aus
  Nextor-Binärdateien und Bootsektor-Code, fällt also unter dieselbe
  `LICENSE.md` (die „binary form" ausdrücklich einschließt).
* **Entscheidende Nuance für den Ground-Truth-Wert:** das Abbild wird
  **nicht von Nextors FORMAT erzeugt**. Das Makefile-Rezept
  (Z. ~219–244) baut es mit **mtools** (`mformat -C … -t 80 -h 2 -n 9`),
  patcht dann per `dd` den OEM-Namen, nullt Offset 30–511 und kopiert den
  standalone assemblierten Bootcode (`tools-disk-bootsect.mac` →
  `bootsect.mac`) an Offset 30 ein. **FAT und Verzeichnis stammen also
  von mtools, nicht von MSX-DOS/Nextor.** Als „fremd erzeugtes,
  Nextor-formatiertes FAT12" taugt es damit **nicht**; als authentische
  Referenz für die 154 Bootcode-Bytes und als **bootfähige
  Werkzeug-Diskette für die blueMSX-Sitzung** (sie bringt COMMAND3 mit
  FORMAT-Befehl gleich mit) ist es dagegen der kürzeste Weg.
* Ein **leeres**, von Nextor selbst formatiertes Abbild liegt weder im
  Repo noch in den geprüften Release-Assets (letzte 3 Releases per
  GitHub-API abgefragt: nur Tools, Tools-Disk, RAM-Treiber-Beispiel).
  Es muss in der Emulator-Sitzung selbst erzeugt werden — was ohnehin der
  Plan (P3-13) ist.

---

## Beschaffungsliste

Gegen `inv["korpus"]` (24 Abbilder) geprüft — nichts davon liegt schon:

1. **blueMSX** (Emulator; laut P3-13 bereits im Oracle-Register seit
   MF-780 vorgesehen — Registereintrag prüfen, nicht neu anfordern).
2. **Nextor-Kernel-ROM** aus den GitHub-Releases (README Z. 9 verweist auf
   die Releases für Binaries; welche Mapper-Variante blueMSX braucht:
   UNGEKLÄRT, Beschaffungsdetail).
3. **`nextor.dsk`** aus `tools-disk-image-and-zip-v1.1` (SHA-256 oben) —
   als Boot-/Werkzeugmedium der Sitzung, nicht als FAT12-Ground-Truth.

Alle drei bleiben außerhalb des Repos (Korpus-Politik); ins Repo gehen
nur Manifeste mit Herkunft und SHA-256.

## OPEN_ITEMS-Vorschläge (2 von max. 5 — als Ergänzung zu P3-13, kein neuer Posten)

1. **[wichtigster — beantwortet die scharfe Vorfrage aus P3-13]**
   Befund festschreiben: jedes von Nextors FORMAT (DOS-2-Modus) erzeugte
   Abbild trägt **154 Bytes Nextor-Bootcode (Offset 0x1E–0xB7)**, wörtlich
   aus `source/kernel/bank2/bootsect.mac`; das Manifest-Feld lautet
   „fremder Code: **ja** (154 B, LICENSE.md-Bedingungen: Notice-Pflicht,
   kein Verkauf)". Kein Commit solcher Abbilder; die Frage
   Behalten-mit-Vermerk vs. dokumentierte Nullung ist Eigentümer-Sache.
   Kennzahl: T3 runter (`msx_disk` → T1b) + FS-T1-Härtung `uft_fat12`.
2. **Lizenz-Vorlage an den Eigentümer (MF-679):** Nextors Lizenz ist eine
   Sonderlizenz (MIT-Rahmen + NC- und No-Fork-Klausel), steht nicht in der
   Matrix → PRÜFEN/ROT. Zu entscheiden ist **nur** die Einordnung für den
   Fall, dass Abbild-Bytes oder Zitate ins Repo sollen; der Oracle-Weg
   (Emulator + Doku) ist nach Matrix in jeder Zone offen und braucht keine
   Freigabe.

## UNGEKLÄRT

* Rechtelage des **DOS-1-Modus-Bootsektors** (stammt vom DiskROM der
  emulierten Maschine, nicht von Nextor) — nicht untersucht.
* Ob `LICENSE.md` („associated documentation files") die Handbücher in
  `docs/` erfasst — Auslegungsfrage, Eigentümer-Vorlage.
* Ob die MSX Licensing Corporation außerhalb dieser Datei weitere
  Bedingungen für die MSX-DOS-2-Originalteile beansprucht — aus dem Repo
  nicht feststellbar.
* Die 154 Bytes sind am Release-Artefakt v1.1 gemessen; `NEW_BOOT_SZ` am
  HEAD wurde **nicht** selbst assembliert (kein Z80-Assembler im
  Werkzeugkasten). Restunsicherheit: Artefakt-Stand ≠ HEAD-Stand, das
  Listing in `bootsect.mac` ist am HEAD aber deckungsgleich mit den
  gemessenen Strings/Offsets.
* Passende Nextor-Kernel-ROM-Variante (Mapper) für blueMSX.
