# Vorfilter — Restliste der Auftraege (2026-08-29)

Ein Durchgang, eine Zeile je Repo. **Grobmessung** (GitHub-Metadaten:
Beschreibung, Sprache, Groesse, letzter Push; bei fuenf Repos zusaetzlich
README/Dateiliste angelesen). Kein Gutachten, keine Lizenzpruefung, keine
Oracle-Bewertung — das kommt erst nach dem Filter. Grundlage der
Tier-Urteile: `docs/VERIFICATION_TIERS.md` (dim, kfx, udi, trd, scl,
sap_thomson = T3; d64 = T1b; st, msa = T2), `scripts/scout_stand.py`
(34 OFFEN + 3 nur erwaehnt), `tools/uft-scout/data/auftraege.json`.

Regel dieses Laufs: wer KEINE der vier Release-Kennzahlen bewegt, geht in
**Fundus, geschlossen** — nicht in „spaeter".

## Bewegt eine Kennzahl (8)

| Repo | Kennzahl | Begruendung | Aufwand |
|---|---|---|---|
| ray77/MaClone | Wandlungspfade rauf | schreibt Amiga-Disketten via Greaseweazle, enthaelt also einen AmigaDOS-MFM-Encoder — genau das, was MF-539 dem Baum fuer ADF→HFE fehlt (Referenz, nicht Port) | M |
| joncampbell123/floppytools | T3 runter (kfx) | KryoFlux-Stream-Decoder in C++ plus `doc/kryoflux_stream_protocol_rev1.1.pdf` — Zweitquelle fuer das referenzlose kfx-Plugin | M |
| nils-eilers/sap2 | T3 runter (sap_thomson) | Thomson-SAP-Lese/Schreib-Werkzeug (C, Botcazou/Pukall-Linie) — Zweitquelle fuer sap_thomson ohne jede Referenz | S |
| atsidaev/trx2x | T3 runter (trd, scl, udi) | ZX-Konverter UDI/TRD/SCL/FDI/TD0/FDD — drei der Formate stehen auf T3 ohne Referenz | S |
| leaded-solder/x68000-floppy-tools | T3 runter (dim) | kleine, gegen VFIC getestete DIM-Implementierung auf Basis der pc98.org-Doku — Zweitquelle fuer dim (T3, referenzlos) | S |
| euanc/DiskFormatID | T3 runter (kfx, schwach) | Python-Parser fuer KryoFlux-Streams (Format-Erkennung); als kfx-Zweitquelle schwaecher als floppytools, nur falls jenes ausfaellt | S |
| konkotgit/ZX-Spectrum-FD-Images | T3 runter (trd/scl, Korpus) | Abbild-Sammlung, kein Code — Korpus-Kandidat fuer trd/scl; Herkunft/Rechte der Inhalte VOR Uebernahme klaeren | S |
| arnaud-carre/dir2msa | Wandlungspfade rauf (schwach) | MSA-Erzeuger (RLE-Encoder) — Oracle-Kandidat fuer ein ST↔MSA-Paar, das die Matrix nicht fuehrt; beide Plugins vorhanden (T2) | S |

## Fundus, geschlossen (29)

