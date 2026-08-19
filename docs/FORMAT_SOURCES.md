# Quellen für Format-Verifikation

**Zweck:** 68 der 88 Plugins stehen auf **T3** — kein Test gegen reale Daten,
keine Spec-Verifikation. Dieses Dokument sammelt, *woher* die Belege kommen
können, damit aus „wir unterstützen 138 Formate" ein „wir unterstützen sie
nachweislich" wird.

**Stand:** 2026-08-18 (MF-426). Tier-Zahlen jederzeit via
`python scripts/gen_verification_tiers.py`.

> ## Beweiskraft dieses Dokuments
>
> Jede Zeile hier ist ein **Hinweis**, kein Beleg. Die URLs sind recherchiert,
> nicht verifiziert: ich habe die Werkzeuge nicht heruntergeladen, nicht
> gebaut, und keine Datei damit erzeugt. Nach den Regeln in
> `.claude/skills/uft-disk-analyst/SKILL.md` gilt eine Web-Fundstelle als
> Hypothese, bis entweder das Herstellerdokument zitiert (→ T2 über
> `docs/spec_verification.json`) oder eine reale Datei damit erzeugt und von
> UFT gelesen wurde (→ T1b über `tests/corpus_manifest/manifest.json`).
>
> Wer hier weiterarbeitet, trägt den Beleg dort ein — nicht hier.

---

## Die zentrale Erkenntnis: Werkzeuge schlagen Specs

Die 68 T3-Formate sind keine 68 Einzelprobleme. Sie **clustern**, und pro
Cluster gibt es quelloffene Werkzeuge, die Abbilder *erzeugen* können. Ein
erzeugtes Abbild, das UFT zurückliest, ist **T1b** — und damit stärker als
jede gelesene Spezifikation, weil es außerhalb von UFTs eigener Annahme
entsteht.

Das Verfahren ist im Repo bereits erprobt: `vice_c1541_35trk.d64`,
`xdftool_dd_ofs.adf`, `atrcopy_dos2sd.atr`, `gw_amigados.hfe` sind genau so
entstanden. Es fehlt nicht die Methode, es fehlt die Anwendung auf die
restlichen Cluster.

| Cluster | Werkzeug | deckt ab (T3-Namen aus der Tier-Tabelle) |
|---|---|---|
| CP/M & Exoten | **libdsk** (`dskform`, `dskconv`) | `apridisk`, `cfi`, `myz80`, `nanowasp`, `qrst`, `rcpmfs`, `logical`, `jv3`, `imd`, `cpm`, `edsk` |
| Multi-Plattform | **MAME `floptool`** | `mfi`, `ipf`, `st`, `msa`, `d77`, `imd`, `td0`, `cqm`, `img`, `dsk` |
| Sinclair / Spectrum | **Fuse** + `fuse-utils` | `trd`, `scl`, `udi`, `fdi`, `mgt`, `sad`, `opus`, `td0` |
| Apple II | **Applesauce**, **CiderPress II** | `2img`, `do`, `po`, `nib`, `d13` |
| Atari 8-bit | **atrcopy** (bereits genutzt), **a8rawconv** | `atx`, `xfd`, `dcm`, `cas` |
| Commodore | **VICE `c1541`** (bereits genutzt) | `d67`, `d80`, `d82`, `g71` |
| Amiga | **xdftool** (bereits genutzt), **DMS** | `adf_arc`, `dms` |

**Reihenfolge nach Ertrag:** libdsk zuerst — ein Werkzeug, elf Formate, und
`dskform`/`dskconv` erzeugen rechtefreie Abbilder ohne Fremdinhalte. Danach
Fuse (acht) und floptool (zehn, teils überlappend).

---

## Werkzeuge

### libdsk — der größte Einzelhebel

- Handbuch: <https://www.mankier.com/1/dskconv>, <https://www.mankier.com/1/dskform>
- Quelle/Fork: <https://github.com/lipro-cpm4l/libdsk>

Treibernamen laut Handbuch: `remote, rcpmfs, floppy, dsk, edsk, apridisk,
copyqm, tele, ldbs, qrst, imd, ydsk, raw, rawoo, rawob, myz80, simh, nanowasp,
logical, jv3, cfi`. Das deckt sich **eins zu eins** mit einem Block unserer
T3-Namen — die Formate wurden offenkundig nach libdsk benannt.

