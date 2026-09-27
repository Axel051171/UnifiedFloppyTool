# UnifiedFloppyTool (UFT) v4.1.0

## Was ist das?

UnifiedFloppyTool ist eine Qt6 C/C++ Desktop-Anwendung für die **forensische Sicherung und Analyse historischer Floppy-Disketten**. Es ist das umfassendste Open-Source-Tool dieser Art — vergleichbar mit einer Kombination aus dd, Wireshark und einem Oszilloskop, aber spezialisiert auf magnetische Speichermedien.

**Zielgruppe:** Archive, Museen, Retrocomputing-Enthusiasten, digitale Forensiker, Kopierschutz-Forscher.

**Philosophie:** "Kein Bit verloren. Keine stille Veränderung. Keine erfundenen Daten." — jede Information auf der Diskette wird erfasst, auch Timing-Anomalien, Kopierschutz-Signaturen und beschädigte Bereiche.

**Design-Prinzipien (verbindlich):** Siehe [`docs/DESIGN_PRINCIPLES.md`](docs/DESIGN_PRINCIPLES.md).
Bei Konflikt zwischen Prinzip und Code-Änderung gewinnt das Prinzip. Bekannte
Compliance-Lücken: [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md).

## Kernfunktionen

### 1. Disk-Imaging (Lesen/Schreiben)
Unterstützt 6 Hardware-Controller (HAL teilweise wired — siehe pro Eintrag):
- **Greaseweazle** (72 MHz Flux-Capture, USB) — read+write+flux, production
- **SuperCard Pro** (40 MHz / 25 ns sample, USB FT240-X 12 Mbps) —
  HAL [~] M3.1 libusb wiring LANDED (MF-254); Tier-3 HW-bench pending
  (UFT-008); CLI available