| Repo | Kennzahl | Begruendung | Aufwand |
|---|---|---|---|
| vampirefrog/fathuman | KEINE | Human68k-FAT-Dateisystem-Werkzeug (fatfs-Schicht), beruehrt den T3-Container dim nicht — FS-Ebene ist keine gefuehrte Kennzahl | S |
| lipro-cpm4l/fdutils | KEINE | Linux-Floppy-Treiber-Utilities (fdrawcmd-Ebene), kein Format, kein Pfad, keine Bench ohne Hardware (MF-310) | — |
| jfdelnero/HxCFloppyEmulatorManager_AmstradCPC | KEINE | Assembly-Programm, das AUF dem CPC laeuft (HxC-Bedienoberflaeche) — kein Format-, Pfad- oder Testbezug | — |
| jesswhyte/floppycapture | KEINE | Workflow-Skript um DTC herum (Ingest/Metadaten), parst selbst nichts | — |
| SergiyKolesnikov/ddi2raw | KEINE | DDI (DiskDupe) ist kein UFT-Format — ein neues Plugin faellt unter die EINFRIER-REGEL, auch als Idee | — |
| rgwan/hdcopy-tools | KEINE | HD-COPY-Dekompressor; Format nicht im Plugin-Bestand → Moratorium | — |
| laurent-fr/flextools | KEINE | FLEX09; kein flex-Plugin in der SSOT → neues Format → Moratorium | — |
| sergev/mfmdisk | KEINE | generisches MFM-Abbild-Werkzeug (2018), kein UFT-Format, MFM-Codec ist bei uns belegt | — |
| schlae/minnow-disk | KEINE | IBM 23FD Minnow — Exot ohne UFT-Format, Kern ist ein Hardware-Aufbau (MF-310) | — |
| psbhlw/floppy-disk-ripper | KEINE | Dump-Toolset fuer das ZX-Evolution-Board (Hardware-seitig), kein Format-/Pfadbezug | — |
| johnkw/dumpfloppy | KEINE | Mehrfachlesen mit CRC-Fehler-Aufbewahrung am PC-Controller — Multiread-Voting hat UFT (Inventar-Lektion MF-611) | — |
| wepl/wwarp | KEINE | WWarp-Format (.wwp) ist kein UFT-Format → Moratorium; laeuft zudem auf dem Amiga | — |
| simon-frankau/floppy-decode | KEINE | Dekodierung aus Oszilloskop-Traces (Haskell) — originell, aber keine der vier Zahlen | — |
| DarrellH89/DiskImageUtility | KEINE | Heathkit H-89/Z-100-Spezialformate fuer Gotek, keines im Plugin-Bestand; cpm-Bezug zu unspezifisch | — |
| jdurno/floppy-utils | KEINE | Perl-Beispielskripte (15 KB, 2016) | — |
| keithadler/2eforthos | KEINE | Ziel-Software (Forth-OS fuer Apple //e), kein Werkzeug | — |
| mvirkkunen/ddfloppy | KEINE | ddrescue-Visualisierer (13 KB) — UFT hat Heatmap; keine Kennzahl | — |
| mrcbax/fuc | KEINE | eigenes Kompressions-Dateisystem → neues Format → Moratorium | — |
| shazzner/LinuxAtariFloppyFormatter | KEINE | 2-KB-Formatierskript ueber den Linux-Floppy-Treiber | — |
| jandelgado/m20-floppy-tools | KEINE | Olivetti M20 — Format nicht im Bestand, Repo archiviert | — |
| Sam36502/disekt | KEINE | D64-Parsen/Mergen/Visualisieren — d64 steht auf T1b, dort bewegt eine Zweitquelle nichts | — |
| jpaulorio/greaseweazle-utils | KEINE | CLI-Komfort-Wrapper um gw + Image-Downloader, kein Mess- oder Formatbeitrag | — |
| ChrisBertrandDotNet/ST-Recover | KEINE | liest ST-Disketten am PC — st ist T2, nicht T3; die Kennzahl zaehlt nur T3-Abbau (Notiz: Korpus-Luecke „kein reales ST-Abbild" bleibt, aber das Repo liefert keine Abbilder) | — |
| laurent68k/AtariDisk | KEINE | Objective-C-Verwaltungstool (2016) fuer ST-Images — nichts, was st/msa (beide T2) oder einen Pfad bewegt | — |
| flindholm-lab/Atari-ST-Floppy-Image-Toolkit-GEM | KEINE | GEM-Dateimanager mit Addons — Anwendersoftware, keine Kennzahl | — |
| prachigauriar/EncryptedDiskImageWrapper | KEINE | macOS-Krypto-DMG-Wrapper (2013, archiviert) — anderes Fachgebiet | — |
| andrewrowell/RDiskImager | KEINE | Ruby-Wrapper um dd+pv | — |
| amigadev/trackloader | KEINE | Amiga-Bootblock-Trackloader (Ziel-Software); allenfalls Fundus fuer spaetere Schutz-Korpora | — |
| gonk23/HXCFE_Amiga_copy_utility | KEINE | Kopier-Utility, das AUF dem Amiga laeuft (2016) — kein Bezug zu hfe (T1b-Gruppe) oder einem Pfad | — |

## Bilanz

* **8** Repos bewegen eine Kennzahl (5× T3 runter, 1× T3-Korpus, 2×
  Wandlungspfade rauf).
* **29** Repos sind **Fundus, geschlossen** — mit Einzeiler-Begruendung,
  nicht „spaeter".
* Kein Repo bewegt „leckende Tests" oder „Bench-Alter" — erwartbar:
  beides sind Baum- bzw. Community-Zahlen, keine Fremdcode-Zahlen.

## Was danach gilt

Die Repo-Erhebung **als Programm endet mit diesem Vorfilter.** Neue Repos
kommen kuenftig nur noch **ereignisgetrieben** herein:

* ein Plan-Baustein braucht eine benannte Referenz,
* eine Kennzahl stagniert und es fehlt ein Oracle,
* ein Format-Dossier braucht eine Zweitquelle.

Nicht mehr listengetrieben. Die 8 Durchgelassenen sind eine
**Rangfolge fuer kommende Zyklen**, keine Auftraege — OPEN_ITEMS-Vorschlaege
entstehen erst aus einem vollen Zyklus (Inventar → Messung → Gutachten).