Kurzbeschreibungen aus dem Handbuch, als Spec-Einstieg:

| Treiber | laut libdsk |
|---|---|
| `apridisk` | ApriDisk-Format des gleichnamigen DOS-Werkzeugs |
| `cfi` | Compressed Floppy Image, erzeugt von `FDCOPY.COM` unter DOS |
| `myz80` | MYZ80-Festplattenabbild — wie `raw`, aber mit 256-Byte-Kopf |
| `nanowasp` | 400k-Microbee-Format des NanoWasp-Emulators; wie `raw`, andere Spurreihenfolge |
| `qrst` | Compaq Quick Release Sector Transfer |
| `rcpmfs` | lässt ein Unix-Verzeichnis wie ein CP/M-Abbild aussehen |

### MAME floptool

- Doku: <https://docs.mamedev.org/tools/floptool.html>
- Quelle: <https://github.com/mamedev/mame/blob/master/src/tools/floptool.cpp>

Unterstützt u. a. MFI, DFI, IPF, MFM (HxC), ADF, ST, MSA, PASTI/STX, CPC-DSK,
D88 (auch `d77`/`1dd`), IMD, TD0, CQM, PC-Raw, NASLite.

### Fuse (ZX Spectrum)

- Handbuch: <https://manpages.ubuntu.com/manpages/jammy/man1/fuse.1.html>

Liest/schreibt DSK, UDI, FDI, TD0, MGT, IMG, D40, D80, SAD, TRD, SCL, OPD.

---

## Spezifikationen (für T2 über `docs/spec_verification.json`)

| Format | Quelle |
|---|---|
| WOZ 1.0 / 2.1 | <https://applesaucefdc.com/woz/reference1/> · <https://applesaucefdc.com/woz/reference2/> — gemeinfrei gestellt |
| TRD | <https://sinclair.wiki.zxnet.co.uk/wiki/TRD_format> |
| SCL | <https://sinclair.wiki.zxnet.co.uk/wiki/SCL_format> |
| UDI | <https://sinclair.wiki.zxnet.co.uk/wiki/UDI_format> |
| Spectrum-Formate allgemein | <https://sinclair.wiki.zxnet.co.uk/wiki/Main_Page> · <https://rk.nvg.ntnu.no/sinclair/faq/fileform.html> |
| ATR / XFD / DCM | <https://atarimuseum.ctrl-alt-rees.com/archives/atari-8-bit-faq/faq-doc-27.html> · <https://atariwiki.org/wiki/Wiki.jsp?page=Diskettenformate> |
| Disk-Image-Formate, Übersicht | <https://justsolve.archiveteam.org/wiki/Disk_Image_Formats> (war beim Abruf nicht erreichbar, nur Suchtreffer) |

**XFD ist laut Atari-FAQ identisch zu ATR ohne den 16-Byte-Kopf.** Das ist
prüfbar und würde `xfd` sofort an den bereits vorhandenen
`atrcopy_dos2sd.atr`-Korpuseintrag anschließen — vermutlich der billigste
T1b-Gewinn im ganzen Dokument.

---

## Konkreter nächster Schritt

1. `pip install atrcopy` ist bereits geschehen (Korpus-Eintrag existiert).
   **`xfd` daraus ableiten** — ATR ohne 16-Byte-Kopf, gegen das vorhandene
   ATR prüfen. Ein Eintrag, ein Test, ein Tier-Aufstieg.
2. **libdsk bauen**, mit `dskform` je einen leeren Datenträger in `apridisk`,
   `cfi`, `myz80`, `nanowasp`, `qrst`, `imd` erzeugen, Marker-Datei
   hineinschreiben, in `tests/corpus_free/` ablegen (rechtefrei, weil selbst
   erzeugter Inhalt), Manifest-Einträge mit `tool = "libdsk <version>"`.
3. Pro erzeugtem Abbild **einen ctest**, der es durch das UFT-Plugin öffnet
   und den Inhalt prüft — ohne konsumierenden Test zählt der Manifest-Eintrag
   nicht (`gen_verification_tiers.py`).
4. `python scripts/gen_verification_tiers.py --write`, dann
   `check_consistency.py`.

Die Einfrier-Regel (MF-363) steht dem nicht entgegen: das ist ausdrücklich
Verifikations- und Korpusarbeit, und jede Hebung von T3 auf T1b ist genau die
Währung, in der das Moratorium rechnet.