- **KryoFlux** (24 MHz, USB via DTC-Tool) — read via subprocess
- **FC5025** (USB 5.25" Read-Only) — read via fcimage CLI
- **XUM1541/ZoomFloppy** (IEC-Bus für Commodore-Laufwerke) — HAL [~] M3.2
  partial. **Berichtigt MF-650:** das libusb-Wiring ist seit MF-301 da
  (16 `UFT_HAS_LIBUSB`-Stellen), und `KNOWN_ISSUES.md` §M.4 führt die
  Protokoll-Deltas seit MF-301 als „RESOLVED IN CODE". Offen ist die
  **CBM-DOS-Kommandoebene** (`U1:`/`M-R`/Kanal 15) — **sechs** Funktionen
  geben unbedingt `UFT_ERR_NOT_IMPLEMENTED` (`identify_drive`,
  `get_status`, `read_track_gcr`, `read_track`, `read_disk`,
  `write_track`; hier stand „sieben", gemessen MF-1025 sind es sechs) —,
  plus die Tier-3-Bank. **Und MF-1025 hat eine zweite Hälfte gemessen:**
  die einzige Konstruktionsstelle im Produkt ist
  `XUM1541ProviderV2(nullptr, nullptr, nullptr)`, einen
  `make_xum1541_*_runner` gibt es im Baum überhaupt nicht — die
  all-null-Konstruktion ist also kein Verdrahtungsversehen, sondern die
  ehrliche Folge davon, dass es nichts zu verdrahten gibt.
  `docs/CAPABILITIES.md` führte XUM1541 dabei als 🟡/🟡; berichtigt
  auf ⬜/⬜
- **Applesauce** (Apple-spezialisiert, 8 MHz / 125 ns, Text-Protokoll
  über serielle USB-Verbindung) — HAL [~] M3.3 partial (utility + tick-
  conversion + lifecycle real, serial I/O pending)

(FluxEngine + UFI/USB-Floppy exist as Qt providers but not as HAL backends.
 M3 plan: alle drei stubbed-HALs sind jetzt [~] partial scaffold mit
 echten Pure-Utility-Funktionen + honest USB/Serial-Stubs; libusb-/Serial-
 Wiring multi-session — siehe `docs/MASTER_PLAN.md` §M3.)

> **Siebter Eintrag, aber kein siebter Controller (MF-1176).** Seit MF-1176
> führt die Merkmalstafel `UFT_CAPS_OS_VOLUME` — den logischen
> Sektortransport über das **Wirtssystem**, also ein gewöhnliches
> PC-Laufwerk am gewöhnlichen Rechner. Für die ganze Klasse der
> Sampler-Formate (Roland S-50/S-330/S-550/W-30, Akai S900/S1000, Korg
> DSS-1) ist das der richtige und billigste Weg: sie liegen als
> gewöhnliches MFM auf 512-/1024-/2048-Byte-Sektoren, und Fluss braucht man
> dort erst, wenn die Diskette beschädigt oder geschützt ist. **Die Zahl
> der Hardware-Controller bleibt 6** — wer sie zählt, zählt diesen Eintrag
> nicht mit.
>
> Der Eintrag ist eine **Absage, keine Zusage**: `can_read_flux = false`,
> `can_read_bitstream = false`, `can_read_sector = true`, und seine
> `limitations[]` sprechen aus, was über diesen Weg **unerreichbar** ist —
> Weak Bits, Phantomsektoren, Kopierschutz, Drehzahl, Index,
> Mehrfachumdrehung. Vorlage ist SDISK for Windows v1.7 (MIT, © 2011
> Miroslav Svetlik), **statisch zerlegt, nicht ausgeführt** — damit ist es
> nach der eigenen Regel von `docs/ORACLES.md` ausdrücklich **kein
> Oracle**, sondern der Kanal *Nachbau* (MF-695): übernommen sind Zahlen,
> keine Zeilen. x50conv wurde dabei anders behandelt, weil seine Lizenz
> Disassemblierung ausdrücklich untersagt — aus ihm stammt nichts als die
> mitgelieferte Dokumentation.
>
> **Und der Auftrag dazu ist nur zur Hälfte erfüllbar, das gehört gesagt.**
> Er lautete „die Oberfläche muss das sagen können". Gemessen über
> `git grep` je Bezeichner haben **alle vier** öffentlichen Funktionen von
> `src/hal/uft_hal_profiles.c` — `uft_hal_get_drive_profile`,
> `uft_hal_get_controller_caps`, `uft_hal_print_controller_caps`,
> `uft_hal_print_drive_profile` — **je 0 Aufrufer** außerhalb ihrer eigenen
> Datei. Die Tafel mit 8 Laufwerksprofilen und 6 Merkmalssätzen liest
> niemand; die Einschränkungen stehen im Quelltext und erreichen keinen
> Bediener. Das ist dieselbe Lage wie beim Kopierschutz-Katalog (P0-2) und
> bei den DeepRead-Modulen (MF-627/MF-767): **Bestand, nicht Fähigkeit.**
> Erster Leser überhaupt ist seit MF-1176
> `tests/test_roland_osvol.c::t_hal_profil_ist_registriert` — ein Test,
> kein Produktivpfad, und er wird rot, wenn die Registrierung verschwindet
> (gemessen). Der Weg heraus steht als **P3-429**, samt der zweiten
> gemessenen Hälfte: die Tafel führt 6 Sätze, `src/hardwaretab.h:45-62`
> listet **9** V2-Provider — drei haben gar keinen Eintrag, und eine
> Anzeige ohne Eintrag darf nicht „keine Einschränkungen" bedeuten.

### 2. Format-Unterstützung (137 Plugins definiert, 138 format IDs)

> **MF-446/447:** 137 = 88 ausgeschrieben + 49 aus dem `DSK_PLUGIN()`-Makro.
> Seit MF-447 registriert `main()` sie beim Start — vorher war die Registry
> zur Laufzeit leer und `uft_disk_open()` lieferte für jede Datei NULL.

> **EINFRIER-REGEL (MF-363, präzisiert MF-498):** Kein neuer **ungeprüfter**
> Code im Format-/Decoder-Layer. „Geprüft" heißt: benannte Referenz oder
> Rotbeweis-zuerst, jede Zahl im Commit gemessen, Referenz im Header — alle
> drei. Moratorium für neue Format-Plugins bis Label-Skript (T1/T1b/T2/T3)
> läuft und ATR/D64/ADF/FDI/NFD-r0 auf T1/T1b gehoben sind; danach 1:2 (ein
> neues Format = zwei Hebungen). Verbindliche Fassung:
> [`docs/VERIFICATION_PLAN.md` §Einfrier-Regel](docs/VERIFICATION_PLAN.md).
> **Was „unterstützt" hier heißt (MF-509):** von den 88 tier-geführten
> Plugins steht **1 auf T3 — ungeprüft** (seit MF-1135 nur noch `syn`) und **2 auf `n/a`**,
> weil sie gar keine Behälterformate sind (MF-1077) (MF-654: `adl` und
> `adf_arc` auf T2; MF-690: `dim_atari` auf T1b, erstes fremd erzeugtes
> DIM im Korpus; MF-716: `do` auf T2, das erste Apple-Format —
> Differenzlauf gegen das Oracle `to_woz2`, 560 von 560 Sektoren
> byteidentisch): kein Test, oder ein
> synthetischer Test ohne Abgleich gegen eine autoritative Quelle. Genau
> in dieser Lage waren die fünf fabrizierten Parser grün
> (FMT-2/3/10/11/12). Belegt sind T1=8, T1b=70, T2=8 (Verlauf jeder Hebung, MF-690 bis MF-1332, woertlich in [`docs/CLAUDE_CHRONIK.md`](docs/CLAUDE_CHRONIK.md) §1). Die Liste unten
> nennt, was **gelesen werden soll**, nicht was **geprüft ist** — pro
> Format: [`docs/VERIFICATION_TIERS.md`](docs/VERIFICATION_TIERS.md).

> **Schreibzusagen (MF-883/930/931):** zwanzig Formate meldeten Schreiben, ohne die Datei zu
> beruehren; seit MF-930 antworten sie `UFT_ERROR_NOT_SUPPORTED`, Tor 57 haelt die Klasse,
> `opus` schreibt seit MF-931 bis in die Datei. Verlauf und Regeln fuer die uebrigen:
> [`docs/CLAUDE_CHRONIK.md`](docs/CLAUDE_CHRONIK.md) §2.
>
> Die Liste unten nennt, was **gelesen** werden soll. Ob ein Format auch
> **geschrieben** wird, sagt seine Merkmalstafel — nicht diese Überschrift.

Liest/schreibt Disk-Images von praktisch jedem 8-Bit- und 16-Bit-Computer:
- **Commodore:** D64, D71, D81, G64, T64, CRT, PRG, P00
- **Apple:** DO, PO, WOZ (v1/v2/2.1), A2R, MOOF, 2MG, NIB, DC42
- **Atari:** ATR, ATX, ST, STX, MSA, DCM, XFD — **und XFD seit MF-1175 auch
  zweiseitig** (XF551 Quad Density, 1440 Sektoren zu 256 Byte = 368 640).
  Vorher wies die Sonde diese Größe AB (`fs > 266240 -> return false`), also
  war eine gültige Atari-Diskette über die Erkennung unerreichbar; `open`
  hätte sie mit fest verdrahtetem `heads = 1` als 80-Spur-Diskette zerlegt
  und `read_track` sagte für Kopf 1 unbedingt ab — **720 von 1440 Sektoren**.
  Derselbe Fehler stand ein zweites Mal als Konstante im Waisen
  `src/formats/atari/uft_xfd_parser_v2.c:57`, wo `XFD_TRACKS_QD 80` neben
  `sides = 2` und 1440 Gesamtsektoren stand — 80 × 2 × 18 = 2880, die Zeilen
  widersprachen sich selbst. **`ATR` kann es weiterhin NICHT:** dieselbe
  Nutzlast mit 16 Byte Kopf davor meldet 80 Zylinder auf einem Kopf, und weil
  seine Sonde 95 gegen 40 vergibt, gewinnt im Erkennungsrennen die falsche
  Geometrie (gemessen, P3-425). Die Abbildung selbst steht genau **einmal**,
  als Datenzeile `UFT_ORDER_SERPENTINE` in
  `include/uft/core/uft_sector_order.h`
- **IBM PC:** IMG, IMA, IMD, TD0, DMK, CQM
- **Amstrad/Spectrum:** DSK, EDSK, TRD, SCL, MGT, TAP, TZX
- **BBC/Acorn:** SSD, DSD, ADF, UEF
- **Flux-Formate:** SCP, HFE (v1/v2/v3), KryoFlux RAW
- **Japanisch:** D88, D77, NFD, DIM — **berichtigt MF-1064:** hier standen auch HDM, XDF und FDX. Für die drei gibt es **kein registriertes Plugin** (gemessen über `gen_format_list.py`). `fdx.c` und `hdm.c` liegen in der verwaisten `FloppyDevice`-Schicht — beide in `docs/orphan_baseline.txt` —, und `fdx.c` nennt sich selbst „heuristic raw sector image“, liest den FDX-Kopf also nicht; `uft_xdf_api.c` ist nicht registriert. Das ist die Klasse MF-930/P3-204 (Leser ohne Tür), verschränkt mit der Klasse MF-509 (die Liste nennt, was gelesen werden SOLL). Ein Verdrahten wäre unter der EINFRIER-REGEL erlaubt — sie lässt ausdrücklich vorhandenen, unerreichbaren Code zu —, aber ein heuristischer Roh-Leser als FDX verdrahtet wäre genau die Wette, die MF-961 bei `86f` ABGELEHNT hat. Siehe P3-349. **NACHTRAG MF-1087 — die Aussage „für XDF gibt es kein Plugin“ hatte eine Folge, die niemand nachgesehen hat.** `uft_format_plugin_dim` („Sharp X68000 Disk Image“) führte die Endungsliste `"dim;xdf"`, und das Wort XDF kommt in seiner Datei sonst **nirgends** vor. `uft_resolve_format_plugin()` wählt aber das **Ziel eines Schreibvorgangs** über die Endung, sobald die Format-ID nicht eindeutig ist — und gemessen tragen **101** registrierte Plugins `UFT_FORMAT_DSK`. Weil `.xdf` im ganzen Baum nur dort stand, lieferte `out.xdf` **gemessen „DIM“**: eine Schreibabsicht wurde still in eine andere übersetzt. Seit MF-1087 löst `out.xdf` auf **nichts** auf, `tests/test_endung_nennt_kein_fremdes_format.c` hält es fest (Rotbeweis zuerst: 2 von 4 Zusagen fielen vor der Korrektur). **Gefunden hat es kein Lesen, sondern ein Werkzeug** — der Erzeuger-Zensus kennt seit MF-1087 auch `floptool`, und `dim` stand darin als einzige Zeile „Werkzeug sagt RW, Kanal UNGEMESSEN“, zugeordnet über eben diese Endung
- Plus: MSX, Thomson, TI-99, Roland, HP LIF, CP/M, Micropolis, Victor, Zilog, etc.

> **Präzisiert MF-1176 — „Roland" heißt hier etwas anderes, als ein Leser
> erwartet.** Das einzige registrierte Roland-Plugin ist
> `DSK_PLUGIN(23, rld, "DSK_RLD", "Roland DSK", "dsk")` in
> `src/formats/dsk_generic/uft_dsk_generic.c` — einer der 49 aus dem Makro,
> eine Geometriezeile für einen `.dsk`-Behälter, **ohne Tier-Zeile**
> (gemessen: `docs/VERIFICATION_TIERS.md` führt weder `rld` noch
> `dsk_rld`). Die **S-Serie** — S-50/S-330/S-550/W-30, rohe
> 737 280-Byte-Sektorabzüge mit den Endungen `.sdk/.s50/.s33/.s55/.out/.w30`
> — ist damit **nicht** gemeint und hat kein Plugin. Erkennen kann UFT sie
> seit MF-1176 (`uft_roland_identify()`, sieben Fälle aus Sektor 0, doppelt
> belegt); registriert ist sie nicht, weil das Moratorium der
> EINFRIER-REGEL gemessen weiter gilt (`nfd` steht auf T2, nicht T1/T1b).
> Siehe **P3-427**. Und `include/uft/formats/rolandd20.h` ist ein
> Phantom-Header: vier Zeilen, kein `.c`, kein Inhalt.

> **Ehrlichkeits-Hinweis (MF-729) — kopflose Formate werden nur an der
> Größe erkannt, und das Werkzeug sagt es jetzt.** Bis dahin vergab jede
> Sonde ihre Konfidenz für sich, ohne gemeinsame Skala. Gemessen an
> einem Puffer aus **lauter Nullen** — der keinerlei Signatur trägt —
> meldeten sie **35 bis 85** für exakt dieselbe Erkenntnis. Der Vergleich
> zwischen zwei Plugins war damit willkürlich, und es hatte Folgen: ein
> **PC-160K**-Abbild verlor gegen `TRD` (82), ein **Macintosh-800K**
> gegen `D81` (80), ein **PC-360K** gegen `MSX` (75) — nicht weil die
> mehr erkannt hätten, sondern weil ihre Zahl größer gewählt war. Und
> `uft_probe_ranking.tied` meldete dabei **1**: „eindeutig", wo niemand
> etwas erkannt hatte.
>
> Seit MF-729 tragen die Konfidenzen **Bedeutung statt Rang** (0–29 kein
> Anspruch · 30–49 nur die Größe · 50–79 Struktur gelesen · 80–100
> Merkmal getroffen), und zwei Eichungen erzwingen sie mechanisch: auf
> einem Nullpuffer darf nichts ≥ 50 melden, und wer 50–79 beansprucht,
> muss ≥ 95 % zufälliger Puffer abweisen. Beide laufen über **alle**
> Plugins, nicht über eine gepflegte Liste.
>
> Für Benutzer heißt das: wer bisher ein PC-Abbild geöffnet hat, das
> (falsch, aber bequem) als TR-DOS aufging, bekommt künftig eine
> Rückfrage. **Die Erkennung ist nicht schlechter geworden — sie war
> vorher nur zuversichtlicher, als sie durfte.**

### 3. Format-Konvertierung (44 Pfade registriert, **18 angeboten**)

> **Ehrlichkeits-Hinweis (MF-526, Zahlen neu gemessen MF-541):** die
> Wandlungstabelle fuehrt **44** Paare. Die Rundlauf-Matrix hat **16**
> Eintraege; **14** davon werden angeboten (zwei stehen als UNMOEGLICH):
>
> * **7 verlustfrei (je mit Messung)** — D64→D64, ADF→ADF, D64→G64,
>   IMG→HFE, **ATR→XFD**, **XFD→ATR**, **ADF→HFE** (neu MF-1081: der
>   AmigaDOS-Encoder, den MF-539 als fehlend benannt hat, ist da und an
>   einer ECHTEN Aufnahme abgenommen — aus `gw_amigados.hfe` dekodiert
>   und neu kodiert stehen **11 von 11 Sektoren byteidentisch** in der
>   Originalspur. Rundlauf ADF→HFE→ADF: **0 von 901 120 Byte**
>   abweichend, an einer Quelle MIT Inhalt — genau das fehlte MF-538,
>   dessen Ruecknahme an einer LEEREN Diskette scheiterte, deren
>   Nulllinie 0,08 % Abweichung wie eine fast perfekte Wandlung aussehen
>   liess. Die erzeugte HFE ist eine **Rekonstruktion, keine Aufnahme**). Jedes einzelne mit einer
>   Bit-Identitaets-Messung im Baum (MF-532/533/539/655), keines auf
>   Zusicherung. Die beiden Atari-Paare sind seit MF-655 die ersten
>   ihrer Familie in der Matrix: XFD ist das ATR ohne seinen
>   16-Byte-Kopf (`atr[16:] == xfd`, byteweise am Korpus-Paar
>   gemessen), und die Grenze steht dabei — ein ATR mit Sektorgroesse
>   256 wird ohne `accept_data_loss` abgelehnt, weil XFD die Angabe
>   nicht speichern kann und die Dateigroesse sie nicht verraet.
> * **11 nur mit ausdruecklichem `accept_data_loss`** (MF-1277: `IMD→IMG`
>   ist der neunte — und der erste der 29 gebauten, aber gesperrten
>   Pfade, der seinen Beleg bekommen hat).
>
> **30 weist das Preflight-Tor als UNGEPRUEFT ab** („conversion pair is
> UNTESTED — not offered until an entry exists“), weil sie keinen Eintrag
> in `src/core/uft_roundtrip.c` haben; zwei weitere als UNMOEGLICH. Das ist
> Absicht (MF-263/UFT-A01) und richtig.
>
> **Seit MF-567 stimmt das auch fuer den Speicher-Weg.** Bis dahin ging
> `uft_convert_memory()` vollstaendig am Tor vorbei: es uebergibt keine
> Dateipfade, und die Pruefung kehrte ohne Pfade sofort mit
> `ABORT_INVALID_ARG` zurueck — ein Wert, den der Aufrufer nicht als
> Abbruch fuehrte. Gemessen kamen aus 4096 Byte Zufall **3 712 758 Byte
> SCP** heraus, ohne Einverstaendnis, bei einem Paar, das die Matrix
> woertlich Fabrikation nennt. Der Kommentar an der Stelle sagte seit
> UFT-A01, der Umweg sei geschlossen.
>
> Die Matrix hat **15** Eintraege (MF-1081: ADF→HFE zurueck). Es waren 17; die drei ohne Wandler
> (`SCP→IMD`, `IPF→ADF`, `STX→ST`) sind seit MF-567 entfernt. Zwei davon
> standen hier seit MF-526 als „Verdikte ohne Konsumenten" — festgestellt
> und stehen gelassen ist nicht behoben, und es war nicht folgenlos: ein
> Urteil laesst das Paar durch das Preflight-Tor, und der Benutzer lief
> bis in den Rueckfall des Verteilers, nachdem Tabelle UND Tor ihm
> zugesagt hatten, der Weg sei gangbar.
>
> Frueher standen hier drei einander widersprechende Zahlen („8 angeboten“
> in der Ueberschrift, „11“ zwei Zeilen weiter, „33 abgewiesen“). Seit
> MF-541 haengen die Matrix-Zahlen an `scripts/update_inventory.py`
> (DERIVED_CLAIMS) und werden bei jedem Commit gegen
> `src/core/uft_roundtrip.c` geprueft — von Hand gepflegte Zahlen driften,
> das ist in diesem Baum dreimal belegt.
>
> Die frueheren LOSSLESS-Eintraege SCP↔HFE trugen keinen Beweis und sind
> seit MF-527 herabgestuft. `ADF→HFE` stand kurzzeitig als verlustbehaftet
> mit bezifferter Liste und ist seit MF-538 zurueckgenommen; seit MF-539
> lehnt der Wandler ausdruecklich ab, weil dem Baum ein AmigaDOS-Encoder
> fehlt.

Konvertiert zwischen allen gängigen Formaten:
- Sektor↔Sektor (D64↔IMG, IMD↔IMG)
- Sektor→Bitstream (D64→G64, ADF→HFE)
- Flux→Sektor (SCP→D64, SCP→ADF, HFE→IMG)
- Flux→Bitstream (SCP→HFE, SCP→G64)
- Flux→Flux (KryoFlux→SCP)

### 4. DeepRead — Adaptive Signal Recovery
Eigenentwickeltes OTDR-basiertes Analyse-System (inspiriert von Glasfaser-Messtechnik):

**3 Decode-Booster:**
- **Adaptive Decode:** Bei CRC-Fehler → OTDR-Analyse → LOW-Confidence-Regionen → aggressiver PLL-Re-Decode (±33%) → Fusion gewichtet nach Qualitätsprofil
- **Weighted Voting:** Float-gewichtete Multi-Revolution-Fusion statt einfacher Majority-Vote
- **Encoding Boost:** OTDR-Histogramm-Analyse verbessert Format-/Encoding-Erkennung

> **Ehrlichkeits-Hinweis (MF-627):** Die fünf Module unten liegen in
> `src/analysis/deepread/` und haben **keinen Aufrufer**. Gemessen: alle
> 13 exportierten Funktionen werden außerhalb ihres Verzeichnisses
> nirgends genannt — nicht in `src/`, nicht in der GUI, nicht in
> `tests/`; die einzigen Treffer sind ihre eigenen Prototypen in
> `include/uft/analysis/`. Unabhängig mit einfachem `grep` gegengeprüft.
> Das ist dieselbe Lage wie beim Kopierschutz-Katalog (P0-2): **Bestand,
> nicht Fähigkeit.**
>
> **BERICHTIGUNG (MF-767).** Hier stand: „Die drei Decode-Booster
> darüber sind davon nicht betroffen — sie haben mit
> `src/gui/uft_otdr_panel.cpp` einen echten Aufrufer." Das trifft auf
> **einen** von dreien zu. Gemessen über den ganzen Baum mit
> `git ls-files`, je Bezeichner:
>
> | Booster | Symbol | Aufrufer |
> |---|---|---|
> | Encoding Boost | `uft_otdr_detect_encoding` | **ja** — `uft_otdr_panel.cpp:888` |
> | Adaptive Decode | `uft_otdr_adaptive_decode` | **nein** — alle vier Fundstellen außerhalb der eigenen Datei sind **Kommentare** |
> | Weighted Voting | `uft_otdr_fuse_sector` | **nein** — nur eigener Prototyp |
>
> Einen Bezeichner `uft_otdr_weighted_vote` gibt es im Baum überhaupt
> nicht; die float-gewichtete Fusion ist `uft_otdr_fuse_sector()` und
> liegt **im Modul der Adaptive Decode** — beide fallen also zusammen.
>
> Das ist bemerkenswert, weil MF-627 selbst ein Ehrlichkeits-Hinweis
> war: die Notiz, die eine Überzeichnung berichtigt hat, trug eine
> zweite. Eine Erreichbarkeits-Zusage gehört gemessen wie jede Zahl —
> `docs/orphan_baseline.txt` und `tools/uft-innendienst/` führen
> `uft_otdr_adaptive_decode` bereits, die Tafel hier hat es nur nicht
> nachgezogen.
>
> Damit steht es bei den 8 DeepRead-Modulen: **1 erreichbar, 7 ohne
> Aufrufer** — nicht 3 zu 5.

**5 Forensik-Module:**
- **Write-Splice Detection:** Erkennt Schreibkopf-Ein/Aus-Übergänge
- **Magnetic Aging Profile:** Unterscheidet Alterung von physischem Schaden
- **Cross-Track Correlation:** Identifiziert radiale vs. magnetische Schäden
- **Revolution Fingerprint:** Einzigartiger Jitter-Fingerabdruck pro Diskette
- **Soft-Decision LLR:** Log-Likelihood-Ratios für Viterbi Soft-Input

### 5. Kopierschutz-Analyse

> **Ehrlichkeits-Hinweis (MF-508):** Automatisch laeuft die Erkennung von
> Schutz-SIGNALEN (Fuzzy Bits, lange/kurze Spuren, No-Flux-Bereiche,
> Overlap, Desync, Weak Bits, Illegal GCR) plus drei heuristisch
> benannten Schemata. Der Katalog der 55+ BENANNTEN Verfahren unten liegt
> in `src/protection/` und hat **keinen Aufrufer** — siehe
> `docs/OPEN_ITEMS.md` P0-2. Die Liste ist Bestand, nicht Fähigkeit.

Im Katalog dokumentierte historische Kopierschutz-Verfahren:
- V-MAX!, RapidLok, CopyLock, Speedlock, ProLok, Vorpal
- Dungeon Master Fuzzy Bits, FatBits, Pirate Slayer
- Lange Tracks, Halb-Tracks, Custom Sync, Density Mismatch
- Amiga: Rob Northen, CAPS/SPS-kompatibel
- Atari ST: CopyLock, Macrodos, dec0de

### 6. Forensischer Report & Audit Trail

> **Ehrlichkeits-Hinweis (MF-366):** Die frühere Audit-Trail-/Forensic-
> Report-C-API (`uft_audit_trail.h`, `uft_forensic_report.h`) war
> Phantom-API (100 % unimplementiert, keine Aufrufer) und wurde entfernt.
> Real existieren GUI-Panels + Provenance (`src/forensic/uft_provenance.c`);
> der volle Audit-Trail unten ist **Zielbild**, nicht Ist-Stand — siehe
> [`docs/SUBSYSTEM_MATURITY.md`](docs/SUBSYSTEM_MATURITY.md).
- Hash-Verifizierung (MD5, SHA1, SHA256, SHA512 parallel)
- Hash-Chain für Integritätsnachweis
- Vollständiger Audit Trail (40+ Event-Typen, Timestamps, CHS-Kontext)
- Export: JSON, HTML, PDF, Markdown, XML, Plain Text
- Risiko-Scoring (0-100) mit Recovery-Empfehlung

## Architektur

```
┌─────────────────────────────────────────────────────────┐
│                    Qt6 GUI (C++)                         │
│  UftMainWindow, UftOtdrPanel (DeepRead), Sector Editor  │
│  ADF Browser, Hex Panel, Heatmap, Histogram             │
├─────────────────────────────────────────────────────────┤
│               Hardware Abstraction Layer (C)             │
│  Greaseweazle │ SCP │ KryoFlux │ FC5025 │ XUM1541 │ AS  │
├─────────────────────────────────────────────────────────┤
│                   Core Engine (C)                        │
│  Format Parsers (80 plugins, 138 IDs) │ PLL Decoder │ MFM/FM/GCR Codec │
│  Flux Decoder │ Sector Extractor │ CRC Engine            │
├─────────────────────────────────────────────────────────┤
│              Analysis Pipeline (C)                       │
│  OTDR (12 Module) │ TDFC │ φ-OTDR Denoise │ Confidence  │
│  DeepRead (3 verdrahtet + 5 unwired) │ Protection (Signale)  │
├─────────────────────────────────────────────────────────┤
│              Recovery Pipeline (C)                       │
│  Multiread Voting │ Adaptive Decode │ Partial Recovery   │
│  Forensic Flux Decoder │ CRC Correction                  │
├─────────────────────────────────────────────────────────┤
│              Filesystem Layer (C)                        │
│  in src/fs/ (mit FS-Stufe): AmigaDOS │ FAT12 │ CBM DOS  │
│  anderswo, ohne FS-Stufe: CP/M │ Atari DOS │ BBC DFS │  │
│  TR-DOS │ GEOS │ … (26 Kandidaten, MF-710)              │
└─────────────────────────────────────────────────────────┘
```

> **FM-Flusspfad (MF-864/938/869):** `flux_decode_fm()` legt seit MF-864 Sektoren an,
> abgenommen gegen `fluxtoimd` (ausgefuehrt, GPL-3) und an einer echten 8-Zoll-Aufnahme
> von 1979. Einen IBM-MFM-Encoder gibt es (`src/core/uft_mfm_encoder.c`), einen FM-Encoder
> nicht. Verlauf: [`docs/CLAUDE_CHRONIK.md`](docs/CLAUDE_CHRONIK.md) §3.

> **Ehrlichkeits-Hinweis (MF-710):** hier stand bis heute
> „AmigaDOS │ FAT12 │ CBM DOS │ Apple DOS/ProDOS │ CP/M │ TRSDOS │
> TI-99 │ Atari DOS". Zwei der acht tragen nicht: **Apple DOS/ProDOS**
> hat im ganzen Baum eine einzige Datei (`src/formats/apple/
> prodos_po_do.c`, 130 Zeilen) mit **null** Verzeichnis-Bezug — sie
> ordnet Sektoren um, sie liest kein Dateisystem; **TRSDOS** hat
> **null** Dateien (das TRS-80-Format `jv3` gibt es, das Dateisystem
> nicht).
>
> Die anderen sechs gibt es — nur nicht dort, wo die Tafel sie
> vermuten liess. `src/fs/` enthält **3** davon; CP/M (1425 Z.),
> Atari DOS (1006 Z.) und weitere liegen unter `src/formats/` und
> `src/detect/`. Das war keine Kleinigkeit: `scripts/gen_fs_tiers.py`
> speist eine der **vier Release-Kennzahlen** und wählte seine Dateien
> mit `(WURZEL/'src'/'fs').glob('*.c')` — die Kennzahl zählte 8, der
> Baum hat 34. Seit MF-710 kommt die Dateimenge aus `git ls-files`,
> und `docs/VERIFICATION_TIERS_FS.md` führt die 26 ungeführten
> Kandidaten sichtbar auf.

## Build-System

- **Primär:** qmake (`.pro`-Datei), 715 Source-Dateien / 481 Header
  (Stand 2026-08-19 nach MF-271; zaehlen mit
  `find src -name '*.c' -o -name '*.cpp' | wc -l`)
- **Tests:** CMake (`tests/CMakeLists.txt`), **266/266 grün mit einem
  benannten Skip** (Stand 2026-08-26, MF-601). Vorher stand hier 205/205
  (2026-08-16) — die Zahl war doppelt veraltet, und bis MF-596 zählten
  32 Testdateien ihren Erfolg bedingungslos, konnten also gar nicht rot
  werden. Was dahinter lag: sieben Tests mit 18 Prüfungen, drei davon
  echte Fehler im Format-Layer;
  GLOB-discovered von 228+ `test_*.c`/`*.cpp` Quelldateien, 39 in
  `EXCLUDED_TESTS` (fehlende Module / WIP-Subsysteme). Zahlen driften —
  bis `update_inventory.py` (Phase 1, MF-363) existiert, gilt: `ctest -N`
  im Build-Verzeichnis ist die einzige Wahrheit, nicht diese Datei.

  > **BERICHTIGT MF-1332 — der Absatz warnt vor genau dem, was ihm
  > passiert ist.** „Zahlen driften … `ctest -N` ist die einzige
  > Wahrheit, nicht diese Datei" steht da seit MF-601, und alle drei
  > Zahlen darueber sind seither gedriftet. Gemessen am 2026-09-21:
  >
  > | | stand hier | gemessen |
  > |---|---|---|
  > | Testquelldateien `test_*.c/.cpp` | 228+ | **514** (`git ls-files`) |
  > | laufende Tests | 266/266 | **534**, davon 4 rot, 1 benannter Skip |
  > | `EXCLUDED_TESTS` | 39 | **2** |
  >
  > Die **2** sind `test_libdsk_formats` und `test_fat_extensions`, beide
  > mit gemessenem Grund im Block selbst (`tests/CMakeLists.txt:77-91`):
  > fehlender libdsk-Klebstoff bzw. 11 von 11 zugesagten Funktionen ohne
  > Umsetzung (P3-219). Die 39 stammen aus einer Zeit, in der die Liste
  > als Sammelbecken diente; seit MF-411 sind die drei Header-Zwillinge
  > wieder in Betrieb.
  >
  > Die vier roten sind der vorbestehende Windows-`tmpnam()`-Befund
  > (`test_2img_nib_stride`, `test_d13_layout_verified`,
  > `test_dms_loch_ist_kein_guter_sektor`, `test_do_layout_verified`) —
  > sie melden woertlich „Wegwerf-Datei fehlgeschlagen".
  >
  > **Behoben MF-1340.** Gemessen liefert MinGW-`tmpnam()` den Namen
  > `\spcc.` — eine Datei im WURZELverzeichnis des Laufwerks, die ein
  > gewoehnlicher Benutzer nicht anlegen darf. Im CI fiel das nicht auf
  > (Linux schreibt nach `/tmp`; warum der Windows-Lauf dort gruen ist,
  > ist NICHT gemessen). Die vier
  > Tests nehmen jetzt `TMPDIR`/`TMP`/`TEMP` wie die uebrigen Dateitests;
  > vorher 4 von 4 rot, danach 4 von 4 gruen. Und `test_2img_nib_stride`
  > uebersprang seine zweite Pruefung STILL, wenn die zweite Wegwerfdatei
  > nicht anzulegen war — das ist jetzt ein Fehlschlag.
- **CI:** GitHub Actions — Linux (GCC), macOS (Clang), Windows (MinGW)
- **Sanitizer:** ASan + UBSan Workflows
- **Coverage:** lcov + Codecov

### Wichtig für Entwickler:
- `CONFIG += object_parallel_to_source` ist ZWINGEND (35+ Basename-Kollisionen)
- C-Header mit `protected` als Feldname → nicht direkt in C++ includierbar
- Qt6 erfordert `static_cast<char>()` für `QByteArray::append()`

### Grundsatz: Dateimengen kommen aus git, nicht aus gepflegten Listen (MF-636)

**Wer in einem Skript entscheidet, WELCHE Dateien geprüft werden, fragt
`git ls-files --cached --others --exclude-standard` — nie eine
hartkodierte Verzeichnisliste.** Der Helfer dafür ist
`scripts/repo_scope.py`.

Der Grund ist gemessen, nicht theoretisch. Eine gepflegte Ausschlussliste
ist eine Aufzählung bekannter Fälle, und die veraltet still. In diesem
Baum ist genau das **viermal** passiert:

| | was aufgezählt wurde | was durchfiel |
|---|---|---|
| MF-567 | Abbruch-Codes | drei Urteile ohne Wandler |
| MF-578 | Offscreen-Tests | neue GUI-Tests |
| MF-598 | `SKIP_RETURN_CODE`-Namen | jeder neue Skip |
| MF-633 | `SKIP_DIRS` in zwei Toren | `tools/uft-scout/work/` — geklonte **Fremd-Repos**, aus denen zwei Tore Befunde meldeten, die CI nie sieht |

Die Regel gilt in beide Richtungen: `git ls-files` liefert auch neue,
noch nicht hinzugefügte Dateien (`--others --exclude-standard`), damit
sich niemand einem Tor entzieht, indem er `git add` unterlässt. Ist git
nicht befragbar, lässt der Filter alles durch **und sagt es**.

> **BERICHTIGT MF-1171 — hier stand: „eine stille Lücke wäre schlimmer
> als ein paar Fremdbefunde mit Hinweis". Der erste Halbsatz stimmt, der
> zweite beschreibt nicht, was gemessen passiert.** Es sind nicht ein paar
> Fremdbefunde. `repo_scope` meldete einmal `git ls-files nicht
> verfuegbar`, prüfte daraufhin den ganzen Verzeichnisbaum samt
> gitignorierter Fremdklone — und `enum_macro_conflicts.py` **starb** an
> `ValueError: invalid literal for int() with base 0: '0170000'`, dem
> `__S_IFMT` aus `tools/uft-scout/work/cpmtools/cpmfs.h:13`.
> `check_consistency.py` endete mit rc 1 und einem Traceback, **bevor die
> übrigen 23 Kategorien liefen**. Ein Absturz ist kein Urteil — die Klasse
> MF-1000/Tor 64 in neuer Gestalt.
>
> Die Großzügigkeit der Rückfallebene ist also keine Nachsicht, sondern
> eine **Anforderung an jeden Parser dahinter**: er bekommt dort Eingaben,
> die im eigenen Baum nicht vorkommen. Sechs Stellen lasen ein
> C-Ganzzahlliteral selbst, mit vier verschiedenen Ausgängen — zwei
> stürzten ab, zwei lasen `0170000` still als 170000 statt 61440, eine
> verwarf den Wert, und eine ließ den Laufwert des **vorherigen**
> Aufzählungseintrags stehen. Seit MF-1171 liegt die Lesart in
> `scripts/c_literal.py`, deren Zusage lautet: sie gibt `None` zurück für
> alles, was sie nicht versteht, und **wirft nie** (Selbsttest 38/38).
>
> **Warum `git ls-files` ausfiel, ist ausdrücklich NICHT gemessen** und
> wird hier nicht durch eine plausible Geschichte gefüllt: der Aufruf
> braucht im Leerlauf 42 ms gegen eine Zeitgrenze von 120 s, und während
> eines laufenden Commits wiederholt liefert `repo_files()` 2907 Dateien
> mit rc 0. Einmal beobachtet, nicht reproduziert — `P3-421`.
>
> Und der zweite Teil dieses Befunds ist **offen**: dass die Rückfallebene
> „alles prüfen" statt „Umfang nicht feststellbar" sagt, betrifft 16
> Skripte und ist eine Entwurfsentscheidung, kein Nebeneffekt.

### Grundsatz: jeder Baustein benennt seine Kennzahl (MF-640)

**Jeder Vorschlag und jeder Baustein sagt, welche der Release-Kennzahlen
er bewegt.** Ein Fund, der keine Zahl bewegt, ist **Fundus, nicht
Auftrag** — er wird notiert, nicht eingeplant.

Die vier geführten Zahlen:

| Kennzahl | Richtung | Quelle |
|---|---|---|
| ungeprüfte Formate (T3) | **runter** | `docs/VERIFICATION_TIERS.md`, abgeleitet |
| angebotene Wandlungspfade | **rauf** | `src/core/uft_roundtrip.c`, abgeleitet |
| leckende Tests | **null halten** | ASan/UBSan in CI |
| Bench-Alter je Controller | **runter** | `docs/CAPABILITIES.md` |

Wer eine **fünfte** Zahl einführt, begründet sie. Eine Kandidatin steht
bereit: **Dateien mit ungeklärter Herkunft**. Sie hat **zwei Stufen**,
und die zu verwechseln wäre genau die Zahlendrift, die dieser Baum
dreimal gesehen hat (MF-645):

| Stufe | Zahl | Quelle | bedeutet |
|---|---|---|---|
| **Verdacht** | **124** | `scripts/audit_attribution_licence.py` (abgeleitet, seit MF-651) | die Frage ist offen — es ist noch kein Befund |
| **Befund** | **9** offene Zeilen | [`docs/QUARANTINE.md`](docs/QUARANTINE.md) | auditiert, Weg festgelegt oder ausstehend |

**Gemeldet wird die Befund-Stufe**, weil sie ein Urteil trägt. Die
Verdachts-Stufe ist der Rückstand, aus dem sie gespeist wird — und
solange er 48 beträgt, ist jede Aussage über die Gesamtlage vorläufig
(`LIZ-1`).

Verfahren dazu: [`docs/QUARANTINE_PROCESS.md`](docs/QUARANTINE_PROCESS.md).

Der Sinn ist nicht Buchhaltung, sondern **Kopplung**: Scout-Priorisierung,
Eigentümer-Entscheidungen und MF-Reihenfolge hängen damit am selben Maß,
ohne dass jemand Reihenfolgen verhandeln muss. Das Release ist kein
Endpunkt, sondern der Messpunkt der Schleife — seine Zahlen sagen, wo der
nächste Durchlauf ansetzt.

### Grundsatz: Kennzahlen sind Folgen, keine Ziele (MF-1077)

**Eine Zahl darf sich nur aendern, weil eine neue Messung vorliegt.**
Wenn eine Änderung eine Kennzahl verbessert, ohne dass es eine neue
Messung dazu gibt, ist sie verboten — auch wenn sie sachlich
richtig aussieht.

**Fehlklassifikation wird umgeschrieben, nicht entfernt.** Löschen
ist keine Behebung. Das Entfernen von Code, Tests, Registereinträgen
oder Doku-Abschnitten ist nur mit ausdrücklicher
Eigentümerentscheidung erlaubt.

**Wo du löschen willst, liefere stattdessen die Auswahl:**
umschreiben / zurücknehmen / lassen — je mit Kosten und mit
der Angabe, welche Kennzahl sich wie ändert und aus welchem Anlass.
Dann halte an.

Der Anlass ist gemessen, nicht ausgedacht: bei `rcpmfs` (P3-338) war
die Fehlbewegung **nicht** das Löschen, sondern **die Zahl als
Motiv** — die Begründung lautete wörtlich, T3 sinke dann
„ohne einen Beweis zu fälschen“. Das ist der
Spiegelfehler zum Fälschen von Belegen, nicht sein Gegenteil. Wer
die Regel nur auf Löschungen legt, verschiebt denselben Reflex zum
nächsten Werkzeug: Ausnahmeliste erweitern, Test ausschließen,
Format umbenennen.

Mechanisch gehalten wird das von der **Namensrolle**
(`docs/FORMAT_ROLL.md`, Tor `scripts/audit_namensrolle.py`) und einem
`commit-msg`-Hook: eine Löschung in der Formatschicht verlangt eine
Zeile `Ruecknahme:` und einen Status `zurueckgenommen` mit Beleg.

### Grundsatz: eine Sondenkonfidenz wird abgeleitet, nicht vergeben (MF-1153)

**Verbindliche Fassung: [`docs/SONDEN_DOKTRIN.md`](docs/SONDEN_DOKTRIN.md).**
Eigentümer-Entscheidung vom 2026-09-15.

Der Anlass ist gemessen: von 23 offenen Punkten in `docs/OPEN_ITEMS.md`
waren **acht dieselbe Frage** — P3-392, P3-393, P3-394, P3-401, P3-402,
P3-403, P3-405, P3-406. Und der Beleg, dass sie je Fall neu beantwortet
wurde, stammt aus **einem Tag**: MF-1151 senkte `dmk` von 100 auf 75,
weil das Format keine Kennung hat, und MF-1152 ließ `ssd` bei 85, obwohl
Acorn DFS ebenso keine hat. Dieselbe Zeile in
`test_register_all_formats.c` musste daraufhin **zweimal am selben Tag**
berichtigt werden.

Die Leiter (`uft_probe_konfidenz()` in `uft_format_plugin.h`):

| Beleg | Wert |
|---|---|
| Kennung an fester Position, formatspezifisch | +50 |
| Selbstkonsistenz (Kopf sagt Größe = Dateigröße) | +25 |
| Struktur an berechneter Stelle (Verzeichnis, BAM) | +15 |
| Geometrie plausibel | +10 |
| **Größe allein** | **0 — nie hinreichend** |

Fünf Regeln und ein Zwischenschritt: **ohne Kennung ist die Obergrenze
45** (Klemme hinter der Summe, weil 25+15+10 = 50 wäre); bei
Gleichstand gewinnt der **engere** Anspruch; **wo der Pfad einen Namen
hat, verengt die Endung** — nur unter den bereits Gleichauf-Liegenden,
nie als Beleg, und `tied` bleibt sichtbar (Regel **2b**, MF-1252,
nachgetragen MF-1260); bleibt es gleich, gewinnt **keiner**
(„mehrdeutig" mit beiden Namen); eine Sonde sieht **nur ihren Puffer**;
ein Format ohne belegbares Merkmal bekommt **keine Sonde, sondern eine
Absage**.

> **BERICHTIGT MF-1260.** Hier stand „Fünf Regeln" und die Aufzählung
> ohne 2b — die Endungsregel lag seit MF-1252 ausschließlich im Code.
> Eine Regel an zwei Stellen mit zwei Bedeutungen ist die Bauform aus
> §MF-1177, und „enger" war in der verbindlichen Fassung bereits als
> **Erklärungsumfang** belegt. Die Endung ist keiner; sie steht deshalb
> als eigener Schritt da und nicht unter demselben Namen.

Gehalten von `scripts/audit_sondendoktrin.py` mit **fallender
Grundlinie** (Bauform Tor 57). Die Zahl darf nur sinken — und der Zweck
ist der Rand: **ein neues Format kann gar nicht mehr anders anfangen.**

> **BERICHTIGT MF-1260, zweite Zahl derselben Zeile.** Hier stand
> „Stand **83** Sonden, die ihre Zahl noch selbst vergeben, 2
> migriert". Gemessen meldet das Tor heute **Grundlinie 81**, „seit der
> Grundlinie migriert: 0" — die 83 war der Stand am Tag des Eintrags.
> **Die Zahl steht deshalb nicht mehr hier:** das Tor druckt sie bei
> jedem Lauf, und eine gepflegte Zahl neben einer gemessenen driftet
> (MF-541, D3). Dieselbe Behebung wie bei den drei Zahlen im Kopf von
> `teilstring.yml` (MF-1258/1259, `P3-507`).

### Grundsatz: eine Größe, eine Rechnung — und wer sie zweimal rechnet, hat sie nicht gemessen (MF-1177)

**Wird dieselbe physikalische Größe an mehr als einer Stelle gerechnet,
driften die Stellen — und die Abweichung sieht aus wie ein Fehler in den
DATEN.** Das ist die teuerste Verwechslung, die dieser Baum kennt, weil
sie Arbeit an der falschen Stelle auslöst.

Der Anlass ist gemessen. Der **Spurhaushalt** einer Diskette — wie viele
Byte nach Spurkopf und Sektoren für die Zwischenräume übrig sind — wurde
an **drei** Stellen gerechnet:

| Stelle | Spurkopf | Index-Adressmarke |
|---|---|---|
| `uft_fdc_calc_gap3()` | fest 146 (MFM) / 73 (FM) | **mitgezählt** |
| `scripts/audit_fdc_gaps.py` | `gap4a + gap1` | **nicht** gezählt |
| `test_fdc_gaps_1440k.c::belegt()` | `gap4a + gap1` | **nicht** gezählt |

Dass `PC 1.44M` in den letzten beiden „mit 16 Byte Luft passte", war kein
Spielraum, sondern **genau die nicht gezählte IAM** (12 Sync + 4 Marke).
Und die erste Rechnung ersetzte den profileigenen Spurkopf durch eine
Konstante — womit die beiden **Atari-ST-Profile** die Identität um 86 Byte
verfehlten, obwohl ihre Zahlen exakt aufgehen
(`0 + 60 + 9 × 614 + 664 = 6250`).

**Gemessen falsch war also nicht die Tafel, sondern die Messung.** Von
„14 von 17 Profilen gehen nicht auf" blieben mit der profileigenen
Rechnung **12**, und nach dem Einarbeiten einer benannten Quelle **6** —
jedes davon mit `gap_beleg` und `gap_quelle` versehen, also mit Grund.

Zwei Regeln daraus, beide billig im Einhalten:

1. **Bevor eine Zahl als falsch gilt, wird die Rechnung geprüft, die sie
   falsch nennt.** Eine Abweichung ist zunächst nur eine Abweichung.
2. **Die Rechnung gehört an EINE Stelle, und Tor und Test rufen sie**
   statt sie nachzubilden. Seit MF-1177 ist das `uft_fdc_gap_space()`,
   das den profileigenen Spurkopf samt `iam`-Angabe nimmt.

Die Bauform ist bekannt: MF-1015 (drei Prüfsummen, keine zwei gleich),
MF-1026 (drei Victor-Geometrien), MF-1032/MF-1034 (die vier
Anordnungsgesetze, zweimal einzeln wiederentdeckt). Neu ist hier nur, dass
die zweite Kopie in einem **Tor** und einem **Test** saß — also genau in
den Werkzeugen, denen man beim Widerspruch glaubt.

### Grundsatz: drei Sperren gegen die eigenen Wiederholungstäter (MF-1096)

Diese drei Regeln stehen nicht hier, weil sie einleuchten, sondern weil
jede von ihnen einen Posten mit **zweistelliger Ordnungszahl** im
Arbeitsprotokoll hat. Sie kosten im Einhalten Sekunden und im Verletzen
jedes Mal zwischen zehn Minuten und einer roten CI-Matrix.

**1. Mehrzeilige Dateiänderungen laufen über Edit/Write, nie über
Heredoc. Ein Skript mit mehr als einer Zeile wird als Datei angelegt und
dann ausgeführt.**

Ein Bash-Heredoc frisst `\n`, `\t`, `\r\n`, `\x7f` und jeden Backslash.
Gemessen ist das **über zwanzig Mal** passiert, zuletzt viermal an einem
einzigen Tag: die Mach-O-Magics `\xCA\xFE\xBA\xBE` wurden zu UTF-8, der
`\r\n`-Test prüfte auf zwei Buchstaben `r` und `n`, und einmal zerriss
das Heredoc **den Helfer, der das Problem umgehen sollte** (MF-1019).
Die Regel ist deshalb ausnahmslos, auch für „nur drei Zeilen“ — die
kaputten Fälle waren alle kurz.

> **Nachtrag MF-1343 — die Regel hatte kein Werkzeug, und der Mechanismus
> war ein anderer.** Fuer die Sperren 2 und 3 gab es seit MF-1096 Werkzeuge,
> fuer Sperre 1 nur diesen Text. Gemessen ueber alle 624 Protokolldateien
> dieses Projekts (Hauptsitzungen, Unteragenten, Workflows): 43 670
> Bash-Aufrufe, 4 157 davon verstossen gegen die Regel, **919 seit
> MF-1096**. Und die Ursache sitzt nicht im Heredoc, sondern im
> **Transport**: das Bash-Werkzeug halbiert jedes `\\`, bevor bash es
> sieht — auch in einfachen Anfuehrungszeichen, auch ohne Heredoc
> (`printf '%s' 'x\\y' | wc -c` ergibt 3, dieselbe Zeichenkette in
> PowerShell hat Laenge 4; ein einzelnes `\n` bleibt erhalten).
>
> Seither haelt **Tor 71** (`scripts/audit_heredoc.py`) die Regel: ein
> PreToolUse-Haken auf User-Ebene (`~/.claude/settings.json`,
> Eigentuemerentscheidung 2026-09-26, strenge Fassung) weist VOR der
> Ausfuehrung ab — jedes `\\` im Bash-Befehl, jedes Heredoc, das eine
> Datei fuellt, und jedes mehrzeilige Skript per Heredoc; Commit- und
> PR-Nachrichten bleiben erlaubt. Einspeiseprobe gemessen: der Haken
> sieht den Befehl VOR der Halbierung. Das CI prueft den Klassifizierer
> (29 Faelle, Mutationsmatrix 11/11); ob der Haken wirkt, misst
> `python scripts/audit_heredoc.py --protokolle --seit <Einbauzeit>`
> (Einbau 2026-09-26T09:05:19Z) — und meldet immer auch, wie viele
> Aufrufe es gesehen hat.

**2. Während ein Commit läuft, schreibt niemand in den Baum.**

Der Pre-Commit-Haken legt `.git/uft-commit.lock` an; jeder Generator
fragt sie über `scripts/commit_lock.py` ab und bricht ab. Der Grund ist
gemessen: dreimal hintereinander hat ein `gen_stand.py`-Lauf während des
laufenden Hakens den Commit abgewiesen — mit `[STAND.md stale]`, einem
Zustand, den **erst die Messung selbst erzeugt** hat (Klasse MF-1043).

Dazu gehört ein Verbot, und es steht hier, weil es einmal teuer war:
**`git checkout-index -f -a` ist verboten.** Das `-a` bezieht sich auf
den GANZEN Index, nicht auf die genannten Pfade; es hat unversionierte
Änderungen an `docs/erzeuger_kanaele.json` **still** verworfen. Wer eine
einzelne Datei aus dem Index zurückholen will, nennt sie:
`git checkout-index -f -- <pfad>`.

**3. Jede Prüfsumme über eine Beweisdatei wird gegen das git-Objekt
gebildet, nie gegen den Arbeitsbaum — und jeder neue Beweis-Ordner
bekommt seine `.gitattributes`-Zeile im selben Commit.**

Mit `core.autocrlf=true` sind die Bytes im Arbeitsbaum **nicht** die
Bytes im Blob. Eine Summe über den Arbeitsbaum ist damit eine Aussage
über die lokale Auscheckung, nicht über den Beleg. Gemessen MF-1094:
lokal grün, in CI fielen **alle 47** Hashes der Commodore-Beschreibungen.
Mechanisch gehalten von **H3** (hasht seit MF-1096 den Blob, nicht die
Datei) und **H6** (`git check-attr text` muss `unset` melden) in
`scripts/audit_repo_hygiene.py`, Selbsttest 18/18.

Die zweite Hälfte ist die wichtigere: H3 vergleicht Zahlen, H6 sorgt
dafür, dass es überhaupt **eine** Zahl gibt. Ohne `-text` hat derselbe
Beleg auf zwei Rechnern zwei Summen.

### Kontextdisziplin (MF-1359)

Eigentümerentscheidung vom 2026-09-26. Jede Zeile, die ein Werkzeug
zurückgibt, bleibt bis zum Sitzungsende im Gedächtnis — ein Agent, der
530 Testzeilen liest, um „5 rot" zu erfahren, hat 525 Zeilen zu viel.

- Dateien nie ganz lesen; grep/sed auf den Bereich, der gebraucht wird.
- Testläufe: nur Zusammenfassung (Failed/Passed) in den Kontext, Details
  in eine Datei.
- Nach jedem abgeschlossenen Punkt: `/compact`.
- Fan-out (viele Dateien durchsuchen) an einen Unteragenten; nur die
  Tabelle zurück.

Die stärkste Form ist ein Prüfskript, das **zählt**, statt einer Liste,
die der Agent liest (`check_consistency.py` ist das Vorbild).

**Zwei gemessene Fallen dabei:**

- `/doctor` erreicht in diesem Projekt nicht die eingebaute Prüfung,
  solange ein Skill gleichen Namens unter `.claude/skills/doctor/` liegt
  (gemessen 2026-09-26: ein lokaler, gitignorierter Mathe-Modellierungs-
  Skill). Die Stop-Hooks dann direkt zählen: Benutzer-`settings.json`,
  Projekt-`settings.json` und die `hooks/hooks.json` der aktivierten
  Plugins.
- Gezählt waren es **12** Stop-Hooks, davon lief der graft-Hook doppelt
  (Benutzer- UND Projekt-Einstellungen) und ecc brachte sieben. Plugin-Hooks
  schaltet man über die Mechanik des Plugins ab (ecc: `ECC_DISABLED_HOOKS`
  im `env` der Benutzereinstellungen), nicht durch Ändern im
  Plugin-Cache — den überschreibt die nächste Aktualisierung.

### Konfliktordnung: was gewinnt, wenn Teile sich widersprechen (MF-640)

1. **Messung vor Plan.** Ein Plan, den eine Messung widerlegt, wird
   geändert, nicht verteidigt. Vorgeführt am Fluss-Widget: der Plan sagte
   „neue Ansicht bauen", die Messung sagte „das Widget existiert und wird
   nur nicht instanziiert" — gebaut wurde die Verdrahtung (MF-630/632).
2. **Lizenz vor Fähigkeit.** Belegt an IPF: ein erreichbarer,
   funktionierender Parser wiegt eine ungeklärte Ableitung nicht auf
   (MF-638).
3. **Ehrlichkeit vor Vollständigkeit.** Die Registry-Zeile „nicht lesbar"
   schlägt 1100 Zeilen Können ohne Tür (MF-635).

### Der staerkste legale Kanal (MF-695)

„Lizenz vor Fähigkeit" heißt **nicht** „Fund verwerfen". Es heißt: *auf
welchem Weg*. Kein Fund verlässt die Pipeline ungenutzt — jeder bekommt
den stärksten Kanal, der legal offensteht:

| Kanal | wann | Beispiel im Baum |
|---|---|---|
| **Port** | Lizenz vereinbar, Attribution gesetzt | — |
| **Nachbau** | Verhalten belegt, Code gesperrt | Ordinal/Dewarp aus flux-analyze (GPL-3) |
| **Helfer-Prozess** | nicht einlinkbar, ausführbar | PFS3lib (BSD-4) |
| **Oracle** | Ausführung frei, Weitergabe nicht | `dtc` (proprietär), SHA-gepinnt |
| **Spec** | nur Doku lesbar | HxC-Formatbeschreibungen |
| **Daten/Fixture** | Abbild frei, Code nicht | Korpus-Zulieferungen |
| **Fundus** | heute kein Kanal | `ipf-flux`, bis die capsimg-Frage steht |

**Der Kanal *Spec* hat Stufen, und die stärkste ist die Firmware des
Laufwerks (MF-1179).** Fast jede Quelle dieses Baums ist ein **Leser** —
ein Werkzeug, das ein fremdes Abbild aufmacht und deutet. Eine
Laufwerks-Firmware ist der **Schreiber**: sie hat die Disketten
hergestellt, um die es geht. Sie wird nicht ausgeführt, also ist sie
kein Oracle; sie ist *Spec* — aber *Spec* mit dem Zeugen auf der
richtigen Seite.

Was eine solche Quelle registrierbar macht, ist ihr **Anker**. Eine
Spec-Quelle ohne Anker ist eine Behauptung. Bei der Atari 1050 Turbo
v3.5 ist der Anker ein Byte-für-Byte-Vergleich zwischen dem Atasm-Quell-
text und dem 1988 ausgelieferten ROM: derselbe md5
`35be2c58f1e0b04ab5a1f2459e5515bd`, **0 abweichende Byte**. Verzeichnis
und Kandidatenliste stehen in
[`docs/ORACLES.md` §Laufwerks-Firmware](docs/ORACLES.md).

**Und die Gattung entscheidet nicht, sie gewichtet.** Die gemessene 1050
Turbo ist ein *Nachrüst*-ROM, nicht Ataris eigenes. Wo sie UFTs
gerechnetem Modell widerspricht — Interleave 12 gegen 13 bei Enhanced
Density —, ist die Abweichung **festgenagelt, nicht aufgelöst** (P3-434);
die 12 einzusetzen wäre eine unbelegte Zahl gegen eine andere, also
genau das Verbot aus MF-1077. Eine starke Quelle ist ein Grund,
genauer hinzusehen, nie ein Grund, eine Zahl zu ändern.

**Fundus heißt benannt wartend, nicht verfallen.** Ein Fund ohne Kanal
wird eingetragen — mit dem, was ihn öffnen würde. Sonst ist „später"
dasselbe wie „nie".

Der Preis der anderen Lesart ist dreimal bezahlt: P0-5 hat ein Release
blockiert, die nibtools-Welle vier Dateien und Tage gekostet, die
IPF-Quarantäne eine ganze Fähigkeit. Eine Lizenzverletzung verbessert
das Werkzeug nicht — sie baut einen Defekt ein, den **kein Rotbeweis
fangen kann**, und trifft am Ende genau das, was dieses Projekt
herstellt: Vertrauenswürdigkeit.

**Ein Werkzeug kann gebaut, gelaufen UND trotzdem kein Oracle sein
(MF-1178).** Die Regel in `docs/ORACLES.md` lautet „Kein Oracle auf
Zusicherung — ein Werkzeug, das nicht gebaut und ausgeführt wurde, ist
kein Eintrag". Das ist eine **notwendige**, keine hinreichende Bedingung,
und der Fall dazu ist gemessen: `atr2imd` aus `jhallen/atari-tools`
übersetzt mit einem `gcc -O2`, läuft, und sein Rundlauf ATR → IMD → ATR
ist byteidentisch (**0 von 92 176** Byte abweichend). Seine
**Zwischendatei** ist trotzdem unbrauchbar — sie trägt keine
`IMD `-Kennung (sondern `ATR2IMD 1.0: <Datum>`) und jedes Datenbyte ist
komplementiert, was nur der eigene Partner zurücknimmt. Ein Hausformat
unter fremdem Namen; UFTs IMD-Leser sagt mit
`UFT_ERROR_FORMAT_INVALID` ab, und das ist richtig.

Die Lehre ist eine **dritte** Frage neben „läuft es" und „dieselbe Hand":
**kann ein Dritter lesen, was es schreibt?** Aus demselben Paket wurde
deshalb der Kanal *Spec* statt *Oracle* — seine `readme.md` belegt die
drei ATR-Größen mit Begründung (92 176 / 133 136 / **183 952**, letzteres
„− 384 because first three sectors are short"), und genau diese 384 Byte
sind die Art Wissen, an der ein Leser still scheitert. Festgenagelt in
`tests/test_atr_groessen_gegen_jhallen.c`; UFT liest alle drei richtig.
Der verworfene Differenzlauf steht als P3-433, die Interleave-Tafeln
desselben Werkzeugs als P3-432.

**Und die Bilanz will gemessen sein wie alles andere.** Beim
Festschreiben dieser Regel wurde die Zeile „cpmtools: 131 diskdefs per
Laufzeit-Parser, voller Nutzen, null Übernahme" nachgeprüft und trägt
nicht: `src/formats/cpm/uft_cpm_diskdefs.c` führt **55** Definitionen
**fest verdrahtet**, keinen Laufzeit-Parser, und die Kopfzeile nennt
cpmtools als Referenz, deren Lizenz ausdrücklich **nicht gemessen** ist
(`LIZ-1`, OPEN_ITEMS „Eine zweite Quelle, die niemand gemessen hat").
Das ist kein Erfolgsfall der Regel, sondern ein offener Fall unter ihr.


Diese drei Vorränge werden ohnehin praktiziert; ausgesprochen verhindern
sie, dass je zwei davon gegeneinander optimieren.

### Grundsatz: eine Attribution ist eine rechtliche Aussage (MF-636)

„Based on X" / „Port of X" im Kopfkommentar erklärt eine **Ableitung**,
keine Höflichkeit. Wer eine setzt, nennt die Lizenz der Quelle dazu; wer
eigenständig implementiert und nur fremde Doku gelesen hat, schreibt das
auch so („Verhalten nach der Dokumentation von X, eigenständige
Implementierung").

`scripts/audit_spdx_policy.py` führt diese Erklärungen seit MF-636 als
**Liste** neben der SPDX-Prüfung — bewusst kein Tor, denn eine
Attribution ist nichts Verbotenes, sondern etwas
Entscheidungsbedürftiges. Der erste Lauf fand **88** davon: 7
ausdrückliche Port-Erklärungen, 43 mit genannter fremder Codebasis
(davon **nur 2 mit genannter Lizenz**), 38 reine Spec-Verweise. Siehe
`LIZ-1` in `docs/OPEN_ITEMS.md`.

## Verzeichnisstruktur

```
include/uft/          — Alle öffentlichen C-Header
  analysis/            — OTDR, DeepRead, TDFC, Confidence
  core/                — Fehler, Typen
                         (BERICHTIGT MF-1174: hier stand zusätzlich
                          „Pfad-Sicherheit". Ein solches Modul gibt es im
                          ganzen Baum NICHT — gemessen über `git ls-files`
                          trägt genau EINE Datei „path" im Namen, und das
                          ist `tests/benchmarks/bench_decode_hotpath.c`.
                          Alle `uft_*path*`-Symbole sind Wandlungspfade
                          oder Verzeichnispfade INNERHALB eines Datei-
                          systems; Nutzer von `uft_path_safe` /
                          `uft_safe_fopen`: 0. Klasse MF-1064/MF-767.
                          Und die Zusage hat gewirkt: die Win98-FDB-
                          Zulieferung öffnet ihre Datei mit blankem
                          `fopen` — nicht aus Nachlässigkeit, sondern weil
                          das versprochene Modul fehlt. Ob es gebaut werden
                          soll, braucht einen gemessenen Anlass: welcher
                          Pfad kommt aus unvertrauenswürdiger Quelle?)
                         Seit MF-1173 liegt hier das Spurmodell
                         (`uft_track_layout.h`, Herkunft eines Sektors), seit
                         MF-1175 die **Anordnungsachse**
                         (`uft_sector_order.h`) — sechs Reihenfolgen, in denen
                         dieselben Sektoren in einer Abbilddatei liegen können,
                         als EINE Datenzeile je Fall statt als Rechnung in
                         jedem Plugin. `src/formats/xfd/uft_xfd.c` ist ihr
                         erster Produktionsaufrufer; `uft_track_layout` hat
                         weiterhin keinen (P3-422)
  encoding/            — Encoding Detection Boost
  flux/                — Flux Decoder, SCP Parser
  formats/             — Format-spezifische Header
  hal/                 — Hardware Abstraction
  protection/          — Kopierschutz-Typen
  recovery/            — Adaptive Decode, Multiread

src/
  analysis/deepread/   — 5 DeepRead Forensik-Module
  analysis/otdr/       — OTDR Core + Widget
  algorithms/          — God-Mode Decoder, Viterbi, Encoding
  core/                — Kernmodule (Multirev, MFM, Error)
  decoder/             — PLL, Sync, Multi-Rev Fusion
  formats/             — 84 Format-Plugins (138 IDs registriert) (nach System sortiert)
  fs/                  — Dateisystem-Implementierungen
  flux/                — KryoFlux, Flux Loader
  gui/                 — Qt6 Widgets (OTDR Panel, Sector Editor, etc.)
  hal/                 — HAL-Implementierungen pro Controller
  hardware_providers/  — Qt-basierte Hardware-Provider
  protection/          — Kopierschutz-Erkennung
  recovery/            — Recovery-Pipeline

tests/                 — 77 C-Tests + 1 Qt-Test
.github/workflows/     — CI, Sanitizer, Coverage
```

## Schlüssel-Metriken

- 715 Source-Dateien, 481 Header — Stand 2026-08-28 nach MF-626
  (fdc_bitstream entfernt). Die Vorgängerzahl „~693/~515" stammte vom
  2026-08-20 und war in beide Richtungen abgedriftet: Quellen zu
  niedrig, Header deutlich zu hoch. Nachzählen mit
  `find src -name '*.c' -o -name '*.cpp' | wc -l` und
  `find include -name '*.h' | wc -l`; früher MF-441
  (src/switch/ + src/cart7/ entfernt, 801 Dateien); davor MF-011 19-Welle
  Cleanup (785 dead-code Files / ~140k LOC entfernt, davon `src/fluxengine/`,
  `src/algorithms/{core,data,fluxio,imageio,tracks}`, `src/loaders/`,
  `src/filesystems/`, `src/encoding/`, plus 250+ einzelne orphan-Header)
- 138 Format-IDs, 137 Plugin-Definitionen (88 ausgeschrieben + 49 DSK-Makro;
  84 davon mit Registrar-Funktion, die niemand aufruft — MF-446; SSOT:
  `scripts/gen_format_list.py`), 44 Konvertierungspfade registriert /
  **18 angeboten**, davon **7 verlustfrei (je mit Messung)**
  (MF-541/567/655/1081), 20 Roundtrip-Matrix-Einträge (SSOT in
  `src/core/uft_roundtrip.c`;
  die Zahlen sind seit MF-541 abgeleitet, nicht gepflegt; MF-567 hat drei
  Urteile ohne Wandler entfernt)
- 6 Hardware-Controller — SCP-Direct M3.1 libusb wiring LANDED (MF-254,
  HW-bench UFT-008 pending); XUM1541 M3.2 libusb verdrahtet seit MF-301,
  offen ist die CBM-DOS-Kommandoebene (MF-650); Applesauce M3.3 weiterhin
  [~] partial scaffold (Pure-Utility + Lifecycle real, Serial-Wiring
  pending; siehe `docs/MASTER_PLAN.md` §M3)
- HAL-Tests grün: Greaseweazle (production) + 10 SCP-Direct + 16 XUM1541
  + 17 Applesauce = 43 Stub-Honesty-Asserts, 0 Failures
- 55+ Kopierschutz-Schemes **im Katalog** (`src/protection/`), davon
  erreichbar: Signal-Erkennung + 3 heuristisch benannte — MF-508
- 8 DeepRead-Module, davon **1 erreichbar** und **7 ohne Aufrufer**
  (MF-767, gemessen je Bezeichner über `git ls-files`). Erreichbar ist
  allein der **Encoding Boost** (`uft_otdr_detect_encoding`, gerufen in
  `src/gui/uft_otdr_panel.cpp:888`). Ohne Aufrufer: die fünf
  Forensik-Module in `src/analysis/deepread/` (13 exportierte
  Funktionen, 0 Nennungen außerhalb — MF-627) **plus Adaptive Decode
  und die float-gewichtete Fusion** — hier stand bis MF-767 „3
  erreichbar", was die frühere Berichtigung MF-627 mitgeschleppt hat.
  Dazu 12 OTDR-Pipeline-Stufen
- 9 SIMD-Dispatch-Punkte (SSE2/AVX2 Runtime)
- ~610 Error-Handling-Fixes (fseek + I/O)
- Thread-Safety: 3 Subsysteme mit Mutex
- Compiler-Hardening: stack-protector, FORTIFY_SOURCE, ASLR
- 27 Agent-Definitionen (`.claude/agents/`, alle auf claude-fable-5);
  neu seit v4.1.6: `uft-scout` — sichtet fremden Code, liefert nur
  Dokumente (`tools/uft-scout/`); `uft-variants` — belegt die Dialekte
  EINES bekannten Formats (`tools/uft-variants/`); `uft-innendienst` —
  misst den eigenen Baum auf Türen ohne Leser, Oracles ohne Eichung,
  Doku ohne Quelle (`tools/uft-innendienst/`, MF-693); `uft-nachbau` —
  bereitet den Clean-Room-Nachbau lizenzgesperrter Vorlagen vor
  (`tools/uft-nachbau/`, MF-696)
