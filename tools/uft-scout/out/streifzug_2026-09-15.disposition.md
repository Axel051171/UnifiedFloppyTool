## Disposition: 5 Funde

## SOFORT (2)
  1. [SCOUT-90] Offizielle NED-Quellen (MIT) als Spec-Referenz fuer syn — das letzte T3
     Repo Synclavier/Software (+ Synclavier/Documentation) · Zone GRUEN · Kanal Spec · Aufwand mittel
     Kennzahl: T3 runter
     Warum hier: bewegt T3 runter, Kanal Spec offen, nichts steht davor
     Beleg: work/Software.messung.json: LICENSE=MIT, Zone GRUEN, head 3b66233fc8. Able/UTILCAT/MODS/DISKFORM (18614 Byte, gemessen 2026-09-15): Geometrie kommt aus der Konfigtabelle (s#spdtrk = Sektoren/Spur, s#totcyl = Spurzahl), NICHT aus Konstanten; CP/M-Zweige setzen sect=26 bzw. sect=16 mit ID-Laengencode 0 (=128 Byte, 'IF cpmflag THEN CALL DEP(1,0)'), der XPL-Zweig setzt Laengencode 2 (=512 Byte, 'ELSE CALL DEP(1,2)'). hansliss/syncextract syncextract.c: BLOCKSIZE 0x200 (512) fuers Dateisystem. Beides widerspricht uft_syn.c (77x2x16x256, P3-340, 'keine nachpruefbare Referenz'). Dazu Documentation/…/APPENDIX - FORMATTING AND DUPLICATING DISKETTES RELEASE V September 1984.pdf im selben MIT-Konto.

  2. [SCOUT-91] gw als FM-Bitstrom-Erzeuger — der P3-218-Blocker wird messbar
     Repo keirf/greaseweazle v1.23 · Zone GRUEN · Kanal Oracle · Aufwand klein
     Kennzahl: Pfade rauf
     Warum hier: bewegt Pfade rauf, Kanal Oracle offen, nichts steht davor
     Beleg: Vormessung 2026-09-15 auf DIESER Maschine, Release-Binary greaseweazle-1.23-win64.zip (10 762 543 Byte, kein Build noetig): selbstbenennende SSD 80x1x10x256 (204 800 Byte) -> 'gw convert --format=acorn.dfs.ss80' -> FM-HFE (2 049 024 Byte, Kopf HXCPICFE) -> zurueck: byteidentisch (204800/204800, 800/800 Sektoren). Determinismus am 40-Spur-Lauf: 2 Laeufe, gleiche SHA-256 b1130b27…; Marke 'UFT-GW' im HFE NICHT im Klartext (FM-kodiert, kein Durchreichen). Encoder-Quelle: src/greaseweazle/codec/ibm/ibm.py:50 fm_encode(), :150 Mode.FM, :402-420 master_track() mit 't = fm_encode(t)'. Lizenz Unlicense laut docs/ORACLES.md (dort gemessen gefuehrt). WARNUNG (MF-1021-Klasse): dieselbe 204 800-Byte-Datei an --format=acorn.dfs.ss (40 Spuren) nimmt KOMMENTARLOS die halbe Datei ('Found 400 sectors of 400 (100%)') — Erzeugnis-Inhalt ist bei jedem Lauf nachzuweisen.

## LISTE (0)
  —

## FUNDUS (3)
  1. [SCOUT-94] NED Release L (Juni 1986): echte Synclavier-Installationsdaten als Fixture-Quelle fuer syn
     Repo Synclavier/Synclavier_Distributions · Zone ROT · Kanal Fundus · Aufwand mittel
     Kennzahl: T3 runter
     Warum hier: heute kein offener Kanal — benannt wartend
     Beleg: work/Synclavier_Distributions.messung.json (head c01668b6d4): KEINE Lizenzdatei -> Zone ROT. Release_L/NED_Synclavier_Release_L_DD_Floppies.simg = 5 242 880 Byte (GitHub-API size, gemessen 2026-09-15); README.txt woertlich: '19 sub-directories should be copied on DD floppy disks with FORMCOPY' — das simg traegt die Floppy-Inhalte, nicht die Floppy-Geometrie. Oeffner: Freigabe beim Konto Synclavier (Cameron Jones) erfragen; das Konto veroeffentlicht die Abbilder ausdruecklich zum Erzeugen von Installations-Floppies, und Software/Documentation desselben Kontos stehen unter MIT.

  2. [SCOUT-92] cp2 create-disk-image als Apple-Erzeuger: d13/nib/2mg/woz — bedient P3-363 und die W0-Liste
     Repo fadden/CiderPress2 · Zone PRUEFEN · Kanal Oracle · Aufwand klein
     Kennzahl: keine
     Warum hier: bewegt keine der vier Kennzahlen — notiert, nicht eingeplant (Regel 9)
     Beleg: work/CiderPress2.messung.json (head 7a055a200e, letzter Commit 2026-07-24): LICENSE=Apache-2.0 (GELB), NOTICE=UNGEKLAERT -> Gesamtzone PRUEFEN. docs/Manual-cp2.md:632-670 (create-disk-image|cdi): '".d13" files are always 13-sector', '".nib" must be 35 tracks, but may be 13- or 16-sector', Beispiele 'cp2 create-disk-image newdisk.nib 35trk dos33' und 'cp2 cdi bigdisk.2mg 32m ProDOS'; :629 'cp2 copy-sectors disk.nib disk.woz'. d13 und 2img stehen auf der W0-Liste (docs/WRITE_VERIFICATION_TIERS.md); P3-363 nennt 2MG-Spielarten 280/800 Bloecke ohne Kanal.

  3. [SCOUT-93] Kuratierte Schutz-Pruefobjekte: Cross-Track-Sync, Half-Tracks, MC3470-Fake-Bits, 3.5us-Timing, Flux-Spuren
     Repo applesaucefdc.com/woz (John K. Morris) — woz_images.zip · Zone ROT · Kanal Fundus · Aufwand klein
     Kennzahl: keine
     Warum hier: bewegt keine der vier Kennzahlen — notiert, nicht eingeplant (Regel 9)
     Beleg: http://evolutioninteractive.com/applesauce/woz_images.zip, 6 016 662 Byte, geladen und gelistet 2026-09-15: WOZ 1.0/2.0/2.1-Ordner, readme.txt v1.4 (2021-06-16) woertlich: 'intended to assist Apple ][ emulator developers in testing', benennt je Titel das geprueft Signal (Take 1/Blazing Paddles = Cross-Track-Sync; Hard Hat Mack = 2 Spuren breite Spur; The Bilestoad = Half-Tracks; The Print Shop Companion = MC3470 fake bits; Border Zone = Optimal Bit Timing 3.5us; ProDOS-Abbild mit FLUX-Spuren 0,2,4,…). KEIN Lizenztext im Zip; Inhalt sind kommerzielle Titel (DOS 3.3 System Master, Planetfall, Stargate, …) -> Regel 'keine Lizenz = alle Rechte vorbehalten'. Oeffner: Eigentuemer-Entscheidung, ob der gitignorierte Korpus (tests/corpus + SHA-Manifest) solche Objekte fuehren darf — und P0-2-Verdrahtung, damit ueberhaupt eine erreichbare Erkennung dagegen gemessen werden kann.

## Nächster Griff
  [SCOUT-90] Offizielle NED-Quellen (MIT) als Spec-Referenz fuer syn — das letzte T3
  — bewegt T3 runter, nichts steht davor.
