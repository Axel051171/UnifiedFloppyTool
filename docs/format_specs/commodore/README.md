# Commodore-Formatbeschreibungen — Herkunft

> **Erzeugt wurde diese Datei einmal aus einer Messung; gehalten wird sie
> vom Tor.** `scripts/audit_repo_hygiene.py` (H3) prueft bei jedem Lauf,
> dass jede Datei in diesem Verzeichnis hier gelistet ist **und** ihre
> SHA-256 stimmt. Eine Datei hinzufuegen, ohne diese Tafel zu ergaenzen,
> laesst das Tor feuern.

## Was hier liegt

**47 Textdateien, 566056 Byte**, Beschreibungen der Dateiformate, die auf
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
| `64LAN.TXT` | 2860 | 1.3 | March 11, 2004 | `07e9ced7e576316097bfb8f136b270df2a5794f242c1b0509e326e81dce45191` | L64 (64LAN container files) |
| `64NET.TXT` | 4234 | 1.3 | March 11, 2004 | `a0ecb4201efdfc92acba8f5b1b992d98498467b8f2c741b2e8470be7081cc45b` | N64 (64NET container files) |
| `ARC.TXT` | 2955 | 1.4 | March 11, 2004 | `4a70ee42310e8f009ce5c18421332b185cdc51adad55f79aa0716781acdafe9b` | ARC (compressed ARChive) |
| `ARK-SRK.TXT` | 7343 | 1.3 | March 11, 2004 | `0d8c102a2c4d831f4a7093b2f16ae3a2845c80d7517c4bdf90437728fe763050` | ARK (ARKive containers) |
| `BINARY.TXT` | 5453 | 1.3 | March 11, 2004 | `62611ad290cf1cb3f73e1168aaf298b1be64d931f2561a7981e0f1a212bdd4f4` | Binary/Raw files |
| `BITMAP.TXT` | 1892 | 1.3 | March 11, 2004 | `75f8d50ef6318f09b2cf79511344ae78000a66d0f98f59b1211e054ce7fed6a8` | Standard C64 BITMAP Files (HIRES & HIRES-MULTICOLOR) |
| `C128BOOT.TXT` | 2416 | 1.1 | March 11, 2004 | `2a12302201fd2b94787e4933013030bec3569d7399a722d87a66e12a41a2d071` | C128 Auto-boot sector layout |
| `C64S_FRZ.TXT` | 2609 | 1.3 | March 11, 2004 | `0ace7bc18c4b1f50a01a9d71f982957a3f9203169451d78142ca4672b4f41dd7` | FRZ (C64s saved-session FRoZen files) |
| `CKIT.TXT` | 2189 | 1.3 | March 11, 2004 | `0906acb413d4faf5b2ac7a3018a0a1f1917340d2be8b7bad76c10458406d4835` | CKIT (Compression KIT compressed files) |
| `CPK.TXT` | 5268 | 1.3 | March 11, 2004 | `815837a8a94954bdda277d50bce2be88dfbf141e30b1f8df35b89ecd20be06ad` | CPK |
| `CRT.TXT` | 54018 | 1.15 | Dec 10, 2009 | `add5588e142a493b45321e59c6dd3cfd810f8c84346c54a4813f54dc334e6e03` | CRT - CaRTridge Images (from the CCS64 emulator) |
| `CVT.TXT` | 8127 | 1.3 | Oct 1, 2007 | `da4678b48faf4018d470ecc733390a20dfdeb97d1fe2d9fc3a8f1226634f9fb1` | CVT (ConVerT containers) |
| `D2M-DNP.TXT` | 38488 | 1.3 | Nov 27, 2005 | `12228f97d242cf1e5224cf00a39e6e1821022be5c4a2dedea8dd4d894802e5e9` | D2M (Electronic form of a CMD FD2000 1.56 Mb floppy disk) |
| `D64.TXT` | 34230 | 1.11 | Nov 7, 2008 | `f15cec6f4c7f3761d824c634edcd84770d667ab4a27896e0586d76f7d8ce7ef9` | D64 (Electronic form of a physical 1541 disk) |
| `D71.TXT` | 21434 | 1.5 | Nov 7, 2008 | `c6430abfe85914345d8f74934b85e9f5dae7273fc63cbb55966920e1e91b29b6` | D71 (Electronic form of a double-sided 1571 disk) |
| `D80-D82.TXT` | 25546 | 1.3 | Nov 7, 2008 | `997e49e1154855f7cba97e1ca1839fc04cfb09832d3033a1f99729ae08f79031` | D80 (Disk image of an 8050 diskette, single-sided) |
| `D81.TXT` | 29709 | 1.4 | Nov 7, 2008 | `73645253f0a3fa2c35de3d81a8dec6bb7ab541d4e818b4871942ce55ed5b3736` | D81 (Electronic form of a physical 1581 disk) |
| `DISK.TXT` | 31572 | 1.3 | March 11, 2004 | `c529d19a12a687067371d78bd918c8c86d74653710602e242dedacecbaab4c25` | Disk File Layout (D64, D71, D81) |
| `F64.TXT` | 10107 | 1.6 | March 19, 2004 | `f8b09f0391605cd7420a4e8f62a23d7e9b1b913b3ca0076ed560bb2760c24229` | F64 (a companion file to certain D64's) |
| `G64.TXT` | 27094 | 1.9 | Feb 19, 2008 | `6dfa432f1b0601e50912318fb9026445aab42d960a1c5579860b22ac16940036` | G64 (raw GCR binary representation of a 1541 diskette) |
| `GEOS.TXT` | 15654 | 1.4 | Nov 27, 2005 | `3f568f7dfe74b71a9987d2c802adfb8668e16b601b0a98afadce19d1dbdc5c4a` | GEOS VLIR (Variable Length Index Record) |
| `INTRO.TXT` | 13612 | — | December 10, 2009 | `59657f438f34a46e295728345034c15d7847dd8d9051fbf6d3a40ebc875706d7` | Introduction to the various Emulator File Formats |
| `LBR.TXT` | 6528 | 1.3 | March 11, 2004 | `2376637a7fa012031253c45ef224c2710f0663495981519bc7870cdf60efa058` | LBR (LiBRary containers, C64 version only) |
| `LHA.TXT` | 6739 | 1.3 | March 11, 2004 | `64f0c968ca6cb743153226b836a0cc4ae33f99fe2510b7642c4ffa09bd2838c2` | LHA, LZH, LZS (LHArc compressed files) |
| `LNX.TXT` | 11407 | 1.3 | March 11, 2004 | `7059b7539916793e440b3e927447c75049c9ee6dff74053996478c494732b892` | LNX (LyNX containers) |
| `PC64.TXT` | 10059 | 1.4 | March 11, 2004 | `d62985c3ba696ab6c1956b33f6e8341c08e254502a5fb9c0498faaab9cdbfae2` | P00/S00/U00/R00 (Container files for the PC64 emulator) |
| `PC64_FRZ.TXT` | 2090 | 1.4 | March 11, 2004 | `46fe6376017dc0bfc1a68dc7313b5bb40e0df6b1fdfbecc60b28a695c41b8a82` | C64 (PC64 saved-session file) |
| `PC64_ROM.TXT` | 966 | 1.1 | March 11, 2004 | `05373c4f861c631870f39569a31544c602e3c4601e64336bf11835491fd6fdd9` | 64x (PC64/DOS ROM files) |
| `PCLINK.TXT` | 1537 | 1.3 | March 11, 2004 | `7dbfb431872542e78b802c9f4c7070f3df00a1e9723462bda380da26435d6997` | C64 (PCLINK container files) |
| `PCVIC.TXT` | 4805 | 1.1 | March 11, 2004 | `dcbe6a334c8305fd03fe255a16c8e1f9dda6af894afb021d5b75d3a327bb2a07` | PCV (PCVIC VIC-20 emulator saved-session files) |
| `PHAUZEH.TXT` | 6964 | 1.2 | March 11, 2004 | `e9135844a2b4e58a4ec62a3b26f54fed90a5b8a826c64628023a2081def8621f` | S20 (Phau Zeh VIC-20 emulator saved-session files) |
| `POWER64.TXT` | 3004 | 1.2 | March 11, 2004 | `b7f50ddf602e3852b240b11b643ea61ebc1c44d5db5276825ca275aa98976fdc` | Power 64 RAM Snapshot File (C64 emulator on Apple Power Macintosh) |
| `REL.TXT` | 5924 | 1.1 | March 11, 2004 | `55dbf1a6f424eae0de2ba341066889fecbfd7693ec812b56dd75897076e98c08` | REL (RELative file layout) |
| `SDA.TXT` | 2732 | 1.4 | March 11, 2004 | `dd501066f89fed82a58909469046bae39a3677ec9cf22427da4f4f12ed9f0d5b` | SDA (Self-Dissolving compressed Archive) |
| `SFX.TXT` | 3457 | 1.3 | March 11, 2004 | `c7343926f4bf7096ff043da6a4561a11e3e0d2f5b898f27f13b649cc85298588` | SFX (SelF-eXtracting LHA/LZH compressed files) |
| `SIDPLAY.TXT` | 22327 | 1.2 | March 15, 2004 | `f4288ae5ab925c9d1b8c86714bc95a0dc5c9b51259e14df5e97eaab33cdc243b` | SID/PSID (Various SIDPlay / PlaySID Formats) |
| `SPYNE.TXT` | 7471 | 1.3 | March 11, 2004 | `701b7b47d1bef926243226dbd4b4c6e6c830ff0573dfe5541126f442b7f21afb` | SPY (SPYne containers) |
| `T64.TXT` | 10295 | 1.5 | March 11, 2004 | `2c4061abbe17b4b9c1be125dd3a4c6b2a750d229c5877e33f6b465155f6cee9c` | T64 (Tape containers for C64s) |
| `TAP.TXT` | 3704 | 1.1 | March 11, 2004 | `35acd8025a1fb6fbf22eccd4fdd53ae2152ae7fe3e779964844b23328e050440` | TAP (raw C64 cassette TAPE images) |
| `VICE_FRZ.TXT` | 45924 | 1.2 | Oct 1, 2007 | `d3eeeb52b5c59f36ce1552778ab1426b6d1ed7947dd93002b9b614c6fa5ad2a1` | VSF (Vice Snapshot File, saved-session file) |
| `WAV.TXT` | 7289 | 1.1 | March 11, 2004 | `67d8c7bb1ee502d7dde9c83df5ebf16a5c7c856b6923591a2fc7bf28d63bd751` | WAV (RIFF audio files) Resource Interchange File Format |
| `WRA-WR3.TXT` | 8238 | 1.4 | March 11, 2004 | `cb1b7ea0625c92ab5da1c6069e650dc679112d244be0f6b21881fb12936045b3` | WRA, WR3 (WRAptor compressed files, and version 3.0 files) |
| `X64.TXT` | 4490 | 1.3 | March 11, 2004 | `a069c5ad7cc6bbbd91e9abdf182b869061fd352d7aa6ed9d3bc448bc53df34dd` | X64 (X64 and VICE emulator image files) |
| `ZIP.TXT` | 5140 | 1.3 | March 11, 2004 | `ee283a0f5574bd4d7bd9203bc6565baecaba0ee70f4ce8a5d59bda85a22a6a52` | ZIP (PKZip compressed files) |
| `ZIP_DISK.TXT` | 8932 | 1.2 | March 11, 2004 | `dea4ed4b3d3dc7e6a2ee247229e18c3832f42082495243269f2790b582b70388` | DiskPacked ZipCode (4 or 5 file version, 1!xxxxxx, 2!xxxxxx, etc) |
| `ZIP_FILE.TXT` | 10013 | 1.4 | March 11, 2004 | `4a4d75d86d18f73f4f562f3bc7c991037b538ddcbf97da8e49eb943dd6916194` | FilePacked ZipCode (A!xxxxxx, B!xxxxxx, etc) |
| `ZIP_SIX.TXT` | 19211 | 1.4 | March 11, 2004 | `b9fa9320ce99843cc617744b8641e9b819bdfd991f4cbec7457e49e4b98b7e4d` | SixPack Zipcode (6-file version, 1!!xxxxx, 2!!xxxxx, etc) |
