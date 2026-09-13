# Commodore-Formatbeschreibungen — Herkunft

> **Erzeugt wurde diese Datei einmal aus einer Messung; gehalten wird sie
> vom Tor.** `scripts/audit_repo_hygiene.py` (H3) prueft bei jedem Lauf,
> dass jede Datei in diesem Verzeichnis hier gelistet ist **und** ihre
> SHA-256 stimmt. Eine Datei hinzufuegen, ohne diese Tafel zu ergaenzen,
> laesst das Tor feuern.

> **Die SHA-256 gilt fuer die Datei, wie sie im Repository liegt** — mit
> **LF** als Zeilenende. `.gitattributes` fuehrt `docs/format_specs/**`
> seit MF-1094 als `-text`, damit kein Auschecken sie umwandelt; ohne
> diese Zeile lieferte ein Windows-Klon mit `core.autocrlf=true` andere
> Bytes und damit andere Hashes. Nachrechnen:
> `git show HEAD:docs/format_specs/commodore/D64.TXT | sha256sum`

## Was hier liegt

**47 Textdateien, 554 487 Byte**, Beschreibungen der Dateiformate, die auf
Commodore-Rechnern und ihren Emulatoren vorkommen — Diskettenabbilder
(D64, D71, D81, D80/D82, G64), Archive (ARC, LNX, LHA, ZIP), Bandformate
(T64, TAP), Einzeldateien (PRG, P00, REL, GEOS) und Emulator-Container.

Zusammengestellt und herausgegeben von **Peter Schepers**. `INTRO.TXT`
nennt als Beginn den **24. August 1996** und als letzte Aenderung den
**10. Dezember 2009**; die Einzeldateien tragen eigene Revisionsnummern
und Staende, die in der Tafel unten stehen.

## Wie sie in den Baum kamen — und was dabei fehlt

**Gemessen (MF-1091):** alle 47 Dateien kamen im **selben** Commit in den
Baum wie die entfernte ELF-Datei `tests/test_smoke`:

    4d622192   2026-02-08   "v4.1.0 Release"

**Es gibt keinen Abrufvermerk und keine Bezugsadresse.** Die einzige
URL-artige Nennung in der Sammlung betrifft nicht sie selbst:
`INTRO.TXT` Zeile 37 verweist fuer das Werkzeug *CBMConvert* auf
`FTP.FUNET.FI`. Woher genau diese Fassung stammt, ist damit **nicht
belegt** — bekannt ist nur, wer sie zusammengestellt hat.

## Der offene Punkt: die Weitergabe ist ungeklaert

**Gemessen ueber alle 47 Dateien:** keine einzige sagt etwas ueber ihre
eigene Weitergabe. Die vier Treffer auf „copyright" betreffen
ausnahmslos Felder **in den beschriebenen Formaten** — ein Kopffeld in
`SIDPLAY.TXT`, die Urhebermeldung eines SFX-Archivs in `SFX.TXT`, eine
CBM-USR-Datei in `WRA-WR3.TXT` und ein Feldname in `D81.TXT`. Ueber die
Dokumente selbst: nichts.

Die Entscheidung des Eigentuemers lautet **behalten mit Herkunftsdatei**
(Klasse C2 der Aufraeum-Anweisung), und die ungeklaerte
Weitergabeerlaubnis bleibt als **benannter offener Punkt** stehen —
siehe `docs/OPEN_ITEMS.md`, **P3-372**. Wer die Sammlung
weiterverbreitet, klaert sie vorher.

## Wofuer sie im Baum gebraucht werden

Sie sind die **Spec-Quelle** (Kanal *Spec* nach MF-695 — gelesen, nicht
uebernommen) fuer sechs Eintraege der Stufentafel:

| Plugin | Datei | Stufe heute |
|---|---|---|
| `d64` | `D64.TXT` | T1b |
| `d71` | `D71.TXT` | T1b |
| `d80` | `D80-D82.TXT` | T1b |
| `d82` | `D80-D82.TXT` | T1b |
| `d81` | `D81.TXT` | T1b |
| `g64` | `G64.TXT` | T1 |

