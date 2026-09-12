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
> Plugins stehen **15 auf T3 — ungeprüft** (MF-654: `adl` und
> `adf_arc` auf T2; MF-690: `dim_atari` auf T1b, erstes fremd erzeugtes
> DIM im Korpus; MF-716: `do` auf T2, das erste Apple-Format —
> Differenzlauf gegen das Oracle `to_woz2`, 560 von 560 Sektoren
> byteidentisch): kein Test, oder ein
> synthetischer Test ohne Abgleich gegen eine autoritative Quelle. Genau
> in dieser Lage waren die fünf fabrizierten Parser grün
> (FMT-2/3/10/11/12). Belegt sind T1=2, T1b=41, T2=30 (MF-1034: `posix` von T3 auf **T1b** — **von den drei Spielarten war nur eine umgesetzt**. libdsks `drvposix.c` definiert `raw`/`rawalt` (SIDES_ALT), `rawoo` (SIDES_OUTOUT) und `rawob` (SIDES_OUTBACK), und `posix_offset()` schaltet darauf — mit einem Kommentar, der ausdruecklich gegen `logical` abgrenzt: „based on the sidedness of the disk image (not the sidedness of the geometry)“. UFT legte die Spuren **immer** linear ab; bei `acorn640` (80 x 2, OUTOUT) liegen damit **158 von 160** Spuren an anderer Stelle. **Der Versatz kommt dabei nicht aus einer zweiten Kopie der vier Gesetze**, sondern aus `uft_logical_track_index()`, das MF-1032 gegen libdsk abgenommen hat — eine zweite Kopie waere die Lage aus MF-1015 (drei Pruefsummen) und MF-1026 (drei Victor-Geometrien). Zweiter Befund: **die Attribution trug nicht.** Der Dateikopf nannte libdsks `drvposix.c` als „geprueft“, und die `.geom`-Nachbardatei kommt im **ganzen** libdsk-Baum nicht vor — sie ist UFTs eigene Konvention und wird jetzt so geführt; seit MF-1034 traegt sie auch die Anordnung, und damit ist sie genau der Kanal, den P3-337 fuer `logical` sucht. Dritter Befund: **ohne `.geom` wurde eine Geometrie erfunden** — Rueckfall 80x2x9x512 und `cylinders = (groesse / 4608) / koepfe`, womit **jede** Datei angenommen wurde; gemessen wurde eine 174 848 Byte grosse D64 als **18** x 2 x 9 x 512 gelesen und 8960 Byte fielen still weg. Vorgabe ist jetzt `require_geom = true`, und ein ausdruecklicher Rueckfall gilt nur, wenn er die Dateigroesse **restlos** erklaert. Vierter: der Schreiber konnte nur `alt`. **T1b, weil libdsk die Abbilder GESCHRIEBEN hat** (`-otype rawoo` und `-otype rawob`) — und beide sind **byteidentisch** mit den `logical`-Pruefdateien aus MF-1032, was genau die Abgrenzung belegt, die libdsk selbst benennt; MF-1033: **`qrst`, `myz80` und `nanowasp` von T2 auf T1b — ohne eine Zeile Codeaenderung.** Die drei Leser waren richtig; ihnen fehlte nur ein Abbild von fremder Hand, und der Grund, warum es keines gab, war ein falscher Eintrag: `P3-333` behauptete, libdsk erzeuge keine Fixtures, weil „sein `raw`-Treiber die Geometrie eines kopflosen Abbilds nicht bekommt“. Der Kanal dafuer ist **`-format <name>`**, und fuer Formate mit eigener `getgeom` braucht es ihn nicht einmal. Gemessen: `dsktrans -itype qrst -otype qrst` schreibt eine **5363 Byte** grosse QRST — nicht byteidentisch mit der Eingabe (156 679 Byte), weil sie durch libdsks **eigenen Packer** laeuft, und genau das macht sie zum Fremderzeugnis; UFT liest daraus **320 von 320** Sektoren byteidentisch. `-itype myz80 -otype myz80` schreibt die **volle** Datei mit **8 388 864 Byte** (256 Byte reservierter Bereich + 64 x 128 x 1024) — die Groesse, die das vorhandene Fixture bewusst nicht hatte —, und Zylinder 0 liefert **128 von 128** Sektoren byteidentisch; damit ist die volle Geometrie erstmals an einem Fremderzeugnis geprueft. Und `-itype nanowasp -otype nanowasp` schreibt eine Datei, die **byteidentisch** mit der vorhandenen ist (unveraenderte SHA-256): **zwei unabhaengige Schreiber, dieselben 409 600 Byte, alle 800 Sektoren an derselben Stelle** — deshalb wird dort kein Duplikat angelegt, die Gleichheit IST der Beleg. Dieselbe Gestalt wie MF-1024 (`kfx`): **der Leser war richtig, die Aussage ueber das Werkzeug war falsch.** „Das Werkzeug kann es nicht“ war eine Aussage ueber den benutzten Aufruf; MF-1032: `logical` von T3 auf **T1b** — und **UFT konnte keine echte Datei lesen, waehrend die Anordnung, die das Format AUSMACHT, gar nicht umgesetzt war**. Verlangt wurden eine Kennung `"LGD\0"` und ein **32-Byte-Kopf** mit Geometriefeldern; libdsks `logical_open()` prueft **nichts** — es gibt beides nicht, die Datei beginnt mit dem ersten Sektor. Fuenfter Fall dieser Klasse nach MF-961, MF-1022, MF-1029 und MF-1030. **Der zweite Befund war das ganze Format:** der Leser lief `for (c) for (h)` linear, also **immer SIDES_ALT**, womit „logical“ byteweise dasselbe war wie ein gewoehnliches flaches Abbild. Es gibt **vier** Gesetze (`dg_pt2lt()`): ALT `zyl*koepfe+kopf`, OUTOUT `kopf*zylinder+zyl`, OUTBACK mit **rueckwaerts laufendem Kopf 1** (`2*zylinder-1-zyl`), EXTSURFACE wie ALT — **dieselben vier, die dieser Baum zweimal einzeln wiederentdeckt hat** (MF-1027 brauchte OUTBACK fuer `v9t9`, MF-1030 OUTOUT fuer `nanowasp`). Dritter Befund: `if (first_sector == 0) first_sector = 1;` — libdsks Tafel fuehrt acorn160/320/640 mit `dg_secbase = 0`, also war dort **jede** Sektornummer um eins zu hoch und mit ihr der Versatz. Vierter: die Geometrie kam aus dem erfundenen Kopf; sie kommt jetzt von aussen, **weil die Dateigroesse sie nicht ersetzen kann — und das ist gemessen**: in libdsks eigener Geometrietafel teilt **jede** der acht nicht-ALT-Geometrien ihre Groesse mit mindestens einer ALT-Geometrie. Die Sonde stimmt deshalb nie zu und `open()` sagt ab, statt ein PC-720K-Abbild mit der OUTBACK-Anordnung zu lesen; der fehlende Kanal fuer eine benutzergesetzte Geometrie ist **P3-337**, dieselbe Lage wie `posix` seit MF-546. Fuenfter Befund: der Schreiber erzeugte das erfundene Format — 32 Byte Kopf in jede Datei, unlesbar fuer jede fremde Umsetzung; jetzt ist der Rundlauf byteidentisch. **Und die Stufe ist T1b, weil libdsk die Pruefdateien GESCHRIEBEN hat**: `dsktrans -itype raw -format acorn640 -otype logical` (und dasselbe mit `ibm720`, SIDES_OUTBACK, `dg_secbase = 1`) erzeugt sie aus einem flachen ALT-Abbild, byteidentisch mit der unabhaengig nachgerechneten Fassung. **Damit ist P3-333 berichtigt** — dort stand, libdsk erzeuge keine Fixtures; der fehlende Kanal war `-format <name>`, und den gibt es die ganze Zeit. Mutationsmatrix: **alle elf** Mutationen gefangen. Nebenbei berichtigt: `docs/ORACLES.md` sagte, libdsk habe keine Versionsabfrage — `dsktrans -version` antwortet „libdsk version 1.5.12“; MF-1031: `2img` von T3 auf T2 — **acht Befunde, und zwei davon trafen jede 3,5"-Datei**. Gemessen gegen MAMEs `src/lib/formats/ap_dsk35.cpp` (BSD-3-Clause, nur gelesen) und gegen hxcfes `APPLE2_2MG`-Modul (GPL-2, nur ausgefuehrt). **Erstens: die byte-vertauschte Kennung `"GMI2"` wurde abgewiesen.** MAME nimmt sie ausdruecklich an, mit **benanntem Erzeuger**: „Bernie ][ The Rescue wrote 2MGs with the signature byte-flipped, other fields are valid“. UFT verlangte `"2IMG"` in Sonde UND `open`; gemessen antwortete `open` **-25**. **Zweitens: die 3,5"-Geometrie war erfunden.** UFT rechnete fuer JEDE Datei `35 x 1 x 16 x 256` und leitete die Spurzahl aus `Datenlaenge / 4096` ab, gedeckelt auf 80. Eine 3,5"-Apple-Diskette hat **512-Byte-Sektoren** und eine **Zonentafel** `ns = 12 - Spur/16`, also 12/11/10/9/8. Gemessen an einer 819 264 Byte grossen Datei: **327 680 von 819 200 Byte erreichbar**, und Zylinder 17 lieferte einen Block von der **anderen Seite** („C05 H1 S04“ statt „C17 H0 S00“). **Sechs weitere Befunde:** eine byte-vertauschte Datenlaenge wurde nicht erkannt (MAME berichtigt sie); die Sonde verwarf die Dateigroesse (`(void)file_size`), womit MAMEs Struktur-Probe `Versatz + Laenge == Dateigroesse` gar nicht nachbaubar war — die Falle aus MF-1029, zum **fuenften** Mal; es gab **keine Tafel**, jede Datenlaenge wurde angenommen; ein Datenversatz unter 64 wurde **still** auf 64 gehoben; `p->cylinders` war ein `uint8_t`, und der Ueberlauf lag **vor** der Schranke, sodass eine 1-MB-2MG still eine 35-Spur-Diskette wurde; und die Schreibseite trug denselben Fehler (MF-931). **Der Ueberlauf hat dabei einen zweiten Fehler verdeckt:** die Datei mit byte-vertauschter Laenge las sich **richtig** — aus Versehen, weil `(uint8_t)(3 146 240 / 4096)` null ergibt und die Zeile danach 35 setzte. **Zwei unabhaengige Haende, und sie widersprechen sich zweimal — beide Male gewinnt MAME mit Begruendung:** hxcfe weist `"GMI2"` ab, **ohne Grund zu nennen**, und meldet fuer die 3,5"-Datei 133 Spuren x 12 x 256 (1024 Byte zu wenig) — es hat keine Zonentafel. MAMEs Tafel rechnet sich dagegen **selbst auf**: `16 x (12+11+10+9+8) x 512 = 409 600` und das Doppelte sind genau die zwei 3,5"-Laengen in seinem eigenen `s_formats[]`. **Und die Pruefdateien sind kein Selbstgespraech:** `hxcfe -conv:APPLE2_DO` gibt die 143 360 Nutzbytes der 5,25"-Datei **byteidentisch** zurueck. Mutationsmatrix: **alle zwoelf** isolierbaren Mutationen gefangen, und die dreizehnte ist **nachgemessen statt behauptet** — die obere Schranke in `read_track` und die in `uft_2img_track_offset` sind **gegenseitig redundant**, jede allein haelt die Zusage, nur beide entfernt faellt sie. **Am Rand:** zwei verwaiste 2MG-Leser im Baum tragen dieselbe Klasse — `uft_2mg_parser.c` behauptet „80 x 10 x 512“ und „80 x 20 x 512“ fuer die 3,5"-Groessen; beschriftet statt repariert (MF-699); MF-1030: `nanowasp` von T3 auf T2 — und **UFT konnte auch diese Dateien nicht lesen, aus demselben Grund wie `myz80` einen Schritt vorher**. Verlangt wurden eine **24 Byte lange Kennung** `"nanowasp floppy image\r\n\032"` und ein **80-Byte-Kopf** mit Geometriefeldern; libdsks `nwasp_open()` prueft **nichts** — es gibt keine Kennung und keinen Kopf, die Datei beginnt mit dem ersten Sektor. Das ist die Klasse von MF-961 (`86f`/`"86BX"`), MF-1022 (`sap`/`"SAP"`) und MF-1029 (`myz80`/`"MYZ80 "`) — zum **vierten** Mal, dreimal davon in dieser Runde. **Und haette die Kennung gestimmt, waere es doppelt falsch gewesen:** 80 Byte Sektordaten waeren als Kopf verworfen worden, und die Anordnung ist nicht linear. Sie ist **kopf-dur** (SIDES_OUTOUT — erst die ganze Seite 0, dann Seite 1, „though the MicroBee actually writes them in SIDES_ALT order“), **und innerhalb der Spur liegen die Sektoren geskewt**: `skew[10] = {1,4,7,0,3,6,9,2,5,8}`, wobei `skew[s-1]` der physische Platz des logischen Sektors `s` ist — der physische Platz 0 traegt also den logischen Sektor **4**. libdsk nennt seine eigene Behandlung davon ausdruecklich „an abuse of libdsk ... cpmtools doesn't support the type of skewing done by the microbee (**sector 1 doesn't map to sector 1**)“. Geometrie fest 40 x 2 x 10 x 512 = 409600 Byte mit `dg_secbase = 1`; UFT nahm 80 Zylinder an. **Eine Zusage haelt ausdruecklich die Lehre aus MF-1029 fest:** dort war ein Groessenrueckfall toter Code, weil die Sonde die Dateigroesse verwarf und gegen die **Puffergroesse** verglich (4096 Byte). Bei NanoWasp waere derselbe Fehler das ganze Format, weil die Groesse die **einzige** pruefbare Eigenschaft ist — also nimmt die Sonde `file_size` als eigenes Argument, und eine Gegenprobe weist einen 409600 Byte grossen Puffer mit Dateigroesse 4096 ab. **Und das Fixture ist kein Selbstgespraech:** libdsks `dskid` meldet 40/2/10/512 mit „First sector: 1“, und `dsktrans -itype nanowasp -otype raw` liefert 409600 Byte, in denen **alle 800 Sektoren in logischer Reihenfolge** stehen — was Skew und Anordnung zugleich prueft, denn nur wenn beide stimmen, kommt die Diskette sortiert heraus. Mutationsmatrix: **alle neun** Mutationen gefangen, und eine Verdrehung der Skew-Tafel um EINE Stelle genuegt. Konfidenz **40** („nur die Groesse“) und nicht hoeher, weil 409600 Byte auch die Groesse einer Apple-800K-Diskette ist; MF-1029: `myz80` von T3 auf T2 — und **UFT konnte keine einzige MYZ80-Datei lesen, aus drei unabhaengigen Gruenden**. **Erstens: die Kennung, nach der es suchte, steht in keiner Datei.** Der Baum beschrieb eine `myz80_header_t` mit `magic[6] = "MYZ80 "`, `version`, `flags`, Geometriefeldern, `label[32]`, `comment[64]` und 142 Byte Polsterung — nichts davon existiert. Die ersten 256 Byte einer echten Datei sind **durchgehend `0xE5`**, und libdsks `myz80_open()` verlangt woertlich, dass **jedes einzelne** davon `0xE5` ist: der reservierte Bereich IST die Erkennung. Klasse von MF-961 (`86f`/`"86BX"`) und MF-1022 (`sap`/`"SAP"`) — zum **dritten** Mal. **Zweitens: die Geometrie war erfunden** — angenommen 77 x 2 x 26 x 128, wirklich **64 x 1 x 128 x 1024** mit `dg_secbase = 0`, also **0-basierten** Sektornummern; eine volle Datei ist 8388864 Byte gross, und der Versatz ist `131072*Zylinder + 1024*Sektor + 256`. **Drittens, und das hat erst die Mutationsmatrix gezeigt: der Groessenrueckfall war toter Code.** Die Sonde nahm ersatzweise 256256 und 1025024 Byte an — aber `myz80_probe_plugin()` verwirft `file_size` (`(void)file_size`) und gibt nur die **Puffergroesse** weiter, und die ist 4096 Byte. Der Vergleich traf nie zu. Bemerkt hat es eine Mutation, die zuerst **durchrutschte**, weil meine eigene Gegenprobe die Dateigroesse als drittes Argument uebergab statt einen wirklich 256256 Byte grossen Puffer. **Eine Regel der Vorlage wird bewusst nur zur Haelfte uebernommen:** libdsk sagt, kurze Dateien seien gueltig und fehlende Sektoren gaelten als `0xE5` („it is not an error to try to read a missing sector“). UFT liefert die Bytes so, **kennzeichnet sie aber als fehlend** — „das Format sagt 0xE5“ und „hier wurde 0xE5 gelesen“ sind zwei Aussagen (MF-980). **Und das Fixture ist kein Selbstgespraech:** libdsks `dskid` meldet 64/1/128/1024 mit „First sector: 0“, und `dsktrans -itype myz80 -otype raw` stellt die Diskette wieder her — Zylinder 0 byteidentisch (131072 Byte, kein Unterschied) und die Zylinder 1..63 zu 100 % `0xE5`, womit Versatzformel und Kurzdatei-Regel von fremder Hand bestaetigt sind. Die Pruefdatei traegt absichtlich nur EINEN Zylinder (131328 statt 8388864 Byte). Mutationsmatrix: **alle zehn** Mutationen gefangen. Konfidenz **70** und nicht hoeher, weil 256 gleiche Bytes eine Konvention sind und keine Signatur (MF-729: Band „Struktur gelesen“); MF-1028: `qrst` von T3 auf T2 — und **der ganze Aufbau war erfunden**. UFT konnte keine einzige echte QRST-Datei lesen, und sein Schreiber erzeugte ein Format, das es nicht gibt. Sieben Abweichungen gegen `doc/qrst.html` (John Elliott, **LGPL-2+**, aus dem libdsk-Klon; **nur gelesen**, Kanal *Spec*): der Kopf ist **796** Byte, nicht 22; die Kennung ist **fuenf** Byte (`'QRST',0`), nicht vier — gemessen nahm die Sonde auch `"QRSTX"` mit Konfidenz 95 an; die **Geometrie steht nicht im Kopf**, sondern folgt aus dem **Kapazitaetskode** bei 0x0C (und auch ein Kode 8 wurde angenommen); der Spursatz ist **drei** Byte und das dritte ist der **Typ**, nicht acht Byte mit `data_size`; es gibt **drei** Spurarten (roh, **leer mit Fuellbyte**, gepackt) statt zwei; und die **Pruefsumme fehlte ganz**, obwohl sie in der Datei steht. **Die Packung ist woertlich die Klasse aus MF-1009:** wirklich wechseln sich ein Literal-Lauf (`<len>` + Bytes) und ein Wiederhol-Lauf (`<len>` + ein Byte) ab; hier stand ein Strom, in dem `0x00` ein Wiederhol-Tripel einleitete — und wie bei `apridisk` war der Rundlauftest gruen, weil Packer und Entpacker Spiegelbilder derselben Erfindung waren. **Und er konnte den Defekt nicht einmal bewachen, weil er nie gebaut wurde:** `tests/test_libdsk_formats.c` steht in `EXCLUDED_TESTS`, und der Grund ist jetzt gemessen — `uft_disk_image_t` ist **zweimal** definiert (P3-336). Das ist nicht „gruener Test bewacht Defekt“, sondern die Klasse aus MF-1000/Tor 64: ein Test, der nicht rot werden **kann**. **Die Abnahme ist ein Beleg am Objekt:** die Pruefsumme (Summe `byte*(1+Versatz)` ueber die Diskette) steht in der Datei, wird ueber die GELESENEN Sektoren nachgerechnet und geht auf — dieselbe Art Beweis wie MF-869 und MF-1013. **Und das Fixture ist kein Selbstgespraech:** libdsks aus dem Klon gebaute Werkzeuge lesen es (`dskid` meldet 40/1/8/512 samt Beschreibung und Etikett) und stellen die Diskette byteweise wieder her (163840 Byte, kein Unterschied) — alle drei Spursatz-Arten gehen also durch einen fremden Entpacker; genau das fehlte MF-1009. Mutationsmatrix **neun von zehn**, und die zehnte ist benannt statt verschwiegen: „unbekannte Spurarten stillschweigend annehmen“ laesst sich **nicht isolieren**, weil eine unbekannte Satzart ihre Nutzlast unbeansprucht laesst und der Satzlauf danach ohnehin scheitert. **Nebenbei registriert:** libdsk ist jetzt ein Oracle (`docs/ORACLES.md`), und der Bau gehoert aufgeschrieben, weil er nicht der dokumentierte ist — `make` heisst hier `mingw32-make.exe` in der Qt-Toolchain, `./configure` laeuft, `make` scheitert an einem unquotierten `C:/Program Files/...`, und uebersetzt wird direkt mit gcc (70 Bibliotheksdateien, ohne `tools/dskutil.c`, mit `-lz`); MF-1027: `v9t9` von T3 auf T2 — und **auf Seite 1 laeuft die Spurzahl rueckwaerts**. MAMEs `ti99_dsk.cpp` (LGPL-2.1+, Michael Zapf; **nur gelesen**, Kanal *Spec*) dokumentiert es woertlich: „all tracks on side 0 as going inwards, and then all tracks on side 1 going outwards — 00 01 ... 38 39 / 79 78 ... 41 40“. Die Datei ist also **kopf-dur**, und innerhalb der Seite 1 ist die Reihenfolge **umgekehrt**. UFT rechnete `(cyl * heads + head) * spt * 256` — zylinder-verschraenkt UND ohne Umkehrung, zwei unabhaengige Abweichungen. Gemessen: bei einseitigen Dateien **0 von 40** falsch, bei zweiseitigen **78 von 80**, bis **359424 Byte** daneben. **Warum es nie aufgefallen ist:** bei `heads == 1` geht die alte Formel in die richtige ueber, und zweiseitig trafen genau zwei Spuren — (0,0) und, rein zufaellig, (1,26), weil `2t+1 = 79-t` bei t = 26 aufgeht. Kopf 1 Spur 0 ist die aeusserste Spur der zweiten Seite und liegt bei Versatz 182016; UFT suchte sie an Position 2304. Vier weitere Befunde: **vier von sieben** Dateigroessen aus MAMEs `identify()` wurden abgewiesen (die beiden 80-Spur-Formate ohnehin unerreichbar, weil die Spurzahl fest auf 40 stand); die **Fehlsektorkarte** (+768 Byte, von MAME toleriert) wurde abgewiesen; die **VIB** in Sektor 0 wurde nie gelesen, obwohl sie Sektoren je Spur, Spuren je Seite und Seitenzahl nennt; und es gab keine obere Schranke — was hier schwerer wiegt, weil die Umkehrung `2*cyl_max - cyl - 1` aus einem zu grossen Zylinder einen **negativen** Versatz machen kann. **Und die VIB entscheidet einen echten Fall:** 184320 Byte sind **zweideutig** (SSDD 1x40x18 gegen DSSD 2x40x9), MAMEs Kommentar sagt das woertlich — UFT nahm immer DSSD, also MAMEs Rueckfall, auch wenn die Datei selbst SSDD sagt. **Eine Abweichung vom Orakel ist bewusst und festgenagelt:** MAME folgt der VIB auch bei Widerspruch zur Dateigroesse und warnt nur; hier gewinnt die Groesse, weil eine Geometrie, die nicht in die Datei passt, hinter das Dateiende liest. Mutationsmatrix: **alle neun** Mutationen gefangen; MF-1026: **drei Formate auf einmal von T3 auf T2** — `victor9k`, `tan` und `fdi_pc98`, jedes im Feldabgleich gegen MAMEs eigene Formatklasse (BSD-3-Clause, aus `neue-ideen/FORMATS.ZIP`; gelesen, nicht uebernommen). **`victor9k` konnte KEINE echte zweiseitige Datei oeffnen:** Victor 9000 hat neun Geschwindigkeitszonen und **zwei verschiedene** Zonentafeln, eine je Kopf — Kopf 1 hat **1167** statt 1224 Sektoren, die Datei ist also 1224192 Byte gross und nicht 1253376, und `open` antwortete gemessen **-25**. Dazu las Kopf 1 mit der Tafel von Kopf 0 (57 von 80 Spuren falsche Sektorzahl, **79 von 80** falscher Versatz, bei Spur 79 um 28672 Byte). **Und der dritte Befund ist die teuerste Gestalt dieses Baums:** auf Kopf 0 lagen zwei Zonengrenzen um eins daneben (Spur 48: 15 statt 14, Spur 70: 12 statt 13) — weil sich +1 und -1 **aufheben**, blieb die Summe 1224, die Groessenpruefung konnte es nie bemerken, und **22 Spuren (49..70) wurden 512 Byte zu hoch gelesen**. Spur 48 gab einen 15. Sektor aus, der Spur 49 gehoert; Spur 70 lieferte ihren 13. nie. **`tan` trug woertlich die zwei Befunde, die MF-1016 an `jv1` behoben hat** — die erfundene zweite Seite (204800 Byte wurden 40 Zylinder / 2 Koepfe statt 80/1) und die Sektor-IDs 1..10 statt 0..9 (800 von 800 falsch). Das ist die Gestalt von MF-519/MF-529, nur zwischen zwei **Dateien** statt zwei Funktionen: eine Korrektur an einer Datei sagt nichts ueber ihre Nachbarn. **`fdi_pc98` dagegen war richtig** — Kopffelder, beide Konsistenzbedingungen aus MAMEs `identify()`, Versatzformel und 1-basierte Sektornummern stimmen ueberein; Uebereinstimmung reicht nur bewacht (MF-1006), also nagelt der Test jede Zahl fest. **Die Mutationsmatrix hat dabei einen Fehler in meiner eigenen Gegenprobe gefunden:** die Pruefung „psize passt nicht zur Geometrie“ war gruen aus dem falschen Grund, weil ein zu grosses `psize` schon die ZWEITE Bedingung reisst — dieselbe Falle wie MF-1014; isoliert wird sie jetzt ueber die Geometrie, bei stimmender Dateigroesse. 13 von 13 Mutationen gefangen. **Am Rand gefunden:** im Baum lagen **drei** Victor-Geometrien; die dritte in `src/formats/victor/victor9k.c` (verwaist) nennt 1285 Sektoren — eine Groesse, die keine Victor-Diskette hat; beschriftet statt repariert (MF-699); MF-1024: `kfx` von T3 auf T1b, **ohne eine Zeile Codeaenderung** — der Leser war richtig, ihm fehlte nur ein Abbild von fremder Hand. hxcfes `KRYOFLUXSTREAM`-Modul schreibt je Spur eine eigene Datei in der kanonischen Benennung `trackNN.S.raw`; zwei davon liegen jetzt im Korpus. Die Zusicherung ist **flussgerecht**, weil ein KryoFlux-Strom eine Spur je Datei ist und keine Sektoren liefert: `uft_kfc_stream_is_valid()` (MF-919) laeuft die OOB-Kette ab und prueft die in den Bloecken **eingebettete** Stromposition gegen die eigene Zaehlung — gemessen je Strom **10 OOB-Bloecke und 3 Indexmarken**. Und die Gegenprobe gehoert dazu, weil `kfx`s Sonde genau daran einmal gescheitert ist: 64 KB Pseudozufall, ein Strom mit EINEM gekippten `0x0D` (OOB 7 statt 10) und eine IMD-Datei fallen alle durch — MF-919 hatte gemessen, dass die alte Sonde `0x0D`-Bytes ZAEHLTE und in 512 Zufallsbytes nie „nein“ sagen konnte; MF-1022: **`sap_thomson` von T3 auf T1b, und UFT hat vorher NIE eine echte SAP-Datei gelesen.** Sonde und `open` verlangten die Zeichenfolge `"SAP"` bei Versatz 0 — dort steht in Wirklichkeit das **Formatbyte**, und ab Versatz 1 die Signatur `"SYSTEME D'ARCHIVAGE PUKALL S.A.P. …"`. Gemessen an einem Erzeugnis von `libsap`: `open` lieferte **-25**. Dieselbe Klasse wie MF-787 (`sad`/„SAD!“) und MF-961 (`86f`/„86BX“), samt „Read: SUPPORTED“ in der Merkmalstafel. Vier weitere Befunde: die Formatwerte 0x00/0x01 gab es nicht (die Tafel kennt 1 und 2, mit 80×256 bzw. 40×128), die Spurzahl war auf 80 fest verdrahtet; die Sektordaten wurden **nicht entschluesselt** (SAP verschleiert jedes Byte mit XOR 0xB3); die Pruefsumme war **dreifach** falsch (falscher Algorithmus, verschleierte Eingabe, ohne die vier Kopfbytes) und konnte nie zustimmen; und der Fuellsektor galt als GUTER Sektor mit gueltiger CRC, weil `uft_format_mark_last_missing()` fehlte — was **Tor 62 nicht sehen konnte**, weil das `memset` vor der Schleife stand (P3-328). **Die Pruefsumme ist aus dem VERHALTEN abgeleitet, nicht aus dem Quelltext uebernommen:** libsap rechnet mit einer eigenen Tafel, und eine Tafel abzuschreiben waere eine GPL-2-Uebernahme; der Parametersatz (gespiegeltes CCITT 0x8408, Start 0xFFFF, Spanne = 4 Kopfbytes + Klardaten) wurde an sechs Sektoren gesucht und eindeutig gefunden. Abgenommen: **16 von 16** Pruefsummen der Spur 0 gehen auf; MF-1021: `st` von T2 auf T1b, an einem hxcfe-Erzeugnis aus einer UFT-eigenen 720K-Eingabe (1440 Mustertreffer nachgewiesen). **Der eigentliche Fund ist ein leeres Fixture, das Erfolg meldete:** `v9t9` sollte mitkommen, hxcfe erzeugte 184 320 Byte — richtige Groesse, gemeldeter Erfolg —, und die Datei war **100 % 0xF6**, null Mustertreffer. Sein RAW-Lader erkennt die TI-Geometrie nicht und sein Layout-Verzeichnis hat keinen TI-Eintrag, also fuellte der Schreiber mit dem Formatier-Fuellbyte. UFT lag richtig; **ohne das Selbstbeschreibungs-Muster in der Eingabe waere `v9t9` auf einer Fuellbyte-Diskette gehoben worden** — sein erster Test ueberhaupt, und wertlos. Daraus die Regel: ein erzeugtes Fixture ist erst dann ein Beleg, wenn sein INHALT nachgewiesen ist; gepackte Container (`mfi`, `ipf`) tragen deshalb keine Stufe (P3-326/P3-327); MF-1020: **sechs Formate auf einmal von T2 auf T1b** — `imd`, `jv3`, `dmk`, `d88`, `stx` und `dsk_cpc`, jedes an einem Abbild, das eine **fremde Hand** erzeugt hat. Der Grund, warum das vorher nicht ging, war eine falsche Annahme: es schien kein Orakel-Binary verfuegbar. **Es lag im Baum** — `tools/uft-scout/work/HxCFloppyEmulator/build/hxcfe.exe` ist gebaut, laeuft (v2.16.15.2, Klon 05b53aa), und seine Modulliste nennt fuer viele Formate `RW`. Damit ist der Kanal **Oracle** nach MF-695 offen, ohne eine Zeile fremden Codes: das Werkzeug wird ausgefuehrt, nicht portiert. Gemessen war die Lage vorher **59 von 88 Formaten ohne jedes Korpus-Abbild**. Die Eingabe ist UFT-eigen und **benennt sich selbst** (je Sektor Spur, Sektornummer und „UFT-K“) — damit sagt ein Leseergebnis nicht nur, DASS etwas kam, sondern ob die RICHTIGE Stelle getroffen war. **Sechs von sieben Lesern waren beim ersten Lauf richtig**; der siebte, `mfi`, gab den **zlib-Kopf `78 9C`** als Spurdaten aus, obwohl sein eigener Dateikopf sagt „Track data is zlib-compressed“ — das Wissen stand im Kommentar und nicht im Code (P3-326); MF-1018: `vdk` von T3 auf T2 — die Kopfgroesse lag in einem `uint8_t` und nahm einen LE16-Wert auf. Bei einem Kopf von exakt 0x0100 Byte wurde daraus **0**, und gemessen lieferte Sektor 1 die Bytes `64 6B`: **`'d' 'k'`, die eigene Kennung der Datei als Sektordaten**, dazu 244 Byte Kopf-Fuellwerk in den Sektoren. MAME liest `uint8_t header[0x100]`, bevor es die Kopfgroesse benutzt — ein 256-Byte-Kopf ist im Format vorgesehen. Zweitens erfand `if (p->tracks == 0) p->tracks = 35;` eine Geometrie, die die Datei verneint. Drittens wurde das Kompressionsbyte ignoriert; **hier ist MAME selbst stumm**, abgesagt wird trotzdem, weil gepackte Daten als Sektoren gelesen erfundene Daten sind. **Und was gemessen wurde und stimmt:** die Sektor-IDs 1..18 — `add_sector`s `+1` trifft hier MAMEs `FIRST_SECTOR_ID = 1`, anders als bei `jv1`; geprueft und nicht angefasst; MF-1017: `jv3` von T3 auf T2 — und die ersten zwei von fuenf Befunden bedeuteten zusammen, dass **jeder Sektor jeder JV3-Datei falsche Bytes lieferte**. `JV3_HEADER_SIZE` war `0x2300`; wirklich sind es `2901*3 + 1 = 0x2200`, denn das Schreibschutz-Byte ist das LETZTE Byte des Kopfbereichs (0x21FF) und es gibt keine Polsterung. Und der Verzeichnislauf brach beim ersten freien Satz ab, obwohl MAME ausdruecklich sagt „These can be intermixed with valid descriptors“ — freie Saetze belegen Datenraum und ihre Groesse verrechnet sich mit **2** statt mit 1. Gemessen an einer nach MAMEs Arithmetik gebauten Datei: 2 von 3 Sektoren, und der erste lieferte die Bytes des zweiten. Dazu drei weitere Befunde: der Freimarker ist die SPUR allein, nicht Spur UND Sektor; `if (sec_num > 0) sec_num--` liess die IDs 0 und 1 beide auf 1 fallen; und Dichte (Bit 7) sowie DAM-Kode (Bits 5-6) standen in falschen Bits — `flags & 0x40` als „deleted“ trifft die 0xF9, nicht die 0xF8. **Und ein zweiter gruener Test hat einen Defekt bewacht:** `test_register_all_formats.c` schrieb `0x2300` fest. Das ist der zweite Fall in derselben Runde nach MF-1016 — die Tests, die je ein Loch geschlossen haben, hielten das Nachbarloch fuer richtig; MF-1016: `jv1` von T3 auf T2 — der Leser nahm bei mehr als 40 Spuren eine **zweite Seite** an, die JV1 nicht hat. Gemessen an einer Datei von 204 800 Byte (80×10×256): gemeldet wurden 40 Zylinder / 2 Koepfe, und Zylinder 40 war nicht lesbar — die Spuren 40..79 lagen auf Kopf 1. Tim Manns Formatbeschreibung sagt woertlich „numbered 0 through 9, and only one side“, MAMEs Tafel fuehrt 35/40/80 Spuren mit durchweg `head_count = 1`. Zweiter Befund: die Sektornummern waren **1..10 statt 0..9**, weil `uft_format_add_sector()` laut eigenem Kopf einen 0-basierten Index nimmt und 1 addiert — der Kommentar dort nennt Apple, Amiga und Commodore als Faelle, fuer die das falsch ist, und TRS-80 gehoert dazu. **Und ein gruener Test hat den Fehler bewacht:** `test_plugin_probe_real.c` verlangte „41..79 tracks are only valid when even (two heads)“ — eine Zusage, die aus der erfundenen zweiten Seite folgte; berichtigt, wobei genau eine Zeile fiel. Dieselbe Gestalt wie MF-992; MF-1015: `udi` von T3 auf T2 — und der Befund war, dass **jede realistisch grosse UDI-Datei abgewiesen wurde**. Das Kopfkommentar des Plugins beschrieb einen Dateikopf **ohne** das 4-Byte-Groessenfeld, und `udi_open()` las Zylinder- und Kopfzahl deshalb aus `data[5]`/`data[6]` — den Bytes 1 und 2 der little-endian **Dateigroesse**. Gemessen an einer gueltigen UDI von 1 125 620 Byte (80×2×6250): `data[6] = 17`, und die Schranke `max_head > 1` verwarf die Datei mit **-25**. Klasse MF-961, samt „Read: SUPPORTED“ in der Merkmalstafel. Zweiter Befund: die Taktmarken folgen **jedem** zerlegbaren Spurtyp, der Versatzlauf sprang nur bei MFM darueber — hinter einer FM-Spur lag alles Weitere um 782 Byte daneben (MF-794). Dritter Befund: **drei Pruefsummen im Baum, keine zwei gleich**; richtig ist die Fassung des Urhebers (Invertierung je Byte), sie lag in `uft_udi.c`, das niemand ruft, waehrend das Plugin das gewoehnliche CRC-32 in **jede von UFT geschriebene UDI** schrieb. **Hier wurde das Orakel begruendet ueberstimmt:** `src/samdisk/udi.cpp` rechnet mit `int32_t`, sein `crc >> 1` ist ein arithmetischer Shift, und gegen den Referenzcode des Urhebers gehalten faellt es — ein Orakel ist eine Referenz, kein Beweis; MF-1014: `scl` von T3 auf T2 — SCL ist kein Abbild, sondern ein **Archiv**, und der Leser legte die **Dateidaten dorthin, wo der Katalog steht**. Gemessen am Vorzustand: 1 Zylinder, 1 Kopf, **5** Sektoren gesamt (die Orakel sagen 80 / 2 / 2560), Byte 0 der ersten Spur war der erste Datenbyte einer Datei statt eines Katalogeintrags, und Kopf 1 — wo die Daten hingehoeren — wurde abgewiesen. Es gab keinen Katalog, keinen Infosatz, und jeder Sektor lag eine ganze Spur zu frueh. Neu gefasst gegen **zwei unabhaengige Haende**, deren neun Feldlagen im TR-DOS-Infosatz byteweise uebereinstimmen (SAMdisk MIT im Baum, HxC GPL-2 nur gelesen); ein drittes Mal bestaetigt sie der eigene Baum, weil `uft_trd.c` die Dateizahl bei `0x8E4` liest. **An einer Stelle folgt UFT den Orakeln bewusst nicht:** den Diskettennamen traegt die SCL nicht — SAMdisk setzt dort den Quelldateinamen, HxC schreibt „HxCFE“, beides erfunden; hier bleibt das Feld leer. Zweiter Befund derselben Familie: `uft_scl_parse()` las Eintragsbyte **12** nie — das obere Byte der Laengenangabe —, und `uft_scl_build()` nullte es mit einem Kommentar, der die falsche Bedeutung nannte; MF-1009: apridisk von T3 auf T2 -- und dieser Abgleich war der schwerste: UFT konnte KEINE echte APRIDISK-Datei lesen, aus drei unabhaengigen Gruenden. Die Satztypen hatten die obere Haelfte 0xE31D verloren UND Sektor und Kommentar vertauscht; compression/header_size wurden als 32 statt 16 Bit gelesen, womit alles danach zwei Byte falsch lag; und die Sektorfelder wurden aus einem eigenen 8-Byte-Deskriptor geholt, der in Wirklichkeit die data_size-Bytes sind. Gemessen an einer nach MAMEs load() gebauten Pruefdatei: 1 Spur, 0 Sektoren, und UFT_OK -- Erfolg ohne Tat. Dazu ein erfundenes Kompressionsformat (ein Byte-Strom statt drei Byte Laenge plus Fuellbyte), dessen Rundlauftest gruen war, weil Packer und Entpacker Spiegelbilder waren -- und dessen Datei ohnehin in EXCLUDED_TESTS steht, also nie gebaut wurde. Vierter Fund am Rand: track->sector_count blieb 0, weil uft_track_alloc() nur Kapazitaet anlegt. Zugleich der vierte der elf aus MF-930, dessen Dateischreiber verdrahtet wurde -- und der erste, bei dem der Schreiber dafuer erst REPARIERT werden musste, weil er dasselbe erfundene Format erzeugte; MF-1006: mgt von T3 auf T2 — Feldabgleich gegen MAMEs coupedsk.cpp, und er fand KEINEN Fehler: Geometrie, Sektor-IDs und Spurversatz stimmen ueberein. Uebereinstimmung reicht nur, wenn sie bewacht ist statt behauptet, also nagelt tests/test_mgt_gegen_mame.c jede Zahl fest, Mutationsmatrix 3 von 3. Das ersetzt zugleich den Blocker aus P3-312, der den fehlenden src/samdisk/SAMCoupe.h nannte, und berichtigt dessen Kernaussage: MAME rechnet dieselbe Anordnung wie UFT und kennt die IMG-Variante ebenfalls nicht; MF-1004: `cfi` von T3 auf T2 — Feldabgleich gegen `src/samdisk/cfi.cpp`, sieben Abweichungen; sechs davon dieselbe Klasse: das Orakel bricht ab, UFT kuerzte still, und ein Spurblock der Laenge 0 verwarf den **Rest der Datei**. Die siebte kam nicht aus dem Abgleich, sondern aus dem Rundlauf der Schreibseite: `uft_cfi_read_mem()` wies jede Datei unter 512 Byte ab, waehrend der eigene Schreiber aus 9216 Byte Nutzlast eine **39 Byte** grosse, einwandfreie Datei erzeugte — CFI komprimiert, und `ReadCFI()` hat keine solche Konstante. **Ein Feldabgleich vergleicht Verhalten; eine Konstante, die das Orakel gar nicht hat, faellt dabei nicht auf.** Gleichzeitig ist `cfi` der zweite der elf aus MF-930, dessen Dateischreiber verdrahtet wurde (P3-204, nach `opus`/MF-931); MF-961: `86f` von T3 auf T2 — der Leser probte auf `"86BX"`, ein Magic, das in **keiner** echten 86F-Datei steht; er nahm einen 32-Byte-Kopf an, erfand eine 12-Byte-Spurtabelle und wies damit **jede** echte Datei ab, während seine Merkmalstafel „Read: SUPPORTED" und „Flux: SUPPORTED" meldete. MF-707/708 hatten das gemessen und ausdrücklich NICHT einzeilig berichtigt — mit richtigem Magic hätte er die Dateien angenommen und falsch zerlegt. Neu gefasst gegen die Spezifikation des **Urhebers** (86Box/docs `dev/formats/86f.rst`), ohne eine Zeile fremden Quellcodes; abgenommen an einer echten 360K-Diskette: 7 von 7 Spuren, 63 von 63 Sektoren byteweise gegen eine unabhängig nachgerechnete Formel. **T2 und nicht T1b**, weil die Prüfdatei nach Messung sehr wahrscheinlich von fluxfox stammt und nicht vom kanonischen 86Box — eine fremde Hand, aber nicht die richtige; MF-905: `opus` von T3 auf T2 — das Plugin las EINE fest verdrahtete Geometrie und wies jede doppelseitige Opus-Diskette **still** ab, obwohl der Bootsektor Zylinder, Sektoren, Kopfzahl und Sektorgröße selbst trägt; das Oracle lag im **eigenen Baum** und bestätigt sich doppelt, weil `ReadOPD()` und `WriteOPD()` in `src/samdisk/opd.cpp` denselben Bytespiegel auswerten; MF-806: `msa` von T2 auf T1b — und die Hand-Frage entschied die RICHTUNG: UFTs Leser ist gegen `src/samdisk/msa.cpp` geschrieben, also musste **hxcfe erzeugen** und SAMdisk zurücklesen, nicht umgekehrt; MF-796: `edsk` — erstes Format mit Container-Kopf im Korpus, SAMdisk erzeugt und hxcfe liest byteidentisch zurück; dabei fiel auf, dass `uft_edsk.c` die Typen des Parsers **von Hand nachdeklariert** hatte, 40 gegen 32 Byte je Sektor — das Plugin meldete „9 Sektoren" und lieferte für **jede Spur jeder EDSK-Datei** keinen einzigen, still, mit `UFT_OK`; MF-794: `sad` rechnete zylinder-dur statt kopf-dur und las **158 von 160 Spuren an der falschen Stelle** — bei einem Format, das bereits auf T1b stand; MF-787: `sad` — und dabei fiel auf, dass das Plugin nach der Kennung `"SAD!"` suchte, die in **keiner** SAD-Datei steht; echt ist `"Aley's disk backup"`, und die Offsets waren aus demselben Grund um 14 Byte verschoben; MF-784: `pdp`, `img`, `t1k`, `sam` und `jvc` — zweite Runde gegen `gw`; dabei **zwei Fehlpaarungen gefangen**: `v9t9` und `cpm` haben dieselbe Dateigröße wie ein gw-Format, aber eine andere Sektoraufteilung — Größengleichheit ist keine Geometriegleichheit; MF-783: `micropolis`, `northstar`, `ssd`, `po` und `trd` — fünf auf einen Streich, jedes gegen `gw`s eigenes Geometriemodell, alle fünf Rundläufe byteidentisch; MF-782: `msx_disk`, das vorher **keinen einzigen Test** hatte — Abbild von `gw` 1.23 unter `msx.2dd`, zwei unabhängige Hände über dieselbe Geometrie; MF-690: `dim_atari`; MF-716: `do` — das erste Apple-Format, 560 von 560 Sektoren byteidentisch gegen das Oracle `to_woz2`; MF-722: `d13`, das vorher **keinen einzigen Test** hatte — 454 von 454 byteidentisch, der 455. benannt statt geraten). Die Liste unten
> nennt, was **gelesen werden soll**, nicht was **geprüft ist** — pro
> Format: [`docs/VERIFICATION_TIERS.md`](docs/VERIFICATION_TIERS.md).

> **Ehrlichkeits-Hinweis (MF-883) — „schreibt" traf für neun Formate nicht
> zu, und sie sagten das Gegenteil.** 86F, CP/M, CQM, DCM, DMS, IMD, MSA,
> SAP und SCL änderten beim Schreiben ihre **Speicherkopie**, meldeten
> `UFT_OK` und gaben sie beim Schließen frei. Gemessen: in keiner der neun
> Dateien steht eine Schreiboperation (`fwrite`/`fputc`/`fprintf`/
> `ftruncate`/`WriteFile`), keine hat ein `.flush` — und `plugin->flush`
> wird im **ganzen Baum von niemandem gerufen**, `uft_disk_close()` ruft nur
> `close`. Selbst ein Plugin mit Flush käme nicht durch.
>
> Alle neun beanspruchten zugleich `{ "Write", SUPPORTED }` und
> `UFT_FORMAT_CAP_WRITE`; für SCL kam eine **vierte** Zusage hinzu, die
> MF-880 noch nicht kannte (`src/policy/uft_write_gate.c`). Der Rotbeweis
> war eine einzige Ergänzung: der seit Jahren grüne Test
> `test_imd_write_roundtrip` schrieb und las **denselben Speicherpuffer** —
> mit `close()` und Neu-Öffnen dazwischen fiel er sofort.
>
> Seit MF-883 antworten alle neun `UFT_ERROR_NOT_SUPPORTED`, und
> **Tor 57** (`scripts/audit_schreibzusage.py`, Grundlinie 0) hält die
> Klasse fest. Was das Tor bewusst **nicht** sieht, steht in seinem Kopf und
> als P3-154/P3-157 in `docs/OPEN_ITEMS.md`.
>
> **Nachtrag MF-930 — es waren nicht neun, sondern zwanzig, und der
> Torkopf hat den Rest verdeckt.** Tor 57 fragt, ob in der Plugin-Datei
> eine Schreiboperation **steht**. Sein Kopf nannte die Lücke ehrlich und
> **zählte acht Verdächtige auf**. Gemessen sind es **elf** — `apridisk`,
> `cfi`, `hardsector`, `logical`, `mgt`, `myz80`, `nanowasp`, `opus`,
> `posix`, `qrst`, `rcpmfs` —, und **vier davon standen auf keiner Liste**.
> Jede dieser Dateien enthält ein vollständiges `uft_<fmt>_write()` mit
> `fwrite`; nur führt **kein Weg** dorthin: kein `.flush`, `close()` gibt
> den Puffer frei, `write_track` macht ein `memcpy` und meldet `UFT_OK`.
> Bei `apridisk` stand der Rückweg wörtlich im Quelltext — *„Call
> flush/close to persist changes"* — und es gab ihn nicht.
>
> Das ist der **vierzehnte** Fall von Aufzählung statt Messung in diesem
> Baum, und der teuerste denkbare Ort dafür: die Aufzählung stand im Kopf
> eines **Tores** und beschrieb dessen Grenze.
>
> Seit MF-930 antworten auch diese elf `UFT_ERROR_NOT_SUPPORTED`, und
> Tor 57 trägt eine **zweite Messung**: erreicht `flush`/`close`/
> `write_track` den Schreiber — transitiv innerhalb der Datei, damit ein
> Helfer dazwischen kein Fehlalarm ist. **11 gegen den Vorzustand, 0
> heute**, Selbsttest 10/10. Über **Dateigrenzen** wird nicht verfolgt;
> das steht benannt im Torkopf. Die fertigen, unverdrahteten Schreiber
> sind **P3-204** — Arbeit, kein Fehler.
>
> **MF-931 — der erste ist gegangen, und der Weg ist damit vorgeführt
> statt beschrieben.** `opus` schreibt jetzt bis in die Datei (noch zehn).
> Vier Regeln, die für alle übrigen gelten: **nicht über `close()`** (das
> ist `void` — ein dort scheiternder Schreibvorgang wäre eine *stille
> Veränderung*); **keine eigene Versatzrechnung**, sondern den vorhandenen
> geprüften `uft_<fmt>_write()` rufen; **`disk->path` prüfen** und ohne
> Ziel *absagen* statt zu lügen; und **die Schreibseite gegen die
> Leseseite halten** — bei `opus` stand dort noch `head != 0` und
> `track_data[cyl]`, was MF-905 auf der Leseseite längst behoben hatte.
> Das ist der dritte Fall von „Leseseite geholt, Schreibseite übersehen“
> nach MF-519/MF-529; in diesem Baum ist die Schreibseite **systematisch
> die ältere**.
>
> Die Liste unten nennt, was **gelesen** werden soll. Ob ein Format auch
> **geschrieben** wird, sagt seine Merkmalstafel — nicht diese Überschrift.

Liest/schreibt Disk-Images von praktisch jedem 8-Bit- und 16-Bit-Computer:
- **Commodore:** D64, D71, D81, G64, T64, CRT, PRG, P00
- **Apple:** DO, PO, WOZ (v1/v2/2.1), A2R, MOOF, 2MG, NIB, DC42
- **Atari:** ATR, ATX, ST, STX, MSA, DCM, XFD
- **IBM PC:** IMG, IMA, IMD, TD0, DMK, CQM
- **Amstrad/Spectrum:** DSK, EDSK, TRD, SCL, MGT, TAP, TZX
- **BBC/Acorn:** SSD, DSD, ADF, UEF
- **Flux-Formate:** SCP, HFE (v1/v2/v3), KryoFlux RAW
- **Japanisch:** D88, D77, NFD, HDM, XDF, DIM, FDX
- Plus: MSX, Thomson, TI-99, Roland, HP LIF, CP/M, Micropolis, Victor, Zilog, etc.

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

### 3. Format-Konvertierung (44 Pfade registriert, **14 angeboten**)

> **Ehrlichkeits-Hinweis (MF-526, Zahlen neu gemessen MF-541):** die
> Wandlungstabelle fuehrt **44** Paare. Die Rundlauf-Matrix hat **16**
> Eintraege; **14** davon werden angeboten (zwei stehen als UNMOEGLICH):
>
> * **6 verlustfrei (je mit Messung)** — D64→D64, ADF→ADF, D64→G64,
>   IMG→HFE, **ATR→XFD**, **XFD→ATR**. Jedes einzelne mit einer
>   Bit-Identitaets-Messung im Baum (MF-532/533/539/655), keines auf
>   Zusicherung. Die beiden Atari-Paare sind seit MF-655 die ersten
>   ihrer Familie in der Matrix: XFD ist das ATR ohne seinen
>   16-Byte-Kopf (`atr[16:] == xfd`, byteweise am Korpus-Paar
>   gemessen), und die Grenze steht dabei — ein ATR mit Sektorgroesse
>   256 wird ohne `accept_data_loss` abgelehnt, weil XFD die Angabe
>   nicht speichern kann und die Dateigroesse sie nicht verraet.
> * **8 nur mit ausdruecklichem `accept_data_loss`.**
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
> Die Matrix hat **14** Eintraege. Es waren 17; die drei ohne Wandler
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

> **Ehrlichkeits-Hinweis (MF-864) — „MFM/FM/GCR Codec" stimmte beim FM
> zur Hälfte.** Der Flusspfad hat fünf Dekoder. Vier legten Sektoren an;
> `flux_decode_fm()` nicht. Er war über den Verteiler
> `flux_decode_track()` **erreichbar**, suchte Syncs, zählte sie — und
> sein Sektorteil bestand aus zwei Kommentaren:
>
> ```c
> /* FM sector decoding would go here - similar to MFM */
> /* For now, just note we found a sync */
> ```
>
> Gemessen über den ganzen Rumpf: keine Zeile schrieb `track->sectors[…]`
> oder erhöhte `sector_count`. Jede FM-Diskette über den Flusspfad —
> **Atari 810/1050, TRS-80 SD, IBM 3740** — lieferte null Sektoren.
>
> Seit MF-864 liest er. Was dabei die halbe Arbeit war, gehört dazu
> gesagt: der Baum hat **keinen FM-Encoder**, also gibt es keine hauseigene Möglichkeit,
> eine Prüfspur zu erzeugen. Ein Fixture aus derselben Hand wie der
> Dekoder wäre wertlos gewesen. Die Spur ist deshalb an **vier** Stellen
> von `fluxtoimd` (Eric Smith 2016, GPL-3-only) abgenommen — ausgeführt,
> nicht übernommen —, das Layout stammt aus **ECMA 54 / ISO 5654 / ANSI
> X3.73**, und die Prüfsummen erzeugt die fremde Klasse, während **UFTs
> eigene** `flux_crc16_ccitt()` sie nachrechnet.
>
> **BERICHTIGUNG MF-938 — hier stand „keinen FM- und keinen
> MFM-Encoder (an vier Stellen als Blocker vermerkt)". Die zweite Hälfte
> trug nicht.** Einen **IBM-MFM-Encoder gibt es**:
> `src/core/uft_mfm_encoder.c`, seit **2026-04-18** (`7087a565`), mit
> IDAM/DAM, A1-Sync `0x4489`, CRC-16-CCITT und Gaps. Er wird in
> **Produktion** gerufen — `src/formats/udi/uft_udi_plugin.c:215` und
> `src/formats/uft_format_convert_bitstream.c:978` (IMG→HFE) — und hat
> mit `tests/test_mfm_encoder_decodes_back.c` einen Rundlauftest.
>
> Was **wirklich** fehlt, ist der **AmigaDOS**-MFM-Encoder. Genau so
> stand es in MF-539: *„für AmigaDOS gibt es in diesem Baum keinen
> MFM-Encoder"*. Der Satz hier hat die Einschränkung verloren — **elf
> Tage** nach MF-539, dessen Commit-Titel *„der richtige MFM-Encoder lag
> die ganze Zeit daneben"* lautet. Und die Belegstelle trug nicht:
> `uft_kfx.c:199` handelt von fabrizierten Sektoren aus rohem Fluss
> (MF-919), nicht von einem fehlenden Encoder.
>
> Für den FM-Weg ändert das nichts — dort ist die Aussage richtig, und
> die Fremdabnahme durch `fluxtoimd` war der richtige Weg. Aber es ist
> derselbe Vorgang wie in MF-930: **eine Behauptung wurde weitergetragen
> statt nachgemessen**, und sie stand am Ende auf der Titelseite.
>
> **Nachtrag MF-869 — inzwischen an einer echten Aufnahme belegt.** Hier
> stand: „das Verhalten an einer echten Aufnahme ist nicht belegt, im
> Korpus liegt kein FM-Flussabzug". Beides hat sich geändert. Seit der
> Korpus-Beschaffung liegt eine **Applesauce-Aufnahme einer 8-Zoll-
> Diskette von 1979** darin, und seit MF-868 lässt sie sich öffnen —
> der A2R-Leser war bis dahin eine WOZ-Kopie, die `OK` zu nichts
> meldete.
>
> Gemessen an Zylinder 0 dieser Aufnahme:
>
> | | |
> |---|---|
> | Zellenzeit | aus dem **Intervall-Histogramm** (~2125 ns), nicht angenommen |
> | dekodiert | **32 Sektoren**, ID-CRC falsch **0**, Daten-CRC falsch **0** |
> | gegen die IMD | **26 von 26 byteidentisch** |
>
> Die CRCs stehen auf der Diskette selbst, 1979 geschrieben. Die IMD
> stammt aus demselben Objekt und wurde von **Applesauces eigenem**
> Dekoder erzeugt — zwei unabhängige Umsetzungen, dieselbe Diskette,
> dieselben 3328 Bytes. Festgehalten als
> `tests/test_fm_echte_aufnahme.c`; ohne Korpus überspringt er sich
> benannt.

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
nicht befragbar, lässt der Filter alles durch **und sagt es** — eine
stille Lücke wäre schlimmer als ein paar Fremdbefunde mit Hinweis.

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
| **Befund** | **8** offene Zeilen | [`docs/QUARANTINE.md`](docs/QUARANTINE.md) | auditiert, Weg festgelegt oder ausstehend |

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

**Fundus heißt benannt wartend, nicht verfallen.** Ein Fund ohne Kanal
wird eingetragen — mit dem, was ihn öffnen würde. Sonst ist „später"
dasselbe wie „nie".

Der Preis der anderen Lesart ist dreimal bezahlt: P0-5 hat ein Release
blockiert, die nibtools-Welle vier Dateien und Tage gekostet, die
IPF-Quarantäne eine ganze Fähigkeit. Eine Lizenzverletzung verbessert
das Werkzeug nicht — sie baut einen Defekt ein, den **kein Rotbeweis
fangen kann**, und trifft am Ende genau das, was dieses Projekt
herstellt: Vertrauenswürdigkeit.

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
  core/                — Fehler, Typen, Pfad-Sicherheit
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
  **14 angeboten**, davon **6 verlustfrei (je mit Messung)**
  (MF-541/567/655), 16 Roundtrip-Matrix-Einträge (SSOT in
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

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