**`g71` steht ausdruecklich NICHT in dieser Liste, und das ist gemessen:**
die Zeichenfolge `G71` kommt in **keiner** der 47 Dateien vor. UFTs
`src/formats/g71/uft_g71.c` nennt selbst „VICE emulator, nibtools" als
Referenz. Wer hier eine sechste Zeile ergaenzen will, braucht erst eine
Quelle.

## Die Dateien

| Datei | Byte | Rev. | Stand | SHA-256 | Titel |
|---|---|---|---|---|---|
| `64LAN.TXT` | 2808 | 1.3 | March 11, 2004 | `dc64afb7b648f4c00e01857f2b4a4fd52a57c9ca6b4ac3802f3aa3ae7470f0e7` | L64 (64LAN container files) |
| `64NET.TXT` | 4152 | 1.3 | March 11, 2004 | `36b2f6219b172fe74b54e7781ca422fb5b76f47dc4a78e7aaf0f10e13d14333b` | N64 (64NET container files) |
| `ARC.TXT` | 2896 | 1.4 | March 11, 2004 | `79327dbfca48bb553d1e5db4bbb3d8050757258cead7e7edcf5131264eb8dd6e` | ARC (compressed ARChive) |
| `ARK-SRK.TXT` | 7207 | 1.3 | March 11, 2004 | `8adddf974f8ff05913308f888008b32478795a25b567caa6ccfc91a5e44a7fe6` | ARK (ARKive containers) |
| `BINARY.TXT` | 5342 | 1.3 | March 11, 2004 | `ef150d7f2abe1cd6d48b963ace7b9d6cfdccd79c7d60dd6d9fab70073379b7c5` | Binary/Raw files |
| `BITMAP.TXT` | 1852 | 1.3 | March 11, 2004 | `d6c8e70b37e4da667353419571d370076019e7af244408143a8df248bf3833ab` | Standard C64 BITMAP Files (HIRES & HIRES-MULTICOLOR) |
| `C128BOOT.TXT` | 2358 | 1.1 | March 11, 2004 | `e0719d8781b8d713cca1de2d126d44b9fdaeb3251c08dda57d4ee184f5a22eba` | C128 Auto-boot sector layout |
| `C64S_FRZ.TXT` | 2554 | 1.3 | March 11, 2004 | `43c4ea3cffbfe8b85e9aa9ce69ef6627d22f199214aaffe233c3e9eb64616808` | FRZ (C64s saved-session FRoZen files) |
| `CKIT.TXT` | 2147 | 1.3 | March 11, 2004 | `7a117a3251d97ca967ef52dae3ceeba4aca632f15f772dfc027c8f812884af6a` | CKIT (Compression KIT compressed files) |
| `CPK.TXT` | 5166 | 1.3 | March 11, 2004 | `d1c27ae40410efcaff35fd4220196ba66e7ae2f52a60d7bbf5c561ea5e1fade1` | CPK |
| `CRT.TXT` | 52728 | 1.15 | Dec 10, 2009 | `5d9c9397bed15205c33dc9c4b8b88ed9c9fc0a264f34f3f83902f278915116a1` | CRT - CaRTridge Images (from the CCS64 emulator) |
| `CVT.TXT` | 7990 | 1.3 | Oct 1, 2007 | `4e60097a2e22b538bfb657a1ef88e3f9a6d635afea27ba75c34ae23091e8f49a` | CVT (ConVerT containers) |
| `D2M-DNP.TXT` | 37799 | 1.3 | Nov 27, 2005 | `10a9acbe542d13f4c5b2dffd045dac1e2dd8c513556b1341232603f7b4e75df6` | D2M (Electronic form of a CMD FD2000 1.56 Mb floppy disk) |
| `D64.TXT` | 33540 | 1.11 | Nov 7, 2008 | `6361809ac15412307b146c544ee007f2189e529239ffc9e8b1776c45279effc3` | D64 (Electronic form of a physical 1541 disk) |
| `D71.TXT` | 21014 | 1.5 | Nov 7, 2008 | `ec2744991df7443299f53764e9b80ca69266ece25eb400d4db1107250c3f1b4e` | D71 (Electronic form of a double-sided 1571 disk) |
| `D80-D82.TXT` | 25082 | 1.3 | Nov 7, 2008 | `2254503902d98e68ebff4385065b9de8f7611ebc73ccad160ab231e7a7f270ed` | D80 (Disk image of an 8050 diskette, single-sided) |
| `D81.TXT` | 29134 | 1.4 | Nov 7, 2008 | `63b7f5ee44411cab54a7b9fb79027190c5317c45395f72b59523d4cd53b0a181` | D81 (Electronic form of a physical 1581 disk) |
| `DISK.TXT` | 30781 | 1.3 | March 11, 2004 | `09eafaafbd1f40d0a5d940dd130482494a870e6690a5e79b0671c587a06039a5` | Disk File Layout (D64, D71, D81) |
| `F64.TXT` | 9911 | 1.6 | March 19, 2004 | `1401e79d9e48baa652eb7301d398708876e4e38a437ef99259711eeec6c9ec0c` | F64 (a companion file to certain D64's) |
| `G64.TXT` | 26621 | 1.9 | Feb 19, 2008 | `acc85991c41ac482edf64ad8f9ae118039381009201cd6c64b3a210e5e4edfc2` | G64 (raw GCR binary representation of a 1541 diskette) |
| `GEOS.TXT` | 15350 | 1.4 | Nov 27, 2005 | `0a319c206054f89e5d49b03aafceb45ae93c28e46bd883879229723011150832` | GEOS VLIR (Variable Length Index Record) |
| `INTRO.TXT` | 13338 | — | December 10, 2009 | `b885c3337ff32635ea5733916ffc2adc59902fcd2ae7d74b36c95ceddcd4616d` | Introduction to the various Emulator File Formats |
| `LBR.TXT` | 6403 | 1.3 | March 11, 2004 | `09a5c367d8ec9e237cfa1e25f1c44c10499a0ac7fd0c36f0f8d0eef36822c081` | LBR (LiBRary containers, C64 version only) |
| `LHA.TXT` | 6620 | 1.3 | March 11, 2004 | `00ca26aa1ba62ea3969481d8311a40c6e572b2923a96bf77d31a652115e1f142` | LHA, LZH, LZS (LHArc compressed files) |
| `LNX.TXT` | 11181 | 1.3 | March 11, 2004 | `326252900cbc1116123d3ebc8cfbc3cd07e5a29693adf09409f525d128d9245d` | LNX (LyNX containers) |
| `PC64.TXT` | 9834 | 1.4 | March 11, 2004 | `b162dbdb02aedec01f67ed55e36ad15033afc7ba5220580c65d66e43b7cdf4ef` | P00/S00/U00/R00 (Container files for the PC64 emulator) |
| `PC64_FRZ.TXT` | 2044 | 1.4 | March 11, 2004 | `5465dffee768f54b1bcfb2d1ae292ae52bd3d97901e8012a5d665bddfcaa22e2` | C64 (PC64 saved-session file) |
| `PC64_ROM.TXT` | 944 | 1.1 | March 11, 2004 | `2e63a9cc8d8b6e8375672312b2e4cf4e328f0f5a965896b693b384ae2958b6fc` | 64x (PC64/DOS ROM files) |
| `PCLINK.TXT` | 1505 | 1.3 | March 11, 2004 | `1cd66346c97b131e3116ecaeca3081d8664aa84816d18bf3fe6ec8ccee744582` | C64 (PCLINK container files) |
| `PCVIC.TXT` | 4698 | 1.1 | March 11, 2004 | `b9d1c6aa35af4908a628bb8db4f2cdd211235902141554dee9d55d0fcf85f97f` | PCV (PCVIC VIC-20 emulator saved-session files) |
| `PHAUZEH.TXT` | 6826 | 1.2 | March 11, 2004 | `724358922116b4b266bf09cf1d9c0633c749b4000885e638220ac43d5ccf07ad` | S20 (Phau Zeh VIC-20 emulator saved-session files) |
| `POWER64.TXT` | 2942 | 1.2 | March 11, 2004 | `1968acff0d9916d6ecf6b3be65703d87b7fc9e8ce72530a768d39fba4021af50` | Power 64 RAM Snapshot File (C64 emulator on Apple Power Macintosh) |
| `REL.TXT` | 5814 | 1.1 | March 11, 2004 | `3249bdf8cd1aa1bb97475fe084cef4b10c898323c589592746995e792a2c4068` | REL (RELative file layout) |
| `SDA.TXT` | 2672 | 1.4 | March 11, 2004 | `f078a2482621886b835d110064728682515fd8f8b0d75783c2391e1492875203` | SDA (Self-Dissolving compressed Archive) |
| `SFX.TXT` | 3390 | 1.3 | March 11, 2004 | `39fc3326f4a1624f2378b4f1ac9e4685d4baa725e6ae3f5f399e247612836f03` | SFX (SelF-eXtracting LHA/LZH compressed files) |
| `SIDPLAY.TXT` | 21817 | 1.2 | March 15, 2004 | `0172ba16207c855b2016d5a469f7db2394960da257b93308935eb1ce613c890f` | SID/PSID (Various SIDPlay / PlaySID Formats) |
| `SPYNE.TXT` | 7340 | 1.3 | March 11, 2004 | `3ddabc3b2fca9be29a8225cc1b867b6408830fafef9b22bc9a792708332b74b2` | SPY (SPYne containers) |
| `T64.TXT` | 10074 | 1.5 | March 11, 2004 | `800ae628de8e18cabc8847a09f08b769df74f67793da792b0de92f927225da34` | T64 (Tape containers for C64s) |
| `TAP.TXT` | 3629 | 1.1 | March 11, 2004 | `d197c1915da2feb5e2c163f8246993efe945412955f944b7609322cda0143e4f` | TAP (raw C64 cassette TAPE images) |
| `VICE_FRZ.TXT` | 44955 | 1.2 | Oct 1, 2007 | `7fa5ac7e533fa1217fcbd3c61093232ca9cfabb79fb733e21087294c834b123e` | VSF (Vice Snapshot File, saved-session file) |
| `WAV.TXT` | 7144 | 1.1 | March 11, 2004 | `82b159b8d300e8addf893e379d5aba224481c90aa24348bbb023ae63c3a1bd61` | WAV (RIFF audio files) Resource Interchange File Format |
| `WRA-WR3.TXT` | 8012 | 1.4 | March 11, 2004 | `facc941c124169bad6ba1486645ed00d45746f45b928350026d8bce5128ad00c` | WRA, WR3 (WRAptor compressed files, and version 3.0 files) |
| `X64.TXT` | 4396 | 1.3 | March 11, 2004 | `fa225862d3f2dc2317dbe57295fdcf82097e2be9c1455f17a460ead9df196084` | X64 (X64 and VICE emulator image files) |
| `ZIP.TXT` | 5043 | 1.3 | March 11, 2004 | `59bd46d11283b3b9d12aca20eb90b743a6f483ef123870a4b7455f5215b8c225` | ZIP (PKZip compressed files) |
| `ZIP_DISK.TXT` | 8747 | 1.2 | March 11, 2004 | `e264fffd0afd7ee750c9cb4645949bcb1845064cb9eb86fd94882e7e9a7dd02a` | DiskPacked ZipCode (4 or 5 file version, 1!xxxxxx, 2!xxxxxx, etc) |
| `ZIP_FILE.TXT` | 9848 | 1.4 | March 11, 2004 | `96e8d228351994ac4d268692da5e8d72dae8b92936b44d7a1bfd300efe4a6267` | FilePacked ZipCode (A!xxxxxx, B!xxxxxx, etc) |
| `ZIP_SIX.TXT` | 18839 | 1.4 | March 11, 2004 | `a787c741a567227cd3dc364e35fcf3c9a31e7c53b0f86e64dd44bd47e7394201` | SixPack Zipcode (6-file version, 1!!xxxxx, 2!!xxxxx, etc) |
