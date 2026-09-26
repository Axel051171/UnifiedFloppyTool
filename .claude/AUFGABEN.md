# Arbeitsliste

**Ein Posten ist in Arbeit. Alle anderen warten.**

Gepflegt vom Skill `.claude/skills/aufgabe/SKILL.md` (`/aufgabe`).
Aufnahme startet keine Arbeit. Abschluss verlangt einen Beleg.

**Diese Liste hält Aufträge, keine Befunde.** Ein Befund gehört nach
[`docs/OPEN_ITEMS.md`](../docs/OPEN_ITEMS.md) — seit MF-588 die einzige
Liste dafür, „neue Befunde kommen dorthin, nirgendwo sonst". Hier steht
nur, **was wann bearbeitet wird**, und der Posten verweist auf seine
`P?-NNN`, statt sie zu wiederholen.

Nichts wird gelöscht: Erledigtes wandert nach unten, Verworfenes bekommt
eine Zeile `Zurückgenommen:` mit Grund. Nummern (`A-001`, `A-002`, …)
laufen fort und werden nie wiederverwendet.

> **Warum die Zählung bei `A-004` beginnt.** `A-001` bis `A-003` wurden am
> 2026-09-16 bei der Abnahme des Skills selbst verbraucht (Paarlauf mit
> zwei Unteragenten, Sitzungsprotokoll). Sie stehen in keiner Fassung
> dieser Datei, und wiederverwendet werden sie nach der Regel oben nicht.

---

## In Arbeit

*(höchstens einer — leer heißt: nichts läuft)*

> **Stand 2026-09-26 (Aufraeumen auf Anweisung „die Liste aufräumen").**
> Laufender Posten ist **`A-032`** allein — so steht es in seiner eigenen
> Statuszeile („auf ausdrückliche Anweisung … anfangen !!"). `A-028` stand
> daneben ebenfalls auf `in Arbeit` und wartet seit 2026-09-17 nur auf den
> öffentlichen Wegwerf-PR; er ist jetzt **angehalten**, nicht erledigt.
> `A-017` (erledigt 2026-09-16) stand noch in diesem Abschnitt, und
> **vierzehn** erledigte Begutachtungen (`A-005` bis `A-016`, `A-022`,
> `A-024`) standen noch in der Warteschlange; alle fünfzehn sind nach
> `Erledigt` gewandert, und wo der Beleg nur ein Gutachten nannte, steht
> jetzt der Commit dazu (gemessen, nicht erinnert). Neu aufgenommen:
> `A-035` (DTC-Upgrade). Gelöscht wurde nichts.
>
> **Stand 2026-09-19 (MF-1258): die Regel hält wieder, und sie brauchte
> keine Entscheidung — nur eine Messung.** Laufender Posten ist **`A-028`**
> allein (Teilstring-Prüfer in der GitHub-Prüfung); er hängt an **einer**
> Zeile seiner Fertig-Bedingung, die ein öffentliches Artefakt braucht und
> deshalb beim Eigentümer liegt: ein Wegwerf-Pull-Request mit gepflanzter
> Falle, damit die Zeilen-Anmerkung einmal wirklich erscheint. Alles übrige
> daran ist belegt. `A-031` ist mit MF-1256 abgeschlossen, `A-030` mit
> MF-1258 nachgetragen; oberster der Warteschlange ist **`A-032`**.
>
> **ZURÜCKGENOMMEN, und der Satz bleibt stehen, weil er zeigt, wie knapp
> ich an einer unnötigen Rückfrage vorbeigelaufen bin.** Hier stand am
> selben Tag: *„die Regel ist verletzt. Zwei Posten tragen `in Arbeit` —
> `A-028` … und `A-030` (Punkt 1 fertig, Punkt 2 mit widerlegter Annahme,
> Punkt 3 blockiert). Beide sind wirklich unfertig; welcher von beiden der
> laufende ist, kann ich nicht aus der Liste ableiten — das ist eine
> Eigentümerentscheidung."* **Falsch war die Prämisse, nicht der Schluss.**
> `A-030`s Statuszeile war veraltet: seine eigene Beleg-Zeile nennt **alle
> drei** Punkte mit Commit, und alle sechs Hashes stehen gemessen auf
> `origin/main`. Ich habe die Statuszeile gelesen und nicht den Beleg
> darunter — dieselbe Klasse wie `tor_kann_nicht_falsch_anschlagen`:
> **nennt eine Zeile Namen, lies sie nach.** Was wie eine
> Eigentümerentscheidung aussah, war eine ungemessene Behauptung in der
> Liste selbst.

### A-032 · `neue-ideen/` vollständig sichten: was vergessen wurde, was den Code verbessert
- **Status:** **in Arbeit** (seit 2026-09-19, auf ausdrückliche
  Anweisung: „Danach weiter mit A-032 (neue-ideen/, 2408 Dateien) —
  oberster der Warteschlange, anfangen !!"). `A-028` steht ebenfalls
  auf `in Arbeit`, wartet aber seit 2026-09-17 auf eine
  **Eigentümerentscheidung** (öffentlicher Wegwerf-PR) und blockiert
  nichts — das ist hier vermerkt statt verschwiegen, weil zwei Posten
  auf `in Arbeit` gegen die Ein-Schloss-Regel verstoßen.
  · **Aufgenommen:** 2026-09-19
- **Wortlaut:** „`C:\Users\Axel\Github\UnifiedFloppyTool-4.1.0\neue-ideen`
  gehe hier alles gründlich , schau ab was vergessen wurde, ob man den
  code noch verbessern kann, nimm alles aus einander , meine
  einwilligung hst du für alles"
- **Gegenstand, gemessen (2026-09-19):** **2408 Dateien**, **636 MB**,
  davon ~~**197 Archive** auf der obersten Ebene und **20** bereits
  entpackte Verzeichnisse~~. **BERICHTIGT beim Beginn, gemessen:** die
  oberste Ebene trägt **216** Einträge — **108** Archive, **19**
  Verzeichnisse, **70** eigene `.md`-Ausarbeitungen, dazu 19 einzelne
  Quell-/Dokumentdateien. Die „197" trifft weder die oberste Ebene
  noch das Ganze: im **gesamten** Baum von `neue-ideen/` sind es
  **424** Archive. Die Zahl stand ungemessen in meinem eigenen
  Aufnahmeeintrag; sie bleibt oben durchgestrichen stehen, weil
  Entfernen keine Behebung ist. **2408 Dateien und 636 MB halten**
  (nachgezählt: 2408 Dateien, 661,7 MB inklusive der obersten
  Dokumente). `neue-ideen/` ist gitignoriert
  (`.gitignore:90`) und wird aus dem Baum **108 Mal in 37 Dateien**
  zitiert — es ist also Arbeitsmaterial mit Belegfunktion, kein Rest.
  `docs/OPEN_ITEMS.md` nennt es **23 Mal**.
- **Kennzahl:** gemischt, und das gehört ausgesprochen. Der einzelne
  Fund bewegt meist **keine** der vier und ist damit nach MF-640
  *Fundus, nicht Auftrag*. Bewegen kann er **ungeprüfte Formate (T3)**
  — nämlich dann, wenn sich darin ein **Erzeuger** oder ein **Oracle**
  findet, das eine Stufe hebt; und **angebotene Wandlungspfade**, wenn
  eine Richtung dadurch belegbar wird. Die Sichtung selbst ist Fundus;
  ihr Ertrag ist es nicht zwangsläufig.
- **Kanal:** **je Fund einer**, nach MF-695 — Port | Nachbau |
  Helfer-Prozess | Oracle | Spec | Daten/Fixture | Fundus. „Lizenz vor
  Fähigkeit" heißt nicht „Fund verwerfen", sondern *auf welchem Weg*,
  und der Weg wird bei der Aufnahme des Funds benannt, nicht wenn der
  Code schon dasteht.
- **Einfrier-Regel:** berührt den Format-/Decoder-Layer — **ja**, also
  **Rotbeweis zuerst**. Dazu das Moratorium: ein **neues
  Format-Plugin** ist auch als *Vorschlag* gesperrt; erlaubt sind
  Bugfixes an Bestehendem, Verifikations-/Korpusarbeit und
  Spec-Korrekturen gegen autoritative Quellen. Ein Fund, der ein neues
  Format nahelegt, wird als Fund notiert und **nicht** gebaut.
- **OPEN_ITEMS:** verwandt und **nicht abzuschreiben** — `P3-8`
  (drei Aminet-Pakete, Lizenzurteil offen), `P3-11` (Lizenz-Klasse
  statt Einzelurteil, Eigentümer-Vorlage), `P3-19` (`COPY130.M65`,
  Fundus), `P3-30` (`pyRT11`, Fundus), `P3-99` (`dskx` widerlegt).
  Neue Befunde bekommen dort eine Nummer; dieser Posten verweist nur.
- **Zwei Sperren, die im Gegenstand liegen und NICHT übergangen werden
  dürfen** — beide gemessen vorhanden:
  · `neue-ideen/x50conv.exe` — seine Lizenz **untersagt
    Disassemblierung ausdrücklich**. Aus ihm stammt nichts als die
    mitgelieferte Dokumentation. „Nimm alles auseinander" gilt hier
    **nicht**; das ist keine Auslegungsfrage, sondern die Lizenz.
  · `neue-ideen/OmniFlop_3.2d_Format_Harvest_C.zip` (239 Formate) —
    **nicht in den Baum**, solange die Eigentümerentscheidung zum
    EU-Datenbankherstellerrecht (§§87a ff. UrhG) aussteht. Dazu
    `neue-ideen/UFT-NN_OmniFlop_Analyse.md`.
  · **Die Einwilligung „für alles" deckt Arbeitsschritte, nicht
    Lizenzen.** Eine Lizenzverletzung baut einen Defekt ein, den kein
    Rotbeweis fangen kann (MF-695).
- **Fertig heißt:** **jeder** Eintrag der obersten Ebene von
  `neue-ideen/` trägt ein festgehaltenes Urteil aus vier Feldern —
  *Lizenz (an der Datei gemessen, nicht am API-Feld)* · *Kanal nach
  MF-695* · *Kennzahl oder „Fundus"* · *nächster Griff* —, und dieses
  Register ist **abgeleitet, nicht gepflegt**: ein Skript liest die
  Verzeichnisebene und meldet, was noch ohne Urteil ist. Solange auch
  nur ein Eintrag ohne Urteil dasteht, ist der Posten offen.
- **Aufwand:** **nicht schätzbar** für das Ganze — 2408 Dateien in 197
  Archiven, und die Lizenzfrage ist je Paket eine eigene Messung.
  Schätzbar ist erst die erste Scheibe, und die wird beim Beginn
  benannt, nicht hier.
- **Vorgehen, das beim Beginn gilt** (damit es nicht später als
  Einschränkung erscheint):
  · **Das Register zuerst, dann die Inhalte.** Ohne abgeleitetes
    Register ist „alles gesichtet" eine Behauptung — 197 Archive kann
    niemand im Kopf halten, und eine gepflegte Liste veraltet still
    (dieser Baum hat das fünfmal gemessen).
  · **Lizenz an der DATEI messen.** `measurement_hit_wrong_class`: das
    API-Feld von GitHub/GitLab meldet die Projekteinstellung, nicht
    die Lizenzdatei — `fdtc` galt als „ohne Lizenz" und trägt BSD-3.
  · **Vor jedem Eintrag „steht das schon irgendwo?"** — der Baum
    zitiert `neue-ideen/` bereits 108 Mal; ein zweiter Eintrag
    derselben Sache wäre die Doppelhaltung, gegen die K4 steht.
  · Höchstens **fünf** neue `OPEN_ITEMS`-Vorschläge je Durchgang, wie
    bei den Aufklärungs-Agenten — sonst füllt sich das Register mit
    allem, was auffällt.
- **Stand 2026-09-19, erster Durchgang — die Gestalt ist gemessen, und
  sie ist NICHT „216 Archive aufmachen":**
  · **Die 70 `.md` sind kein Material, sondern die Spur der
    bisherigen Sichtung** — eine durchnummerierte Reihe UFT-29 bis
    UFT-105, 64 Dokumente auf 62 Nummern (UFT-30 und UFT-32 doppelt
    vergeben), mit **15 Lücken**. Damit bekommt „was vergessen wurde"
    eine prüfbare Form statt einer Willenserklärung.
  · ~~**Fünf Lücken liegen in `~/Downloads`, nicht in `neue-ideen/`:**
    UFT-34, UFT-35, UFT-41, UFT-42, UFT-50. Das ist buchstäblich „was
    vergessen wurde".~~ **BERICHTIGT noch vor dem Push (MF-1267) —
    die Aussage war in ihrem schärfsten Teil falsch, und der Fehler
    war meiner.** Meine Reihen-Messung las `glob("UFT-*.md")`, also
    **nur die oberste Ebene**. Rekursiv liegen **vier der fünf längst
    in `neue-ideen/`**, verschachtelt in ihren eigenen
    Materialordnern; die Downloads-Fassungen sind **byteidentisch**
    (SHA-256 je Paar gleich) — Dubletten, keine Funde. **Wirklich
    vergessen war genau EINE:** `UFT-50-Korg-TSeries.md`, jetzt
    kopiert; `~/Downloads` blieb unangetastet, und die vier
    Dubletten, die ich dabei selbst angelegt hatte, sind wieder
    entfernt. Klasse MF-1024/MF-1033: **der Leser war richtig, die
    Aussage über den Gegenstand war zu weit** — „nicht auf der
    obersten Ebene" ist keine Aussage über „nicht vorhanden".
  · **Rekursiv stimmen auch die übrigen Zahlen anders:** 70 Dokumente
    auf **67** Nummern (nicht 62), **drei** doppelt vergebene Nummern
    statt zwei — neu ist **UFT-33**, das als
    `dcopy/UFT-33-DCopy-Bootblock-Identifikation.md` neben dem
    `UFT-33-HAL-Erweiterungen.md` der obersten Ebene liegt. Lücken
    sind **zehn**, nicht fünfzehn: 36–40, 44, 45, 47, 48, 98 — für
    diese zehn wurde in `neue-ideen/` (rekursiv), `~/Downloads`,
    `docs/` und `tools/` gesucht und nichts gefunden.
  · **Ein Ertrag bleibt, und er ist vorbeugend:** `UFT-42` berichtigt
    `UFT-33` ausdrücklich, dessen `uft_bb_entry_t` dort „eine
    plausible Rekonstruktion ohne Beleg" heißt. Gemessen gibt es
    `uft_bb_entry_t` im ganzen Baum **nicht**, und
    `docs/nachbau/XCOPY_VERHALTEN_HAND-A.md` nennt Bootblock nur als
    Abgrenzung — die Berichtigung verhindert etwas, sie repariert
    nichts.
  · **Gelesen, mit Urteil:** `UFT-50` beschreibt ein **eigenständiges**
    Korg-T-Serien-Format (80×2×10×1024 = 1 638 400 Byte) neben dem
    vorhandenen DSS-1 (80×2×5×1024 = 819 200) — Quelle `korgutils
    0.9.1`, GPLv2. Ein **neues Format-Plugin** fällt unter das
    Moratorium der EINFRIER-REGEL, auch als Vorschlag; also notiert,
    nicht gebaut. `UFT-41` sagt ausdrücklich „kein Bug gefunden,
    UFTs ATR-Modul hält stand" — ein **negatives** Ergebnis, das
    festzuhalten sich lohnt, damit es niemand ein zweites Mal
    erarbeitet.
  · **Die Gegenprobe hat den zweiten Befund halbiert, und das gehört
    dazu:** 9 Ausarbeitungen zitiert der Baum unter ihrer **Nummer**
    nirgends — nach **Inhalt** gesucht kennt er UFT-94 (COPYLOCK 80),
    UFT-99 (NIBtools 64 / RapidLok 65) und UFT-95/96 (GEOS 43) sehr
    wohl. Wirklich unberührt sind **vier bis fünf**: UFT-102, UFT-103,
    UFT-101, UFT-105, am Rand UFT-97. „0 gefunden" ist keine
    Entwarnung, sondern die Frage, was die Messung nicht sieht.
  · **Eine Zahl ist ausdrücklich eine GRENZE, keine Zahl:** von 216
    Einträgen nennt der Baum 71 — davon 9 nur über einen Namen, der
    als Suchwort nichts taugt (`1`, `disk`, `copy`, `.claude`).
    Unerwähnt sind also **mindestens 145**, höchstens 154. (Seit der
    Kopie von `UFT-50` sind es **217** Einträge; das Register führt
    entsprechend 215 ohne Urteil statt 214.)
  · **Zwei Zahlen des eigenen Aufnahmeeintrags hielten nicht** (197
    Archive, 20 Verzeichnisse) — berichtigt oben, durchgestrichen
    stehen gelassen.
  · **Festgehalten als `P3-512`** (berichtigt) und **`P3-513`** (der
    Korg-Fund).
- **Stand 2026-09-19, zweiter Durchgang — das Register steht, und die
  ersten Urteile sind GEERNTET statt neu erfunden:**
  · `scripts/gen_neue_ideen_register.py` + `docs/neue_ideen_urteile.json`
    → `docs/NEUE_IDEEN_REGISTER.md`. Es meldet die Differenz in **beide**
    Richtungen (Eintrag ohne Urteil **und** Urteil ohne Eintrag) und
    sagt bei fehlendem Verzeichnis „Umfang nicht feststellbar" statt
    „alles beurteilt". Selbsttest 9/9, zwei Mutationen gefangen.
    **Kein Tor** — ein Tor, das seinen Gegenstand in CI gar nicht
    sehen kann, wäre eines, das nicht anschlagen KANN.
  · **Geerntet, nicht erfunden:** von 217 Einträgen tragen **9** ihr
    Lizenzurteil längst in `docs/ORACLES.md` bzw.
    `docs/QUARANTINE.md`; sie sind mit Verweis eingetragen, nicht
    abgeschrieben (K4: keine Doppelhaltung). Dazu `x50conv.exe`,
    `OmniFlop…zip` und `UFT-50`. **Stand: 12 von 217 beurteilt, 205
    offen.**
  · **Zwei Einträge stehen bewusst OHNE Urteil**, obwohl die Suche sie
    zunächst als „beurteilt" meldete: `atari.zip` und `Skript.pdf`
    trafen nur über einen **Wortstamm** (`atari`, `Skript`), nicht
    über ihren Namen. Zu keinem von beiden habe ich eine Aussage
    gefunden — also keine eingetragen.
  · **Und die Zahl 12 ist selbst das Ergebnis einer Korrektur:** die
    Stammsuche meldete **12 mit Lizenzurteil**, die wörtliche Suche
    nur **6**. Beide Richtungen sind falsch — `FLOFOR` und
    `IPF-Format` stehen im Baum ohne ihre Endung. Eingetragen ist
    nur, wozu ich eine Aussage **gelesen** habe: neun.
  · **137 von 217 stehen in KEINER der vier Quellen** (`ORACLES.md`,
    `QUARANTINE.md`, `erzeuger_kanaele.json`, `OPEN_ITEMS.md`) — das
    ist der eigentliche Rückstand, und er ist jetzt beziffert statt
    geschätzt.
- **Stand 2026-09-19, dritter Durchgang — die Lizenzen sind GELESEN,
  nicht geraten: 47 von 217 beurteilt, 170 offen:**
  · **Erst sortiert, dann gelesen.** Von den 205 unbeurteilten tragen
    **35** eine Lizenzdatei im Paket, **75** keine, **7** sind `.lha`
    und mit der Standardbibliothek **nicht lesbar** — die werden als
    „keine Aussage" geführt, nicht still als „ohne Lizenz" —, und
    **88** sind gar keine Archive (die UFT-NN-Ausarbeitungen und
    lose Quellen). Gelistet, nicht ausgepackt.
  · **Die 35 Lizenzdateien gelesen: 30 bestimmt, 5 nicht.** Die
    Zuordnung läuft über einen Wortschatz, und was er nicht erkennt,
    heißt **„unbestimmt"** und bekommt seinen Anfang mitgedruckt —
    er rät nie. Verteilung **der 30**: 7× Apache-2.0, 8× MIT, 6×
    GPL-2.0, 2× GPL-3.0, 4× LGPL, 1× BSD-3-Clause, 1× Unlicense, 1×
    zlib. (Über alle 47 Urteile gezählt sieht es anders aus — 8×
    Apache und 5× GPL-2.0 —, weil `nibtools-extra` von Hand
    dazukam und `casutil_extract_c` nachträglich auf „ungemessen"
    zurückgestuft wurde. Zwei Zahlen zu derselben Sache sind genau
    die Drift, gegen die dieser Baum steht; deshalb steht hier,
    welche Menge gemeint ist.)
- **Stand 2026-09-19, fünfter Durchgang — die Readmes gelesen, das
  Dekompilat gefunden, und der Eigentümer hat über den Quarantäne-
  Nachtrag entschieden: 170 von 217 beurteilt, 47 offen:**
  · **Readme-Messung** über die Einträge ohne Lizenzdatei: **12** mit
    einer Bedingung (Trefferzeile zitiert, nicht klassifiziert), **19**
    durchsucht ohne Treffer, **44** ohne Readme, 0 Fehler. Sie hat den
    vierten Durchgang berichtigt: „75 ohne Lizenzdatei" war eine
    **Obergrenze** — `akaiutil` trägt `gpl-2.0.txt`, der Protection-
    Catalog `LICENSE_UFT_PROJECT.txt`, beides von meinem Dateinamen-
    Muster (LICENSE/COPYING) nicht erkannt. Und ein „nicht
    kommerziell"-Treffer stammte aus **GPL-2 §3 selbst** („allowed
    only for noncommercial distribution"), nicht aus einer
    Zusatzklausel. Eine Copyright-Zeile allein zählt nicht als
    Bedingung.
  · **`dtc_code/` ist die rohe Dekompilation des proprietären
    KryoFlux-`dtc`** (`dtc_decompiled.c` mit 116 Routinen,
    `dtc_text_arm64.asm` 24,7 MB, `dtc_text_section.bin`). Der Baum
    kannte es: `QUARANTINE.md` führt dtc als **Verdacht, Zone ROT**,
    `src/dtc_components/` kam mit MF-1099 als **Eigentümerentscheidung,
    nicht als Lizenzprüfung**, und `src/dtc_components/LICENSE:24`
    schließt „decompiler output" aus. Neu ist die **Lesung der
    SPS-Lizenz** selbst (`Linux_Release3.50/LICENCE.txt` aus
    `fertige/kryoflux_3.50_linux_r4.tar.gz`, 14 442 Byte): Z. 61 „only
    for **private & non-profit purposes**", Z. 150 „All rights
    reserved"; im gegrepten Text **keine** reverse/decompile-Klausel —
    eine Grenze des grep, keine Erlaubnis. Für ein öffentliches
    GPL-2-Projekt ist das ohnehin nicht weitergabefähig.
  · **Der Nachtrag dazu in `QUARANTINE.md` ist auf Anweisung des
    Eigentümers zurückgenommen** („lösche docs/QUARANTINE.md",
    präzisiert per Rückfrage zu „nur meinen Nachtrag verwerfen"; die
    Datei bleibt — 7 Skripte lesen sie, darunter `gen_stand.py` und
    die Kennzahl-Ableitung in `update_inventory.py`). Die Messung
    lebt im Register-Urteil zu `dtc_code`, Patch gesichert im
    Job-Verzeichnis.
  · **50 Urteile gesät**, jedes mit benannter Quelle: 12 aus der
    Readme-Bedingung, 19 aus „durchsucht, kein Treffer" (ehrlich als
    Zustand), 19 aus Baum-Zitaten (SDISK MIT/MF-1176, formats1
    BSD-3/MF-1078, pc98-disk-tools/MF-1224, 1050-Turbo/MF-1179 …),
    eigenem Material (UFT-NN-Zips, UFT_Paket, `.claude`) und den
    vier verschachtelten Analysen (Kurzurteil wörtlich). Verteilung
    der 170: Kanal Spec 81 · Fundus 71 · Port 6 · Oracle 5 ·
    Helfer-Prozess 3 · Daten/Fixture 2 · Nachbau 2; Kennzahl T3 runter
    3, sonst Fundus.
  · **Entwurfsfrage, nicht entschieden:** fünf Einträge sind selbst
    Sammelordner — `1` (143 Dateien), `copy` (176), `floppy1` (92),
    `fertige` (1354), `exsource` (24) — zusammen **1789 der 2408
    Dateien**. Das Register sieht sie als je einen Eintrag; ob eine
    zweite Ebene hinein gehört, ist die Frage, an der „vollständig
    gesichtet" hängt. Offen für den Eigentümer.
  · **Die 47 Offenen** haben weder Readme noch Baum-Zitat noch
    Analyse; sie sind nicht geöffnet, und ein Urteil „nicht geöffnet"
    wäre eines über den Zustand, nicht über den Inhalt — deshalb
    stehen sie ohne.
- **Richtungswechsel 2026-09-19, Wortlaut des Eigentümers:** „ich
  möchte das du A-032 nimmst und damit mein tool verbessert und
  leistungsstärker mach | allen externen code nehmen, verbessern, und
  in unser tool implementiern" — mit dem Paket **`uft_disk2`** (Kopf,
  Umsetzung, Test, Entwurfsdokument „UFT-NN — Das Zentrum") im selben
  Auftrag. Der Posten heißt damit nicht mehr „sichten", sondern
  **„einbauen"**; das Register aus den Durchgängen eins bis fünf bleibt
  die Landkarte dafür (170 von 217 beurteilt).
- **Stand 2026-09-19, sechster Durchgang — `uft_disk2` ist im Baum
  (MF-1272), mit Brücke und zwei Tests:**
  · **Fünf Namen des Entwurfs gab es hier schon**, mit anderer
    Bedeutung: `uft_encoding_t` (21 Dateien), `uft_layer_t` (zweimal!
    — `uft_unified_image.h` und `uft_track.h`), `uft_diag_t` (8),
    `UFT_CONF_CERTAIN`, `UFT_FS_FAT12`. Alle öffentlichen Namen tragen
    deshalb `uft_d2_`/`UFT_D2_`; die Kodierung nimmt den vorhandenen
    `uft_encoding_t` (D3), keinen zweiten Enum.
  · **Neun Verbesserungen gegenüber dem Entwurf**, jede im Kopf von
    `uft_disk2.h` benannt — darunter: der Bericht sagt „CRC nicht
    getragen" statt „falsche CRC: 0" (die Einbahn-Aussage, die
    MF-662 erfunden nennt); der letzte Befundplatz ist von Anfang an
    reserviert (der Entwurf nahm den 512. an und überschrieb ihn
    still); `uft_d2_report()` gibt die BENÖTIGTE Länge zurück, eine
    Kürzung ist erkennbar; `UFT_D2_CONF_UNVERIFIED` (128) hat einen
    Namen statt ein Literal zu sein.
  · **Die Brücke `uft_d2_from_disk()`** liest jede Spur über
    `plugin->read_track()` und übersetzt ehrlich: CRC „bekannt" nur
    bei Fehlerflagge oder Prüfwert ≠ 0/0 (`add_sector` setzt OK
    unbedingt — kein Beleg); Zuversicht 255 nur mit stimmender CRC,
    sonst 128, bei falscher CRC 64 (als **Richtlinie** benannt),
    Füllmaterial 0; Lage im Bitstrom SIZE_MAX (drei Versatzfelder im
    alten Modell ohne Aussage, welches gilt); `raw_data` ohne
    `raw_bits` wird gezählt, nicht erfunden. Was sie nicht trägt
    (Fluss, per-Bit-Weak-Maske), meldet sie als je EINEN Befund.
  · **Gemessen am Korpus-86F** (im Git, also in CI): 160 Spuren, 1440
    Sektoren — dieselbe Zahl wie das Plugin direkt, byteidentisch als
    Kopie, 0 mit 255, 0 mit CRC-Angabe; zweite Quelle das `.img`
    derselben Diskette (720 = 1440/2, das 86F legt jeden Zylinder
    doppelt ab). Dazu ein gestelltes Plugin für die Fälle, die der
    Korpus nicht hat: 255 / 64 / 0 / 128, jede Zahl aus ihrer Regel.
  · **Vier Rotbeweise, jeder an genau seiner Zusage:** Brücke ohne
    Einspeisung (0 statt 1440), Zuversichtsregel entfernt, Entwurfs-
    Größenvergleich, Entwurfs-Überlauf. Alle sechs Dateien 0 Warnungen
    unter `-Wall -Wextra -Wpedantic`.
  · **Nebenbefund, gemessen:** der Baum hat **zwei halbe Aufräumer**
    für `uft_track_t` (MF-599 kannte es): `uft_track_cleanup()` gibt
    Sektoren/`flux`/`raw_data` frei, `uft_track_free()` zusätzlich
    `confidence`/`weak_mask`/`flux_times`/`revisions`, `raw_data`
    aber nur mit `owns_data`. Die Brücke räumt deshalb beides selbst.
    Und die alten Schicht-Zeiger `flux_layer`/`bitstream_layer`/
    `sector_layer` in `uft_track_t` haben 0/0/1 Nutzer — dieselbe
    Idee wie die vier Schichten, nie gefüllt.
  · ~~**Noch kein Produktivleser** — der ist der nächste Commit.~~
    **Eingelöst mit MF-1273 (siebter Durchgang, unten).**
- **Stand 2026-09-19, siebter Durchgang — der erste Produktivleser
  (MF-1273): der Disk-Analyzer zeigt, was der Träger trägt:**
  · `DiskAnalyzerWindow::loadImage()` speist das geöffnete Abbild
    über sein Plugin in das Zentrum ein (`uft_d2_from_disk`) und
    zeigt `uft_d2_report()` in einem neuen Kasten `textDiskReport`
    unter dem Sektorbericht; der HTML- und Text-Export nimmt ihn mit.
    Ohne Plugin steht dort „Kein Bericht: kein Plugin konnte dieses
    Abbild oeffnen" — kein leerer Kasten, der wie „nichts gefunden"
    aussieht.
  · **Gemessen an der 35-Spur-D64 des vorhandenen `no_fiction`-Tests:**
    „Traeger: 35 Spuren, hoechste Lage C34 H0 (gemessen)", „Sektoren:
    683 — mit CRC-Angabe: 0 (davon falsch: 0), ohne CRC-Angabe: 683",
    „Schichten: Sektoren". 683 und 35 sind Formateigenschaften
    (17×21 + 7×19 + 6×18 + 5×17), keine Ablesung aus unserem Code —
    und das D64-Plugin liefert `crc_stored = crc_calculated = 0`, also
    darf der Bericht kein CRC-Urteil fällen; er sagt „ohne Angabe".
  · **Rotbeweis:** Brückenaufruf in `traegerBericht()` entfernt →
    genau die neue Zusage `theCarrierReportIsMeasuredNotAssumed()`
    fällt (5 bestanden, 1 gefallen), die drei alten `no_fiction`-
    Zusagen bleiben grün; wiederhergestellt 6/6. Damit hat die Brücke
    aus MF-1272 ihren Produktivaufrufer und einen Test, der rot wird,
    wenn der Aufruf verschwindet (D2).
  · **Der Einbauort ist meine Wahl** (der Entwurf nannte als Punkt 2
    „img + FAT12"; `uft_fat_robust` liegt noch in den Zips) — die
    Brücke und der Bericht sind ortsunabhängig, ein Umzug in einen
    anderen Reiter ist Minuten. **Die eine Verhaltensänderung:** das
    Laden liest jetzt jede Spur; bei Sektorabbildern Millisekunden,
    bei Flussabbildern dekodiert das Plugin je Spur — das steht im
    Kopf von `traegerBericht()`.
  · **Kein Klick-Smoke-Test** — es gibt hier keine Anzeige; belegt ist
    der Weg durch den Qt-Test offscreen (`loadImage` → Kasten
    gefüllt → fünf Zusagen). Was der Kasten im Fenster tatsächlich
    zeigt, hat der Eigentümer noch nicht gesehen.
- **Stand 2026-09-20, achter Durchgang — die ZWEITE FASSUNG des
  Zentrums (MF-1274), vom Eigentümer eingereicht, mit derselben
  Namensmessung wie beim ersten Mal:**
  · **Was dazukam:** Generationen je Schicht samt `uft_d2_validate()`
    (fünf Widersprüche: STALE_DERIV, POS_BEYOND, ENC_MISMATCH,
    REV_EMPTY_INDEX, FS_RANGE); ein **Ableitungsregister** mit
    Kennungen statt eines Herkunfts-Structs je Objekt; **Stimmen je
    Bit** (`agree[]` + `nrevs_fused` — „3 von 5" ist nachprüfbar,
    „Konfidenz 153" nicht); **mehrere Dateisysteme** je Diskette;
    Kodierung je **Sektor**; `crosses_index` **gerechnet** statt
    gesetzt, und zwar über BEIDE Felder (IOI ≠ DOI); die
    Zuversichtsregel an **allen** Schichten; Spurindex `[cyl][head]`;
    Merkmalcache; dynamische Metadaten mit Überlaufmeldung; die
    Fehlerzahl im Bericht — oben UND in der Kürzungszeile.
  · **Zwei Messungen am Baum, beide gegen den Entwurf:** die sieben
    Namenskollisionen aus MF-1272 gelten unverändert (der Entwurf
    benutzt dieselben Namen wieder) → alles bleibt `uft_d2_`/`UFT_D2_`;
    und **`UFT_ENC_GCR` gibt es hier nicht** — der Baum führt gemessen
    **20** Kodierungen statt vier, GCR nach Familie getrennt (CBM,
    Apple 5.25", Apple 3.5", Victor), dazu `UFT_ENC_AMIGA_MFM` und
    `UFT_ENC_M2FM`. Ein Sammelwert hätte bei Kopierschutz genau die
    Frage verschluckt, auf die es ankommt.
  · **Eine Schemaänderung mit Grund:** `by`/`params` waren ein
    `const char *` mit der Auflage „muss statisch sein". Ein Zeiger ist
    nicht serialisierbar, und die Auflage war eine Bitte an den
    Aufrufer statt einer Eigenschaft des Modells — jetzt wird in feste
    Feldbreite KOPIERT. Der Test führt das vor: ein Name wird nach dem
    Registrieren überschrieben und steht danach noch richtig da.
  · **Eine Falle, die ich in den Kopf geschrieben habe, weil sie sonst
    niemand sieht:** `uft_d2_track()` gibt einen Zeiger in ein Feld
    zurück, das beim Anlegen der NÄCHSTEN Spur umzieht. Der Index hält
    deshalb Nummern, keine Zeiger — und wer eine Spur über ein
    weiteres `uft_d2_track()` hinweg hält, hält ins Leere.
  · **Rotbeweis: sieben Mutationen, sieben gefallen** — Generation
    steigt nicht (6 Zusagen), Zuversichtsregel lässt alles durch (8),
    Stimmen ohne Basis angenommen (4), `crosses_index` nur auf
    `dam_bit` (2), Kürzungszeile zählt verdeckte Fehler nicht (2),
    Metadaten-Überlauf bleibt still (2), Brücke trägt eine andere
    Herkunft ein (1441). Wiederhergestellt byteidentisch, beide Tests
    grün, volle Suite 520/520, Bau 0 Warnungen.
  · **Und eine Lücke in meinem EIGENEN Test hat der Rotbeweis
    gefunden:** die IOI/DOI-Unterscheidung war nicht bewacht — meine
    Fälle waren so gewählt, dass eine Prüfung nur auf `dam_bit`
    dasselbe Ergebnis liefert. Erst der Fall „Adressfeld bei 10,
    Datenfeld ab 70, Index 64" trennt die beiden. Nachgetragen, dann
    fällt Mutation D.
  · ~~**GESTOPPT:** der Behälter UFTD kommt als eigener Commit.~~
    **Eingelöst mit MF-1275 (neunter Durchgang, unten).**
- **Stand 2026-09-20, neunter Durchgang — UFTD: der erste Weg, auf dem
  ein Abzug diesen Baum OHNE Verlust verlässt (MF-1275):**
  · **Der Behälter.** Kopf `"UFTD"` + Version + Flags + Gesamtlänge;
    danach Blöcke aus Kennung, Länge, Inhalt und **eigener CRC32**:
    DERV (Ableitungsregister), META, TRAK mit FLUX/BITS/SECT, FSYS,
    DIAG, END mit der Gesamt-CRC. Drei Eigenschaften, die nicht
    verhandelbar sind: unbekannte Blöcke werden **übersprungen und
    gemeldet** (vorwärtsverträglich); jeder Block trägt seine CRC, also
    wird ein gekipptes Bit **an seiner Stelle** gefunden und alles
    davor ist trotzdem geladen; und **Laden ist nie stiller als
    Speichern** — was fehlt oder nicht stimmt, steht danach als Befund
    im Modell.
  · **Der Produktivaufrufer, und er ist nicht nachgereicht:** das
    Analyzer-Fenster **behält** seit MF-1275 das Modell (`m_traeger`),
    statt es nach dem Bericht wegzuwerfen; `traegerSichern()` schreibt
    genau **diese** Messung als UFTD, und der Export legt sie neben den
    HTML-/Text-Bericht. Ein zweites Lesen wäre eine zweite Messung, und
    zwei Messungen können auseinandergehen.
  · **Gemessen am D64 des vorhandenen Tests:** gesichert, mit
    `uftd_load_file()` **von fremder Hand** zurückgelesen — 35 Spuren,
    683 Sektoren, und der Bericht aus der Datei ist **Zeichen für
    Zeichen** derselbe wie der im Kasten. Am vollen Modell (Fluss mit
    zwei Umdrehungen, Bitstrom mit allen vier Nebenreihen, zwei
    Dateisysteme, Metadaten, Ableitungen, Befunde): **3845 Byte,
    Doppelrundlauf vollständig byteidentisch.**
  · **Drei Abweichungen vom Entwurf, jede mit Grund:** (a) seine
    Versionsprüfung `(ver >> 8) > (UFTD_VERSION >> 8) && …` konnte für
    **keine** Fassung unter 256 zuschlagen — eine Datei der Fassung 2
    hätte ein Leser der Fassung 1 klaglos geöffnet; (b) `meta_hidden`
    wurde nicht mitgesichert, womit ein Modell mit Überlauf nach einem
    Rundlauf einen **anderen Bericht** gehabt hätte; (c) Befunde gehen
    beim Laden **direkt** in die Liste statt durch `uft_d2_diag()` —
    sonst zählt eine volle Liste ihren eigenen Überlauf-Eintrag noch
    einmal als Überlauf.
  · **Rotbeweis: sechs Mutationen, sechs gefallen.** A `uftd_save_file`
    im Analyzer nicht gerufen → genau die neue Zusage fällt (6/1), die
    sechs alten nicht. B `meta_hidden` nicht gesichert (2). C verdeckte
    Befundzähler verloren (2). D die Versionsprüfung des Entwurfs
    eingesetzt (3 — „Fassung 2 abgewiesen, war ok"). E Gesamtlänge nach
    der Gesamt-CRC eingetragen (6 — **4068 statt 3845 Byte** und ein
    `TOTAL_CRC`-Befund, den es nicht geben dürfte). F Zuversichtsregel
    beim Laden umgangen (5). Wiederhergestellt byteidentisch, volle
    Suite **521/521**, Bau 0 Warnungen.
  · **Was der Entwurf selbst als offen nennt und offen bleibt:** keine
    Kompression (ein `FLXZ`-Block ginge ohne Änderung an Lesern, die
    ihn nicht kennen), kein Streaming, keine `SECTOR_OVERLAP`-Prüfung,
    und die Versionsregel „Nebenversion darf höher sein" — sie kommt,
    wenn es eine zweite Version gibt; vorher wäre sie eine Regel ohne
    Fall.
  · **Nicht belegt:** ein Flussabzug in Originalgröße. Das volle
    Testmodell hat 3845 Byte; wie sich `uftd_save()` bei fünf
    Umdrehungen über 160 Spuren verhält, ist **nicht gemessen** — der
    Entwurf nennt „mehrere Megabyte", und der Aufbau ohne Streaming
    hält alles im Speicher.
- **Stand 2026-09-20, zehnter Durchgang — `uft_fat_robust` eingebaut
  (MF-1276), und zwar IN den vorhandenen Leser statt daneben:**
  · **Die Entscheidung aus §10.1 der Ausarbeitung ist gefallen, und sie
    ist gemessen begründet.** Das eingereichte Modul deklariert drei
    Namen, die es hier schon gibt — `uft_fat_geometry_t`,
    `uft_fat_chain_t` und `uft_fat_chain_free`, alle drei **dieselben
    Begriffe mit anderen Feldern**. Ein zweites Modul daneben wäre der
    dritte FAT12-Kettenläufer im Baum gewesen (der zweite steht in
    `src/formats/msx/uft_msx.c:463-491`) — genau die Krankheit, die der
    Zentrum-Entwurf als „FAT-Kettenläufer x2" benennt. Also integriert.
  · **Der Defekt, den die Messung gefunden hat, stand schon da:**
    `uft_fat_get_chain()` hielt den nächsten Cluster nur gegen die
    **ersten sechzehn** Glieder (`for (i = 0; i < count && i < 16; i++)`).
    Eine Schleife, die sich später schließt — 2..40, dann 40 → 20 —
    war unsichtbar: `has_loops` blieb **false**, die Kette lief bis zur
    65536er-Bremse, und der Aufrufer bekam zehntausende Cluster ohne
    jede Warnung. Seit MF-1276 eine volle Bitmenge (360 Byte bei 1,44
    MB), und `loop_at` sagt, **wo** sie sich schließt.
  · **Und der Kettenläufer hatte NULL Tests** — gemessen
    `git grep -l uft_fat_get_chain -- tests/`: kein Treffer. Die
    Funktion, an der jede FAT12-Extraktion hängt, war unbewacht.
  · **Neu:** `uft_fat_get_chain_sized()` fragt MIT der Dateigröße und
    kann deshalb sagen, ob die Kette reicht: `needed`, `from_chain`,
    `status`. Reißt sie ab, wird fortlaufend weitergelesen — und der
    Zustand heißt `CONTIG`, „fortlaufend geraten (Versuch)", nicht
    „ok". Produktivaufrufer ist `uft_fat_extract()`, das die Größe
    bisher nur zum Abschneiden benutzte statt zum Fragen.
  · **Vier Aussagen der Ausarbeitung über UNSEREN Baum nachgemessen:**
    drei treffen zu (null Treffer für contig/cross/fallback, der zweite
    Kettenläufer in `uft_msx.c`, keine Querverweiserkennung), eine ist
    gedriftet — sie sagt 854 Zeilen, gemessen sind es **884**.
  · **Rotbeweis: vier Mutationen, vier gefallen** — alte 16er-Bremse
    (4 Zusagen), kein Rückfall (5), keine Deckelung auf den Bedarf (2),
    `uft_fat_extract` fragt wieder ohne Größe (2). **Mutation C fiel
    beim ERSTEN Lauf nicht**: meine Gruppe 4 deckte sie nicht ab, weil
    dort die Diskette bremst und nicht der Bedarf. Gruppe 5
    nachgetragen (Kette 10 Glieder, Datei 2 Cluster), dann fällt sie.
    Dieselbe Klasse wie MF-1014 und MF-1274-D: ein grüner Test, der aus
    dem falschen Grund grün war.
  · **Herkunft:** disk-peek (Joost Yervante Damad, MIT,
    `js/fat12.js:137-159`), Verfahren übernommen, Code neu geschrieben
    — Kanal *Nachbau* nach MF-695. Die Vermerke stehen in allen drei
    berührten Dateien; `audit_attribution_licence.py` meldet Rückstand
    **29** gegen Grundlinie 31. **Ein NOTICE gibt es in diesem Baum
    nicht** — die Ausarbeitung verlangt einen Eintrag dort, der
    Mechanismus hier ist die Attribution im Dateikopf (MF-636).
- **Stand 2026-09-20, elfter Durchgang — der erste der 29 gesperrten
  Wandlungspfade ist frei (MF-1277), und der Plan dazu steht:**
  · **`docs/PLAN_MODERNISIERUNG.md`** hält den Fahrplan in sechs Phasen,
    jede in einer frischen Sitzung ausführbar. Eigentümerauftrag
    wörtlich: „nimm alle den fremd code aus neue-ideen und verbessere
    ihn, gehe den gesamt code durch suche nach schwachstellen … fange
    dann zuerst mit IMD→IMG an."
  · **Die Lage vorher, mit einem Wegwerf-Prüfstand gegen die ECHTE
    `uft_convert_file()` gemessen:** 46 Wandlungspfade, 17
    Matrix-Einträge, **29 gesperrt** — und zwar aus **drei**
    verschiedenen Gründen: 18 am Preflight („UNTESTED"), 5 an
    mehrdeutiger Quelle, 4 weil die Tabelle eine Format-ID nennt, die
    **kein Plugin trägt** (`KRYOFLUX`), 2 ohne Quelldatei (`NBZ`).
  · **Der teuerste Nebenbefund:** von **17** `.img`-Korpusdateien
    werden nur **zwei** als `plugin='IMG'` erkannt; die übrigen holen
    sich MSX, Victor9K, Micropolis, NorthStar oder HardSector — und
    die deklarieren **alle** `UFT_FORMAT_DSK`. Die Wandlungstabelle ist
    auf Format-IDs verschlüsselt, der ID-Raum ist aber eingeschmolzen
    (MF-1087: 101 Plugins tragen `UFT_FORMAT_DSK`).
  · **`IMD→IMG` ist eingelöst.** Beleg: `hxcfe_pc160.imd` (164 785 B,
    **fremde Hand**) → 163 840 B, **0 von 163 840 abweichend** gegen
    `uft_pc160.img`. Keine Gleichheit ohne Aussage (MF-1039):
    163 840 = 40 × 1 × 8 × 512 ist der Sektorinhalt selbst.
  · **Stufe `LOSSY_DOCUMENTED`, nicht `LOSSLESS`** — die Prüfdiskette
    hat 0 defekte Sektoren, die IMD-Metadatenschicht geht trotzdem
    verloren: neun Posten, aus den **Feldern** belegt. Der neunte ist
    der forensisch teure: fehlende Sektoren werden mit **`0xE5`**
    gefüllt und sind danach von echten `0xE5`-Sektoren nicht mehr zu
    unterscheiden; der Wandler zählt und meldet sie, die Zieldatei kann
    es nicht tragen.
  · **Gemessen und festgehalten:** `accept_data_loss` hilft bei einem
    UNGEPRÜFTEN Paar **nicht** (der Preflight sperrt zu jedem Preis) —
    **nach** dem Eintrag greift es und wird verlangt. Die Absage
    wechselt von „conversion pair is UNTESTED" auf „requires
    accept_data_loss=true". Genau so ist es gebaut.
  · **Rotbeweis: vier Mutationen, vier gefallen** — Matrix-Eintrag auf
    ein anderes Ziel (14 Zusagen), zwei Stichworte aus der Notiz (4),
    Füllbyte `0xE5`→`0x00` (2), Zustimmung verweigert (6).
    **Mutation C fiel beim ERSTEN Lauf nicht**, weil die Prüfdiskette 0
    fehlende Sektoren hat — Gruppe 5 baut deshalb eine IMD **mit**
    Lücke (Sektortyp `UFT_IMD_SEC_UNAVAIL`). Dieselbe Klasse wie
    MF-1274-D und MF-1276-C: ein grüner Test aus dem falschen Grund.
  · **Kennzahl:** angebotene Wandlungspfade **15 → 16**, Matrix 17 → 18.
  · **Nebenbefund, als `P3-517` festgehalten:** ein **Kalman-PLL** ist
    in diesem Baum nie gebaut worden — drei Funktionen ohne Aufrufer,
    eine Flagge, ein README-Eintrag für eine Datei, die es nicht gibt,
    und zwei entschuldigende Kommentare.
- **Stand 2026-09-20, zehnter Durchgang, Nachtrag — GESTOPPT bei
  `uft_fat_robust`:** die **Querverweiserkennung** (zwei Dateien
    auf demselben Cluster) und die **Geometrie ohne BPB** sind nicht
    gebaut. Beide brauchen eine eigene Datenstruktur und einen eigenen
    Produktivaufrufer; drei halbe Stücke sind schlechter als eines,
    das trägt (Scope-Regel). Die Ausarbeitung nennt Querverweise
    ausdrücklich als das, was in **jedem** Dateisystem des Baums fehlt
    — das wäre ein Modul über allen, nicht eines in FAT12.
- **Warteschlange „einbauen", aus dem Register abgeleitet** (die
  eigenen Extrakte in `exsource/`: `uft_advanced_flux_v8.zip`,
  `uft_copy_protection_v7.zip`, `uft_cbm_code_extraction_v4.zip`,
  `track_layout_gen_c_v5.zip`, `micropolis_gcr_extract_c.zip`,
  `mpi_gcr_extract_c.zip`; die sechs Module der Reihe
  `uft_revolution`, `uft_protection_scan`, `uft_splice`,
  `uft_fat_robust`, `uft_a2_order`, `uft_amiga_media` — ~~sobald ihr
  Paket gefunden ist~~ **berichtigt MF-1274: DREI davon liegen im
  Baum, und die erste Suche hat sie nur nicht gesehen.** Gesucht wurde
  nach Dateinamen (`find -iname '*fat_robust*'`) und in `.md`-Dateien;
  die Pakete heissen aber auf DEUTSCH und die Modulnamen stehen INNEN:
  `neue-ideen/UFT-NN — FAT12 lesen.zip` -> `uft_fat_robust.{h,c}` +
  `test_fat_robust.c` + `UFT-NN_FAT12_Robustheit.md`;
  `neue-ideen/Apple-Sektorordnung.zip` -> `uft_a2_order.{h,c}` +
  Test + Ausarbeitung; `neue-ideen/UFT-NN — Amiga.zip` ->
  `uft_amiga_media.{h,c}` + Test + Ausarbeitung. Gefunden hat sie ein
  `grep -rl` ueber den GANZEN Baum je Modulname — dieselbe Lehre wie
  bei `adf_ext` (MF-1222): eine Suche ueber Dateinamen ist eine Aussage
  ueber Dateinamen, nicht ueber Inhalte. Fuer `uft_revolution`,
  `uft_protection_scan` und `uft_splice` gibt es weiterhin KEIN Paket
  (0 Treffer ausserhalb dieser Liste und der Bauartefakte).
  **`uft_fat_robust` ist damit der naechste Griff** — es ist zugleich
  Punkt 2 des Zentrums-Entwurfs („ein Adapter, der beweist, dass es
  traegt: img + FAT12"): je Modul EIN Commit, Rotbeweis zuerst, Lizenz
  an der Datei, Kanal nach MF-695, und unter der EINFRIER-REGEL kein
  neues Format-Plugin — Bugfixes und Verifikation ja.
  · **Und genau die fünf Absagen waren die interessanten.**
    `capsimage`: „THIS IS NOT FREE SOFTWARE" — und der Volltext liegt
    laut eigener Datei in einem übergeordneten Archiv, das fehlt.
    `samdisk_plus`: eine **nicht-kommerzielle** Klausel, festgehalten
    als **`P3-514`** (Eigentümer-Vorlage). `ST-Recover`: Ms-RL.
    `OpenCBM`: die Treffer-Datei ist die eines mitgelieferten
    GUI-Teils, nicht die des Pakets.
  · **Ein Mangel der eigenen Saat, gemessen und behoben:** die erste
    gefundene Lizenzdatei ist nicht immer die des Pakets. Nachgezählt
    war von 30 Pfaden genau **einer** verdächtig
    (`casutil_extract_c.zip` → `third_party/…/COPYING`); er steht
    jetzt als „ungemessen für das Paket selbst". Die beiden anderen
    Fälle dieser Art (`samdisk_plus`, `OpenCBM`) hatte die
    „nie raten"-Regel ohnehin abgefangen.
- **Stand 2026-09-19, vierter Durchgang — die eigenen Ausarbeitungen
  sind beurteilt, die `.lha` gemessen, und zwei der fünf unberührten
  Analysen machten prüfbare Aussagen: 120 von 217 beurteilt, 97
  offen:**
  · **Die vier bis fünf Ausarbeitungen, die niemand zitiert, sind
    gelesen.** `UFT-103` (86F Loch/Weak): die Testlücke stimmt (453
    Zeilen, 0 Treffer), die Aussage „nicht als fehlend dokumentiert"
    nicht — `uft_86f_plugin.c:373` führt es seit MF-961 als
    `UNSUPPORTED` mit Grund; **die Ausarbeitung hat den Test
    gemessen, nicht das Plugin.** Festgehalten als **`P3-515`**, mit
    dem gemessenen Blocker: das Korpus-86F hat **keine**
    Oberflächenbeschreibung (Disk-Flags `0x1088`, Bit 0 = 0), ein Test
    dagegen könnte nicht rot werden; und `uft_track_t.weak_mask`
    kann ein LOCH nicht von einem Weak Bit unterscheiden. `UFT-105`
    (ProDOS aux_type): Prämisse zu weit — `prodos_po_do.c` liest kein
    Dateisystem (MF-710), kein eigener Befund. `UFT-101/102/97`:
    Fundus der Klasse P0-2.
  · **`UFT-NN_TRS80_JV13_Analyse.md` war nie vergessen, nur namenlos
    eingelöst:** ihr Hauptfund (`JV3_HEADER_SIZE 0x2300`, 256 Byte zu
    weit, Verlust in jedem JV3-Abbild) ist genau **MF-1017**.
  · **Die 66 Ausarbeitungen der obersten Ebene tragen jetzt ein
    Urteil aus ihrem eigenen Kurzurteil** — der erste Satz, wörtlich;
    Beleg sind ihre Kopfzeilen (Datum, Quelle); Kanal `Spec`
    (Sekundärliteratur, gelesen, nichts übernommen); die Lizenz der
    beschriebenen Quelle ausdrücklich „ungemessen". Nur die oberste
    Ebene: die fünf verschachtelten gehören zum Urteil ihres
    Materialordners (K4). Die zwei ohne Kurzurteil (`UFT-NN_*`) von
    Hand.
  · **Die sieben `.lha` sind keine „keine Aussage" mehr:** 7-Zip liegt
    unter `C:\Program Files\7-Zip\7z.exe`, alle sieben gelistet (rc
    0), **keines** trägt eine Datei namens LICENSE/COPYING/COPYRIGHT,
    jedes ein Readme/Guide (nicht gelesen). Eine erste Messung meldete
    einen Lizenztreffer — das war die 7-Zip-Kopfzeile („Copyright (c)
    1999-2026 Igor Pavlov"), keine Datei; gemessen, bevor es
    eingetragen wurde.
  · **Eigener Fehler beim Eintragen, gefangen vom JSON-Parser:** in
    vier handgeschriebenen Werten stand das ASCII-`"` als schließendes
    Anführungszeichen mitten im String. Behoben mit dem typografischen
    `“`, danach ein Zensus über alle Zeilen (jede trägt 0, 2 oder 4
    unmaskierte Anführungszeichen). Die Messung dazu scheiterte
    zuerst selbst — an Bash-Escapes im `-c`-String, die
    Heredoc-Klasse aus MF-1096.
  · **Verteilung der 120 Urteile:** Kanal Spec 69 · Fundus 44 ·
    Helfer-Prozess 3 · Daten/Fixture 2 · Oracle 2; Kennzahl Fundus
    118 · T3 runter 2. **Die 97 Offenen** sind: 75 Archive ohne
    Lizenzdatei, dazu lose Quell- und Dokumentdateien und die
    entpackten Verzeichnisse — jedes davon braucht ein Lesen, kein
    Listen mehr.
- **Stand 2026-09-20, MF-1284 (Schritt 1a des TD0-Umbaus):** das
  TD0-Plugin ist der eine richtige Leser geworden. Der Anlass war eine
  Messung, die den Plan verändert hat: `uft_td0_read_mem()` sollte
  gelöscht und die Wandler auf den Plugin-Leser gezeigt werden — gemessen
  konnte der Plugin-Leser **gepackte TD0 gar nicht lesen** (`compressed`
  gesetzt und nie benutzt; `Transylvania.td0` meldete 188 Zylinder und
  null Sektoren mit `UFT_OK`), und der LZSS-Entpacker hatte **0 Aufrufer
  ausserhalb `uft_td0_lzss.c`**, wäre mit `read_mem` also mitgestorben.
  1a hängt ihn deshalb ins Plugin, bevor 1b löscht. Belegt gegen hxcfe:
  80×2/1440, 40×2/720, 41×2/738. Mutationsmatrix 5 von 5.
  **Offen bleibt 1b** — löschen und die beiden Wandler umhängen.
- **Stand 2026-09-20, MF-1285 (1b, erster Teil):** 1b ist gemessen **kein
  Commit, sondern zwei**, und der Grund ist wieder eine Messung, nicht
  eine Vorliebe:
  · **Der Speicherweg hat keinen Pfad.** `uftc_td0_to_img_mem()` bekommt
    Bytes, `uft_disk_open()` will einen Dateinamen, und
    `uft_format_plugin_t` hat kein Öffnen-aus-dem-Speicher — der
    Verteiler sagt das selbst und führt es als `KNOWN_ISSUES ARCH-6`.
    „Wandler auf den Plugin-Leser" war für diesen Weg also gar nicht
    machbar.
  · **Das Plugin warf den Kommentarblock weg.** `uft_td0_read_mem()` las
    ihn, `uft_td0_to_imd()` holt daraus Zeitstempel und Text. Löschen
    ohne Ersatz hätte still das METADATA-Merkmal verloren, das
    `uft_format_traegt()` seit MF-1283 für TD0 **und** IMD als getragen
    führt.
  MF-1285 baut deshalb den **Strom-Kern**: `uft_td0_strom_t` plus
  `_aus_bytes`/`_frei`/`_spur`, an Bytes hängend statt an einem Griff,
  mit behaltenem Kommentarblock. Das Plugin ist nur noch ein Aufsatz
  darüber. **Nebenbei ist Schritt 5 des Umbauplans belegt:** das
  Monatsfeld ist 0-basiert — libdsks Schreiber setzt `ptm->tm_mon`,
  SAMdisk liest `bMon + 1`, und die Korpusdatei vom 2026-09-12 trägt
  eine 8. Der Header-Kommentar „Month (1-12)" ist berichtigt; für das
  **Jahr** bleibt eine Abweichung offen (`P3-522`).
  **Offen bleibt 1b-ii** — die drei Wandler auf den Kern umhängen,
  `uft_td0_read_mem` samt `uft_td0_image_t`-Lauf löschen.
- **Stand 2026-09-20, MF-1286 (Sperre vor 1b-ii):** beim Umhängen der
  IMD-Wandlung auf die kanonischen Sektorfelder fiel auf, dass **das
  Plugin das falsche Bit als CRC-Fehler las** — `0x01` ist DUP, die
  doppelte Sektor-ID; der CRC-Fehler ist `0x02`. Vier Quellen sagen
  `0x02`, eine davon ein **Schreiber** (libdsk `drvtele.c:732`). Die
  Wirkung ging in beide Richtungen: echte CRC-Fehler kamen als gute
  Sektoren heraus, doppelte IDs als kaputte.
  **Das musste vor 1b-ii kommen**, weil 1b-ii die IMD-Wandlung genau
  auf dieses `crc_ok` umstellt — sonst hätte der Umbau den Fehler in den
  neuen Pfad getragen und der alte wäre für den CRC-Fall der genauere
  gewesen.
  **Und ein grüner Test hat den Defekt bewacht:**
  `tests/test_td0_error_marks.c` behauptete „bit0 (0x01) = data CRC
  error" und baute sich die Prüfdatei dazu selbst — neunzehn Zeilen über
  derselben Stelle, an der er die Klasse schon einmal beschrieben hatte
  („both sides shared the same mistake", MF-389). Berichtigt, plus ein
  vierter Sektor, der die bisher falsche Richtung absichert.
  Matrix 4 von 4 über die **vier** TD0-Testziele; der erste Lauf meldete
  3 von 4 und das war das Messwerkzeug, nicht die Abdeckung.
  Nebenbefund `P3-523`: `TD0_FLAG_NO_DATA` im Wandler ist `0x08`, ein
  Bit, das TD0 nicht benutzt — der `UNAVAILABLE`-Zweig ist damit
  unerreichbar. Nicht mitrepariert, weil die Funktion in 1b-ii fällt.
- **Stand 2026-09-20, MF-1287 — 1b ist ABGETRAGEN:** `uft_td0_read_mem()`
  ist gelöscht, mit ihm sieben weitere Funktionen und die drei
  Aggregattypen; alle drei Wandler gehen über den Strom-Kern. Jedes
  Symbol war vor dem Schnitt gezählt und hatte außerhalb der sterbenden
  Datei null Aufrufer.
  · **`uft_td0_to_imd()` liest jetzt die kanonischen Sektorfelder** statt
    der TD0-Flaggen — damit fällt `td0_flags_to_imd_stype()`, und
    `P3-523` ist im neuen Pfad erledigt.
  · **Der Wandler hatte nie einen Test.** `test_convert_imd_img_belegt.c`
    prüft, dass das Preflight TD0→IMD SPERRT — dabei läuft er nie.
    Gruppe 7 ist sein erster, und sie belegt die Kernzusage: ein
    CRC-Flag in der TD0 kommt als `UFT_IMD_SEC_ERROR` an.
  · **Neun Zahlen, zwei Namen, zwei Orte:** die privaten
    `IMD_STYPE_*`-Konstanten im Wandler waren eine zweite Kopie von
    `uft_imd_sectype_t` aus dem Formatheader — mit dem Kommentar „Not
    symbolically named in the header", der nicht stimmte. Zusammengeführt
    (§MF-1177).
  Offen und benannt: `P3-524` (Sektorgrößen werden auf die des ersten
  geebnet, im Widerspruch zur eigenen Merkmalstafel) — das entscheidet
  mit, ob der künftige Matrixeintrag „identisch" oder „projiziert
  identisch" heißt.
  **Als Nächstes aus dem Umbauplan:** Schritt 3 (`UFT_CONV_LOSSLESS` für
  TD0→IMD auf UNVERIFIED) und Schritt 4 (Vorwärtsprüfung gegen hxcfe,
  Korpus ≥ 3).
- **Stand 2026-09-20, MF-1288 — Schritt 3 ist abgetragen, und er war
  größer als eine Zeile:** `TD0→IMD` steht auf `UFT_CONV_UNVERIFIED`.
  Den Wert gab es vorher nicht — die Aufzählung kannte nur LOSSLESS,
  LOSSY, SYNTHETIC, IMPOSSIBLE, **„nicht gemessen" war also gar nicht
  sagbar**, und im Zweifel stand dort die freundlichste Behauptung.
  · **Es war ein Fall von acht.** Gemessen zur Laufzeit: 13 Pfade
    behaupteten LOSSLESS, **acht ohne jeden Matrixeintrag** — darunter
    `SCP→G64`, Fluss nach Bitstrom, wo `CLAUDE.md` selbst sagt
    „Flux→Sektor verliert Timing+WeakBits".
  · **Die Güte-Spalte hatte keinen Leser.** `uft_convert_can()` null
    Aufrufer, `uft_convert_print_matrix()` null Aufrufer und nicht
    einmal eine Header-Deklaration. Deshalb konnte die falsche Zeile
    stehen bleiben; der neue Test ist ihr erster Leser (`P3-525`).
  · **Ratsche statt Einzelkorrektur:** `test_wandlungstafel_luegt_nicht.c`
    hält seit MF-1288 die Regel „kein LOSSLESS ohne Matrixeintrag",
    Grundlinie **7**, sie darf nur fallen (`P3-526` führt die sieben
    namentlich).
  **Offen bleibt Schritt 4** — Vorwärtsprüfung gegen hxcfe, Korpus ≥ 3,
  danach der Matrixeintrag. Erst er entscheidet, ob aus UNVERIFIED
  „identisch" oder „projiziert identisch" wird (`P3-524`).
- **Beleg:** —

---

## Warteschlange

*(Reihenfolge = Bearbeitungsreihenfolge; oben ist als Nächstes dran)*

### A-018 · Zulieferung `Apple DOS.zip` — DOS-3.3-Dateisystem + BASIC-Detokenisierer
- **Status:** **angehalten am gemessenen Blocker** (das Orakel ist da, ein
  echtes Abbild fehlt) · MF-1207 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „\"C:\Users\Axel\Github\UnifiedFloppyTool-4.1.0\neue-ideen\Apple
  DOS.zip\" finde was ich vergessen habe und verbessere damit das tool"
- **Kennzahl:** **keine der vier.** Die FS-Achse ist keine der vier
  Release-Zahlen (MF-640). Der Wert des Postens ist, dass er `P3-384`
  abträgt — nicht eine Zahl.
- **Kanal:** **Port.** Gemessen: alle fünf Quellen tragen
  `SPDX-License-Identifier: GPL-2.0-or-later`, also eigener Code unter der
  Projektlizenz — keine Lizenzfrage (dieselbe Messung wie MF-1188).
  **Aber `P3-384` stellt eine eigene Bedingung, und sie gilt vor:** sein
  Status sagt „erst Orakel, dann Port" und nennt `catseye/a2tools` (GPL-2,
  837 Zeilen) als fremde Hand, samt Vormessung — kann `a2tools` ein
  LEERES DOS-3.3-Abbild anlegen oder nur ein vorhandenes befüllen? Eine
  zweite Hand desselben Hauses ersetzt das nicht; das ist die Gestalt von
  `apridisk` (MF-1009) und `qrst` (MF-1028), wo Packer und Entpacker
  Spiegelbilder derselben Erfindung waren und der Rundlauf grün.
- **Einfrier-Regel:** **ja** → Rotbeweis zuerst. Es ist die FS-Ebene, kein
  neues Format-Plugin, also greift das Moratorium nicht; die drei
  Bedingungen aus `docs/VERIFICATION_PLAN.md` greifen sehr wohl.
- **OPEN_ITEMS:** **`P3-384`** (wörtlich: „Apple DOS 3.3 fehlt in der
  Dateisystem-Stufenleiter VOLLSTÄNDIG") · berührt `P3-317` (ProDOS-
  Verzeichnisleser fehlt), `P3-228`, `P3-235`
- **Fertig heißt:** ein DOS-3.3-Leser mit Stufe in
  `docs/VERIFICATION_TIERS_FS.md`, die Stufe gegen eine **fremde Hand**
  belegt (nicht gegen den eigenen Erzeuger), Produktivaufrufer im selben
  Commit (D2), Vollsuite grün.
- **Aufwand:** **nicht schätzbar** — er hängt an der Orakel-Vormessung
  oben, nicht am Umfang der Zulieferung.
- **Gemessene Lage im Baum (damit die Lücke belegt ist, nicht behauptet):**
  Apple liegt ausschließlich auf der Behälterachse —
  `src/formats/apple/prodos_po_do.c`, 130 Zeilen, null Verzeichnisbezug
  (MF-710 hat es deshalb aus der FS-Tafel genommen). `applesoft` hat **0**
  Treffer, `track_sector_list` **0**, `TSLIST` **0**; `detokenize` gibt es
  genau **zweimal** und beide Male für **ZX**-BASIC
  (`src/formats/zx/uft_zxbasic.c`). Die 21 `VTOC`-Dateien sind
  **Atari**-VTOC. Zulieferung: 5 Quellen, **1450** Zeilen (268/340 FS,
  112/278 BASIC, 452 Test), **71** `CHECK`-Zusagen; der Bericht hat §0
  „was ich nicht gebaut habe, und warum" und §6 „Belegkette", seine zwei
  Funde sind §2.3 gelöschte Einträge und §2.4 T/S-Listen mit Löchern.
- **Stand:** **ANGEHALTEN, und der Halt ist das Ergebnis** — die eine
  Vorbedingung ist **erfüllt**, die andere ist **gemessen unerfüllbar.**
  · **Das Orakel ist da, und das ist neu.** `P3-384` verlangt „erst Orakel,
    dann Port" und nennt `catseye/a2tools` (GPL-2, 837 Zeilen). Gemessen:
    es **baut und läuft auf dieser Maschine** —
    `gcc -O2 -DDOS -o a2tools_dos.exe a2tools.c` unter MinGW 13.1.0, und
    `a2tools_dos.exe dir <abbild>` antwortet mit seiner Hilfe. Die
    Verteilung geht über `argv[1]` (DOS-Fassung) statt über `argv[0]`
    (UNIX-Fassung, wo `a2ls.exe` ≠ `a2ls` scheitert). **Damit ist die
    erste Hälfte von `P3-384` erfüllt** — und sie war offen.
  · **ABER: es gibt nichts, was es lesen könnte.** Beide Korpusdateien mit
    „dos33" im Namen werden abgewiesen („Not an Apple DOS 3.3 .dsk
    image"), und der Grund ist gemessen: an der VTOC-Stelle (Spur 17,
    Sektor 0, Versatz `0x11000`) stehen in **beiden** die Bytes
    `55 46 54 2D 4B 20 54 31` = ASCII **„UFT-K T1"**. Es sind UFTs eigene
    Selbstbenennungs-Marken; die Dateien sind **Sektorordnungs-Prüfmuster
    ohne Dateisystem**. Das Manifest sagt es selbst: `"origin": "derived"`,
    `"tool": "UFT-eigen (MF-1050) — kein Fremdwerkzeug, daher KEIN
    Stufenkredit"`.
  · **Baumweit nachgemessen:** über **alle** `.do`, `.dsk` und `.po` unter
    `tests/` gibt es **0** Dateien mit einer gültigen DOS-3.3-VTOC
    (Prüfung: DOS-Fassung 3, 35 Spuren, 16 Sektoren, 256 Byte/Sektor).
  · **Folge für die Abschlussbedingung:** sie verlangt „die Stufe gegen
    eine **fremde Hand** belegt (nicht gegen den eigenen Erzeuger)". Ein
    DOS-3.3-Leser wäre baubar — **belegbar wäre er nicht**, und ein Leser
    mit selbstgebautem Prüfstück ist genau der geschlossene Kreis von
    `apridisk` (MF-1009) und `qrst` (MF-1028). Dazu die Lehre aus
    **MF-1021**: ein erzeugtes Fixture ist erst dann ein Beleg, wenn sein
    INHALT nachgewiesen ist.
  · **Der nächste Griff ist damit benannt und klein:** ein **echtes**
    DOS-3.3-Abbild beschaffen. `a2tools` kann **nicht formatieren** (es
    hat `dir`/`out`/`in`/`del`, kein `format`), also muss das Abbild von
    außen kommen — ein zeitgenössisches Objekt oder ein Emulator-Erzeugnis.
    Erst danach ist dieser Posten arbeitsfähig.
- **Beleg:** Halt gemessen MF-1207; Orakelbau
  `tools/uft-scout/work/a2tools/a2tools_dos.exe` (gitignoriert).

### A-019 · Zulieferung `UFT-NN — TR-DOS.zip` — drei behauptete Feldadressen, SCL, Hobeta
- **Status:** **angehalten mit Ergebnis** — alle drei Adressen stimmen
  überein, der Defekt ist seit MF-970 behoben · MF-1207 ·
  **Aufgenommen:** 2026-09-16
- **Wortlaut:** „\"C:\Users\Axel\Github\UnifiedFloppyTool-4.1.0\neue-ideen\UFT-NN
  — TR-DOS.zip\" finde was ich vergessen habe und verbessere damit das"
- **Kennzahl:** **keine der vier.** `trd` steht auf **T1b**, `scl` auf
  **T1** — eine Stufe bewegt sich nicht. Der Posten ist trotzdem Auftrag
  und nicht Fundus, weil ein falsch lesender Erkenner im **erreichbaren**
  Pfad die Missionszeile selbst trifft („keine stille Veränderung"); die
  EINFRIER-REGEL lässt Bugfixes an Bestehendem ausdrücklich zu.
- **Kanal:** **Port** — alle drei Quellen `GPL-2.0-or-later`.
- **Einfrier-Regel:** **ja** → Rotbeweis zuerst, und die drei Adressen
  werden **von Hand gegen die Quelle abgezählt**, bevor eine Zeile fällt.
  Der Grund steht in `P3-281`: der damalige Bericht derselben Familie
  meldete einen echten Fehler und **hätte einen neuen eingebaut** — er
  schlug `0xF4-0xF5` vor, also zwei Byte, und `0xF5` ist bereits das erste
  Zeichen von `disc_name[8]`.
- **OPEN_ITEMS:** Nachbarn **`P3-281`** (dieselbe Klasse, ✅ behoben
  MF-970, aber **andere Datei**: der verwaiste `uft_trd_parser_v2.c`) und
  **`P3-322`** (SCL-Rest). Ein **eigener** Eintrag wird geschrieben, wenn
  die Behauptung gemessen ist — nicht vorher. Diese Liste ist keine
  zweite Befundliste, und eine ungemessene Behauptung ist kein Befund.
- **Fertig heißt:** jede der drei Adressen einzeln gegen zwei
  unabhängige Quellen abgezählt, Rotbeweis pro Adresse, und der
  Widerspruch unten aufgelöst oder als S5 festgenagelt.
- **Aufwand:** **nicht schätzbar.**
- **Ein Widerspruch steht schon jetzt im Raum (S5-Gebiet):** MF-729 hat
  **alle** Sonden geeicht — auf einem Nullpuffer darf nichts ≥ 50 melden,
  wer 50–79 beansprucht, muss ≥ 95 % zufälliger Puffer abweisen — und
  `trd` ist dort namentlich als Gewinner gegen ein PC-160K-Abbild (82)
  gemessen. Liest dieselbe Sonde die falschen Bytes und besteht die
  Eichung dennoch, sagt eine der beiden Messungen nicht, was sie zu sagen
  scheint. Welche, entscheidet der Lauf, nicht der Bericht.
- **Gemessene Lage:** 13 TR-DOS/SCL-Dateien im Baum (zwei Parser, ein
  Plugin, `src/samdisk/trd.cpp`, ein Korpusabbild `gw_trd.img`, ein Test);
  `0x8E4` — die Dateizahl aus MF-1014 — steht in `uft_trd.c`,
  `uft_scl_plugin.c` und drei Tests. Zulieferung: 3 Quellen, **1148**
  Zeilen (306/387/455), **67** `CHECK`-Zusagen; der Bericht nimmt in §6
  eine eigene Behauptung **zurück**.
- **Stand:** **ANGEHALTEN — der gemeldete Defekt existiert nicht mehr, und
  das ist gemessen.** Alle drei behaupteten Feldadressen einzeln gegen den
  Baum abgezählt:
  | Feld | Zulieferung | `src/formats/trd/uft_trd_parser_v2.c` | |
  |---|---|---|---|
  | Dateizahl | `0xE4` | `info->file_count = sys[0xE4];` (:310) | **stimmt** |
  | freie Sektoren | `0xE5` | `sys[0xE5] \| (sys[0xE6] << 8)` (:311) | **stimmt** |
  | gelöschte Dateien | `0xF4`, EIN Byte | `info->deleted_files = sys[0xF4];` (:343) | **stimmt** |
  · **Und der Baum sagt es selbst:** Zeile 314 trägt den Vermerk
    „**MF-970: hier stand `sys[0xE9] | (sys[0xEA] << 8)`.**" — genau der
    Defekt, den die Zulieferung meldet, samt zitierter Rücknahme an Ort und
    Stelle. Das ist der **zweite** Fall dieser Art nach A-008 (dort
    MF-402): eine Zulieferung meldet richtig, was der Baum bereits behoben
    hat.
  · **Das registrierte Plugin liest die Felder gar nicht.**
    `src/formats/trd/uft_trd.c` — die einzige Datei mit
    Plugin-Registrierung — liest **nur** die Dateizahl bei `0x8E4`, und
    zwar für die Sonde. Weder `0xF4` noch `0xE9`/`0xEA` kommen dort vor.
    Die falsche Lesart konnte also seit MF-970 **nirgends** wirksam werden.
  · **Ein zweiter Befund fiel nebenbei an, und er ist derselbe wie bei
    A-018:** `tests/corpus_free/gw_trd.img` (655 360 B) hat **keinen
    echten TR-DOS-Infosektor**. Die Bytes `0x8E0..0x8FF` sind eine lineare
    Rampe mit Schrittweite `0x1F` (`58 77 96 B5 D4 F3 …`) — ein
    erzeugtes Muster auf Behälterebene, kein Dateisystem. Gemessen ergäbe
    es „212 Dateien", „4851 freie Sektoren" und „196 gelöschte" —
    allesamt unmöglich. **Zweites Abbild in Folge, das seinen Namen trägt
    und sein Dateisystem nicht** (MF-1021).
  · **Der Widerspruch aus der Aufnahme (S5-Gebiet) löst sich damit auf:**
    MF-729s Sondeneichung und die Feldlage können beide stimmen, weil die
    Sonde nur `0x8E4` liest und die übrigen Felder nie anfasst.
- **Beleg:** Halt gemessen MF-1207; keine Änderung am Baum nötig.

### A-020 · Zulieferung `UFT-NN — Amiga.zip` — Dateisystem-Diskette oder Trackloader mit DOS-Kopf?
- **Status:** **angehalten am Umfang** (14 Entscheidungsstellen; der Umbau
  überschreitet die Scope-Regel) · MF-1208 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „\"C:\Users\Axel\Github\UnifiedFloppyTool-4.1.0\neue-ideen\UFT-NN
  — Amiga.zip\" finde was ich vergessen habe und verbessere damit das"
- **Kennzahl:** **keine der vier** (MF-640). Der Wert liegt in der
  Missionszeile: eine Diskette, die nur einen DOS-Kopf trägt, darf nicht
  als Dateisystem gemeldet werden — „unbekannt" ist nicht „widerspricht"
  (MF-980/D6).
- **Kanal:** **Port** — alle drei Quellen `GPL-2.0-or-later`; der Bericht
  hat dazu ein eigenes §6 „Lizenzlage".
- **Einfrier-Regel:** **ja** → Rotbeweis zuerst.
- **OPEN_ITEMS:** Nachbarn **`P3-209`** (AmigaDOS-Prüfsumme mehrfach),
  **`P3-210`** (FS-Treiber meldete für jedes ADF ein leeres Verzeichnis),
  **`P0-16`** (HFE→ADF liefert leere ADF), **`P3-385`** (ADFlib-
  unabhängige Zweitmeinung)
- **Fertig heißt:** die vier Zustände unterscheidbar **und** an einem
  Abbild belegt, das kein Dateisystem trägt; kein Zustand, der nur
  behauptet statt gemessen ist; Produktivaufrufer im selben Commit (D2).
- **Aufwand:** **nicht schätzbar.**
- **Gemessene Lage:** die Entscheidung „ist das eine AmigaDOS-Diskette"
  fällt heute an **12** Stellen — `git ls-files` findet 12 Dateien, die
  die `DOS`-Kennung prüfen, darunter `src/analysis/uft_triage.c`,
  `src/algorithms/advanced/uft_bayesian_detect.c`, drei ADF-Parser und
  `src/formats/misc/polyglot_boot.c`. **4** `.c` unter `src/` nennen eine
  AmigaDOS-Prüfsummenfunktion (`mfm_detect.c`, `uft_adf_parser_v3.c`,
  `uft_adf.c`, `src/fs/uft_amigados.c`), während `P3-209` „DREIFACH"
  führt — ob das eine Unterzählung ist oder drei verschiedene Funktionen
  mit ähnlichem Namen, entscheidet der Lauf; hier steht es als **Frage an
  `P3-209`**, nicht als Korrektur an ihm. FS-Stufen:
  `uft_amigados` FS-T2, `uft_amigados_extended` und
  `uft_bootblock_scanner` FS-T1 („alle Tests bauen ihre Eingabe selbst"),
  `uft_fs_amigados_driver` **FS-T0 — kein Test nennt ein Symbol dieses
  Lesers**. Zulieferung: 3 Quellen, **870** Zeilen (268/269/333), **37**
  `CHECK`-Zusagen; §1 heißt „Der Anlass: ein gemessener Gegenbeweis", und
  der wird nachgefahren, bevor er zitiert wird — bei A-006 fielen drei von
  vier meiner eigenen Erwartungen.
- **Stand:** **ANGEHALTEN am Umfang, nicht an der Sache — und die Lage ist
  besser als bei A-018/A-019.** Beide Belegstücke, die die
  Abschlussbedingung verlangt, liegen im Baum, gemessen am Wurzelblock
  (Block 880, Versatz `0x6E000`):
  | Abbild | Kennung | Wurzelblock | Urteil |
  |---|---|---|---|
  | `tests/corpus_free/xdftool_dd_ofs.adf` (901 120 B) | `DOS\0` | `type=2`, `sectype=1` | **echte AmigaDOS-OFS-Diskette, von fremder Hand** (`xdftool`) |
  | `tests/differential/corpus/sources/amiga_dd.adf` (901 120 B) | `00 21 A4 B6` | kein gültiger | **ADF OHNE Dateisystem**, 99,6 % der Bytes ungleich null — der Trackloader-Fall |
  | `dim_adfs_{d,e,f}.adf` | — | — | **Acorn ADFS**, nicht Amiga; gehören nicht hierher |
  · **Was fehlt, ist der dritte Fall:** ein ADF **mit** DOS-Kopf und
    **ohne** gültigen Wurzelblock. Der wäre aus `xdftool_dd_ofs.adf`
    herstellbar, aber dann selbstgebaut — und genau darüber wacht MF-1021.
  · **DER GRUND DES HALTS, gemessen:** die Entscheidung „ist das eine
    AmigaDOS-Diskette" fällt an **14 Stellen** (ohne Fremdcode und ohne
    `uft_hdf_parser`, der RDB prüft, nicht ADF): `uft_bayesian_detect.c`,
    `uft_triage.c`, `mfm_detect.c`, `disk_image_validator.cpp`,
    `forensictab.cpp`, drei ADF-Parser, `uft_adf_plugin.c`,
    `polyglot_boot.c`, `uft_adf.c`, `uft_format_validators.c`,
    `uft_format_versions.c`, `uft_adf_bam.c`, `uft_bootblock_scanner.c`.
    Die Aufnahme sagte 12; der Unterschied ist die Musterweite, nicht die
    Sache. **Das ist die größte Verdopplung dieser Sitzung** — größer als
    die zwölf `volume_serial`-Header aus A-011.
  · **Vier Zustände als 15. Stelle einzuführen wäre `MF-1177`**: eine
    Größe, fünfzehn Rechnungen. Richtig ist EINE Stelle, die die
    vierzehn rufen — und das ist ein Umbau über vierzehn Dateien, weit
    jenseits der Umfangsregel (`.claude/CLAUDE.md` §Scope: >150 Zeilen,
    mehrere Subsysteme → anhalten, größtes fertiges Teilstück liefern).
  · **Das größte fertige Teilstück ist damit die Messung selbst**, und sie
    steht als `P3-467`.
- **Beleg:** Halt gemessen MF-1208; Befund `P3-467`.

### A-021 · Zulieferung `UFT-NN — FAT12 lesen.zip` — FAT12 lesen, wenn die Diskette nicht mehr heil ist
- **Status:** **angehalten vor einer API-Entscheidung** (`size` trägt zwei
  Bedeutungen) · MF-1209 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „\"C:\Users\Axel\Github\UnifiedFloppyTool-4.1.0\neue-ideen\UFT-NN
  — FAT12 lesen.zip\"  finde was ich vergessen habe und verbessere damit das"
- **Kennzahl:** **keine der vier.** FAT12 ist Dateisystem-Ebene (FS-T1 laut
  `docs/VERIFICATION_TIERS_FS.md`), keine Formatstufe. Der Wert liegt in
  der Missionszeile: ein beschädigtes Abbild darf nicht erfundene Daten
  liefern.
- **Kanal:** **Port** — alle drei Quellen `SPDX: GPL-2.0-or-later`.
- **Einfrier-Regel:** **ja** → Rotbeweis zuerst.
- **OPEN_ITEMS:** `P3-15` (FAT12 hat mit `mformat` einen fremden Erzeuger)
  · `P3-62` (erster fremder Prüfvektor für die Nibbelentpackung)
  · `P3-142` (die FAT12/16-Grenze hat ZWEI hergeleitete Werte; dazu gibt
  es ein Tor, gemessen 0)
- **Fertig heißt:** jede neue Prüfung mit benannter Quelle im Header, die
  Kettenfälle (abgerissen, im Kreis) mit Rotbeweis, und **kein** Sektor,
  der erfundene Bytes als `UFT_SECTOR_OK` meldet.
- **Aufwand:** **nicht schätzbar.**
- **Die wichtigste Lage dazu, gemessen:** das ist die **direkte
  Fortsetzung von MF-1183**. Dessen §„Geometrie: BPB, Größe, geraten" ist
  genau der Dreizustand, den ich dort gebaut habe — BPB stimmt mit der
  Dateigröße → nehmen; BPB widerspricht → absagen (MF-1039/MF-1027); kein
  BPB → exakter Treffer in der benannten Achtzeilentafel, sonst absagen.
  Die „fünfte BPB-Prüfung" der Zulieferung gehört deshalb gegen die vier
  gehalten, die seit MF-1183 dort stehen, nicht gegen einen leeren Baum.
  Umfang: 5 Einträge, **966** Zeilen (232 h / 360 c / 374 Test), **51**
  `CHECK`-Zusagen; der Bericht hat §6 „was mir dabei passiert ist" und §9
  „was ich nicht gebaut habe".
- **Stand:** **ANGEHALTEN vor einer API-Entscheidung — und der Befund ist
  schärfer als die fehlende Prüfung.**
  · **Die fünfte Prüfung fehlt wirklich.** Gegen die vier aus MF-1183
    gehalten (`uft_fat_bootsector.c:316-332`): `has_valid_bpb` fällt bei
    (1) `bytes_per_sector` null / keine Zweierpotenz / > 4096,
    (2) `sectors_per_cluster` null / keine Zweierpotenz,
    (3) `fat_count` null oder > 4, (4) `media_type` < 0xF0. **Keine
    Bedingung fragt, ob das beschriebene Dateisystem in das Abbild
    passt** — obwohl die Funktion `first_data_sector`, `data_sectors` und
    `total_bytes` bereits ausrechnet (`:360-375`). Sie vergleicht sie nur
    mit nichts.
  · **DER EIGENTLICHE BEFUND: der Parameter, der das beantworten würde,
    trägt zwei Bedeutungen.** `fat_analyze_boot_sector(data, size, …)`
    benutzt `size` **nur** als `if (size < FAT_SECTOR_SIZE)` — also
    Puffergröße. An den Aufrufstellen heißt es Verschiedenes:
    `uft_win98_fdb_probe()` (`uft_win98_fdb.c:111`) reicht die
    **Sondenpuffergröße** durch, `uft_format_convert_bitstream.c:917`
    reicht `src_size` durch — die **Abbildgröße**. Die fünfte Prüfung auf
    `size` zu bauen wäre für den Wandler richtig und **für die Sonde
    falsch**: sie sagte gültige Abbilder ab, weil der Sondenpuffer kleiner
    ist als die Datei. Klasse `MF-1015` („zwei Aussagen in einem Feld")
    und die Falle aus `MF-1029`.
  · **Warum das anhält statt zu bauen:** die Behebung ist ein
    ausdrücklicher Parameter für die Abbildgröße (0 = unbekannt → keine
    fünfte Prüfung) statt `size` zu überladen. Das berührt **drei
    Produktivaufrufer** und die Tests — eine API-Entscheidung, kein
    Zeileneinschub. Steht als `P3-468`.
  · **Die Missionszeile hängt daran**, und deshalb ist es kein
    Schönheitsfehler: ein BPB, dessen Summe über die Datei hinausreicht,
    führt zu gelesenen Bytes, die es nicht gibt.
- **Beleg:** Halt gemessen MF-1209; Befund `P3-468`.

### A-023 · Zulieferung `Apple-Sektorordnung.zip` — der offene Punkt aus MF-714
- **Status:** **angehalten mit Befund** — die Tafel ist belegt, seit
  MF-1214 von zwei **ausführenden** fremden Händen, und dabei ist ein
  **Defekt an zwei anderen Stellen** gefunden worden (`P3-469`). Der Bau
  bleibt aus, aber **mit berichtigtem Grund**: nicht der A-018-Blocker
  (so stand es in MF-1213, gemessen zu weit gefasst), sondern `P3-422` —
  eine Datenzeile ohne erreichbaren Verbraucher bewegt keine der vier
  Kennzahlen · MF-1211, MF-1213, MF-1214 ·
  **Aufgenommen:** 2026-09-16
- **Wortlaut:** „\"C:\Users\Axel\Github\UnifiedFloppyTool-4.1.0\neue-ideen\Apple-Sektorordnung.zip\"
  finde was ich vergessen habe und verbessere damit das"
- **Kennzahl:** **keine der vier** direkt; die Anordnungsachse ist
  Vorarbeit für Formatstufen (`do`/`po`/`d13` stehen auf T1b/T2).
- **Kanal:** **Port** — alle drei Quellen `GPL-2.0-or-later`.
- **Einfrier-Regel:** **ja** → Rotbeweis zuerst, und die Skew-Tafel gehört
  gegen die vorhandene Datenzeile gehalten, nicht daneben.
- **OPEN_ITEMS:** `P3-422` (`uft_track_layout` hat gemessen **keinen**
  Aufrufer) · berührt `P3-234` (die Apple-6-and-2-Tafel liegt
  **siebenfach**, und nur eine Fassung ist oracle-geprüft)
- **Fertig heißt:** die Ordnung steht als **eine** Datenzeile in
  `include/uft/core/uft_sector_order.h` (MF-1175) — **kein** siebter
  Apple-Tafel-Ort —, mit Produktivaufrufer im selben Commit (D2).
- **Aufwand:** **nicht schätzbar.**
- **Gemessene Lage:** 814 Zeilen (219 / 252 / 343), **48** `CHECK`. Der
  Titel sagt „gelöst (**teilweise**)", §5 heißt „und wo der Weg nicht
  trägt", §7 „was mir dabei passiert ist" — die Zulieferung benennt ihre
  eigenen Grenzen, und §3 („die Skew-Tabelle ist ihr eigenes Inverses")
  ist eine prüfbare Behauptung, die vor jeder Übernahme nachgerechnet
  wird.
- **Stand:** **Vorprüfung durchgeführt — die Behauptung hält, die Achse
  stimmt, der Bau ist ein eigener Commit.**
  · **Die prüfbare Behauptung ist nachgerechnet und trifft.** Die Tafel
    `k_dos_skew[16] = {0x0, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, 0x7, 0x6,
    0x5, 0x4, 0x3, 0x2, 0x1, 0xF}` ist eine **Permutation** von 0–15 und
    **selbstinvers für alle 16** Einträge: zwei Festpunkte (0 und 15),
    sieben vertauschte Paare (1↔14, 2↔13, 3↔12, 4↔11, 5↔10, 6↔9, 7↔8).
    **Eigene Fehlmessung dabei, benannt:** mein erster Auslesevorgang las
    `0x0u` als zwei Dezimalzahlen und meldete „keine Permutation, 26
    Einträge" — die Tafel ist hexadezimal. Zweiter Lauf korrekt.
  · **Die Achse stimmt überein, und das war die offene Entwurfsfrage.**
    `include/uft/core/uft_sector_order.h` (MF-1175) bildet **linearer
    Index ↔ CHS** ab (`uft_order_index_to_chs`, `uft_order_chs_to_index`)
    — und genau das ist die DOS-3.3-Ordnung: Index → (Spur = i/16,
    Kopf 0, Sektor = `skew[i % 16]`). **Es ist also dieselbe Achse, kein
    zweiter Begriff.** Damit ist die Abschlussbedingung „eine Datenzeile,
    kein siebter Apple-Tafel-Ort" erfüllbar.
  · **Gemessen fehlt sie dort noch:** die Aufzählung führt sechs Ordnungen
    (`CHS_INTERLEAVED`, `CHS`, `HCS`, `SERPENTINE`, `INTERLEAVED_DS`,
    `SINGLE_SIDED`), keine Apple-Ordnung; und die Skew-Tafel selbst kommt
    in `src/` und `include/` **0-mal** vor — sie wäre also wirklich neu
    und nicht die siebte Kopie (`P3-234` zählt sieben Apple-**GCR**-Tafeln,
    das ist eine andere Tafel).
  · **Und das Prüfstück liegt bereit:** `tests/corpus_free/
    uftk_dos33_35trk.do` ist laut Manifest ein **Sektorordnungs-Prüfmuster
    mit selbstbenennenden Sektoren** — genau das richtige Objekt, um eine
    Ordnung zu belegen (und genau das falsche für ein Dateisystem, siehe
    A-018).
  · **Nicht gebaut**, und der Grund ist Umfang plus Sorgfalt: es ist Code
    im Format-Layer unter der EINFRIER-REGEL — Rotbeweis zuerst,
    Mutationsmatrix, Produktivaufrufer im selben Commit (D2, Kandidat
    `src/formats/apple/prodos_po_do.c`), Vollsuite. Das ist ein eigener
    Commit, kein Anhängsel an eine Begutachtungsreihe.
- **Nachtrag MF-1213 — der Bau wurde begonnen und hat etwas anderes
  gefunden. Die Reihenfolge dreht sich damit um.**
  · **Die Tafel ist jetzt DREIFACH belegt, und die Rechnung ist geführt.**
    `src/a8rawconv/diska2.cpp:3-9` — **im Baum**, und von UFTs eigenem
    `include/uft/formats/apple/uft_apple_order.h` als Quelle zitiert —
    nennt `kLogicalToPhysicalA2DOS` und `kLogicalToPhysicalA2ProDOS`.
    MAME `ap2_dsk.cpp:449-459` (BSD-3-Clause) führt beide in der
    **Gegenrichtung**, und gemessen ist jede die **exakte Inverse** der
    a8rawconv-Tafel. Aus beiden Quellenpaaren unabhängig gerechnet ergibt
    die DO↔PO-Abbildung **denselben** Wert
    `{0,14,13,12,11,10,9,8,7,6,5,4,3,2,1,15}` — und der trifft die Tafel
    der Zulieferung. **Gegenprobe an UFTs eigenen Zahlen:**
    `uft_apple_order.h` sagt „ProDOS-logische Sektoren 4 und 5, physisch 8
    und 10 … Versätze 0xB00 und 0xA00" — **alle vier treffen** aus MAMEs
    Tafeln gerechnet.
  · **DER BEFUND: der Baum führt die falsche Tafel unter dem richtigen
    Namen.** `src/formats/apple/prodos_po_do.c:25` und
    `src/formats/2img/uft_2img_parser_v2.c:137` tragen beide
    `dos_to_prodos[16] = {0,13,11,9,7,5,3,1,14,12,10,8,6,4,2,15}` — **das
    ist byteidentisch mit `kLogicalToPhysicalA2DOS`**, also logisch→
    physisch, nicht DOS→ProDOS. `uft_2img_parser_v2.c:338` benutzt sie
    unmittelbar als Ordnungswandlung. Steht als **`P3-469`**.
  · **Und der grüne Selbsttest belegt nichts:** `uft_2img_parser_v2.c:
    508-517` prüft den Rundlauf `prodos_to_dos[dos_to_prodos[i]] == i` —
    der gilt **tautologisch**, weil `prodos_to_dos` als Inverse berechnet
    wird. Er hält für jede Permutation.
  · **Warum NICHT gebaut wurde, und das ist der Kern:** beide Fundstellen
    sind **unerreichbar** (`uft_apl_prodos_po_do_open` ein Treffer — seine
    Definition; `img2_dos_to_prodos` `static`, ein Treffer), werden aber
    gebaut. Die Ordnung jetzt in `uft_sector_order.h` einzuführen hätte
    **keinen erreichbaren Verbraucher** — das wäre `P3-422` ein zweites
    Mal, wo `uft_track_layout` gemessen keinen Aufrufer hat. Der einzige
    erreichbare Apple-Verbraucher ist `src/formats/do/uft_do.c` (T2), und
    eine Verhaltensänderung dort ist ohne ein **echtes** DOS-3.3-Abbild
    nicht gegen eine fremde Hand belegbar — **derselbe Blocker wie A-018**.
  · **Reihenfolge damit umgedreht:** erst das Abbild (A-018), dann Ordnung
    **und** Verbraucher in EINEM Commit. Vorher wäre es Vorrat ohne Tür.
- **Nachtrag MF-1214 — das Abbild ist da, und zwei Sätze darüber sind
  falsch.** Der Halt oben sagte, der Bau hänge am A-018-Blocker. Gemessen
  war das **zu weit gefasst**, und die Berichtigung ist der eigentliche
  Ertrag dieses Durchgangs:
  · **Die Tafel steht jetzt auf zwei AUSFÜHRENDEN fremden Händen**, nicht
    nur auf drei Quelltext-Lesungen. MAMEs floptool — seit MF-1083 im Baum
    gebaut — legt mit `flopcreate a2_16sect_prodos prodos_140k` ein
    143 360-Byte-Abbild an und wandelt es mit
    `flopconvert a2_16sect_prodos a2_16sect_dos` in die DOS-Anordnung;
    der Rücklauf ist **byteidentisch**. Aus den Marken **je Spur einzeln**
    abgeleitet: **34 Spuren, EINE Tafel, 0 Sektoren über Spurgrenzen**,
    Wert `{0,14,13,12,11,10,9,8,7,6,5,4,3,2,1,15}`, selbstinvers. Die
    Baum-Tafel trifft **nicht**.
  · **Das leere Abbild hätte nichts belegt** — es trug gemessen nur **7
    von 560** unterscheidbare Sektoren (MF-1021). Spur 0 bleibt deshalb
    floptools ProDOS-Wurzelverzeichnis, die Spuren 1–34 tragen je Sektor
    `UFT-A023 Tnn Snn` im Klartext. Den **Inhalt** liefert damit die
    eigene Hand, die **Permutation allein** MAME — Bauform MF-1084, und
    nötig, weil floptools `prodos` `fr-` ist: formatieren und lesen,
    **kein** Schreiben. `flopwrite` sagt wörtlich ab.
  · **hxcfe bestätigt es, ohne dafür laufen zu müssen.** MF-1067 hat
    gemessen, **dass** `-conv:APPLE2_PO` in **119 420 von 143 360 Byte**
    abweicht — nicht **wie**. Die obige Tafel auf
    `tests/corpus_free/uftk_dos33_35trk.do` angewandt ergibt **genau
    119 420**; die Baum-Tafel ergibt **102 340**. Die Zahl unterscheidet
    die Kandidaten also.
  · **Berichtigung 1:** oben steht „`src/formats/do/uft_do.c` (**T2**)".
    Falsch — `docs/VERIFICATION_TIERS.md:96` führt `do` auf **T1b**
    (MF-1067), Zeile 173 `po` ebenfalls auf **T1b**.
  · **Berichtigung 2:** „**derselbe Blocker wie A-018**" trägt nicht.
    A-018 braucht ein **Dateisystem** (DOS-3.3-VTOC), A-023 ein Abbild
    mit belegbarer **Anordnung**. Das zweite liegt vor, das erste nicht.
    **Dieser Teil ist aber NICHT mein Fund:** `P3-390` hält seit MF-1123
    wörtlich fest, „floptools Dateisystemliste führt unter Apple nur
    ProDOS", und nennt `flopcreate apple_gcr prodos_800k` schon als
    arbeitsfähigen fremden Erzeuger. Meine Nachzählung der neun Familien
    (`prodos coco_rsdos cbmdos unformatted pc_fat hplif isis oric_jasmin
    vtech`) **bestätigt eine vorhandene Messung** — `D3`, kein Wissen
    zweimal halten. Neu ist allein die **abgeleitete** Permutation und
    die hxcfe-Gegenprobe.
  · **Was sich NICHT ändert, und deshalb bleibt der Bau aus:** `P3-422`
    hängt nicht am Abbild. Eine Datenzeile in `uft_sector_order.h` hat
    weiterhin **keinen erreichbaren Verbraucher**, und weil `do` und `po`
    beide schon auf T1b stehen, bewegt der Bau **keine** der vier
    Kennzahlen — nach MF-640 also **Fundus, nicht Auftrag**. Der Halt
    bleibt, aber mit dem richtigen Grund.
  · **Eigener Messfehler, benannt:** zweimal stand `rc=$?` hinter einer
    Pipe und meldete `head`s Status statt den des Prüfskripts; einmal sah
    ein ROT damit nach rc 0 aus — die Klasse aus MF-1040. Ohne Pipe
    nachgemessen: 1 / 0 / 0.
  · **Zurückgezogene Vermutung:** ich hielt `po`s T1b für unbelegt, weil
    **0** versionierte Dateien auf `.po` enden und die Referenzspalten
    leer sind. Es hält — der Kredit hängt an `tests/corpus_free/gw_po.img`
    (greaseweazle 1.23, `origin: cross-tool`, `test_corpus_gw_geometrie`);
    die Endung ist `.img`, die leeren Spalten kommen aus
    `docs/spec_verification.json`.
- **Beleg:** Vorprüfung gemessen MF-1211; Befund `P3-469` gemessen
  MF-1213; von zwei ausführenden fremden Händen bestätigt und zweifach
  berichtigt MF-1214. Der Bau bleibt aus — nicht mehr wegen A-018,
  sondern weil er keine Kennzahl bewegt (`P3-422`, MF-640).

### A-004 · AUFTRAG „UFT offene Punkte, autonome Abarbeitung" — Punkt 7
- **Status:** **angehalten am vereinbarten Schnitt** · **Aufgenommen:**
  2026-09-16 (nachgetragen) · verschoben ans Ende 2026-09-16
- **Wortlaut:** „danach die verbleibenden Punkte aus OPEN_ITEMS.md, nach
  demselben Muster." · zuletzt bestätigt mit „weiter mit punkt 7"; der
  Schnitt ist genehmigt mit „ja, schnitt bei P3-423 ist ok"
- **Kennzahl:** je Teilposten verschieden; die Punkte haben T3 und die
  Wandlungspfade nicht bewegt, sondern Falschaussagen behoben — die
  Kennzahl steht im jeweiligen Commit, nicht hier
- **Kanal:** entfällt (eigener Baum)
- **Einfrier-Regel:** ja für jeden Format-Teilposten → **Rotbeweis zuerst**
- **OPEN_ITEMS:** die Restliste; nach dem Schnitt offen: **P3-425**
  (dieselbe Diskette, zwei Geometrien — `xfd` liest die Quad Density als
  40×2, `atr` weiter als 80×1, und `atr`s Sonde gewinnt mit 95 gegen 40),
  **P3-426** (`tests/CMakeLists.txt` sammelt die Format-Schicht an zwölf
  Stellen mit je einem eigenen GLOB ein, elf davon halten eine
  Aufzählung), **P3-421 Teil 2** (die Rückfallebene von `repo_scope`
  betrifft 16 Skripte), die MF-1162-Punkte und die Eigentümerränge 4–10.
  Neu dazugekommen durch die Arbeit selbst: **P3-439**, **P3-440**,
  **P3-441**.
- **Fertig heißt:** kein Punkt aus der Restliste mehr ohne Commit **oder**
  ohne benannte Stoppbedingung (S1–S5) in `docs/OPEN_ITEMS.md`
- **Aufwand:** nicht schätzbar (Restliste offen)
- **Stand (berichtigt 2026-09-16, MF-1212): der Satz unten ist überholt.**
  „Zwei Commits lokal, nichts gepusht" stimmte, als er geschrieben wurde;
  gemessen stehen **beide auf `origin/main`** — MF-1182 = `1d43b345`,
  MF-1183 = `e5daccf1`, gegangen mit der Welle `76c56535..38d20378`. Der
  **Schnitt selbst bleibt**: er ist vom Eigentümer genehmigt („ja, schnitt
  bei P3-423 ist ok"), und der Posten ist damit nicht offen, sondern
  **entschieden**. Was nach dem Schnitt offen ist, steht unverändert in der
  `OPEN_ITEMS`-Zeile oben; dazugekommen sind seither `P3-463`, `P3-465`,
  `P3-466`, `P3-467` und `P3-468`.
  <br>Die ursprüngliche Fassung bleibt zitiert stehen:
- **Stand (ursprünglich):** **Schnitt erreicht, zwei Commits lokal, nichts gepusht.**
  MF-1182 (P3-406, Sonden-Doktrin Regel 2) und MF-1183 (P3-423, Geometrie
  aus dem BPB) liegen; `git rev-list --count origin/main..HEAD` = **2**.
  Beide Male Bau 0 Warnungen, Suite **493/493 grün** mit einem benannten
  Skip (`test_freezer`), `check_consistency` 0, alle Vorhaken grün.
  **Eine Lehre aus dem Lauf, die beim Wiederaufnehmen gilt:** die
  Generator-Reihenfolge ist vollständig zu fahren —
  `gen_verification_tiers.py --write` → `gen_fs_tiers.py` →
  `gen_erzeuger_zensus.py` → `update_inventory.py` → `gen_stand.py`
  **zuletzt**. `kette1182.sh` ließ die ersten zwei aus, und
  `check_consistency` brach mit `[verification tiers stale]` und
  `[FS tiers stale]` ab — der Fehler lag im Kettenskript, nicht im Baum.
  Die fertige Kette liegt als `kette1183.sh` im Job-Kratzverzeichnis.
  **Nächster Griff beim Wiederaufnehmen:** P3-425, weil dort zwei Leser
  dieselbe Nutzlast verschieden zerlegen und das Erkennungsrennen die
  falsche Geometrie gewinnt — ein messbarer Befund mit Rotbeweis-Ort.
- **Beleg:** MF-1175…MF-1181 = `90c4f88b..76c56535`; **MF-1182 =
  `1d43b345`**; **MF-1183 = `e5daccf1`**. Der Posten bleibt offen, weil die
  Restliste offen ist — nicht weil ein Beleg fehlt.

---

### A-025 · Alle offenen Formate so hoch wie möglich (T1b, sonst T1) — zwei Stränge
- **Status:** **angehalten an fünf Gegenständen und einer Entscheidung**
  (2026-09-17, nach MF-1226) — Strang A **vorgelegt** (MF-1221), Strang B
  **4 von 9 gehoben** (`adf_ext`, `cas`, `fdi_pc98`, `fds`), **5 offen**
  · **Aufgenommen:** 2026-09-17
- **Wortlaut:** „Alle, mach einen nach dem anderen, mach dir einen Plan,
  der sinnvoll ist, und arbeite es ab" — vorausgehend „Mach was möglich
  ist, Ziel ist T1b ,wenn es Probleme macht dann T1"
- **Kennzahl:** **ungeprüfte Formate (T3) runter** — die erste der vier
  aus MF-640. Stand T1=8, T1b=66, T2=11, T3=1.
- **Kanal:** **Daten/Fixture** (Beschaffung). Für Strang A entfällt er:
  das ist eine Entscheidung, kein Fund.
- **Einfrier-Regel:** **nein** — Beschaffung und Tests sind ausdrücklich
  erlaubte Verifikationsarbeit. **Ja**, sobald ein Leser geändert würde.
- **OPEN_ITEMS:** `P3-474` (die vierfache Messung, warum heute nichts
  hebt) · `P3-403` (Strang A) · `P3-359`, `P3-356` (Lizenzbestand)
- **Fertig heißt:** je Format **entweder** eine Stufe mit
  Manifest-Eintrag **und** Test, **oder** ein benannter Halt mit Grund
  und nächstem Griff. Kein Format bleibt ohne Aussage.
- **Aufwand:** **nicht schätzbar** — er hängt an Quellen und
  Lizenzurteilen, nicht an Aufwand im Baum.
- **DER PLAN, und warum er zwei Stränge hat statt einem:** die zwölf
  offenen Formate haben **nicht** denselben Blocker. `P3-403` hat
  gemessen, dass bei flachen Abbildern mit gleichförmiger Sektorgröße
  **jedes** passende Tripel dieselben Bytes in derselben Reihenfolge
  liest — dort kann **kein** Werkzeug und **kein** echtes Abbild die
  Geometrie belegen. Für diese drei wäre jede Beschaffung vergeblich;
  sie zuerst zu trennen verhindert, dass Arbeit in eine Sackgasse geht.

  | Strang | Formate | Art |
  |---|---|---|
  | **A** — Geometrie prinzipiell unbelegbar (`P3-403`) | `akai_s900`, `korg_dss1`, `syn` | **Entscheidung** |
  | **B** — Werkzeug/Abbild fehlt | `adf_ext`, `cas`, `dim`, `fdi_pc98`, `fds`, `lisa_twiggy`, `nfd`, `pro`, `udi` | **Beschaffung** je Format |

  **Strang B wird Format für Format abgearbeitet, nach Lizenz-Sauberkeit
  sortiert, nicht alphabetisch** — je Format: Quelle benennen,
  Lizenzurteil, dann erst holen. Für jedes Ergebnis gilt die
  Manifest-Pflicht (`origin`, `tool`, `source`, `sha256`) **plus ein
  ctest, der das Abbild durch das UFT-Plugin öffnet** — ohne diesen Test
  rechnet `gen_verification_tiers.py` keine Stufe, egal wie echt die
  Datei ist.
- **Stand:** **Strang A ist vorgelegt, nicht entschieden** — MF-1221
  schreibt `P3-403` fort und liefert die Auswahl (a) auf T2 bleiben /
  (b) eigene Stufe für „flach, gleichförmig, linear" / (c) T1 zulassen
  und den Vorbehalt in die Tafel schreiben, je mit Kosten. **Zwei
  Messungen sind dabei neu:** der Rückfall auf T1 löst die Frage
  **nicht** (die Herkunft belegt Echtheit, nicht Geometrie), und die
  Stufe würde **mechanisch trotzdem** steigen, weil `compute_tiers()`
  keine Bedingung „der Test muss diskriminieren" kennt — genau der von
  `MF-1077` verbotene Zug. **Berichtigt:** `syn` gehört in diese Klasse
  (in MF-1220 stand es als Beschaffungsfall), `lisa_twiggy` nicht.

  **STRANG B, 1 von 9: `adf_ext` ist auf T1b (MF-1222).** Und die
  Reihenfolge „nach Lizenz-Sauberkeit" hat sich sofort bezahlt: der
  Erzeuger ist **Unlicense / public domain** — die sauberste Lage im
  Baum, sauberer als jedes GPL-Oracle — und er lag die ganze Zeit
  **im Baum, ungebaut** (`neue-ideen/disk-utilities-master.zip`).
  Warum ihn `P3-474` vierfach als „kein Erzeuger" gemessen hatte, ist
  jetzt selbst gemessen: der Zensus ordnet Werkzeuge über **Endungen**
  zu, und `adf_ext` teilt `adf` mit dem längst gehobenen `adf`. Direkt
  nach **Modulnamen** gefragt bleibt das Nein für die übrigen acht
  bestehen (hxcfe: `AMIGA_EXTADF;R`, sonst kein Modul; floptool: keines;
  libdsk: keiner seiner 27 Typen). Nebenbei fiel ein Defekt: die
  Spurlänge in **Bit** stand in der Datei und wurde verworfen.
  **STRANG B, 2 von 9: `cas` ist auf T1b (MF-1223) — ohne eine Zeile
  Codeänderung.** Der Leser war seit MF-1040 richtig; gefehlt hat die
  fremde Hand, und gefunden wurde sie, weil diesmal nach dem **Werkzeug**
  gesucht wurde statt nach einem Abbild: `wav2cas` aus
  `joyrex2001/castools` (GPL-2, `COPYING` im Paket gemessen). **Und der
  Grund, mit dem MF-1040 T1b verneint hatte, ist hier der Beleg** — der
  Umweg durch eine FSK-Wellenform (1584 B → 1 853 036 B WAV → 1584 B,
  0 abweichend) schließt ein Durchreichen aus. MAMEs `imgtool` wurde
  geprüft und **verworfen**: sein `fmsx_cas` ist die Referenz des Lesers,
  das wäre die `dms`-Falle. Drei Hände, 16 Zusagen, und dass der Test rot
  werden kann, ist vorgeführt (gekürztes Abbild → 5 Zusagen fallen).

  **STRANG B, 3 von 9: `fdi_pc98` ist auf T1b (MF-1224) — durch eine
  Eigentümerentscheidung, nicht durch eine Suche.** Wortlaut vom
  2026-09-17: „Lizenzentscheidung (fdi_pc98, nfd) ja wir machen das".
  `pc98-disk-tools` trägt **keine Lizenz** (kein Lizenzwort im ganzen
  README, vollständig gelesen); gehandhabt wie `dtc`/`epstool` —
  **ausführen ja, weitergeben nein**, und dass das Erzeugnis im Korpus
  liegen darf, hängt an einer Messung: 4064 Kopfbytes alle Null, keine
  ASCII-Kette, kein Werkzeugstempel. **Der schwächste der drei Belege**,
  und das steht überall dabei: `hdm_to_fdi.py` modelliert nichts, es
  stellt einen Kopf voran und hält die Geometrie fest verdrahtet —
  belegt ist die Behälter-Zerlegung, nicht die Geometrie-Herleitung,
  weshalb `hxcfe` (`NEC_FDI` → IMD, 1232/1232) als dritte Hand Pflicht
  ist. **Der Rotbeweis fand dabei einen echten Defekt:** `open()` prüfte
  keine der zwei Konsistenzen, die seine eigene Sonde prüft — ein Kopf,
  der über die Geometrie log, wurde mit Erfolg geöffnet und **16
  Sektoren = 16 384 Byte fielen still weg**. Behoben, 21 Zusagen.
  **Zwei Berichtigungen, beide meine:** die Entscheidung deckt `nfd`
  **nicht** ab (das Paket erwähnt NFD in keiner seiner 14 Dateien —
  dort fehlt ein Schreiber, nicht eine Lizenz), und die alte
  CMake-Begründung „keines der hier verfügbaren Werkzeuge schreibt eine"
  war zum dritten Mal an einem Tag dieselbe Populationsfrage.

  **STRANG B, die restlichen SECHS sind einzeln befragt (MF-1225) — und
  keines lässt sich heute heben.** Gesucht wurde nach **Schreibern** und
  nach Modulnamen, nicht nach Endungen; fünfte Werkzeugfamilie neu
  gemessen: `gw` führt `dim.py` **und** `nfd.py`, beide mit
  `read_only = True` (am Quelltext, nicht über die Endungstafel).

  | Format | Grund, gemessen |
  |---|---|
  | `dim` | **kein Schreiber** — gw read-only, hxcfe `;R`, floptool `r-`, libdsk kennt es nicht; `fathuman`/`dis68k`/`x68000-floppy-tools` **lesen** |
  | `nfd` | **kein Schreiber** — `98imgtools` (Unlicense!) schreibt **NHD**, das Festplattenformat, nicht NFD; ein Buchstabe, anderes Format |
  | `pro` | APE kommerziell; **Altirra GPL-2**, Schreiben unbestätigt (eine offene Messung); `atari800` ist **dieselbe Hand** wie UFTs Referenz |
  | `fds` | **Schreiber da** (`fdstool`, `fdtc`, `qd2fds`) — **alle ohne Lizenz** → Eigentümerentscheidung, Abstammung wäre sauber |
  | `udi` | **doppelt blockiert**: `trx2x` hat keine Lizenz **und** ist von Makeev, dessen Spezifikation UFTs Leser ist — dieselbe Hand |
  | `lisa_twiggy` | LisaEm **GPL-2** (sauber), aber raw↔dc42 modelliert die Zonierung nicht; echtes Abbild = Quellenfrage |

  **Eines von sechs** (`fds`) hängt an einer Entscheidung derselben Art,
  die für `pc98-disk-tools` schon gefallen ist. **Eines** (`pro`) an
  einer einzigen Messung. Die übrigen vier an Gegenständen, die es
  womöglich nicht gibt.

  **STRANG B, 4 von 9: `fds` ist auf T1b (MF-1226) — und die Zeile
  darüber trug eine falsche Lizenzmessung, die hier zuerst berichtigt
  wird.** Sie sagt „`fdstool`, `fdtc`, `qd2fds` — **alle ohne
  Lizenz**". Gemessen war das **API-Feld** (GitHub/GitLab melden nur die
  *Projekteinstellung*), nicht die **Datei**: `fdtc` trägt ein `COPYING`
  mit **BSD-3-Clause**, „Copyright 2024 Matthew Gilmore", 1457 Byte.
  Dieselbe Falle, die in derselben Tabelle bei `lisa_twiggy` **richtig**
  aufgelöst wurde (LisaEms `NOASSERTION` ist GPL-2) — dort habe ich die
  Datei gelesen, bei `fdtc` nicht. **Regel: wer eine Lizenz feststellt,
  liest die Lizenzdatei.** Für `fdstool` bleibt „keine Lizenz" richtig
  (README **10 Byte**, keine Lizenzdatei) — die Eigentümerentscheidung
  vom 2026-09-17, wörtlich **„fds auch, ja machen"**, war also nur für
  dieses eine Werkzeug nötig, und der **Behälter** des Belegs kommt
  bewusst vom BSD-3-lizenzierten `fdtc`.

  Kette, gemessen: zwei selbstbenennende 8192-Byte-Nutzlasten →
  `bintofdf` (Typ-3-Kopf mit **8-Byte-Namen**, 2 × 8209 B) → `fdtc`
  (Typ-1 56 B mit `*NINTENDO-HVC*` + Typ-2 2 B = **16 476**) →
  Polsterung auf 65 500 → `fdstool -a` → **65 516 B**, sha256
  `d58dc05a…`, Arbeitsbaum == Blob, `text: unset`. Von UFT kommen **nur
  die Nutzdaten**. **22 Zusagen, 0 rot**, Hebung **ohne eine Zeile
  Codeänderung**; Rotbeweis **durch den ctest-Pfad**: ein gekipptes
  Nutzlastbyte senkt „32 von 32 Brocken" auf **31 von 32**, rc 1.
  **Befund am Orakel:** `fdstool.c:641` läuft `x < 3` über ein **8
  Byte** langes Namensfeld — der Test prüft die Namen deshalb selbst.
  **Bestätigt von fremder Hand:** `fdstool.c:19` definiert
  `FDS_LENGTH 65500`. **Hausregel bleibt Hausregel:** 128 × 512 = 65 536,
  die 476 Byte des letzten Sektors rechnet der Test selbst nach.

  **Offen sind jetzt fünf:** `dim`, `nfd` (kein Schreiber gebaut), `pro`
  (Messung an Altirra), `udi` (Abstammung), `lisa_twiggy` (Abbild).
- **Stand beim Anhalten (2026-09-17, nach MF-1226) — ANGEHALTEN, und der
  Halt ist gemessen, nicht geschätzt.** Vier von neun sind gehoben, und
  jede Hebung hatte dieselbe Ursache: `P3-474` sagte „kein Erzeuger", und
  viermal war die Messung richtig und die befragte **Population** zu
  klein. MF-1225 hat die restlichen fünf deshalb **einzeln** befragt —
  nach Schreibern, nach Modulnamen statt Endungen, über den Baum
  hinaus — und je Format bleibt ein **Gegenstand** übrig, keine
  Anstrengung:
  · `dim`, `nfd` — **kein Schreiber gebaut**. Fünf Werkzeugfamilien
    gemessen (gw read-only am Quelltext, hxcfe `;R`, floptool `r-`,
    libdsk kennt sie nicht); die gefundenen X68000-Werkzeuge lesen, und
    `98imgtools` schreibt **NHD** (Festplatte), nicht NFD.
  · `pro` — **die EINE noch offene Messung, und sie ist der nächste
    konkrete Schritt dieses Postens:** schreibt **Altirra** (GPL-2)
    `.pro`? Unbestätigt. Sein Bau ist eine Visual-Studio-Sache, also
    gemessen teuer auf dieser Maschine. `atari800` ist **dieselbe Hand**
    wie UFTs Referenz (`sio.c`, Quellstand `b6bdf05c`) und fällt aus.
  · `udi` — **doppelt blockiert**, und der zweite Grund ist der
    schwerere: `trx2x` schreibt UDI, hat aber keine Lizenz **und** stammt
    von **Alex Makeev**, dessen Spezifikation UFTs Leser IST
    (`uft_udi_plugin.c:8`, samt wörtlich übernommenem Prüfsummen-
    Referenzcode). Eine Lizenzentscheidung allein hebt es NICHT.
  · `lisa_twiggy` — **Lizenz sauber** (LisaEm GPL-2), aber `raw↔dc42`
    modelliert die Zonierung nicht, die das Format ausmacht. Ein echtes
    Twiggy-Abbild ist eine Quellen- und Lizenzfrage wie `P3-359`.
  **Strang A ist vorgelegt und wartet auf den Eigentümer** (`P3-403`,
  drei Wege). Dazu neu aus MF-1226: **`P3-477`** — 18 Formate auf T1/T1b
  begründen wörtlich „T2 und nicht T1b", je Fall aus dem MF-Eintrag der
  Hebung zu holen; das ist Arbeit an DIESEM Posten, aber keine Hebung.
  **Nicht angefasst und ausdrücklich nicht verloren:** der
  FDS-nach-QD-Umweg (`fdstool -c`), der ein stärkerer Beleg wäre.
- **Beleg:** `P3-403` fortgeschrieben (MF-1221); `adf_ext` T2 → **T1b**
  (MF-1222, `0809351a`), `P3-475` neu; `cas` → **T1b** (MF-1223,
  `e0490dea`); `fdi_pc98` → **T1b** (MF-1224, `90a44445`), `P3-476` neu;
  die sechs restlichen einzeln befragt (MF-1225, `f38ec8f5`); `fds` →
  **T1b** (MF-1226, `b8107fd0`), `P3-477` neu. Tafel **T1b 66 → 70,
  T2 11 → 7**; Strang B 5 offen.

### A-026 · xDMS 1.3.2 zerlegen, gegen `uft_dms.c` abgleichen, Lücken implementieren
- **Status:** **angehalten nach dem ersten Durchgang** (2026-09-17,
  MF-1227 `a697fa91`) — vier Befunde stehen als `P3-478`, und `A-027`
  wurde vom Eigentümer vorgezogen · **Aufgenommen:** 2026-09-17
- **Wortlaut:** „nimm den code komplett auseinander , sehr genau / finde
  alles und alles raussuchen was ich übersehen habe , stimme es mit mein
  aktuellen tool ab / - wo können die formate verbessert werden / - ist es
  auf andere formate übertragbar / - welche einstellungen fehlen noch /
  - was habe wir noch nicht und implemtiere es in das tool"
- **Gegenstand, gemessen bei der Aufnahme:** `xdms-1.3.2.tar.bz2`,
  **43 010 Byte**, sha256 `367ec4f02dd6a3a2…`, liegt in `~/Downloads`
  (nicht im Baum). Eigener Bestand: `src/formats/dms/uft_dms.c` **1035**
  Zeilen, `uft_dms_plugin.c` **292**, `include/uft/formats/uft_dms.h`
  **178**, drei Tests (`test_uft_dms.c` 722, `test_dms_gegen_adf2dms.c`
  327, `test_dms_plugin_gegen_bibliothek.c` 339), ein Korpusstück
  (`adf2dms_uftk_rle_880k.dms`).
- **Kennzahl:** **keine der vier für den Abgleich selbst** — `dms` steht
  seit MF-1135 auf **T1b**, und T3 besteht nur noch aus `syn`. Zwei Wege,
  auf denen sie sich doch bewegt: **(a)** die Übertragbarkeitsfrage des
  Auftrags — trägt xDMS' Maschinerie eines der **5** offenen
  Beschaffungsformate (`dim`, `nfd`, `pro`, `udi`, `lisa_twiggy`) oder
  `syn`, ist es *ungeprüfte Formate runter*; **(b)** MF-1135 hat die
  **Betriebsarten quick/medium/deep/heavy ausdrücklich als NICHT BELEGT**
  festgehalten — das zu schließen ist Verifikationsarbeit am vorhandenen
  Beleg. Nach MF-640 wäre der Rest **Fundus**; der Eigentümer hat die
  Umsetzung aber ausdrücklich angeordnet („implemtiere es in das tool"),
  und die Priorität setzt er, nicht ich.
- **Kanal:** **Port** — und es ist der seltene Fall, in dem der wirklich
  offensteht: xDMS ist **Public Domain**, belegt gegen Debians
  `copyright` (SCOUT-10, SPDX `LicenseRef-PublicDomain-xDMS`; eine
  formlose PD-Erklärung wird dort **zitiert**, nicht in eine Lizenz
  umgedeutet). Die Kanaltafel in `CLAUDE.md` §MF-695 führt bei *Port*
  bis heute einen Gedankenstrich als Beispiel — `uft_dms.c` ist einer.
- **Einfrier-Regel:** **ja, der Auftrag liegt mitten darin → Rotbeweis
  zuerst.** Abgleich, Bugfix an Bestehendem und Korpusarbeit sind
  ausdrücklich erlaubt; ein **neues Plugin** für eine gefundene Spielart
  fällt unter das Moratorium, auch als Vorschlag (1:2 nach MF-363/498).
- **OPEN_ITEMS:** `P3-71`, `P3-347`, `P3-373`, `P3-474`, `SCOUT-10`
- **Fertig heißt:** für **jede** von xDMS unterstützte Kompressionsart
  steht im Baum entweder ein grüner Testfall oder eine benannte Absage
  mit Grund, jede Abweichung zwischen `uft_dms.c` und der xDMS-Quelle ist
  als Befund mit `Datei:Zeile` festgehalten, und `ctest -R dms` ist grün.
- **Aufwand:** nicht schätzbar — der Umfang hängt daran, was der Abgleich
  findet; der Tarball ist bei der Aufnahme nicht entpackt worden.
- **Stand (2026-09-17, erster Durchgang):** **Der Quellstand ist
  vollständig gelesen** — 1890 Zeilen `.c` in 12 Dateien plus 178 Zeilen
  Header, dazu Handbuchseite, `xdms.txt` und `ChangeLog.txt`.

  **Drei Antworten stehen schon, und zwei davon fallen kleiner aus als
  die Frage:**
  · **„welche Einstellungen fehlen noch"** — gemessen **keine aus dem
    Fassungsunterschied**. 1.3 → 1.3.2 ist laut `ChangeLog.txt` 1.3.1 =
    reine Portierung (C99-`stdint.h`, `tmpnam()` → `mkstemp()`) und
    1.3.2 = **genau eine** Neuerung, `-f` „override errors … for
    desperate data salvation" — und die liegt als `override_errors`
    schon in `dms_unpack()`. Von xDMS' Bedienseite (`u b d f t v x z`,
    `-d -f -p -q -v`) sind `-p` (Kennwort) und die Befehle `b`/`d`
    (Banner, FILEID.DIZ) die einzigen mit Substanz, und die hängen
    nicht an einer fehlenden Einstellung, sondern an zwei `NULL`s im
    Plugin → `P3-478` (1) und (2).
  · **„ist es auf andere Formate übertragbar"** — **nein, und das ist
    gemessen statt vermutet.** Die fünf Entpacker (RLE, QUICK, MEDIUM,
    DEEP, HEAVY) sind an DMS-Eigenheiten gebunden: ein gemeinsames
    `text[]`-Wörterbuch, dessen Zustand über Spurgrenzen läuft
    (`flags & 1`), Huffman-Bäume, die aus dem Vorsatz übernommen werden
    (`flags & 2`), und fensterabhängige Endkorrekturen
    (`quick_text_loc + 5`, `medium_text_loc + 66`, `deep_text_loc + 60`).
    Kein anderes Format dieses Baums hat diese Verschränkung. Was
    übertragbar WÄRE, ist nicht der Code, sondern die **Bauform**: ein
    Spur-Callback, der je Einheit sagt, ob sie bestätigt ist — genau der
    Mechanismus, der hier ungerufen lag.
  · **„wo können die Formate verbessert werden"** — der Port ist TREU
    (acht Kopffelder, acht Versätze, gleiche 3-Byte-Lesart), also nicht
    am Port. Die Lücken sind **Erreichbarkeit**: `P3-478`.

  **Behoben und belegt in diesem Durchgang** (zwei Instanzen derselben
  Klasse — die Bibliothek kann es, das Plugin ruft es nicht):
  · **der ungerufene Spur-Callback.** Gemessen an einer DMS mit EINEM
    gekippten Byte im Spursatz 40: vorher **1760 von 1760** Sektoren
    `UFT_SECTOR_OK`, **0** gekennzeichnet, während die Selbstbenennung
    nur 1759-mal traf. Jetzt 1738 OK / **22 gekennzeichnet**, und die
    Warnung nennt „1 von 80 Spursätzen NICHT bestätigt" neben den
    „901 120 von 901 120 Byte". Rotbeweis 16 grün / **3 rot** vorher.
  · **die Sonde verglich vier Byte und meldete 98.** Jetzt nach
    `docs/SONDEN_DOKTRIN.md` abgeleitet: **100** für das echte Archiv,
    **50** für vier Byte, **50** für einen verfälschten Kopf-CRC — vorher
    98/98/98, also **kein** Unterschied. `docs/sondendoktrin_baseline.txt`
    fällt 83 → **82**.
  · Nebenbefund in der Bibliothek: `checksum_ok` war mit **1**
    vorbesetzt und wurde nur im Erfolgszweig überschrieben — bei
    fehlgeschlagenem Entpacken meldete der Callback also „Prüfsumme gut"
    für eine Spur, deren Prüfsumme nie gerechnet wurde (MF-980). Jetzt
    ist „nicht bestätigt" die Vorbesetzung, und `decomp_ok` sagt warum.

  **Offen und benannt: `P3-478`** (verschlüsseltes Archiv bekommt die
  falsche Diagnose · Banner und FILEID.DIZ werden verworfen · HD-Archiv
  zur Hälfte unerreichbar · xDMS liest 8 von 19 Kopffeldern, und eine
  zweite Quelle nennt CPU, Maschinentyp und Taktrate). Und die
  **Modus-Abdeckung bleibt 1 von 7** mit `flags = 0` durchgehend —
  `adf2dms` kann die übrigen gemessen nicht herstellen.
- **Beleg:** erster Durchgang **MF-1227**, `a697fa91` — Spur-Callback
  verdrahtet (Loch: 1760/0 → 1738/22), Sonde abgeleitet (98/98/98 →
  100/50/50), `checksum_ok` nicht mehr mit 1 vorbesetzt; `P3-478` neu;
  `docs/sondendoktrin_baseline.txt` 83 → 82; 26 Zusagen, zwei
  Rotbeweise (3 rot bzw. 8 rot vorher). **Der Posten bleibt offen**,
  weil `P3-478` (1)/(2)/(4) machbar sind und nicht gemacht sind.

### A-027 · Teilstring-Fallen: Prüfer übernehmen und alle Fundstellen abtragen
- **Status:** **angehalten nach dem ersten Durchgang** (2026-09-17, MF-1228 `bd1957ea`; A-028/A-029 vorgezogen, siehe Stand) · vorher in Arbeit:
  „führe das Das Teilstring-Paket aus und finde alles und fixe es")
  · **Aufgenommen:** 2026-09-17
- **Wortlaut:** „gibt es viele Teilstring-Fallen in meinem tool , fixe
  sie alle !!" — dazu das Paket `neue-ideen/UFT-NN — Teilstring-Fallen
  Prüfer.zip`; und vorgezogen mit „führe das Das Teilstring-Paket aus
  und finde alles und fixe es"
- **Gegenstand, gemessen:** 56 175 Byte, 8 Einträge; `neue-ideen/` ist
  gitignoriert (`.gitignore:85`). Inhalt: `substring_audit.py`
  (18 344 B, **acht Regeln**), `c_lex.py` (6 917 B, C-Zerteiler),
  `uft_match.h`/`uft_match.c`, `run_audit_selftest.py`, `test_match.c`,
  `fixture_traps.c`, `Makefile`. **Keine Lizenzdatei im Paket**
  (gemessen) — nach MF-636 gehört die Herkunft ausgesprochen, bevor
  `uft_match.c` in den Baum geht.
- **Kennzahl:** **keine der vier** — Verlässlichkeit des Prüfstands,
  wie `P3-421`, `P3-426`, `P3-461`, `P3-477`. Der Eigentümer hat die
  Umsetzung ausdrücklich angeordnet.
- **Kanal:** entfällt für die Herkunft (Zulieferung für dieses Projekt,
  das Dokument sagt selbst „Nummer bitte selbst zuweisen").
- **Einfrier-Regel:** **ja → Rotbeweis zuerst.** Nicht wegen
  `uft_match.c` (Hilfseinheit), sondern wegen der Fundstellen: C1/C2
  betreffen `strstr` in **Erkennungspfaden**. Dazu D2 (Aufrufer + Test,
  der rot wird, wenn der Aufruf verschwindet).
- **OPEN_ITEMS:** wird angelegt, sobald die Zahl **geprüft** ist — ein
  Eintrag mit „viele" wäre die unbelegte Zahl, gegen die das Paket
  geschrieben ist.
- **Fertig heißt:** `substring_audit.py` läuft im Torkanon und meldet
  **0 Fundstellen der Einschätzung „sicher"**; jede übrige ist behoben
  oder in ihrer Datei bzw. in `OPEN_ITEMS.md` als bewusst benannt; und
  `run_audit_selftest.py` ist grün, damit der Prüfer selbst geprüft ist.
- **Aufwand:** nicht schätzbar.
- **Stand (2026-09-17, Lauf gemacht):** **Der Prüfer ist gelaufen, und
  seine Zahl ist NICHT die Zahl der Fallen.**
  · Selbsttest zuerst: **bestanden**, 9 Funde in der Köderdatei,
    10 Zerteilerfälle + 3 Gegenproben, 0 Fehler.
  · **Erster Lauf STÜRZTE AB** statt zu urteilen:
    `UnicodeEncodeError` auf cp1252 beim Drucken eines Schnipsels mit
    `∕` (U+2044) aus `src/formats/reference/uft_floppy_reference.c:168`
    — und der Rückgabewert war **1**, sah also wie „hat gefunden" aus.
    Das ist wörtlich die Klasse, die MF-1171 benennt („ein Absturz ist
    kein Urteil"). Das Werkzeug wird **nicht** gepatcht, die Umgebung
    liefert die Kodierung: `PYTHONIOENCODING=utf-8`.
  · Danach gemessen über `src` und `include`: **325 Fundstellen**,
    davon **70 „sicher"** und **255 „prüfen"** —
    C1 84 (prüfen) · C2 43 sicher + 6 prüfen · C3 2 sicher ·
    C4 4 sicher · C5 21 sicher · C6 165 (prüfen).
  · **BEFUND AM PRÜFER, gemessen an seinem Quelltext:**
    `NCMP_FNS = {'strncmp','strncasecmp','memcmp','strncpy','strncat'}`
    wirft **Kopier- und Vergleichsfunktionen in einen Topf**, und
    C4/C5 haben nur für Vergleiche Sinn. Alle **21** C5-Fundstellen
    sind `strncpy`, und `strncpy(dst, "lit", n)` mit `n > strlen(lit)`
    ist **korrekt**: der Standard füllt den Rest mit Nullbytes und
    liest die Quelle nur bis zu ihrer Null. Der Befundtext sagt dabei
    selbst „bei strncmp/memcmp ist das undefiniertes Verhalten" —
    während er `strncpy` anschlägt. Das ist die eigene Klasse des
    Prüfers, auf ihn selbst angewandt.
    **Umgekehrt ist C4 bei `strncpy` schärfer als gedacht:** dort
    bedeutet `n < strlen(lit)` Kürzung **und** fehlenden Nullabschluss.
  · Damit ist die Zahl „viele" weder bestätigt noch widerlegt — sie
    wird je Regel einzeln geprüft, und der Prüfer selbst braucht
    zuerst die Trennung von Kopieren und Vergleichen.

  **Zweiter Abschnitt: der Prüfer ist übernommen und berichtigt.**
  · Übernommen in Hausform: `scripts/audit_teilstring.py`,
    `scripts/c_lex.py` (Nachbar von `c_literal.py`),
    `tests/formats/fixture_traps.c`, **Tor 66** in
    `check_consistency.py`, fallende Grundlinie
    `docs/teilstring_baseline.txt`.
  · **Acht** gemessene Defekte behoben — die sechs oben plus zwei, die
    erst die Übernahme fand: **(G)** `audit_selbsttest.py::lade()`
    registrierte das Modul nicht in `sys.modules`, womit jedes Werkzeug
    mit `@dataclass` **und** future-annotations **nicht ladbar** war
    (latent; gemessen genau zwei Dateien mit beiden Zutaten, das neue
    Tor als erstes, das ihn auslöst). **(H)** Mein eigener Fehler:
    `walk()` baute den `repo_scope`-Filter aus dem Skriptort statt aus
    dem untersuchten Baum → das Tor war im gepflanzten Prüfbaum
    **blind**, drei gepflanzte Defekte nicht gemeldet. Gefunden hat es
    `audit_selbsttest.py` — genau der Prüfstand, der dafür da ist.
  · Zahlen: C-Seite **325 → 304** Funde, **70 → 49** „sicher";
    Python-Seite **237 → 179** und **10 → 2**; Fremdklone
    **mehrere → 0**; Absturz **ja → nein**. Zusammen
    **562 → 483 Funde, 80 → 51 „sicher"**, Grundlinie **50** Schlüssel.
  · Abnahme: eigener **Selbsttest 14/14** (je ein Fall pro Defekt),
    Meta-Tor `audit_selbsttest.py` **68 grün / 0 rot** (vorher 64/4),
    Torkanon „Teilstring-Fallen : 0". **Rotbeweis:** dieselben Zusagen
    gegen die Originalfassung des Pakets ergeben **2 grün, 7 rot**.
  · **Offen und benannt: `P3-479`** — die **51** echten Fundstellen.
    Davon 28 im Erkennungspfad (der eigentliche Auftrag), 10
    Selbstprüfungen über Berichtstext, 5 ein JSON-Parser aus `strstr`,
    3 echte `strncpy`-Kürzungen in einer **Waise**, 2 Präfixvergleiche
    einer vollen Kennung, 2 Python-Muster.
  · **Der Auftrag „fixe sie alle" ist damit NICHT erfüllt**, und das
    steht hier statt in einer Fußnote: behoben ist der Prüfer, messbar
    gemacht sind die Fundstellen, und die Grundlinie verhindert ab
    jetzt neue. Die 51 selbst brauchen je eine Beurteilung und einen
    Rotbeweis.

  **ANGEHALTEN (2026-09-17, nach MF-1228 `bd1957ea`)**, weil der
  Eigentümer zwei weitere Pakete nachgeschickt hat, die **denselben
  Prüfer in neuerer Gestalt** bringen (`A-028`, `A-029`). Die 51
  Fundstellen abzutragen, während die Fassung des Prüfers sich noch
  ändert, wäre Arbeit an einem wandernden Ziel — `A-029`s Paket zieht
  die CI-Maschinerie nach `audit_common.py` aus, und dort liegt auch
  `walk()`, also Defekt C. Der nächste konkrete Schritt an DIESEM
  Posten sind die **28 C2-Fundstellen im Erkennungspfad**, beginnend
  mit `uft_scp_writer.c:433` (`strstr(hint, "st")` in einem Schreiber).
- **Stand, NACHTRAG 2026-09-18 — eine Teilstring-Falle hat drei
  fertige Commits aufgehalten, und gefunden hat sie nicht der Prüfer,
  sondern der Riegel (MF-1247):**
  · Zwei Tore der zweiten Sitzung (`audit_faehigkeitsaussage`,
    `audit_faehigkeitsmatrix`) meldeten vier Befunde gegen
    `uft_woz_plugin.c` und `uft_td0.c`. Beide streiften Kommentare ab
    — mit dem richtigen Satz „Ein Flaggenname in einem Kommentar ist
    keine Flagge (MF-767)". Die Regel war **ein Wort** zu kurz: ein
    Flaggenname in einer **Zeichenkette** ist ebenfalls keine Flagge.
  · Gemessen mit dem Code der Tore selbst, einzige Änderung die
    Maskierung des Bezeichners INNERHALB der Zeichenkette: Befund →
    **kein Befund**, und die Gegenprobe mit einer wirklich gesetzten
    Flagge meldet weiter. In beiden Dateien steht der Name genau
    zweimal — im Kommentar und in dem Satz, der ihn **verneint**
    („beansprucht kein `UFT_FORMAT_CAP_WRITE`"), während daneben
    `.capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY` steht.
  · **Behoben nicht mit einem neuen Werkzeug, sondern mit dem
    vorhandenen.** Mein erster Griff war ein eigener Zerteiler
    `scripts/c_text.py`; `scripts/c_lex.py` liegt seit diesem Posten
    (MF-1228) verfolgt im Baum, wird von drei Toren benutzt und kann
    mehr. `c_text.py` ist weggeworfen, bevor es je in den Baum kam.
    Neu und **additiv**: `c_lex.lex_mit_makros()`, weil
    `uft_dsk_generic.c` seine 49 Plugins über EIN `#define` erzeugt und
    ein reiner Token-Umstieg dort drei echte Flaggen VERLOREN hätte —
    derselbe Fehler in die andere Richtung, gemessen.
  · `c_lex` hatte **keinen** Selbsttest; jetzt **10/10**, zwei davon
    Rot-Proben. Tor 69 **7/7**, Tor 70 **10/10**, beide **0 Befunde**,
    Grundlinien unberührt. Die drei bisherigen Nutzer unverändert
    (Teilstring 464/0 neu, Code-Fallen 20/0 neu).
  · **Vier eigene Fehlgriffe unterwegs**, jeder von einer Messung
    umgeworfen — festgehalten in `P3-501`, weil der Vorgang mehr sagt
    als das Ergebnis.
  · **Noch nicht committet:** die zwei Tore und `src/formattab.cpp`
    gehören der zweiten Sitzung. Nach der Ordnungsregel des
    Eigentümers („ein Tor wird allein und zuerst committet, bevor es
    urteilen darf") landet die Reparatur mit deren Tor-Commit; im
    Arbeitsbaum liegt sie und der Haken ist frei.
- **Beleg:** **MF-1228**, `bd1957ea` — Tor 66 verdrahtet, acht Defekte
  am Prüfer behoben, Grundlinie 50, Selbsttest 14/14, Meta-Tor 68/0,
  Rotbeweis gegen die Urfassung 2 grün/7 rot, `P3-479` neu.
  **Der Posten bleibt offen**: die 51 Fundstellen sind nicht behoben.
- **Stand (2026-09-18, MF-1232 — die ersten fünf sind weg):**
  · Werkzeug übernommen: `include/uft/util/uft_match.h` +
    `src/util/uft_match.c` + `tests/test_match.c`, byteidentisch aus dem
    `A-029`-Paket. Baut warnungsfrei unter `-Wall -Wextra -Wpedantic`,
    Test besteht, **0 eigene Fundstellen**, keine Basisnamen-Kollision
    (`uft_match` kam im Baum 0-mal vor).
  · **Ein Befund an Tor 66 selbst ist damit behoben:** es nannte
    `uft_suffix_eq()`, `uft_magic_at()` und `uft_id_eq()` als Ersatz,
    und `git grep` fand diese Namen **ausschließlich in
    `audit_teilstring.py`**. Ein Tor, das auf Funktionen verweist, die
    es nicht gibt, verschiebt Arbeit statt sie zu benennen.
  · **5 Fundstellen behoben** in `src/formats/sega/uft_genesis.c`, und
    es war ein Lesen über die Puffergrenze: `strstr` auf einem Zeiger
    ins Rohabbild, geprüft war nur `size >= 0x200`. Versatz und
    Feldlänge nennt der Baum selbst; die Länge ist **abgeleitet**
    (`sizeof ((const genesis_header_t *)0)->system`), nicht wiederholt.
  · Rotbeweis zuerst: `tests/test_genesis_sucht_nicht_ueber_das_feld.c`
    fiel gegen den Vorzustand mit **5 von 14** Zusagen, genau den fünf
    Fällen mit dem Köder AUSSERHALB des 16-Byte-Feldes; die neun
    Positiv- und Randfälle blieben grün.
  · **Nicht behauptet:** `genesis_detect_system`/`_format` haben
    gemessen **keinen Produktivaufrufer** (nur `tests/test_genesis.c`).
    Der Defekt war echt, hat aber heute keinen Benutzer geschützt.
  · **Die Verdrahtung kostete drei Fehlmessungen, alle als Zahl
    ausgegeben:** „zwei Testziele" (gegrept statt gemessen — die Datei
    kommt über `GLOB_RECURSE`), „vier gefallene Ziele" (`ninja` hält
    nach den ersten Fehlschlägen an, `-j4`), und „60/61 Fehlbetrag 0"
    (auf `tests/CMakeFiles/` eingeschränkt, worauf `cli/uft-decode`
    fiel — dessen eigene Liste der Kommentar in derselben Datei
    genannt hatte). Über den ganzen Bau: **61 Ziele**, gedeckt über
    einen Eintrag in `UFT_FORMAT_LAYER_DEPS` **und**
    `uft_wire_match()`, die die Quellenliste des Ziels befragt — genau
    das Doppel, das MF-1189/`P3-454` gemessen hat. Endstand
    61 / 63 / **Fehlbetrag 0**.
- **Stand (2026-09-18, MF-1233 — acht weitere weg, und die Einteilung
  ist jetzt gemessen):**
  · **Die Einteilung kommt jetzt aus den DEKLARATIONEN**, je Fundstelle,
    nicht aus den Schnipseln: **11** durchsuchen ein begrenztes lokales
    Feld (Gefahr tritt nicht ein), **13** eine vom Aufrufer gelieferte
    Zeichenkette, **9** Prosa (`*_description()`, `report`), **7** liegen
    im Waisen `uft_xfd_parser_v2.c`, **1** ist die echte Falle in
    fremdem Code (`src/samdisk/SpectrumPlus3.cpp` `&data[653]`), **3**
    bleiben zu lesen. Beide früheren Zahlen bleiben zurückgenommen.
  · **8 Fundstellen behoben** in `uft_scp_writer.c`:
    `scp_disk_type_from_hint()` wählte den Diskettentyp mit `strstr`,
    und weil `st` VOR `hd` geprüft wird, ergab `"pc-hd-fastest"`
    **0x08 (Atari ST) statt 0x30 (PC HD)** — „st" steckt in „fastest".
  · Rotbeweis `tests/test_scp_hinweis_trifft_ganze_woerter.c`:
    **6 von 20 rot** gegen den Vorzustand, 20/20 danach; die 14
    Positiv- und Randfälle (inkl. `"1.44"` und `"pc hd."`) immer grün.
  · **Nicht entschieden (S5):** die Funktion hat keinen Aufrufer, ihr
    Vertrag ist unbestimmt (Kennung oder Fließtext?). Geändert ist nur
    die **Wortgrenze**, die unter beiden Lesarten falsch war; die
    Groß-/Kleinschreibung bleibt wie `strstr` sie hatte.
  · Deckung **vor** dem Bau gemessen: `uft_scp_writer.c` in **74**
    Zielen, 14 davon ohne `uft_match.c` → `uft_wire_match()`-Ausdruck
    auf `uft_genesis|uft_scp_writer` erweitert, danach Fehlbetrag **0**
    (`uft_match` in 78 Zielen).
  · **Offen: 38** (36 in C, 2 in Python).
- **Stand (2026-09-18, MF-1235 — der JSON-Verteiler von XDF, und der
  Teilstring war die kleinste der Sünden):**
  · `xdf_api_process_json()` wählte den Befehl mit **sechs** `strstr()`
    über die ganze Zeichenkette. Gemessen mit einer Wegwerf-Sonde am
    echten Symbol, **vor** der Korrektur: `{"command":"close"}` gibt
    `{"success": false}`; dasselbe mit einem harmlosen
    `,"label":"analyze"` gibt `{"success": false, "confidence": 0.00}`
    — es lief **analyze**. Mit `,"note":"open"` kommen **6 Byte**
    zurück, erstes Byte `0x50`, kein JSON.
  · **Drei von sechs Zweigen gaben nicht initialisierten Heap aus.**
    `result` kam aus einem blanken `malloc(4096)`, und `"open"` ohne
    `"path":` sowie `"info"` und `"grid"` hatten **kein `else`** —
    gemessen alle drei dieselben 6 Byte.
  · Der leere Pfad `""` wurde zu `}`, weil die Skip-Schleife **alle**
    Anführungszeichen verschlang; das unmaskierte `}` zerbricht dabei
    das JSON, in dem es steht. Und der Pfad wurde bei 255 Zeichen
    **still** gekappt und dann geöffnet (D5).
  · **Rotbeweis zuerst**, wie die EINFRIER-REGEL es verlangt — und es
    gibt hier keine fremde Beschreibung, also ist die Grundlage eine
    Messung am Produktionspfad: `tests/test_xdf_json_verteiler.c`,
    **34 grün / 13 rot** gegen den Vorzustand, **47 grün / 0 rot**
    danach.
  · **Der Test hat eine meiner eigenen Behauptungen widerlegt.** Der
    Abschnitt „Pfad über 255 Zeichen" war im Vorzustand **vollständig
    grün**: der gekappte Pfad lässt sich ebenso wenig öffnen wie der
    ganze, also kam ohnehin ein `"error"` — grün aus dem falschen
    Grund (Klasse MF-1014). „Öffnet die FALSCHE Datei" ist damit
    **nicht belegt** und im Testkopf zurückgenommen; belegt ist, dass
    die Länge nicht genannt und mit dem gekappten Pfad geöffnet wurde.
    Die beiden Zusagen stehen als **Sperre** weiter da, und zwei
    schärfere entscheiden jetzt.
  · **Und eine zweite eigene Zahl ist zurückgenommen:** ich hatte
    gesagt, Regel C2 melde „5 der 6" Aufrufe und `"analyze"` entkomme
    mit 9 Zeichen der Längenschwelle. Falsch. Die Regel hat einen
    **Rückfallzweig**: ist der Inhalt länger als 8 Zeichen, kommt der
    Fund trotzdem — nur als `pruefen` statt `sicher`
    (`audit_teilstring.py:235-253`). Der Prüfer meldet **alle sechs**;
    ich hatte „Fundstellen" mit „`sicher`-Fundstellen" verwechselt. Es
    gibt dort **keine Blindstelle**, sondern eine gewollte Schwelle.
  · Zahlen, nachgerechnet statt behauptet: in der Datei **6 → 3**
    Fundstellen, davon `sicher` **5 → 1**; Grundlinie **477 → 474** —
    5 Fingerabdrücke weg (die fünf Befehlswörter), 1 neuer mit
    Vielfachheit 2 (die Tafel), `"path":` unverändert. 477 − 5 + 2 =
    474, und das Werkzeug druckt die Zahl selbst.
  · Die drei übrigen Fundstellen bleiben **mit Grund** in der
    Grundlinie: die Suche ist bewusst, weil das Schema unbekannt ist
    (S5) — geändert ist, dass sie nicht mehr **entscheidet**.
  · Am Rand berichtigt: der Header versprach `"export"` und
    `"compare"` (**0 Treffer** in den Quellen) und nannte `"info"` und
    `"grid"` nicht; sein Nutzungsbeispiel rief `xdf_api_export()`, das
    im ganzen Baum **nur in diesem Beispiel** vorkommt.
  · Drei Nebenbefunde benannt statt mitgefixt: **`P3-482`** (der Baum
    hat genau einen JSON-Maskierer, `static` und `FILE*`-basiert)
    — ✔ **ERLEDIGT MF-1241**, siehe den eigenen Stand unten —,
    **`P3-483`** (zwei Schranken aus MF-554 können nie zutreffen, vier
    `-Wtype-limits`, Decke ~1,07 GB statt 1024), **`P3-484`** (der
    Selbsttest von Tor 67 fällt unter cp1252 — genau der Fall, für den
    seine Härtung geschrieben wurde). Hauptbefund: **`P3-485`**.
- **Stand (2026-09-18, MF-1237 — die schärfste Fundstelle lag in der
  ERKENNUNG, und ich hatte sie zwei Abträge lang nicht angesehen):**
  · Nach MF-1235 blieben **32** `sicher`-Fundstellen. Zwei Dateien
    darin hatte ich **nie gelesen** — `src/detect/mfm/mfm_detect.c` (5)
    und `src/formats/snk/uft_neogeo.c` (5) —, und **beide liegen in
    Erkennungspfaden**. Genau dort ist eine Teilstring-Falle die
    FMT-2/3-Klasse: ein Format beansprucht eine Datei, die es nur
    ENTHÄLT. Lehre für die Reihenfolge: nach Wirkung sortieren, nicht
    nach Bequemlichkeit.
  · **Der Befund:** `mfm_detect_atari_st()` suchte im OEM-Feld des
    FAT-BPB nach `ATARI`, `TOS`, `atari`, `GEM` — und entschied
    zwanzig Zeilen weiter mit `return true`. Der Puffer ist **sauber
    begrenzt** (`char oem[9]`, terminiert), es wird nichts überlesen;
    der Fehler ist semantisch. Gemessen am Produktionssymbol gegen die
    HEAD-Fassung: `"TOSHIBA"`, `"PROTOS"`, `"GEMINI"` und `"ATARIX"`
    gelten **alle vier** als Atari ST.
  · **Und es wiegt schwer:** `mfm_detect.c:711` prüft Atari ST
    ausdrücklich **vor** DOS („weil BPB kompatibel") und setzt bei
    einem Treffer **Konfidenz 80**. Eine nicht bootfähige PC-Diskette
    hat bei Byte 0 einen beliebigen Wert, also greift `!has_x86_jump`.
  · **Rotbeweis** `tests/test_mfm_oem_trifft_ganze_woerter.c`:
    **21 grün / 4 rot** vorher, **25 grün / 0 rot** nachher. Jeder Fall
    trägt eine **Sperre** — die Kette ist `has_68k_jump` →
    `has_atari_checksum` → OEM, also setzt jeder Fall Byte 0 auf `0x00`
    und prüft, dass die Prüfsumme **nicht** 0x1234 ist. Ohne das könnte
    eine Zusage grün sein, ohne die OEM-Zeile berührt zu haben
    (Klasse MF-1014).
  · **Der Fix hat eine Doppelhaltung AUFGELÖST, nicht geschaffen.** Die
    Wortgrenzen-Regel lag seit MF-1233 dateilokal in
    `uft_scp_writer.c`; eine zweite Kopie wäre „eine Größe, zwei
    Rechnungen" (MF-1177) — und genau das hatte `P3-482` einen Commit
    vorher benannt. Sie steht jetzt als **`uft_wort_treffer()`** in
    `src/util/uft_match.c`. Verhaltensneutralität belegt:
    `test_scp_hinweis_trifft_ganze_woerter` (20 Zusagen) und
    `test_match` bleiben grün.
  · Verdrahtung **vor** dem Bau gemessen (Lehre aus MF-1232):
    `mfm_detect.c` in **63** Zielen, genau **2** ohne `uft_match.c`;
    ein Wort in `uft_wire_match()` deckt beide und jedes künftige.
  · Zahlen: Grundlinie **474 → 470**, nachgerechnet — 4 Fingerabdrücke
    weg, 0 neu, 0 Vielfachheiten geändert. In `src/detect` bleibt
    **eine** `sicher`-Fundstelle (`fs_type`/`FAT12`, nur `conf += 5`).
  · **Der Preis ist benannt:** `"ATARITOS"` trifft nicht mehr; ob es
    vorkommt, ist **nicht gemessen**. Die Wahl folgt MF-729 — ein
    falsches JA bei Konfidenz 80 vor DOS verdrängt die richtige
    Antwort, ein falsches NEIN fällt in die BPB-kompatible Lesart.
  · **`uft_neogeo.c` ist NICHT in diesem Commit** und der Befund ist
    größer als eine Teilstring-Falle: `upper[0] == 'P'` entscheidet
    nach dem **ersten Buchstaben** (`PUZZLE.BIN` wäre ein P-ROM), der
    Name wird bei **15 Zeichen still gekappt**, `toupper(name[i])` mit
    blankem `char` ist für Bytes ≥ 0x80 **undefiniert**, und die
    Vorgabe `NEO_ROM_P` **behauptet** einen Typ statt „unbekannt" zu
    melden. Gemessen: **0 Produktionsaufrufer**, aber 5 Testzusagen
    (`tests/test_neogeo.c:136-140`), die nur den Glücksfall decken.
    Eigene Aufgabe.
  · Hauptbefund: **`P3-486`**.
- **Stand (2026-09-18, MF-1238 — und die Einteilung der RESTLICHEN ist
  damit VOLLSTÄNDIG, zum ersten Mal):**
  · `neogeo_detect_chip_type()` trug **fünf** Defekte auf
    zweiundzwanzig Zeilen, und nur **einer** davon war eine
    Teilstring-Falle: der erste Buchstabe entschied (`PUZZLE.BIN` war
    ein Programm-ROM), `-P` traf überall (`MVS-PACK.ROM`), der Name
    wurde bei **15 Zeichen still gekappt**, `toupper()` mit blankem
    `char` ist für Bytes ≥ 0x80 **undefiniert**, und die
    Prüfreihenfolge entschied bei zwei Marken. Dazu wurde der
    Backslash nur geprüft, **wenn** der Schrägstrich fehlte.
  · **Die fünf vorhandenen Zusagen konnten nichts davon sehen** — ihre
    Namen beginnen mit `0` und sind kürzer als 15 Zeichen. Rotbeweis
    fällt gegen HEAD an der ersten neuen Zusage; nachher 10/10.
  · Gesucht wird jetzt `-<buchstabe><ziffer>` im **vollen** Namen, und
    es gilt die **letzte** Marke. Die Ziffer ist die einzige neue
    Bedingung und der Kern. **Als HAUSREGEL benannt** — belegt an den
    fünf Zusagen und `uft_neogeo.h:44-50`, eine fremde Beschreibung
    liegt nicht vor (Lehre aus MF-1038).
  · **GESTOPPT (S5):** die Vorgabe `NEO_ROM_P` **behauptet** einen Typ,
    wo keiner erkannt wurde — weil der TYP es nicht anders kann.
    `neogeo_rom_type_t` hat keinen UNKNOWN-Wert. Ein `NEO_ROM_UNKNOWN`
    wäre additiv möglich, ist aber eine Änderung an einem öffentlichen
    Header. Siehe `P3-487`.
  · Grundlinie **470 → 465**; `src/formats/snk`: **0** Fundstellen.
- **Stand (2026-09-18, MF-1240 — die ERKENNUNGSSCHICHT steht auf 0):**
  · MF-1237 hatte `strstr(bpb.fs_type, "FAT12")` ausdrücklich liegen
    gelassen, mit einem Grund statt einer Ausrede: die Stelle bewegt
    keine Ja/Nein-Antwort, sondern nur `conf += 5`, und ein Rotbeweis
    muss deshalb die **Konfidenz** beobachten — eine andere
    Testgestalt. Der Beweis existiert jetzt.
  · **Gemessen, zwei Bootsektoren, die sich in NICHTS unterscheiden
    außer im achtstelligen `fs_type`-Feld:**
    vorher `"FAT12   "` → **100**, `"XFAT12  "` → **100**,
    `"FAT16   "` → 95; nachher `"XFAT12  "` → **95**.
    Ein Feld, das `FAT12` nur ENTHÄLT, bekam also dieselbe Konfidenz
    wie ein echtes.
  · Rotbeweis `tests/test_mfm_fs_type_ist_ein_wort.c`: **5 grün /
    3 rot** vorher, **8 grün / 0 rot** nachher. **Vier Sperren
    zuerst** — dass überhaupt ein Kandidat entsteht und seine
    Konfidenz nicht 0 ist; ohne sie sagt ein Abstand nichts. Dazu die
    Gegenprobe mit `"FAT16"`, damit die Hauptzusage nicht auch mit
    einer kaputten Regel grün wäre.
  · **Warum die Wortgrenze hier richtig und bei den DOS-OEM-Marken
    falsch ist**, steht an der Zeile: das Feld **ist** der Typ, es ist
    kein Präfixstempel wie `"MSDOS5.0"`, bei dem der Punkt als
    Wortzeichen alles zu einem Wort macht.
  · Der MF-1237-Kommentar, der die Stelle als liegengelassen führte,
    ist **mitgezogen**. Ein Quelltext, der seinen eigenen Stand falsch
    angibt, ist die Klasse, die dieser Baum am teuersten bezahlt.
  · Zahlen: Grundlinie **465 → 464**, 1 Fingerabdruck weg, 0 neu;
    `sicher` gesamt **23 → 22**; **`src/detect` steht auf 0**.
    Einzelheiten: **`P3-489`**.
- **Stand — DIE EINTEILUNG DER RESTLICHEN, VOLLSTÄNDIG GEMESSEN
  (23 bei MF-1238, **22 nach MF-1240** — die eine `fs_type`-Zeile ist
  unten als erledigt gekennzeichnet, nicht entfernt):**
  Nach MF-1238 sind **23** `sicher`-Fundstellen übrig, und **keine
  davon ist ein erreichbarer, entscheidender Defekt.** Jede ist genau
  einer dieser Klassen:
  · **7 — Modul-Waise mit 0 Aufrufern**
    (`src/formats/atari/uft_xfd_parser_v2.c`). Gemessen haben
    `xfd_probe`, `xfd_open` und `xfd_read_sector` **0 Aufrufer**
    außerhalb ihrer Datei; die Kette ist
    `xfd_open()` → `xfd_detect_dos()` (static) → die Fundstellen.
    **Und die Datei steht NICHT auf `docs/orphan_baseline.txt`** —
    eine 0-Aufrufer-Einheit im Produktbau, die das Waisenregister nicht
    kennt. Sie trägt zwei echte Klassen: unbegrenzte `strstr` auf
    einem Zeiger ins Rohabbild (`(char*)bs + 16`, die Genesis-Klasse)
    und `strncpy(…, "SpartaDOS", 8)` bzw. `"Atari DOS 2.x compatible"`
    in 8 Byte — also **ohne Nullterminierung**. Nach MF-699
    beschriften statt reparieren; das Fehlen im Waisenregister ist der
    eigentliche nächste Griff. Nebenbei: `src/formats/xfd/uft_xfd.c:163`
    hat ein zweites, `static`es `xfd_open` mit anderer Signatur — das
    ist das registrierte Plugin.
  · **10 — Dateien der Waisenliste**: `uft_dsk_cpc_parser_v2.c` (4),
    `uft_fdi_parser_v2.c` (3), `uft_supercopy_detect.c` (1),
    `uft_stx_parser_v2.c` (1), `uft_zxbasic.c` (1). Alle in
    `docs/orphan_baseline.txt`.
  · **3 — schwache Selbsttest-Zusagen**, nicht Produktionscode:
    `uft_d64_parser_v3.c:1930` `assert(strstr(report, "Track 17"))`,
    `uft_g64_parser_v3.c:2103` `"Track"`, `uft_scp_parser_v3.c:1981`
    `"17"`. Alle drei prüfen einen **erzeugten Textbericht**. Die
    letzte ist fast tautologisch — „17" steckt in jeder Zahl, die 17
    enthält. Das ist **Testqualität**, keine Teilstring-Falle: der
    Ersatz ist eine schärfere Zusage, kein Vergleicher.
  · **1 — `mfm_detect.c` `fs_type`/`FAT12`** — ✔ **ERLEDIGT MF-1240.**
    Hier stand: „dort wäre die Wortgrenze richtig, sie bewegt aber nur
    `conf += 5`, und die EINFRIER-REGEL verlangt einen Rotbeweis, der
    die **Konfidenz** beobachtet statt der Ja/Nein-Antwort. Andere
    Testgestalt (`P3-486`)." Der Beweis existiert jetzt: `"XFAT12  "`
    bekam Konfidenz **100** wie ein echtes FAT12, jetzt **95**.
    Damit steht `src/detect` auf **0** und die Restzahl auf **22**.
    Siehe `P3-489`.
  · **1 — `uft_xdf_api_impl.c`**: bewusst, begründet, dokumentiert
    (`"path":`, S5 — das Befehlsschema ist unbestimmt, `P3-485`).
  · **1 — Fremdcode**: `src/samdisk/SpectrumPlus3.cpp`, und gemessen
    **nicht** in `UnifiedFloppyTool.pro` — es wird nicht ins Produkt
    gebaut.
- **Stand (2026-09-18, MF-1241 — der Nebenbefund `P3-482` ist
  abgetragen, und damit ist eine Auskunft ZURÜCK, die MF-1235 opfern
  musste):**
  · MF-1235 hatte aus `xdf_api_process_json()` den Fehlertext des Kerns
    **entfernt**, weil er den Pfad trägt und ein unmaskiertes `}` oder
    `\` das JSON zerbricht — gemessen war der Vorzustand
    `{"success": false, "error": "Cannot open file: }"}`. Die Notlösung
    war `"error_code"`: eine **stärkere** Zusage, aber weniger Auskunft.
  · Der Grund für die Notlösung war gemessen: der Baum hatte **genau
    einen** JSON-Maskierer, `write_json_string()` in
    `src/core/uft_loss_report.c:37`, `static` und auf einen `FILE*`
    schreibend. **0** exportierte Maskierfunktionen im ganzen Baum.
  · Seit MF-1241 liegt die Tafel in `src/util/uft_json.c` mit
    `include/uft/util/uft_json.h` — **verschoben, nicht neu
    geschrieben**, und `uft_loss_report.c` ruft sie. Es bleibt bei
    EINER Tafel (`CLAUDE.md` §MF-1177).
  · **Abweichung von der eigenen Skizze, benannt statt still:**
    `P3-482` sagte `src/core/`; gewählt ist `src/util/`, wo seit
    MF-1232 `uft_match.c` als Präzedenzfall liegt. Die Skizze war eine
    Absicht, kein Messergebnis — die Begründung steht in `P3-482`.
  · **Zwei Funktionen, eine Regel:** `uft_json_escape_byte()` trägt die
    Tafel, `uft_json_escape()` ist die Puffer-Form für Aufrufer mit
    bekannter Größe. Der Verlustbericht kennt seine Längen nicht und
    behält deshalb die Strom-Form — „erst puffern" bräuchte eine Größe,
    die niemand kennt, und wäre die nächste stille Kappung (D5).
  · **Rotbeweis** `tests/test_json_maskierung_eine_tafel.c`: vorher
    **43 grün / 2 rot**, nachher **45 grün / 0 rot**. Die zwei roten
    sind das Feld `"error"` und sein maskierter `\` — bei **vier
    grünen Sperren**, die belegen, dass der Kerntext den Pfad mit rohem
    `\` wirklich trägt. Ohne diese Sperren sagte „`error` ist da"
    nichts (Klasse MF-1014).
  · **Der Anker gegen den Leerlauf liegt in einem FREMDEN Test:**
    `test_loss_report::json_escape_special_chars` bleibt grün — und nur
    das belegt, dass die Verschiebung die `.loss.json` um kein Byte
    geändert hat.
  · **Ein zweiter Befund fiel dabei an, und er ist die teuerste Klasse
    dieses Baums:** der Kopfkommentar von `write_json_string()` sagte
    „Writes escaped string (**without** surrounding quotes)" — das
    Gegenteil seines Codes, der die Anführungszeichen selbst setzt, und
    alle acht Aufrufstellen verlassen sich darauf. Berichtigt, alter
    Wortlaut zitiert. Klasse MF-930.
  · **Sprengradius VOR dem Bau**, aus `build.ninja` und ungeschränkt:
    64 Ziele für `uft_loss_report.c`, 62 für `uft_xdf_api_impl.c`,
    Vereinigung **66** — davon **eines** außerhalb `tests/`
    (`cli/uft-decode`). Deshalb beide Verdrahtungswege:
    `UFT_FORMAT_LAYER_DEPS` **und** ein neues `uft_wire_json()`.
  · Warnungsfrei unter `-Wall -Wextra -Wpedantic`; Tor 66 **464, 0
    NEU**, Tor 67 **20, 0 NEU** — MF-1241 bringt keine neue Falle mit.
- **Stand — was der Rest kostet, und meine Zahl ist ZURÜCKGENOMMEN:**
  Ich hatte „33 Versatz / 9 Prosa / 7 Waise" gemessen, dann
  „29 begrenztes Feld / 9 / 7 / 4" — **beide aus einem Muster über die
  Schnipsel statt aus den Deklarationen.** `uft_genesis.c` hat sie
  widerlegt: `strstr(system, …)` sah nach „begrenztes Feld" aus und war
  ein Zeiger ins Rohabbild. Belastbar ist nur: **46 offen** (44 in C,
  2 in Python), 7 davon im Waisen `uft_xfd_parser_v2.c` — und die
  Einteilung verlangt, **je Fundstelle die Deklaration zu lesen**. Das
  ist der Grund, warum „fixe sie alle" mehrere Posten braucht und kein
  Suchen-und-Ersetzen ist.

### A-028 · Teilstring-Prüfer in die GitHub-Prüfung einbauen
- **Status:** **angehalten an einer Eigentümerentscheidung** (umgestellt 2026-09-26 beim Aufraeumen: zwei Posten standen auf `in Arbeit`, laufend ist nach ausdrücklicher Anweisung `A-032`; offen ist allein der öffentliche Wegwerf-PR) · vorher **in Arbeit** (seit 2026-09-17; zusammen mit `A-029`, weil
  dessen Paket dieses hier überholt — siehe Stand)
  · **Aufgenommen:** 2026-09-17
- **Wortlaut:** „das ist code von mir !! … Bau den Teilstring-Prüfer in
  die GitHub-Prüfung ein !!"
- **Gegenstand, gemessen:** `neue-ideen/Teilstring-Prüfer in die
  GitHub-Prüfung.zip`, 58 261 Byte, 5 Einträge; inneres Archiv 12
  Dateien. Neu gegenüber `A-027`:
  `.github/workflows/substring-audit.yml` (11 729 B),
  `substring_baseline.json` (202 B, **leer**: `count: 0`), und eine
  neuere `substring_audit.py` mit **27 503** statt 18 344 Byte.
  **BERICHTIGT:** hier stand zusätzlich „`c_lex.py` mit 8 908 statt
  6 917". Das war ein Spalten-Fehlgriff in der Archivliste — die 6 917
  gehören zu `uft_match.h`, das daneben stand. Gemessen sind **alle
  vier** Kopien von `c_lex.py` byteidentisch (8 908 B, sha256
  `d4806f34b…`): die im Baum und die aus allen drei Paketen. Der
  Zerteiler braucht also keine Zusammenführung.
- **Kennzahl:** keine der vier — Verlässlichkeit des Prüfstands, wie
  `P3-421`, `P3-426`, `P3-461`, `P3-477`, `P3-479`.
- **Kanal:** **entfällt, und das ist ausgesprochen statt vermutet.** Der
  Eigentümer sagt wörtlich „das ist code von mir" — damit ist die Frage
  aus MF-636 für dieses **und** das Paket aus `A-027` beantwortet; beide
  tragen zusätzlich SPDX `GPL-2.0-or-later` je Datei.
- **Einfrier-Regel:** nein — Werkzeug- und CI-Schicht. D2 gilt: kein
  neuer Ausgabeweg ohne Aufrufer und ohne Fall, der rot wird.
- **OPEN_ITEMS:** `P3-479`
- **Fertig heißt:** eine Teilstring-Falle in einem Pull Request
  erscheint **als Annotation an ihrer Zeile** in der GitHub-Oberfläche,
  der Selbsttest ist grün, und es gibt **eine** Grundlinie im Baum,
  nicht zwei.
- **Aufwand:** nicht schätzbar.
- **Stand (bei der Aufnahme gemessen):**
  · **Der wörtliche Auftrag ist seit MF-1228 schon erfüllt — über einen
    anderen Weg.** `.github/workflows/ci.yml:47-48` ruft
    `python3 scripts/check_consistency.py`, und dort hängt Tor 66. Der
    Prüfer IST in der GitHub-Prüfung. Was das Paket darüber hinaus
    bietet, ist der eigentliche Wert und heißt anders: **`emit_sarif`**
    (GitHub Code Scanning) und **`emit_github`**
    (`::warning file=…,line=…`). Ein Torkanon liefert
    bestanden/gefallen; SARIF liefert die Fundstelle **an ihrer Zeile im
    Diff**. Das kann `check_consistency.py` nicht.
  · **Keiner meiner acht Fixes ist in der 27 503-Fassung**, gemessen:
    `NCMP_FNS` mischt weiter Kopieren und Vergleichen (Z. 68), Escape-
    Klasse unverändert `\\[dDwWsSbBAZ]` (Z. 362), `repo_scope`/
    `ls-files` **0 Treffer**, `fullmatch` nur im Muster der
    Funktionsnamen (Z. 345), `errors='replace'` betrifft das **Lesen**
    (Z. 604), nicht die Ausgabe. Eins zu eins übernehmen würde MF-1228
    zurücknehmen.
  · **Und `A-029`s Paket überholt dieses:** dort ist
    `substring_audit.py` nur **17 649** Byte, weil die CI-Maschinerie
    nach `audit_common.py` (10 406 B) ausgezogen ist. Deshalb stehen
    beide Posten auf EINEM Arbeitsstrom, und gebaut wird auf der
    ausgezogenen Gestalt.
  · Zwei Entscheidungen, die ich vorlege statt still zu treffen: **ein**
    Workflow oder zwei (es liegen schon sechs, alle mit
    `cancel-in-progress`), und **eine** Grundlinie —
    `docs/teilstring_baseline.txt` mit 50 Schlüsseln gegen den
    JSON-Mechanismus aus `audit_common.py`.

  **Beide Entscheidungen sind getroffen und begründet:**
  · **Ein Workflow** (`.github/workflows/teilstring.yml`, der siebte),
    weil seine `paths:`-Filter ihn nur laufen lassen, wenn C- oder
    Python-Quellen wirklich geändert sind — `ci.yml` läuft dagegen bei
    jedem Push. Und weil PR-Annotationen einen eigenen Auftrag mit
    `fetch-depth: 0` brauchen. Die Spur für `code_audit.py` (`A-029`)
    kommt in **dieselbe** Datei, nicht in eine achte.
  · **Eine Grundlinie**, und zwar die des Eigentümers:
    `docs/teilstring_baseline.json` über `audit_common`. Mein
    `.txt`-Format ist entfernt — dieselbe Absicht, dieselben
    Fundstellen, ein Mechanismus (D3). Der Kopf des Pakets macht dieses
    Argument selbst.

  **Umfang der Grundlinie berichtigt, gemessen:** erst trug sie nur die
  51 `sicher`-Fälle, womit der SARIF-Lauf **255** Ergebnisse meldete —
  den ganzen `prüfen`-Altbestand. 255 Warnungen am ersten Tag im
  Sicherheitsreiter ertränken jedes neue Signal. Jetzt **486
  Fundstellen / 404 Fingerabdrücke**, SARIF **0**; blockiert wird
  weiter nur über `--fail-on sicher`.

  **Zwei weitere Defekte, und einer hätte das Tor in CI wirkungslos
  gemacht:** **(I)** die Auslagerung war unvollständig — der Prüfer
  importierte `walk`/`C_EXT` aus `audit_common` und definierte beide
  darunter neu; die zweite Kopie gewann, die gemeinsame `walk()` war
  toter Code. **(J)** `Finding.fingerprint()` hasht den Pfad, wie er
  hereinkommt: `src\…` und `src/…` ergeben **verschiedene**
  Fingerabdrücke, und eine auf Windows geschriebene Grundlinie ist in
  CI (Linux) damit wirkungslos — **alle** bekannten Fundstellen wären
  als NEU gemeldet worden. `check()` konnte es nicht sehen, weil es mit
  demselben `relpath` schreibt und liest; gefunden hat es erst der
  Handlauf mit dem Pfad, den ein Mensch tippt.

  **Rotbeweis über alle vier Wege** mit einer gepflanzten neuen Falle:
  Annotation an `:15` rc 1 · SARIF 1 Ergebnis Regel C1 · `--tor`
  „486 in der Grundlinie, 1 NEU" rc 1 · Torkanon FAIL. Danach
  entfernt, alles zurück auf 0. Selbsttest **17/17**, je ein Fall pro
  Defekt A bis J.

- **Stand 2026-09-19 (MF-1258) — die Erzeugnisse liegen verfolgt im
  Baum, und die Abnahme hat drei veraltete Zahlen in genau dieser
  Datei gefunden:**
  · **Gelandet ist das alles längst:** `d46d2a77` (MF-1229) brachte
    `.github/workflows/teilstring.yml`, `docs/teilstring_baseline.json`
    und `scripts/audit_common.py`; `af693c8e` (MF-1230) legte die
    `code_audit.py`-Spur in **dieselbe** Datei, wie die Entscheidung
    oben es angekündigt hat. Dem Posten fehlte nur der Beleg.
  · **Die drei „Fertig heißt"-Bedingungen, je gemessen:** Selbsttest
    **17/17** · genau **eine** Grundlinie im Baum
    (`docs/teilstring_baseline.json`; das `.txt` ist weg) · Tor-Lauf
    **„464 in der Grundlinie, 0 NEU", rc 0**.
  · **Die SARIF-Hälfte ist an GitHub selbst belegt, nicht nur lokal:**
    `gh api …/code-scanning/analyses` nennt **30 Analysen**, beide
    Kategorien getrennt (`teilstring-audit`/`substringAudit`,
    `codefallen-audit`/`codeFallen`), je **0 Ergebnisse** — die
    Grundlinie unterdrückt den Altbestand wie entworfen, und die
    Zwei-Kategorien-Begründung im Workflow-Kopf hält am API.
  · **DREI ZAHLEN IM KOPF DIESES WORKFLOWS WAREN VERALTET**, und das
    ist der Ort, der dabei zählt: der Kopf des Prüfers, der gegen
    genau diese Klasse antritt. „Stand der Grundlinie: 51 Fundstellen,
    50 Fingerabdrücke" (heute `count: 464`, 382 Fingerabdrücke), „16
    Zusagen" (heute 17), „36 Zusagen" (heute 42, unter cp1252 41 —
    `P3-484`). Behoben **durch Herausnehmen, nicht durch Nachziehen**:
    der Kopf nennt keine Zahl mehr, ein Schritt „Grundlinien nennen
    ihre eigene Zahl" druckt sie bei jedem Lauf, die zurückgenommenen
    Sätze bleiben zitiert stehen. Klasse als `P3-507` benannt, samt
    der Messung, die vor einem Tor dafür zu machen wäre.
  · **Und eine eigene Fehlmessung, gefangen vor dem Commit:** ich
    hatte „um Faktor neun abgedriftet" notiert (51 gegen 464). Das
    vergleicht zwei verschiedene Größen — die 51 zählten die
    Einschätzung `sicher`, die 464 tragen **beide**, weil dieser
    Posten den Umfang der Grundlinie bewusst erweitert hat. Gemessen
    über `src include scripts`: **278 Fundstellen, 256 `prüfen`, 22
    `sicher`**.

- **Was NOCH OFFEN ist, und es ist genau eine Zeile der
  Fertig-Bedingung:** „eine Teilstring-Falle erscheint in einem **Pull
  Request** als Annotation an ihrer Zeile". Gemessen ist jeder der 25
  Läufe von `teilstring.yml` ein `push`, und der letzte Pull Request
  des Baums ist **#39 vom 2026-09-14** — drei Tage **vor** dem
  Workflow (`d46d2a77`, 2026-09-17 21:24). Der Auftrag `geaendert` mit
  `if: github.event_name == 'pull_request'` **ist nie gelaufen**;
  ungeprüft sind damit sein `if`, das `fetch-depth: 0` und GitHubs
  Darstellung der `::warning`-Zeilen. Die Ausgabe selbst ist lokal
  belegt (Annotation an `:15`).
  **Der Weg dorthin ist klein und liegt beim Eigentümer**, weil er ein
  öffentliches Artefakt erzeugt: ein Wegwerf-Pull-Request mit einer
  gepflanzten Falle, Anmerkung ansehen, schließen. Das mache ich nicht
  unaufgefordert. Bis dahin bleibt der Posten `in Arbeit` — die
  Mechanik ist gebaut, ein Ausgabeweg ist **unbelegt**, und „fast" ist
  kein Zustand.
- **Beleg (der geleistete Teil):** `d46d2a77` (MF-1229) ·
  `af693c8e` (MF-1230) · `MF-1258` (die drei Zahlen aus der Hand
  genommen). Der Posten schließt erst mit der PR-Abnahme.

### A-029 · Vier Code-Fallen (K4, K6, P2, P3): prüfen, testen, abtragen
- **Status:** **teilweise erledigt** (Prüfer + Tor 67 + CI-Spur, MF-1230)
  · **Aufgenommen:** 2026-09-17
- **Wortlaut:** „das ist code von mir !! … teste alles und fixe alles"
- **Gegenstand, gemessen:** `neue-ideen/Vier Code-Fallen.zip`,
  80 883 Byte, 6 Einträge; inneres Archiv 19 Dateien. Neu:
  `tools/code_audit.py` (15 448 B, vier Regeln),
  `tools/audit_common.py` (10 406 B, **gemeinsames Gerüst**:
  `load_baseline`, `write_baseline`, `apply_baseline`, `emit_text`,
  `emit_github`, `emit_sarif`, `add_common_args`, `run_and_report`,
  **und `walk()`**), `tests/run_code_selftest.py` (7 896 B), zwei
  Köderdateien (`fixture_code_a.c` 4 149 B, `fixture_code_b.c`
  2 090 B), `.github/workflows/code-audit.yml` (13 187 B).
- **Die vier Regeln, aus dem Bericht:** **K4** eine physikalische
  Konstante in mehr als einer Datei (`prüfen` ab 2, `sicher` ab 3) —
  benannte Beispiele aus dem Baum: `uft_hfe.c:556` *errät* 12500,
  `uft_fdc_gaps.h:181` *weiß* 10416. **K6** eine Warnung hinter einer
  Schleife im begrenzten Puffer. **P2** verschachtelte
  Kommentarklammer (`sicher`). **P3** `#undef` vor spätem Gebrauch
  (`sicher`).
- **Kennzahl:** keine der vier. **Aber K4 ist der Torbau zu einem
  Grundsatz, der in `CLAUDE.md` steht und fünfmal bezahlt wurde:**
  MF-1177 („eine Größe, eine Rechnung"), MF-1015 (drei Prüfsummen,
  keine zwei gleich), MF-1026 (drei Victor-Geometrien),
  MF-1032/MF-1034 (die vier Anordnungsgesetze, zweimal einzeln
  wiederentdeckt).
- **Kanal:** entfällt — „das ist code von mir".
- **Einfrier-Regel:** nein für die Prüfer; **ja für jede Fundstelle in
  der Format-/Decoder-Schicht** → Rotbeweis zuerst.
- **OPEN_ITEMS:** **`P3-480`** (eigene Nummer, die Zahl ist jetzt
  geprüft: 20 K4-Fundstellen in der Grundlinie); `P3-479` ist die
  Schwester-Familie.
- **Fertig heißt:** `run_code_selftest.py` grün, alle vier Regeln mit je
  einem Köderfall **und** je einer Gegenprobe, im Torkanon verdrahtet,
  und jede Fundstelle der Einschätzung `sicher` entweder behoben oder
  benannt.
- **Aufwand:** nicht schätzbar.
- **Stand (bei der Aufnahme gemessen):**
  · Auch diese Fassung trägt **keinen** meiner acht Fixes: `NCMP_FNS`
    unverändert, Escape-Klasse unverändert, `repo_scope` 0 Treffer,
    keine `fullmatch`-Behandlung.
  · **Der Umzug macht Defekt C dabei breiter, nicht schmaler:** `walk()`
    liegt jetzt in `audit_common.py:65`, also im GEMEINSAMEN Modul — die
    hartkodierte Ausschlussliste statt `git ls-files` (MF-633/MF-636)
    würde damit **beide** Prüfer betreffen statt einen. Kein Vorwurf an
    den Entwurf, sondern der Grund, die Fixes VOR der Übernahme
    aufzusetzen.
- **Stand nach MF-1230 — was erledigt ist:**
  · `scripts/audit_codefallen.py` auf dem berichtigten `audit_common`,
    **sechs** Defekte behoben, je mit Selbsttestfall: **(A)** P2 meldete
    eines je Kommentar statt jedes, **(B)** P3 war blind für seinen
    eigenen Kopffall, **(C)** P3 meldete **alle 8** Funde im Baum falsch
    und blockierend, **(D)** K4 kollidierte mit CRC-Tafeln, **(E)** die
    Konstantenliste verletzte ihr eigenes Kriterium, **(F)**
    `_norm_num()` streifte Hex-Ziffern als Suffixe ab (3110 Literale).
    **Vier der sechs hat `gcc` entschieden**, nicht ich.
  · Köder `tests/formats/fixture_code_a.c` + `_b.c` byteidentisch
    übernommen; Tor 67 in `check_consistency.py`; Grundlinie
    `docs/codefallen_baseline.json` (20/20); Spur in `teilstring.yml`
    mit **zweiter** SARIF-Kategorie.
  · **Drei echte `-Wall`-Warnungen im Baum behoben** —
    `uft_format_convert_flux.c:2025`, `dim/uft_dim.c:23`,
    `gui/wiring_runtime.h:13` (dort zwei). Damit hielt D7
    („warnungsfrei unter `-Wall -Wextra -Wpedantic`") gemessen **nicht**.
  · Selbsttest **38/38**, Meta-Tor **75 grün / 0 rot**, Rotbeweis über
    alle vier CI-Wege, doppelt bezeugt von gcc.
- **Stand nach MF-1244 — alle 20 K4-Primärstellen sind eingeordnet, und
  das Ergebnis dreht den Wert der Regel um:**
  · **4 Fehlalarme durch Wertkollision** — `0x1900` (`P3-493`), `0x4E`
    trifft die Dezimalzahl **78** in einem Kommentar (eine
    Lückenlänge), `0xAA` trifft `UFT_PROT_UBI_SOFT = 170`, `0xF6`
    trifft `SNES_CHIP_ST018`.
  · **15 legitime Datenhaltungen** — sechs Zeilen EINER Tafel
    (`supercopy_formats[]`, 313 Einträge), drei Namenskonstanten
    (`dc_disk_size_t`), drei Profilzeilen, zwei Erkennungstafeln, ein
    Verteiler-`switch`.
  · **1 richtig benutztes Füllbyte** (`.format_fill = 0xE5`).
    Summe 4 + 15 + 1 = 20.
  · **Der einzige echte Defekt der Runde stand an KEINER der 20
    Primärstellen.** `hfe_create()` (MF-1242) liegt in
    `src/formats/hfe/uft_hfe.c`, das K4 in seiner Dateiliste gar nicht
    führt — gefunden habe ich ihn, indem ich der KONSTANTEN gefolgt
    bin, nicht der gemeldeten Stelle. Das ist der eigentliche Nutzen
    der Regel und zugleich `P3-492`.
  · **Der Umfang ist begrenzt und das steht dabei:** eingeordnet sind
    die **20 Primärstellen**, NICHT die Nebenstellen der `also`-Listen
    — allein `0xE5` nennt 94 Dateien, ungelesen. „K4 findet nichts"
    wäre zu weit gelesen.
  · Vorschlag als **Hypothese mit ihrem Test** (S5): eine Bedingung
    „Füllbytes zählen nur in HEX-Schreibweise" stellt alle vier
    Fehlalarme still — ob sie einen echten Treffer mitnimmt, ist
    **nicht gemessen**. `P3-494`.
  · **Damit ist A-029s „Fertig heißt" für K4 erfüllt:** jede
    `sicher`-Fundstelle ist behoben (1) oder benannt (19). Offen
    bleiben die anderen drei Regeln K6, P2, P3 und die Frage aus
    `P3-492`.
- **Stand nach MF-1243 — die zweite K4-Fundstelle ist eingeordnet, und
  sie ist ein FEHLALARM; die Behauptung stand im Kopf des Tores:**
  · Der Docstring von `audit_codefallen.py` führte `0x1900` unter
    „WAS AUSDRUECKLICH KEIN DEFEKT IST" als „einen der wertvollsten
    Funde" — `uft_track_analysis.c:261` trage `.track_length_max =
    6400` dezimal, DMK dieselbe Größe hex.
  · **Gemessen tragen die vier Fundstellen VIER verschiedene Größen**
    mit demselben Zahlenwert: eine BBC-ADFS-Erkennungstoleranz (mit
    min 6200 / nominal 6250 / Langspur 6350 daneben), eine
    C64-GCR-Zonengrenze, ein UDI-Datenraten-Schwellwert in Fremdcode —
    und als einzige echte DMK-Länge eine Stelle in einem
    `#ifdef DMK_PARSER_TEST`, das **nirgends** definiert wird und schon
    in `docs/selbsttest_baseline.txt:21` steht.
  · **DMK hält seine Konstante sauber** (`:37`, `#define
    DMK_DD_TRACK_SIZE 0x1900`, Dezimalzahl im Kommentar). Es gibt dort
    nichts zu vereinheitlichen.
  · Berichtigt, alter Wortlaut zitiert; `P3-493`. **Kein Code
    geändert** — die zwei wörtlichen `0x1900` im toten Selbsttest
    bleiben, Politur ohne Anlass ist MF-1077.
  · **Die Lehre für den Rest von A-029:** K4 vergleicht Zahlen, nicht
    Bedeutungen. Jede ihrer 20 Fundstellen ist ein **Verdacht**, und
    die Unterscheidung „dieselbe Größe" gegen „derselbe Wert" verlangt,
    je Fundstelle die Deklaration zu lesen. Genau deshalb ist die Liste
    nicht als Stapel abzuarbeiten — dieselbe Einsicht wie bei A-027,
    wo „fixe sie alle" mehrere Posten brauchte.
- **Stand nach MF-1242 — die erste der 20 K4-Fundstellen ist
  abgetragen, und sie war der Fall, den der Eigentümer selbst benannt
  hat:**
  · Sein Werkzeugkopf und seine Köderdatei
    `tests/formats/fixture_code_a.c:19-22` nennen `uft_hfe.c` mit
    `(bitrate >= 500u) ? 12500u : 6250u` wörtlich als den belegten
    K4-Fall. **Die Begründung stimmte, der Fehler war größer als
    notiert.**
  · **Zwei Fehler in einer Zeile.** (1) EINHEIT: HFE speichert den
    ZELLSTROM, die Rechnung lieferte DEKODIERTE Bytes — die
    Profiltafel führt beides getrennt (`track_bytes` gegen `raw_bits`,
    Faktor 16), und `hfe_create()` nahm das falsche. Jede erzeugte HFE
    erklärte eine **halbe Umdrehung**. (2) DREHZAHL: die Länge hing an
    `bitrate` allein, und der Kopf schrieb unbedingt `rpm = 300` — für
    eine 1,2-M-Diskette eine falsche Aussage IN der Datei.
  · **Das Orakel, das `P3-309` vermisst hat, gibt es** — und es zu
    finden war eine Frage des AUFRUFS, nicht des Werkzeugs (Gestalt
    von MF-1033): `hxcfe -uselayout:X -conv:HXC_HFE -foutput:Y` legt
    eine leere Diskette an, ganz ohne Eingabedatei. Gemessen je Seite:
    `DOS_DD_720KB` **12504**, `DOS_HD_1M44` **25016**,
    `X68000_2HD_1232KB` **20840** — gegen UFTs Tafel 12500 / 25000 /
    20833, also 4 bis 7 Byte Luft. `hfe_create` schrieb 6250 / 12500 /
    12500.
  · **Wo ich dem Orakel begründet NICHT folge:** alle drei HxC-Dateien
    tragen `floppyRPM = 0`, auch die mit 360 U/min. UFT schreibt die
    bekannte Drehzahl, weil HxCs eigene X68000-Datei sich damit selbst
    widerspricht — aus `bitRate 500` und `rpm 0` (= 300) rechnet ein
    Leser 25000, ihre Spurtafel sagt 20840. Ein Orakel ist eine
    Referenz, kein Beweis (MF-1015). Erfunden wird nichts: **ohne**
    Profil steht dort `0` (unbestimmt) statt der früheren falschen 300.
  · **Rotbeweis** `tests/test_hfe_create_zellen_statt_bytes.c`: vorher
    **16 grün / 8 rot**, nachher **24 grün / 0 rot** — und eine der
    acht roten war MEINE falsche Annahme (ich hielt 77x1x26x128 für
    profillos; die Tafel führt es als `UFT_FDC_FM_SD`). Der Test hat
    den eigenen Irrtum gefangen, nicht den des Prüflings; berichtigt
    auf die im Baum belegte Nicht-Treffer-Geometrie 42x2x18x512.
  · **Was ausdrücklich NICHT angefasst wurde:** `bitrate` trifft
    gemessen alle drei HxC-Werte (250/500/500) — kein Befund, also
    keine Änderung. Der Schnittstellenmodus ist ein Befund, aber nur zu
    zwei von 17 Profilen belegt und steht deshalb als `P3-490`.
  · Sprengradius vor dem Bau: 70 Ziele mit `uft_hfe.c`, 63 mit
    `uft_fdc_gaps.c`, **10 fehlende** — neue Verdrahtung
    `uft_wire_fdc_gaps()`; im Produktbau nichts zu tun
    (`UnifiedFloppyTool.pro:1181` führt die Datei schon).
  · Drei Punkte neu benannt statt mitgefixt: **`P3-490`** (Rest von
    `hfe_create` plus zwei vorbestehende tote Stellen), **`P3-491`**
    (die Profiltafel liegt im Header), **`P3-492`** (K4 nennt 6
    Dateien, gemessen sind es 9 — und die fehlende war die mit dem
    Defekt; Ursache **nicht** gemessen, S5).
  · **Rest von A-029:** 19 der 20 K4-Fundstellen offen. Sichtbar
    zerfallen sie in zwei Klassen — Größentafeln, in denen dieselbe
    Zahl legitim viele Zeilen füllt (`supercopy_formats.h` allein 15
    bis 29 Mal), und echte Doppelrechnungen wie diese. Die
    Unterscheidung ist je Fundstelle zu treffen.
    **ÜBERHOLT durch den Stand nach MF-1244 weiter oben:** die
    Zweiteilung war zu grob — gemessen sind es drei Klassen, weil
    **vier** der Funde Fehlalarme durch Wertkollision sind. Die Zahl
    „19 offen" gilt seit MF-1244 nicht mehr.
- **Stand nach MF-1234 — die D7-Lücke aus MF-1230 hat ein Tor, und der
  Weg dorthin fand einen schwereren Defekt als die Warnung:**
  · MF-1230 hatte gemessen, dass D7 („warnungsfrei unter `-Wall -Wextra
    -Wpedantic`") **kein Tor** hat — acht Verletzungen an einem Tag,
    vier behoben, vier benannt. Die vier benannten waren
    `uft_track_release`/`uft_track_alloc`-Aufrufe **ohne Deklaration**.
  · **Beim Nachmessen war eine davon keine Warnung, sondern eine
    Falschaussage mit umgekehrtem Vorzeichen.**
    `tests/test_spur_freigabe_zaehlt.c:155` behauptete
    `ZUSAGE(rc == UFT_OK, "uft_track_alloc() gelingt")` — und war grün,
    **WEIL der Aufruf fehlschlug**: `uft_track_alloc()` ist ein
    `uft_track_t*(size_t, size_t)`, die Adresse `&t` landete in
    `max_sectors` (**86 467 659 344**), `calloc()` hätte **16,6 TB**
    gebraucht, Rückgabe `NULL`, ohne Deklaration auf `int` verkürzt zu
    **0** — und `0` ist `UFT_OK`. `t` wurde dabei **nie** beschrieben.
    Schärfer als Tor 64: nicht „wird nie rot", sondern „grün durch den
    Misserfolg". Einzelheiten und Belege: **`P3-481`**.
  · **Der Rotbeweis ist der Übersetzer**, wie schon bei vier der sechs
    Defekte in MF-1230: mit sichtbarer Deklaration bricht gcc 13.1.0 mit
    `incompatible types when initializing type 'uft_error_t'` ab.
  · **Das Tor ist eine Zeile** — `tests/CMakeLists.txt:254` gibt jedem
    Testziel `-Werror=implicit-function-declaration` neben dem `-UNDEBUG`
    aus MF-830. **Sprengradius vor dem Scharfstellen gemessen:** 478
    übersetzbare C-Dateien unter `tests`, **3** fielen, **473** sauber,
    **26** mit flachem Include-Pfad nicht messbar (benannt; der Vollbau
    deckt sie). Zwei weitere Treffer sind Prüf-Eingaben (`fixture_*` in
    `tests/formats`) und **kein Bauziel** — `build.ninja` nennt sie
    0 Mal —, also **keine Ausnahmeliste**, und das ist der Punkt.
  · Ersatz für die zurückgenommene Zusage, **schärfer** als sie:
    `t.sector_capacity == 32`, die Erstwachstum-Konstante aus
    `uft_track_add_sector()` (`uft_format_plugin.c:480`). Die falsche
    Zusage bleibt **zitiert stehen** („nicht entfernen weiter
    erweitern").
  · Eine Gedächtnisnotiz hält die Klasse:
    `test_gruen_weil_der_aufruf_scheiterte.md`.
- **Stand nach MF-1236 — die Härtung war richtig, sie lief nur nicht,
  und CI konnte es nicht sehen:**
  · Beim Abnehmen von MF-1235 meldete `audit_codefallen.py
    --selbsttest` **41/42**. Der fallende Fall heißt „Ausgabe hält ein
    Zeichen ausserhalb von cp1252 aus" und druckt genau das `U+2044`
    aus dem MF-1171-Absturz. Mit `PYTHONIOENCODING=utf-8`: **42/42**.
  · **Nicht die Kodierung war die Ursache.** `_ausgabe_haerten()`
    (`audit_common.py:85`) ist richtig — `reconfigure(errors='replace')`
    lässt cp1252 stehen und ersetzt das Zeichen. Sie **lief nur
    nicht**: der Aufruf stand an **einer** Stelle, in
    `run_and_report()`, und der `--selbsttest`-Pfad läuft daran vorbei,
    weil beide Prüfer gleich nach `parse_args()` verzweigen.
  · **Die Lücke war schon einmal geschlossen worden — für einen von
    zweien.** `audit_teilstring.py` hat einen eigenen Aufruf in seinem
    `selbsttest()` (`:659`); `audit_codefallen.py` enthielt den Namen
    `haerten` **0 Mal**. Das ist die Gestalt aus MF-519/MF-529: eine
    Korrektur an einer Datei sagt nichts über ihre Nachbarn.
  · Seit MF-1236 steht der Aufruf in **`add_common_args()`** — dem
    Engpass, den **beide** Eingänge vor jeder Verzweigung passieren
    (`:693` bzw. `:896`). Der Aufruf beim Import wäre stärker, ist aber
    eine Nebenwirkung beim Einbinden; die Wahl steht begründet im
    Kommentar. Der alte Aufruf bleibt für einen Nutzer von
    `run_and_report()` ohne die Argumentschicht.
  · **Rotbeweis in beide Richtungen**, dieselbe Kommandozeile und
    dieselbe Umgebung (`stdout encoding = cp1252`): **41/42 → 42/42**.
    `audit_teilstring --selbsttest` bleibt 17/17, Tor 66 474/0, Tor 67
    20/0, Meta-Tor 75/0.
  · **Und die unangenehme Hälfte:** `.github/workflows/teilstring.yml`
    fährt diesen Selbsttest sehr wohl (`:131`), und **zwei Jobs hängen
    daran** — aber auf `ubuntu-latest` **ohne** `PYTHONIOENCODING`,
    `LANG` oder `LC_ALL` (gemessen, 0 Treffer). Dort ist die Kodierung
    UTF-8, der Fall kann **nie** feuern. Die einzige Umgebung mit der
    Bedingung ist der Windows-Rechner, und den bewacht nichts: die
    **Umkehrung** der Lehre aus `ci_test_gating.md`.
  · Ein Tor dafür ist ausdrücklich **nicht** gebaut — ein CI-Schritt
    mit `PYTHONIOENCODING=cp1252` wäre billig und der offensichtliche
    nächste Griff, aber das ist eine Entscheidung über die CI-Matrix
    und kein Nebeneffekt dieses Fixes. Steht in `P3-484`.
- **Stand (2026-09-18, MF-1239 — das Waisenregister ist namensblind,
  und ich habe den Befund mit einem eigenen Fehlgriff bezahlt):**
  · Anlass war ein Nebenbefund aus MF-1238: `uft_xfd_parser_v2.c` hat
    acht exportierte Funktionen, **0 echte Aufrufer** und steht
    **nicht** auf `docs/orphan_baseline.txt`. Gemessen liegt es an
    `scripts/audit_orphan_modules.py:229` — die Erreichbarkeitsprüfung
    ist **namensbasiert**, ein **ODER** über alle Namen einer Datei,
    und **blind für `static` und Signaturen**.
  · `src/formats/xfd/uft_xfd.c:163` definiert ein **eigenes**
    `static uft_error_t xfd_open(uft_disk_t*, const char*, bool)`,
    `:238` ein `static void xfd_close(uft_disk_t*)`, und `:361`
    verdrahtet `.open = xfd_open, .close = xfd_close` auf eben diese
    Statics. Für den Index sind das zwei Treffer.
  · **Und die Grundlinie sieht vollständig aus:** 204 Einträge gegen
    204 in der Spalte „keiner". Eine Summe, die aufgeht, sagt nichts
    über die Verteilung darin (MF-1026, MF-1079) — hier auf ein
    **Register** angewandt.
  · Suchraum begrenzt statt geschätzt: **84** Kollisions-Kandidaten in
    **40** Namen über 747 Quelldateien. **Ein Kandidat ist kein
    Befund** — `serial_open` hat drei externe Definitionen, aber das
    sind Plattformvarianten (`serial_linux/null/win32.cpp`), von denen
    nur eine übersetzt. Wer aus 84 Paaren 84 Befunde macht, hat nicht
    gemessen.
  · **Mein Messwerkzeug trug ZWEIMAL dieselbe Klasse, die es sucht:**
    der Regex verlangte Leerraum zwischen Typ und Namen und verpasste
    `Type* name(` — ausgerechnet `xfd_context_t* xfd_open(` fiel
    heraus; und die Auszählung lief über `[a-z_]+` und kannte keine
    Ziffern, also fehlten `crc16`, `crc16_ccitt`,
    `d64_sectors_on_track`. Erste Zahlen 76/32 waren eine untere
    Schranke **meines Werkzeugs**, richtig sind 84/40.
  · Nebenbefund, belastbar: `crc16_ccitt` steht 1× extern und **5×
    `static`** in fünf Dateien, `d81_sector_offset` 1× extern und 3×
    `static` — MF-1177 im Maßstab. Ob die sechs `crc16_ccitt`
    **übereinstimmen**, ist **nicht** gemessen; das ist die
    MF-1015-Frage.
  · Einzelheiten und der Weg: **`P3-488`**. Kein Fix in diesem
    Commit — `used_elsewhere()` zu ändern bewegt die Grundlinie und
    braucht einen Rotbeweis am bekannten Fall **plus** eine
    Gegenprobe für die Plattformvarianten.
  · **MEIN FEHLGRIFF, und er gehört hierher:** ich habe diesen
    Befund in `OPEN_ITEMS.md` eingetragen, **während der Versand von
    MF-1238 lief**. `docs/STAND.md` führt die Zeilenzahl von
    `OPEN_ITEMS.md`, also wies der pre-push-Haken mit
    `[STAND.md stale]` ab — ein Zustand, den **meine eigene
    gleichzeitige Änderung** erzeugt hat. `CLAUDE.md` §MF-1096 Regel 2
    nennt den **Commit**; der pre-push-Haken fährt aber dieselbe
    Prüfung. Die Regel gilt dort genauso, und das steht jetzt in der
    Gedächtnisnotiz. Nichts war kaputt, die Artefakte kamen zurück,
    der Commit blieb intakt — es hat einen Versandlauf gekostet.
- **Stand — was AUSDRÜCKLICH offen bleibt:**
  · **Der qmake-Produktbau trägt die neue Fahne NICHT.** Sie sitzt im
    CMake-Testbau; CI baut alle drei Plattformen mit qmake, die 715
    Produktivdateien sind damit unbewacht (`P3-481` Punkt 2).
  · **`uft_error_t` hat zwei Zweige** (`uft_error.h:129` gegen `:137`,
    `#ifdef UFT_ERROR_ENUM_DEFINED`). Wo der `int`-Zweig gilt, ist
    derselbe Fehler nur eine `-Wint-conversion`-**Warnung** — der harte
    Fehler ist also nicht allgemein, und nur das Tor an der **Ursache**
    deckt beide Zweige.
  · **Das C-Modul `uft_match` ist NICHT übernommen.** Das Paket bringt
    `include/uft/util/uft_match.h` (6 917 B), `src/util/uft_match.c`
    (6 817 B) und `tests/test_match.c` (12 737 B) mit — zehn Funktionen,
    je eine pro Fallenklasse C1…C6. **Und dafür gibt es einen gemessenen
    Anlass, der über dieses Paket hinausgeht:** `audit_teilstring.py`
    nennt in seinen Fundmeldungen `uft_suffix_eq()`, `uft_magic_at()`
    und `uft_id_eq()` als Ersatz, und `git grep` findet diese Namen
    **ausschließlich** in dieser einen Datei — **Tor 66 verweist auf
    Funktionen, die es im Baum nicht gibt.** Das Modul ist damit das
    Werkzeug für die 51 Fundstellen aus `A-027`/`P3-479` und gehört in
    einen eigenen Posten (Scope-Regel: neues C-Modul + Test +
    Verdrahtung von 51 Stellen).
  · Die **20 K4-Fundstellen** sind sichtbar, nicht abgetragen —
    darunter `0x1900`, wo `uft_track_analysis.c:261` dieselbe Spurlänge
    dezimal führt, die DMK hex führt. Genau „eine Größe, eine Rechnung"
    (MF-1177), und nicht umgesetzt.
  · **K1, K2, K3, K5 fehlen** (Klemmen, Zähler beim Abbruch,
    Vorzeichenbreite, Abbruch bei „frei").
  · `tests/run_code_selftest.py` ist NICHT als eigene Datei übernommen:
    seine Prüfungen liegen in `--selbsttest` (Regeln, Verbote,
    Heimatfilter in beide Richtungen) und im Meta-Tor (Blindheit über
    gepflanzte Bäume). Was dort NICHT nachgebaut ist: die Prüfung der
    Rückgabewerte über einen Unterprozess — die deckt die CI-Spur ab,
    und der Rotbeweis hat sie mit rc 1 vorgeführt.
- **Beleg:** MF-1230 (Commit folgt in derselben Sitzung).

  **DREI DINGE, DIE SCHON BEI DER AUFNAHME FESTGEHALTEN GEHÖREN,
  weil sie später falsch gelesen würden:**

  **1. Die Falle ist eingebaut, nicht vermeidbar.** xDMS ist die **Hand,
  aus der UFTs Leser stammt** (MF-837, MF-1135). Ein Differenzlauf gegen
  xDMS ist deshalb eine **Port-Treue-Prüfung**, kein Stufenbeleg — wer
  diese Zeile später liest, darf „xDMS-Differenzlauf" nicht für
  T1b-Beleg nehmen. Genau das hat MF-1135 gekostet, und der Ausweg war
  `dlitz/adf2dms` als **unabhängige Schreiber-Linie** (MIT, Quellstand
  `8adfe6acfdc9`). Wer hier etwas heben will, braucht wieder eine
  dritte Hand.

  **2. Die Fassungen sind nicht dieselben.** Unser Port zitiert **xDMS
  1.3**, der Tarball ist **1.3.2**. Eine Abweichung kann deshalb
  zweierlei sein: ein Portfehler ODER eine Änderung der Quelle zwischen
  den Fassungen. Die Unterscheidung ist je Befund zu treffen und nicht
  pauschal — sonst entsteht die Lage aus MF-1015, wo drei Prüfsummen
  nebeneinander standen und keine zwei gleich waren.

  **3. Eine Zahl im Baum stimmt nicht mit der Messung.** Der
  Scout-Bericht in `docs/OPEN_ITEMS.md` nennt „`uft_dms.c` ist ein
  **1940-Zeilen-Port** ohne Abgleich"; gemessen sind es heute **1035**.
  Entweder ist die Datei geschrumpft oder die Zahl war falsch. Das ist
  **nicht entschieden** (S5) und gehört in die Arbeit, nicht in die
  Aufnahme — über `git log -p` auf die Datei zu klären.

### A-033 · Code-Review `b8d2419a…HEAD` abtragen (18 Befunde, zwei Arbeitsströme)
- **Status:** **aufgenommen** (die drei eigenen sind abgetragen, der Rest
  wartet) · **Aufgenommen:** 2026-09-19
- **Wortlaut:** „Code-Review b8d2419a…HEAD (8 Commits, 42 Dateien,
  +7054/−127) … Summe: Standards 10 (4 hart, 6 Ermessen) — schwerster:
  #3, dieselbe Fähigkeitsflagge wird im selben Dialog als ‚nicht
  feststellbar' und als ‚kein GCR-Format erkannt' ausgegeben (erfundene
  Negativmessung). Spec 8 (6 fehlend/teilweise, 1 Scope-Creep, 1
  falsch) — schwerster: #1, der Kopierplan wird von keinem Vorgang
  gelesen."
- **Kennzahl:** keine der vier — Verlässlichkeit des Prüfstands und
  Prinzip 7, wie `P3-421`, `P3-479`, `P3-484`, `P3-507`
- **Kanal:** entfällt — eigener Baum
- **Einfrier-Regel:** teils **ja** (Spec #7 nennt eine Varianten-API mit
  +831 Zeilen im Format-Layer), für die abgetragenen drei **nein**
- **OPEN_ITEMS:** `P3-505` (geschlossen), `P3-508`, `P3-509`, `P3-510`
  (neu); berührt `P3-439`, `P3-507`
- **Fertig heißt:** jeder der 18 Befunde ist entweder behoben, als
  `P?-NNN` eingetragen, oder mit Messung widerlegt — keiner bleibt
  unbeantwortet stehen
- **Aufwand:** nicht schätzbar
- **Stand 2026-09-19 — nachgemessen statt übernommen:**
  · **Bestätigt und ABGETRAGEN (meine):** St-2 (die Doktrin kannte die
    Endungsregel nicht → Regel **2b** in `docs/SONDEN_DOKTRIN.md`,
    MF-1260) · St-1 (`P3-505` stand „offen", obwohl MF-1255 die
    geforderte Form gebaut hatte → geschlossen mit Beleg).
  · **Bestätigt und EINGETRAGEN statt angefasst** (liegen in
    `d7536e14`, dem Strom der zweiten Sitzung): St-3 → **`P3-508`**
    (der schwerste des Reviews) · Sp-1 → **`P3-509`** · Sp-8 →
    **`P3-510`**. Sp-8 ist dabei am eigenen Baum belegt:
    `src/formats/d81/uft_d81_parser_v2.c:9` sagt „MFM encoding (not
    GCR!)".
  · **WIDERLEGT: Sp-5.** Der Review nennt
    `include/uft/uft_format_plugin.h:1277` als Ort des Satzes „bei
    Gleichstand der zuerst registrierte" — dort steht er nicht.
    Gefunden habe ich ihn nur in `src/core/uft_format_plugin.c:493`,
    und dort ist er die **Vergangenheitsform im Kommentar der
    Behebung**: „Der Code gab trotzdem einen Sieger zurueck — den
    zuerst registrierten." Das beschreibt den Defekt, den MF-1251
    beseitigt hat.
  · **HALB widerlegt: Sp-4.** `uft_smart_open.c:161` gibt wirklich
    unbedingt `ranked->winner` zurück — bestätigt.
    `uft_probe_format_impl.c` dagegen gibt bei `!r.winner`
    `UFT_FORMAT_UNKNOWN` zurück **und füllt `tied_with[]`**; es meldet
    den Gleichstand, statt ihn zu verschlucken.
  · **Eine eigene Annahme fiel beim Messen**, und das gehört
    hierher: ich hielt den MYZ80-gegen-`cpm`-Gleichstand aus `P3-439`
    für einen Fall, den Regel 2b inzwischen entscheidet. Gemessen
    gewinnt auf `cpmtools_cf2dd_720k.cpm` **`MSX` mit 45 bei `tied`
    = 1** — der Gleichstand bei 25 liegt gar nicht an der Spitze, und
    2b rührt ihn nicht an. `P3-439` bleibt unberührt offen.
  · **Ermessensfrage St-10 angenommen, aber nicht abgetragen:** mein
    Heredoc in `teilstring.yml` ist nach MF-1096 kein harter Verstoß
    (die Sperre begründet sich mit Escape-Verlust in Agenten-Shells,
    mein Block trägt keinen Backslash) — als Datei in `scripts/` wäre
    er aber selbsttestbar. Offen.
  · **NICHT nachgemessen und deshalb NICHT als bestätigt geführt:**
    St-4 bis St-9, Sp-2, Sp-3, Sp-7.
  · **Nebenbei gemessen, als Signal notiert statt als Fund:** 556
    Tabellenzeilen in `docs/OPEN_ITEMS.md` weichen in ihrer naiven
    `|`-Zahl von ihrer eigenen Kopfzeile ab. Das ist **kein** belegter
    Rendering-Fehler — mein Zähler splittet naiv, ein Renderer tut das
    womöglich anders. Wer es verfolgt, misst zuerst am gerenderten
    Ergebnis. (Dabei aufgefallen, weil ich mir mit `P3-508` selbst eine
    Zeile zerrissen hatte: drei `|` aus einem Flaggen-Ausdruck.)
- **Stand 2026-09-19, zweiter Durchgang — `P3-508` abgetragen
  (MF-1261), nach Eigentümerentscheidung „Fixe den Kern":**
  · **Die Kollisionsfrage war gegenstandslos, und das ist gemessen:**
    `src/formattab.cpp` und `src/core/uft_copy_plan.c` tragen **keine**
    offenen Fremdänderungen; letzte Berührung `d7536e14`, committet und
    draußen. Ich hatte sie vorgelegt statt sie anzunehmen — richtig
    gefragt, und die Antwort war „geh rein".
  · **Rotbeweis zuerst**, `tests/test_copy_plan.c` P4b: er fährt jedes
    Profil mit leerer Merkmalsmaske und einem Formatnamen, den kein
    Behelf führt. Gefunden: **VIER** behauptende Begründungen
    (`doscopy`, `bamcopy`, `nibblecopy`, `cyclone`) — **eine mehr, als
    der Review nannte**. Danach: 8 Ablehnungen, **0 behauptet, 0 ohne
    Namen**.
  · **Behoben wurde die Ursache, nicht der Wortlaut.** Die `if`-Kette
    zählte sechs der acht Flaggen ein zweites Mal auf — in der Datei,
    deren Tafelkopf sich selbst „die EINZIGE Stelle" nennt. Jetzt
    trägt `k_cap_name[]` eine dritte Spalte `fehlt`, die Funktion läuft
    über die Tafel, und die latente Lücke (`Timing`, `schwache Bits`)
    ist zu.
  · **Die richtige Sprache lag schon vor:** von den sechs alten
    Zeichenketten war „keine Mehrfachlesung **zugesagt**" die einzige
    ehrliche. Sie ist jetzt die Form für alle acht.
  · **Der Wächter prüft die Gestalt, nicht ein Wort** — „setzt …" …
    „nicht zugesagt". Rot-Probe vorgeführt: die Mutation „kein
    GCR-Format **gefunden**", die das Wort „erkannt" vermeidet, fällt
    trotzdem (2 Ablehnungen, rc 1). Ohne diese Verschärfung wäre die
    Zusage ein Wortfilter gewesen.
  · **Nebenfolge benannt:** bei mehreren fehlenden Flaggen nennt die
    Schleife die erste der Tafel — `bamcopy` sagt bei leerer Maske
    „setzt Dateisystem voraus" statt „Commodore-BAM". Beides wahr;
    sobald eine zugesagt ist, nennt sie die wirklich fehlende.
  · **Die Oberfläche blieb unberührt**, wie beauftragt. Der Qt-Test
    `test_format_tab_copy_plan.cpp:450` trägt weiter, weil er auf den
    **Fähigkeitsnamen** prüft und nicht auf die Prosa.
- **Stand 2026-09-19, dritter Durchgang — Auftrag „weiter mit p3-509 &
  p3-510": EINER ist abgetragen, der andere vermessen:**
  · **`P3-510` behoben (MF-1262).** `D81` raus aus der Behelfsliste von
    `nibblecopy`; `bamcopy` behält es, weil es `CBM_BAM | FILESYSTEM`
    verlangt und eine BAM Dateisystemebene ist. Rotbeweis über die
    öffentliche API: von 2 GCR-Profilen bot sich **1** für D81 an,
    danach **0**; zwei Gegenproben halten es davon ab, eine pauschale
    Verweigerung zu werden.
  · **Vier Zeugen, alle im eigenen Baum** — und der vierte ist der
    stärkste: `cyclone` verlangt ebenfalls GCR und hat D81 **nie**
    geführt. Der Baum widersprach sich selbst, die richtige Seite lag
    schon darin. Dazu `uft_d81_parser_v2.c:9`, `mfm_detect.c:87/115`
    (die 1581 ist eine Geometrie des MFM-**Erkenners**) und
    `commodore/d81.c:100`.
  · **Grenze ausgesprochen:** der Baum hat **kein** maschinenlesbares
    Kodierungsfeld je Format. Die Zusage ist ein Regressionsnagel,
    keine Ableitung — ein falscher NEUER Eintrag fällt nicht auf.
  · **`P3-509` NICHT abgetragen, und der Grund ist gemessen, nicht
    geschätzt.** (1) `src/formattab.cpp` führt **nichts** aus — `git
    grep` nach `DecodeJob`, `uft_convert`, `QThread`, `start(` gibt 0
    Treffer; der Plan entsteht an einer Stelle ohne Vorgang. (2) Eine
    bloße Abbildungsfunktion `uft_copy_plan_to_convert_options()` wäre
    **D2-widrig** — ein Algorithmus ohne Aufrufer, also dieselbe Tür
    ohne Leser eine Ebene höher. (3) Von den Verdrahtungspunkten sind
    `toolstab.cpp`, `mainwindow.cpp` und `statustab.cpp` **fremd
    geändert**; der einzige saubere (`decodejob.cpp`) hat wegen (1)
    keinen Weg zum Plan.
  · **Was zu entscheiden ist, bevor jemand Code schreibt:** welcher
    Vorgang liest den Plan — bekommt der Formatreiter einen eigenen,
    oder holt `MainWindow` ihn beim Start eines `DecodeJob` ab? Davon
    hängt auch ab, ob die EINFRIER-REGEL greift.
  · **Nebenbefund derselben Klasse, notiert statt vergessen:**
    `uft_batch_run()` hat **0** Aufrufer außerhalb seiner Datei — eine
    zweite Tür ohne Leser, und vielleicht der natürliche Verbraucher
    eines Plans.
- **Stand 2026-09-19, vierter Durchgang — `P3-509` abgetragen
  (MF-1263), nach „Fixe den Kopierplan das P3-509 läuft":**
  · **Die Kette läuft:** `FormatTab::copyPlan()` →
    `MainWindow::m_formatTab` → `speichereNach()` → `uftSaveImageAs()` →
    `uft_copy_plan_to_convert_options()` → `uft_convert_file()`.
    `FormatTab` war bei `mainwindow.cpp:157` eine **lokale** Variable —
    der Plan existierte, und niemand kam an ihn heran.
  · **Zwei der vier Achsen erreichen die Wandlung, und welche nicht,
    steht dabei.** Lesestrategie (`decode_retries`,
    `use_multiple_revs`) und Richtlinie (`verify_after`) kommen an;
    Ebene und Erhaltung nicht, weil `preserve_timing`, `normalize` und
    `interpolate_errors` gemessen **null** Leser haben, die danach
    handeln. Auf sie abzubilden hätte „der Plan wirkt" behauptet, ohne
    dass sich etwas ändert — `P3-509` eine Ebene höher.
  · **Die Zahlen sind abgeleitet, nicht erfunden:** sie stehen in
    `k_strategie[]` (FAST 0/1, STANDARD 3/2, DEEP 10/5, CONSENSUS über
    `consensus.enabled`). Und wo die Tafel keine Zahl nennt, mache ich
    keine: `SALVAGE` trägt `read.retries = "hoch"` — ein
    Forderungswort. **Rot-Probe:** die `strtoul`-Falle (aus „hoch" wird
    0) fällt sofort.
  · **D2 ist erfüllt, nicht behauptet.** `planAngewandt` liest die
    **fertigen** Optionen ab statt den Plan zu wiederholen, und
    `kopierplan_erreicht_die_wandlung` fällt gemessen (rc 1), wenn man
    den Abbildungsaufruf entfernt.
  · **Ein eigener Denkfehler, gefangen vom Test:** die erste Fassung
    speicherte `d64 → d64` — das ist die **Identität**, die nur kopiert
    und den Optionsblock nie erreicht. Der Plan kann nur wirken, wo
    gewandelt wird; das Ziel ist jetzt `.g64`.
  · **Und eine Messfalle in eigener Sache:** ein `rc=0` hinter einer
    Pipe meldete `tail`, nicht das Testprogramm — erst die Umleitung in
    eine Datei zeigte `rc=1`. Dieselbe Klasse wie
    `grep_exitkode_bricht_die_kette`.
  · **`src/mainwindow.cpp` trägt Fremdänderungen** (MF-1194, vier
    verdrahtete Reiter). Vorgemerkt wurden nur meine Hunks; im Index
    stehen **0** Zeilen MF-1194.
  · **Offen bleibt der zweite Halbsatz der Vorgabe** („… ist EIN
    Vorgang"): erreicht ist der **Speicherpfad**, nicht jeder Vorgang.
    `uft_batch_run()` hat weiterhin 0 Aufrufer.
  · **Noch nicht angefasst, aus dem Review benannt:** Standards #5
    (`setzt_t.value` trägt vier Wertarten — Zahlen, `true`/`false`,
    Forderungen; `nur_zahl()` umgeht es, behebt es aber nicht), #6
    (Stufenzuordnung zweimal gerechnet) und Spec #2 (der Auffangzweig
    `return "plan"`).
- **Stand 2026-09-19, fünfter Durchgang — die drei Optimierungen
  (MF-1264), und zwei davon sahen anders aus als gemessen:**
  · **#5 behoben.** `uft_wert_art_t` + `uft_copy_wert_art()`: die
    Einteilung steht an **einer** Stelle, `uft_copy_enforced_t` trägt
    sie mit (additiv). Vorher entschied jede Stelle für sich —
    `haenge_wert()` schnüffelte beim JSON-Schreiben, `nur_zahl()` beim
    Abbilden, der Struct-Kommentar zählte auf. **Berichtigt:** es sind
    **drei** Wertarten (46 Wahrheit · 13 Forderung · 9 Zahl), nicht
    vier — die vier waren die Forderungswörter.
  · **#6 in der Prämisse berichtigt, nicht „behoben".** `stage` grenzt
    **Konflikte** ein („ein Konflikt gilt nur innerhalb einer Stufe"),
    `abschnitt()` gruppiert **JSON**. Zwei Fragen; `write.verify` ist
    in beidem richtig. Eine Änderung an `abschnitt()` wäre eine
    Behebung ohne Befund gewesen. **Der echte Rest:** die
    Rückfall-Vorsilbenliste *in* `uft_copy_param_stage()` widerspricht
    der Tafel bei genau `write.verify` (155 Parameter, 1 Abweichung, 40
    ohne passende Vorsilbe) — P9 hält fest, dass die Tafel gewinnt.
  · **Spec #2 eingetragen, nicht entschieden** (`P3-511`): **98 von
    155** Parametern fallen in den Auffangzweig, weil ihre Stufen
    (DECODE, LAYOUT, FS, VERIFY) gar keinen Abschnitt haben. Die
    JSON-Gestalt ist deine Vorgabe — vier Abschnitte zu erfinden wäre
    MF-1077 in Reinform. Der Review nannte „10 von 48"; das misst
    dieselbe Lage an den **erzwungenen** Parametern. Beide Zahlen
    stimmen.
- **Stand 2026-09-19, sechster Durchgang — der zweite Halbsatz
  (MF-1265): „EIN Vorgang" ist eingelöst:**
  · **Gemessen füllen genau DREI Stellen** `uft_convert_options_t`:
    `uft_save_image.cpp` (seit MF-1263), `decodejob.cpp`,
    `toolstab.cpp` — dazu eine **zweite** `DecodeJob`-Erzeugung in
    `workflowtab.cpp`, die MF-1263 gar nicht kannte. Vorher trug
    einer von dreien den Plan.
  · **Die Bauform ist gemessen, nicht gewählt:** die drei
    Verbraucher kennen den Formatreiter nicht, und `git grep "new
    FormatTab"` gibt genau **einen** Treffer. Also gibt der
    Besitzer den Plan heraus statt dass drei Wege ihn durchreichen.
  · **BERICHTIGT NOCH IM SELBEN DURCHGANG — hier stand
    „(`FormatTab::aktuellerPlan()`)", und der BINDER hat daran
    einen Entwurfsfehler gefunden, nicht ich.** Eine statische
    Auskunft in `FormatTab` hängt `ToolsTab` an einen anderen
    Reiter; gemessen binden `test_tools_tab_convert` und
    `test_tools_tab_track_view` genau `toolstab.cpp` **ohne**
    `formattab.cpp`, und der volle Bau endete mit `undefined
    reference to FormatTab::aktuellerPlan()`. Die Auskunft wohnt
    jetzt im **Kern**: `uft_copy_plan_current()`, gespeist von
    `uft_copy_plan_set_quelle()`. Eine **Funktion**, kein
    hinterlegter Wert — eine Kopie könnte veralten, sobald der
    Bediener etwas umstellt. Und die Anmeldung steht **zuletzt**
    im Konstruktor, weil `copyPlan()` die Auswahlfelder liest.
  · **Zwei Wege, und der Unterschied ist der FADEN:** `ToolsTab`
    fragt direkt (GUI-Faden), `DecodeJob` bekommt eine **Kopie**
    vor `moveToThread()`. Ein Reiterzugriff aus dem Arbeitsfaden
    wäre ein Fehler, den kein Test zuverlässig fängt.
  · **Rot-Probe auf das eigentliche Risiko, und sie fällt schärfer
    aus als erwartet:** die Abmeldung im Destruktor weglassen →
    `ctest` meldet **`Exception: SegFault`**, nicht einen falschen
    Wert. Der hängende Zeiger ist nicht theoretisch. Isoliert
    gegen die Nachbarn: von den drei Zusagen derselben Datei
    stürzt genau diese ab (rc 139), die beiden anderen bleiben
    rc 0.
  · **`accept_data_loss` steht in allen drei Pfaden NACH dem Plan**
    und bleibt false (UFT-A05).
  · **Ehrlich offen:** für die drei neuen Aufrufstellen gibt es
    keine je eigene Verhaltensprobe — belegt sind sie durch
    Übersetzung und die Speicherpfad-Zusage aus MF-1263. Und
    `uft_batch_run()` hat weiterhin 0 Aufrufer.
  · **Zwei Rot-Proben:** der `strtoul`-Rückfall („" wird Zahl") und die
    Vorsilbe gegen die Tafel — beide rc 1. Bei der zweiten feuert eine
    **fremde** Zusage mit („40 Parameter der Tafel haben keine Stufe")
    und bestätigt die Messung unabhängig.
- **Beleg:** —

### A-035 · DTC-Upgrade-Paket umsetzen — nach `docs/plans/DTC_UPGRADE.md` (DTC-0 bis DTC-7)
- **Status:** aufgenommen · **Aufgenommen:** 2026-09-26
- **Wortlaut:** „mach einen plan wie wir das perfekt umsetzten können"
  (zu `UFT_DTC_Upgrade_v1.zip`) · „ja , aufnehmen und die Liste aufräumen"
- **Kennzahl:** **Bench-Alter runter** (Fähigkeitszusage KryoFlux/FluxEngine:
  heute kann kein echter DTC- oder FluxEngine-Aufruf gelingen, `P3-562`).
  DTC-4 und DTC-5 bewegen keine der vier → dort **Fundus**, bis ihre
  Bedingung erfüllt ist.
- **Kanal:** **Nachbau** für DTC-1 bis DTC-3 — die drei Befunde sind im
  Baum gemessen und brauchen keine Zeile aus dem Paket; **Port** einzelner
  Teile nur nach Klärung der Herkunft (DTC-0, Präzedenz MF-1099).
- **Einfrier-Regel:** DTC-1 bis DTC-3 **nein** (Prozess-Läufer und Provider,
  keine Format-/Decoder-Schicht). DTC-5 (Umdrehungs-Solver) **ja →
  Rotbeweis und benannte Referenz zuerst**.
- **OPEN_ITEMS:** `P3-562` (neu), `P3-342`, `P3-341`
- **Fertig heißt:** DTC-1, DTC-2 und DTC-3 je mit eigenem Commit und MF,
  jeder mit Rotbeweis gegen den Vorzustand und grünem Test danach;
  `test_kryoflux_*` kann an stdout-als-Fluss und an doppeltem `argv[0]`
  rot werden; DTC-4/DTC-5 stehen mit ihrer Öffnungsbedingung im Fundus;
  `docs/CAPABILITIES.md` führt KryoFlux Read weiter als 🟡, bis DTC-7
  (Bench) gelaufen ist.
- **Aufwand:** DTC-1 ~40, DTC-2 ~120, DTC-3 ~60 Zeilen (Schätzung aus dem
  Plan, nicht gemessen); DTC-7 nicht schätzbar (Gerät, MF-310).
- **Stand:** Plan steht (`8b39700d`, MF-1357). **Wartet auf DTC-0:**
  (1) wer hat das Paket verfasst, unter welcher Erteilung; (2) Solver in
  den Fundus oder Differenzlauf gegen `uft_multi_rev_fusion`.
- **Beleg:** —

## Fundus

*(aufgenommen, bewegt aber keine der vier Kennzahlen aus `CLAUDE.md`
§„jeder Baustein benennt seine Kennzahl" — nach MF-640 ist das Fundus,
nicht Auftrag. Steht hier, bis ein Anlass es hochholt.)*

### A-034 · `src/dtc_components/` — Einbaustand und Verbesserung
- **Status:** **aufgenommen** · **Aufgenommen:** 2026-09-20
- **Wortlaut:** „https://github.com/Axel051171/UnifiedFloppyTool/tree/main/src/dtc_components
  in wie weit sind die teile schon in das tool implemtiert , gibt es was zu
  verbessern"
- **Bei der Aufnahme gemessen (2026-09-20), die Frage nach dem Einbaustand
  ist damit beantwortet:** das Paket liegt versioniert im Baum (10 `.c`,
  1 Header, 1 Testdatei, **13 928 Byte** Quelltext — eine Zeile je
  Funktion, deshalb sind es nur 160 Zeilen). Der Header exportiert
  **30 Funktionen**. Je Bezeichner über `git ls-files` gezählt, Kommentar-
  gegen Aufrufzeilen getrennt:
  · **0 Produktivaufrufer.** Kein einziger.
  · **16 nur aus Tests** (`test_dtc_nenner.c`, `test_dtc_ungeprueft.c`,
    `test_crc_gegen_norm.c`, `test_zellregel.c`).
  · **14 ohne jeden Aufrufer** (`dtc_fm_decode`, `dtc_mfm_decode`,
    `dtc_gcr_decode`, `dtc_flux_to_bits`, `dtc_scan_amiga_sync`,
    `dtc_ctraw_read`, `dtc_format_name`, …).
  **Eine eigene Fehlmessung dabei berichtigt:** `dtc_find_run_violation`
  sah zunächst wie der eine Produktivaufrufer aus, weil `grep` es in
  `src/core/uft_zellregel.c:10` und `include/uft/core/uft_zellregel.h:59,94`
  findet. Alle drei Fundstellen sind **Kommentarzeilen**; die einzigen
  echten Aufrufe stehen in `tests/test_zellregel.c:306,308` als
  Differenzlauf. Das ist wörtlich die Klasse
  [[aufrufer-gegen-kommentar]] — ein Bezeichner im Kommentar ist kein
  Aufruf.
  **Im Bau ist es ausdrücklich OPT-IN** (`UnifiedFloppyTool.pro:485-517`,
  `CONFIG+=uft_dtc_components`, MF-1100); voreingestellt wird nichts davon
  übersetzt. Die Abnahme liegt auf der CMake-Seite
  (`tests/CMakeLists.txt:4878 ff.`).
  **Damit ist der Stand: Bestand, nicht Fähigkeit** — dieselbe Lage wie
  beim Kopierschutz-Katalog (P0-2), den DeepRead-Modulen (MF-627/MF-767)
  und der Merkmalstafel (MF-1176).
- **Kennzahl:** **keine der vier** — deshalb steht der Posten hier und
  nicht in der Warteschlange. P3-377 sagt denselben Satz mit derselben
  Begründung: „Vier neue Messungen, kein Format gehoben, keine Fähigkeit
  dazu (MF-1077)."
- **Kanal:** das Paket selbst ist **MIT** (© 2026 EMUUAC) und nach eigener
  Aussage ein unabhängiger Nachbau aus beobachtetem ARM64-Verhalten, kein
  DTC-Quelltext (`README.md`, `SOURCE_MAP.md`, `RECONSTRUCTION_STATUS.md`).
  Für UFT gilt heute: **Oracle/Vergleich** — ausgeführt, nichts
  übernommen; `tests/CMakeLists.txt` sagt wörtlich, die Dateien werden
  nicht angefasst, „wer sie ändert, macht aus einem Beleg eine Ableitung".
- **Einfrier-Regel:** **ja** — jede der drei Verbesserungs-Routen berührt
  den Format-/Decoder-Layer. Rotbeweis zuerst, benannte Referenz im
  Header.
- **OPEN_ITEMS:** **`P3-377`** (der Befund steht dort, nicht hier) ·
  fachlich berührt `P3-356`, `P3-374`.
- **Fertig heißt:** der Eigentümer hat zwischen den drei in `P3-377`
  bezifferten Wegen entschieden — **(a)** die vier ungeprüften Module
  reparieren (verboten ohne seine Entscheidung, weil es den Beleg in eine
  Ableitung verwandelt), **(b)** die Fähigkeiten in UFTs **eigenem** Code
  bauen und dort den GUI-Schalter setzen, **(c)** lassen und den Bestand
  als Beleg führen — und der gewählte Weg ist mit Commit belegt
  abgetragen.
- **Aufwand:** **nicht schätzbar**, weil er ganz an der Wegwahl hängt.
  Gemessen ist bisher nur eine Zahl: die `track.c`-Korrektur unter Weg (a)
  wäre ein **Einzeiler** (`b+i+3,7` → `b+i,10`, P3-377).
- **Nachtrag 2026-09-20, Eigentümer, wörtlich:** „bei gelegenheit A-034
  weiterverfolgen bis fertig und vertratet und im tool nutzbar und
  verbessert ist !!"
  **Das ist die Entscheidung, auf die „Fertig heißt" gewartet hat** — und
  sie schließt Weg (c) aus: der Bestand soll nicht Bestand bleiben.
  **Zwei Dinge sind damit aber NICHT entschieden, und sie müssen es vor
  der ersten Zeile sein:**
  · **(a) gegen (b).** „Verbessert" kann heißen, die vier ungeprüften
    Module zu reparieren — das ist Weg (a), und er ist nach der eigenen
    Kanal-Zeile oben **gesperrt**: die Dateien sind heute ein *Beleg*
    (Oracle/Vergleich, ausgeführt, nichts übernommen); wer sie ändert,
    macht daraus eine **Ableitung**, und der Satz steht wörtlich in
    `tests/CMakeLists.txt`. Weg (b) — die Fähigkeiten in UFTs eigenem
    Code bauen und dort verdrahten — hat diese Folge nicht.
  · **„im Tool nutzbar"** heißt Produktivpfad plus Bedienelement; heute
    sind es 0 Produktivaufrufer und ein Opt-in-Bauschalter
    (`CONFIG+=uft_dtc_components`). Der Schalter allein macht nichts
    nutzbar (Klasse MF-635: eine Fähigkeit ohne Tür).
  Beim Aufgreifen wird zuerst die (a)/(b)-Frage vorgelegt, nicht
  entschieden.
- **Stand:** aufgenommen, nicht begonnen — „bei Gelegenheit", also nach
  dem laufenden Posten. Laufender Posten bleibt `A-032` (dort MF-1284,
  TD0: Schritt 1a, das Plugin wird der eine richtige TD0-Leser).
- **Beleg:** —

---

## Erledigt

*(mit Beleg: Commit-Hash und MF-Nummer)*

### A-005 · Zulieferung `UFT_Floppy_Reference_AARD_Paket.zip` begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist offen und
  wäre ein eigener Posten) · **Aufgenommen:** 2026-09-16 · hochgezogen mit
  „weiter zieht A-005 hoch". Der Posten bleibt an dieser Stelle stehen, bis
  der nächste `weiter`-Griff ihn nach `Erledigt` verschiebt — verschoben wird
  er dann als Ganzes, nicht kopiert.
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen / erstelle mir code Beispiele was besser gemacht werden
  kann / plan verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier → und das ist hier keine Absage.** Der
  Auftrag ist eine **Beurteilung**, kein Einbau; MF-640 regelt, was in die
  Arbeitsliste geplant wird, nicht was der Eigentümer bestellt. Welche der
  vier ein Einbau bewegen würde, ist Teil der Antwort und wird gemessen,
  nicht behauptet. Vorwegnahme ausdrücklich nicht: die Wikipedia-Tafel ist
  eine **Sekundärquelle**, und eine Sekundärquelle hebt nach dem Maßstab
  von `docs/ORACLES.md` keine Tier-Stufe.
- **Kanal:** zweiteilig, und das muss es bleiben. **Daten** —
  `floppy_reference_wikipedia.tsv` (113 logische + 24 physische Sätze),
  geerntet aus „List of floppy disk formats", **CC BY-SA 4.0**, also
  *copyleft* und mit derselben Frage wie die OmniFlop-239-Ernte
  (EU-Datenbankherstellerrecht, §§ 87a ff. UrhG). **Code** — `LICENSE` des
  Pakets ist GPL-2.0-or-later, © 2024–2026 Axel Kramer, also **eigener**
  Code und keine Fremdübernahme. Für die Begutachtung selbst: *gelesen,
  nicht eingebaut*.
- **Einfrier-Regel:** für die **Begutachtung** nein (kein Code). Für einen
  **Einbau** ja, mittelbar: `src/formats/reference/` liegt im Format-Layer
  und der Abgleicher rangiert Format-Kandidaten — also Rotbeweis zuerst,
  benannte Referenz im Header, jede Zahl gemessen.
- **OPEN_ITEMS:** keiner. Gemessen 2026-09-16:
  `grep -n 'OmniFlop\|Wikipedia\|CC BY-SA' docs/OPEN_ITEMS.md` findet keine
  Zeile zu dieser Ernte. Fachlich berührt, aber **nicht** erledigt:
  `LIZ-1` (Attributions-Zensus) und `P3-435` (Spurbilanz ohne Quelle →
  kein FDC-Profil, S1).
- **Bei der Aufnahme gemessen — eine Zusage des Pakets trägt nicht:**
  `INTEGRATION.md` sagt „The current UFT workspace is already wired" und
  „The repository copy already contains this wiring in
  `tests/CMakeLists.txt`". `git ls-files | grep -iE 'aard|floppy_reference'`
  ist **leer** — kein Byte des Pakets liegt im Baum, weder Quelle noch Test
  noch Daten. Das ist Teil des Prüfauftrags, nicht seine Antwort.
- **Fertig heißt:** ein Gutachten als Dokument (`tools/uft-scout/out/` oder
  `docs/`), das **(a)** jede der fünf Fragen mit einer Messung gegen den
  Baum beantwortet (`git grep` je Bezeichner, keine Aufzählung — MF-930),
  **(b)** jeden Code-Vorschlag an `Datei:Zeile` der Stelle festmacht, die er
  ersetzt, mit der Kennzahl nach MF-640, **(c)** die Lizenzfrage der Ernte
  als benannte **Eigentümer-Entscheidung** stehen lässt statt sie zu
  empfehlen, und **(d)** kein Byte des Pakets nach `src/`, `include/`,
  `tests/` oder `data/` schreibt. Der Einbau ist ein **eigener** Posten mit
  eigenem Rotbeweis.
- **Aufwand:** Begutachtung eine Sitzung. Einbau **nicht schätzbar**, weil
  er an der Lizenzentscheidung hängt.
- **Stand:** **Begutachtung fertig.** Gutachten liegt als
  `tools/uft-scout/out/a005_floppy_reference_aard.gutachten.md` (571 Zeilen).
  Alle fünf Bedingungen der Zeile `Fertig heißt` sind erfüllt: fünf Fragen je
  mit Messung, vier Code-Vorschläge je an `Datei:Zeile` mit Kennzahl, die
  Lizenzfrage steht als Frage, und `git status --short` zeigt **kein** Byte
  des Pakets unter `src/`, `include/`, `tests/`, `data/`.
  **Kernbefunde:** der Abgleicher sagt bei Gleichstand nicht ab — für eine
  PC-1,44-M-Diskette drei Sätze mit score 100, 7 verglichen, 0 abweichend,
  und keiner ist der PC-Satz; „unbekannt" und „widerspricht" sind dasselbe
  (3 von 3 gemessen); von acht angeblich fehlenden Einstellungen hat der Baum
  **fünf**; die physische Achse (`coercivity`/`tracks_per_inch`/
  `bits_per_inch`) ist mit **je 0 Treffern** die einzige echte Lücke; die
  AARD-Flagge feuert auf das Wort **AARDVARK** (confidence 45), während ihre
  Zufallsrate gemessen in Ordnung ist (6 gegen 5,60 erwartet, 0 von 64
  Flaggen). **Der Einbau ist NICHT gemacht** und braucht die
  Lizenzentscheidung — das ist ein eigener Posten, wenn der Eigentümer ihn
  will.
- **UNGESTOPPT 2026-09-16** — wörtlich „mach A-005 ungestopt". Damit ist die
  Lizenzsperre **S3** für die CC-BY-SA-4.0-Ernte aufgehoben, und der Einbau
  ist in Arbeit genommen. **Drei Dinge hebt die Entscheidung nicht auf, und
  sie bleiben Bedingungen des Einbaus:** (a) eine Sekundärquelle hebt nach
  `docs/ORACLES.md` **keine Tier-Stufe** — der Katalog kommt als Referenz
  herein, nicht als Beleg; (b) die Namensnennungs- und Weitergabepflichten
  von CC BY-SA 4.0 bleiben (die Quell-URL steht in
  `UFT_FLOPPY_REFERENCE_SOURCE_URL` und in beiden TSV); (c) **der gemessene
  Defekt des Abgleichers ist zu BEHEBEN, nicht mitzuliefern** — er meldet
  für eine PC-1,44-M-Diskette drei Sieger mit score 100 und zählt
  „unbekannt" als „widerspricht" (`P3-445`). Ihn einzubauen wie er ist,
  wäre eine Falschaussage im Erkennungspfad und verstößt gegen die
  EINFRIER-REGEL. Der Weg steht als Code im Gutachten §8.1/§8.2.
- **Beleg:** `2aa7bda6` (MF-1184) (nachgetragen 2026-09-26 beim Aufraeumen: der Commit, der das Gutachten in den Baum gebracht hat, gemessen mit `git log --diff-filter=A`) · Gutachten `tools/uft-scout/out/a005_floppy_reference_aard.gutachten.md`,
  **MF-1184 = `2aa7bda6`** (2 Dateien, 1012 Zeilen, alle Vorhaken grün).
  Nachgetragen wie angekündigt: die Zeile lag im MF-1184-Commit selbst und
  konnte ihren eigenen Hash nicht kennen. Befunde als `P3-442`, `P3-444`,
  `P3-445`, `P3-446` eingetragen (MF-1188).

### A-006 · Zulieferung `hacking floppy disk.zip` (Kopierschutz/Umdrehungen) begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist offen) ·
  **Aufgenommen:** 2026-09-16 · hochgezogen mit „weiter zieht A-006 hoch"
- **Wortlaut:** „finde alles und alles raussuchen was ich übersehen habe /
  - wo können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen / erstelle mir code Beispiele
  was besser gemacht werden kann / plan verstanden, was kannst du besser
  machen ??"
- **Kennzahl:** **keine der vier für die Begutachtung.** Ein Einbau würde
  keine der vier bewegen — Schutzerkennung ist keine Tier-Stufe, kein
  Wandlungspfad, kein Leck, kein Bench-Alter. **Das ist genau der Fall, für
  den MF-640 die fünfte Zahl offengelassen hat**, und der Punkt gehört bei
  der Begutachtung benannt, nicht umgangen. Die Zulieferung nennt sich
  selbst „T2-Niveau, nicht T1b" — sie weiß das.
- **Kanal:** **Spec.** Die Belegkette nennt DrCoolZic Rev 1.2 („Copyleft",
  laut Zulieferung zitieren und Werte nehmen, nicht Zeilen), Chris Evans
  2020 und Atari-Forum-Faden 21952. Der **Code** des Pakets (`uft_revolution
  .c/.h`, `uft_protection_scan.c/.h`, `test_protect.c`) trägt in der
  Zulieferung **keine Lizenzzeile** — gemessen: `grep` nach `SPDX`, `GPL`,
  `MIT`, `Lizenz`, `License` findet im Analysedokument nur die
  DrCoolZic-Zeile, und im Paket liegt **keine `LICENSE`-Datei** (anders als
  bei A-005). Herkunft und Lizenz des Codes sind damit bei der Aufnahme
  **ungeklärt** und vor jedem Einbau zu klären (`LIZ-1`-Klasse).
- **Einfrier-Regel:** **ja.** `src/protection/` und der Flusspfad sind
  Decoder-Layer. Rotbeweis zuerst, benannte Referenz im Header, jede Zahl
  gemessen — und die Zulieferung sagt selbst, dass **kein geschütztes
  Abbild im Korpus** liegt, also ist alles darin synthetisch geprüft.
- **OPEN_ITEMS:** **Treffer, und sie sind der Kern der Prüfung.** `P3-236`
  (Umdrehungsvergleich zur Weak-Bit-Erkennung, ✅ behoben), `P3-237`
  (Weak-Bit-Erkennung für mehrfach umdrehende Formate, ✅ geklärt),
  `P3-238` (HAL reicht Umdrehungsgrenzen durch, ✅ so weit es ohne Gerät
  geht), `P0-2` (Schutzkatalog ohne Aufrufer, ✅ MF-983). Dazu offen:
  `P3-41` (`uft_fuzzy_bits.c`, vier Mängel), `P3-46`/`P3-47` (Index- und
  Ovl16-Randfälle), `P3-222` (Phantom-Modul `uft_flux_analysis.h`).
- **Bei der Aufnahme gemessen — zwei Zahlen der Zulieferung, zwei
  Ergebnisse:** ihre Kopftafel sagt `uft_flux_revolution_t` habe 2 Header
  und **0** `.c`-Dateien; das **trifft zu** (gemessen über `git ls-files`).
  Ihre Aussage, die HAL reiche Umdrehungen nicht durch, steht dagegen neben
  drei ✅-Punkten: `uft_hal_read_flux_ex` hat **2** `.c`-Aufrufer, und
  `uft_multi_rev_fusion.c/.h` liegt im Baum. Die Zulieferung will
  `uft_revolution.c/.h` **neu** anlegen — Dateien dieses Namens gibt es
  nicht, ein Mehrfach-Umdrehungs-Modul aber schon. Ob das eine Lücke oder
  eine **zweite Kopie** derselben Rechnung ist (Klasse MF-1015/MF-1026/
  MF-1177), ist die erste zu klärende Frage, nicht ihre Antwort.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet, **(b)** je Aussage
  der Zulieferung „trifft zu / überzeichnet / veraltet seit MF-NNN"
  vergibt — mit `git ls-files`-Messung je Bezeichner, nicht per
  Aufzählung (MF-930), **(c)** die Lizenz- und Herkunftsfrage des
  mitgelieferten Codes als offene Eigentümer-Entscheidung stehen lässt,
  **(d)** für jeden Code-Vorschlag die Stelle nennt, die er ersetzt, samt
  Rotbeweis-Skizze, und **(e)** kein Byte des Pakets nach `src/`,
  `include/` oder `tests/` schreibt. Einbau ist ein eigener Posten.
- **Aufwand:** Begutachtung eine Sitzung (521 Zeilen Analysedokument +
  4 Quelldateien + 1 Test). Einbau **nicht schätzbar** — er hängt an der
  Lizenzfrage und daran, ob ein geschütztes Abbild beschafft werden kann.
- **Stand:** **Begutachtung fertig.** Gutachten liegt als
  `tools/uft-scout/out/a006_kopierschutz_umdrehungen.gutachten.md`
  (399 Zeilen). Alle sechs Bedingungen der Zeile `Fertig heißt` erfüllt.
  **Kernbefunde:** der Code ist gut — Bau mit `-Werror -pedantic` 0
  Warnungen, 7 Tests grün **mit eigenen Rot-Proben**, und der Klassierer
  sagt `UNDECIDED` statt zu raten (an 20 000 Bits gemessen: weak 0, fuzzy 0
  bei 50,17 % Flackern). Die **Lagebeschreibung** ist zur Hälfte veraltet:
  `P3-238` ✅ MF-951/954 (`uft_hal_read_flux_ex()` trägt die
  Umdrehungsgrenzen, **SCP verdrahtet**), `P3-236` ✅ MF-949, `P3-237` ✅
  MF-950. Drei Behauptungen berichtigt: Zeile **386** statt 355;
  **Greaseweazle behält** die Indexzeiten (`uft_gw_decode_flux_index_times`);
  vier Zählungen um 1-2 daneben. Echte Lücken: `uft_flux_revolution_t` 0
  `.c`-Verwender **plus Phantom-API** (`uft_flux_revolution_alloc`, 1
  Deklaration / 0 Definitionen), und Sektor-im-Sektor sowie verschobene
  Spuren (je 0 Treffer). **`uft_rev_align()` ist im Vertrag genannt und
  nicht geliefert** — MF-950 hat gemessen, dass ihr Fehlen 99,71 %
  falsches Flackern kostet. **Und drei meiner eigenen vier Erwartungen
  fielen**, weil die Feldbeschreibung von `flux_count` („gemittelt") ihrer
  Umsetzung (`(hi-lo)*16`) widerspricht — das ist der Befund, nicht mein
  Testergebnis. **Der Einbau ist NICHT gemacht** (S1 für zwei unbelegte
  Schwellen, Lizenzfrage zu DrCoolZics „Copyleft" offen).
- **Beleg:** `6dbb4ebf` (MF-1185) (nachgetragen 2026-09-26 beim Aufraeumen: der Commit, der das Gutachten in den Baum gebracht hat, gemessen mit `git log --diff-filter=A`) · Gutachten
  `tools/uft-scout/out/a006_kopierschutz_umdrehungen.gutachten.md`,
  **MF-1185 = `6dbb4ebf`** — nachgetragen wie angekündigt.

### A-007 · Zulieferung `DiskImageTool-extrakt.zip` (Herkunft · Schreibnähte · PC-Schutzmuster) begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist offen) ·
  **Aufgenommen:** 2026-09-16 · hochgezogen mit „weiter mit A-007"
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen / erstelle mir code Beispiele was besser gemacht werden
  kann / plan verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier für die Begutachtung.** Herkunftsnachweis,
  Schreibnahterkennung und Schutzmuster sind keine Tier-Stufe, kein
  Wandlungspfad, kein Leck, kein Bench-Alter. Wie bei A-006 ist das der
  Fall, für den MF-640 die fünfte Zahl offengelassen hat („Dateien mit
  ungeklärter Herkunft" — und **hier geht es um die Herkunft eines
  Datenträgers**, nicht einer Quelldatei; ob das dieselbe Zahl ist, gehört
  zur Antwort).
- **Kanal:** **Port zulässig, und das ist gemessen, nicht angenommen.**
  Gegenstand ist `github.com/Digitoxin1/DiskImageTool` (VB.NET,
  **GPL-3.0**). Die Zulieferung begründet die Zulässigkeit damit, UFTs
  verteilbare Kombination sei über `uft_kfstream_air.c`, `uft_ipf_air.c`
  und `uft_stx_air.c` ohnehin GPL-3 — gemessen über `git ls-files` liegen
  **alle drei** im Baum (1 / 2 / 2 Treffer). Die Prämisse hat also einen
  Boden; ob sie trägt, ist eine Lizenz- und keine Messfrage und bleibt
  Eigentümer-Entscheidung (Konfliktordnung 2: Lizenz vor Fähigkeit).
- **Einfrier-Regel:** **ja.** `src/protection/` und der Flusspfad sind
  Decoder-Layer; `src/analysis/` grenzt daran. Rotbeweis zuerst, benannte
  Referenz im Header (`Modules/BootstrapDB.vb` ist benannt), jede Zahl
  gemessen.
- **OPEN_ITEMS:** berührt `P3-271` (die Spleissstelle ist bei UFT **ein**
  Zeitpunkt je Spur, in IPF eine Gap-Größe auf **beiden** Seiten je
  Sektor) und `P3-7` (der Schreibstartpunkt hat keinen Verbraucher, P6,
  MF-769) — beide direkt auf der Schreibnaht-Hälfte. Status dort ist bei
  der Begutachtung zu lesen, nicht hier zu behaupten.
- **Bei der Aufnahme gemessen — der zweite Fund liegt schon im Baum, und
  zwar ungerufen:** `include/uft/analysis/uft_deepread_splice.h` +
  `src/analysis/deepread/uft_deepread_splice.c` + `tests/
  test_flux_splice_pos.c` existieren. Gemessen über `git ls-files` nennen
  **genau zwei** Dateien `deepread_splice`/`uft_splice_`: das Modul selbst
  und der Test — **kein Produktivaufrufer.** Das ist die Write-Splice
  Detection aus `CLAUDE.md` §DeepRead, eines der **7 von 8** Module ohne
  Aufrufer (MF-627/MF-767). Ein neues `uft_splice.c` daneben wäre die
  zweite Kopie derselben Rechnung (MF-1015/MF-1026/MF-1177) statt der
  fehlenden Tür (P3-204). Welche der beiden Lagen es ist, ist die erste zu
  klärende Frage. **Gleiches gilt für den ersten Fund:**
  `src/forensic/uft_provenance.c` hat **6** `.c`-Nenner und
  `uft_fundus_provenance.c` liegt daneben — die Behauptung „für den
  Datenträger selbst führt UFT keine Provenienz" ist damit eine Aussage
  über eine **Unterscheidung**, die gemessen werden muss.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet, **(b)** für jeden
  der drei Funde entscheidet „fehlt / liegt ungerufen da (P3-204) / ist
  eine zweite Kopie" — je mit `git ls-files`-Messung, nicht per
  Aufzählung (MF-930), **(c)** die GPL-3-Prämisse als benannte
  Eigentümer-Entscheidung stehen lässt und dabei die IPF-Quarantäne
  ausdrücklich prüft, **(d)** die fehlende Bootstrap-Datenbank benennt —
  die Zulieferung sagt selbst „die Datenbank ist nicht im Baum ... wer sie
  nicht hat, hat 379 leere Einträge", also dieselbe Datenbank-/Ernte-Frage
  wie A-005 —, und **(e)** kein Byte des Pakets nach `src/`, `include/`
  oder `tests/` schreibt. Einbau ist ein eigener Posten.
- **Aufwand:** Begutachtung eine Sitzung (542 Zeilen Analysedokument +
  6 Quelldateien + 1 Test + 1 Wandlerskript). Einbau **nicht schätzbar** —
  er hängt an der Lizenzentscheidung und an der Beschaffung der Datenbank.
- **Stand:** **Begutachtung fertig.** Gutachten liegt als
  `tools/uft-scout/out/a007_diskimagetool_herkunft.gutachten.md`.
  Alle fünf Bedingungen der Zeile `Fertig heißt` erfüllt.
  **Die drei Funde, je mit Urteil:** Bootstrap-Herkunft **fehlt für den
  Datenträger** (`oem_name` 9 `.c`, `bootstrap` 0 `.c`; `uft_provenance`
  6 `.c` führt Dateien, nicht Träger) · Schreibnaht **fehlt für
  IBM-MFM, aber das Muster hat drei Vorgänger** — `uff_detect_splices()`
  0 Aufrufer, `uft_deepread_detect_splice()` 0 Aufrufer, und
  `G64_DIAG_SPLICE_DETECTED` ist **ein Etikett, das niemand setzt**; die
  Zulieferung nennt zwei Stellen und übersieht die dritte · PC-Schutz aus
  Fehlsektormustern **fehlt** (`bad_sector` 25 `.c` baumweit, 1 in
  `src/protection/`, 0 Musterschluss). **Selbstzusagen tragen alle vier**,
  Bau `-Werror -pedantic` 0 Warnungen, 6 Tests grün mit drei Rot-Proben.
  **Der Nebenfund reicht über den Posten hinaus:** die Lizenzprämisse ist
  belegt — alle drei AIR-Dateien tragen `GPL-3.0-only`, und MF-698 bindet
  „die VERTEILBARE Fassung des Gesamtwerks an GPL-3". Damit messen
  `P3-385` und mein eigener `A-014`-Eintrag gegen **GPL-2**, während
  Apache-2.0 mit **GPL-3** verträglich ist — der Port-Kanal für `A-013`
  und `A-014` ist möglicherweise offen. **Eigentümer-Entscheidung, nicht
  meine.** Fünf Vorschläge stehen in §13 des Gutachtens.
- **Nachtrag MF-1187 („mach weiter mit A-007"):** zwei der vier
  Nicht-geprüft-Punkte aus §12 sind **geprüft**, und beide Ergebnisse
  betreffen mich selbst. **(1) `Modules/BootstrapDB.vb` gelesen** — sie liegt
  geklont im Baum (213 Zeilen). Die Belegkette **trägt wörtlich**: CRC32 über
  den Bootstrap-Code ist der Schlüssel (`Dictionary(Of UInteger, …)`), der
  OEM-Name der Prüfwert, und der Win9x-Rückfall steht im Original. **Neu
  gefunden:** `FindXDFMatch()` nullt bei Bootstrap-Länge `&H1AD` = 429 Byte
  die Bytes `0xE7`–`0xEE` (8 Byte) und rechnet die CRC neu — die Zulieferung
  hat das **nicht** (0 Treffer), eine echte XDF-Diskette verfehlt damit ihren
  Eintrag. Trifft MF-1087/ARCH-18. **Und ein Vorwurf von mir fällt:** der
  `Verified`-Zustand **ist** übernommen (490 OEM-Namen, 223 verifiziert).
  **(2) Die `0x4E`-Kandidatenzahl ist geprüft und ZURÜCKGENOMMEN.** Die
  Fundstellen sind überwiegend Kennungen („RAIN", „BOUN"), 68000-Opcodes
  (`0x4EFx`, `0x4E75`), ein Versatz `h[0x4E]`, eine Tafelzeile und
  Greaseweazle-Befehlsbytes — kein Füllbyte. **Und meine Nachzählung war
  selbst falsch:** 43 von 36, weil die zweite Messung still auf `grep -li`
  schaltete (36 + 7 = 43). Es steht **keine dritte Zahl** an ihrer Stelle;
  Vorschlag V3 ist zurückgenommen, V6 (XDF) ist neu.
- **Beleg:** `87abb0b3` (MF-1186) (nachgetragen 2026-09-26 beim Aufraeumen: der Commit, der das Gutachten in den Baum gebracht hat, gemessen mit `git log --diff-filter=A`) · Gutachten
  `tools/uft-scout/out/a007_diskimagetool_herkunft.gutachten.md`,
  **MF-1186 = `87abb0b3`**; Korrektur und Nachtrag **MF-1187** (Hash folgt).

### A-008 · Zulieferung `UFT_C64PP_Protection_Catalog.zip` begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist ein
  eigener Posten) · MF-1201 = `618ff62b` · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen / erstelle mir code Beispiele was besser gemacht werden
  kann / plan verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier für die Begutachtung.** Schutzerkennung ist
  keine Tier-Stufe (wie A-006). **Ein Teil bewegt aber etwas Messbares, und
  das ist ungewöhnlich:** die Zulieferung stuft die RapidLok-Heuristik zu
  einer neutralen Strukturmeldung herab. Damit trägt sie auf der Seite der
  **Ehrlichkeit** ab, was P3-38/P3-39 als Befund führen — das ist Rücknahme
  einer Falschaussage, nicht Zuwachs, und genau die Richtung von MF-1077.
- **Kanal:** **Spec** für den Inhalt (die Seiten des C64 Preservation
  Project, Abschnitt 9 „Quellen" des Auditberichts), **MIT** für den
  mitgelieferten Kern: `LICENSE_C64PP_MODULE.txt` ist MIT © 2026 „UFT
  Project contributors", `LICENSE_UFT_PROJECT.txt` liegt daneben — MIT in
  ein GPL-2-or-later-Projekt ist unproblematisch (permissiv in Copyleft).
  **Vorsicht ist trotzdem geboten:** C64-Schutzarbeit hat in diesem Baum
  die nibtools-Vorgeschichte („vier Dateien und Tage gekostet"), also
  gehört die Herkunft je Katalogeintrag geprüft, nicht je Paket.
- **Einfrier-Regel:** **ja.** `src/protection/` ist Decoder-Layer; dazu
  kommt eine GUI-Änderung. Rotbeweis zuerst — und die Zulieferung sagt
  selbst, ihre Tests seien **synthetisch** („synthetische Positiv-/Negativ-
  verträge").
- **OPEN_ITEMS:** **viele Treffer, und sie sind der Maßstab.** `P3-38` (die
  C64-Schutzerkennung rät Markennamen, das Unterscheidungsmerkmal fehlt
  ganz), `P3-39` (alle drei Markennamen an der Quelle widerlegt), `P3-144`
  (der einzige erreichbare Schutzerkenner bestand aus drei Heuristiken),
  `P3-79` (Artefaktsignaturen identifizieren Codefamilien, nicht Produkte —
  Korpus von 17 C64-Kopierprogrammen), `P3-187` (C64-Signaturkonstanten
  ohne benannte Quelle). Dazu auf der Behälterseite `P3-36`, `P3-147`,
  `P3-194`, `P3-195`.
- **Bei der Aufnahme gemessen — die schwerste Feststellung dieser vier
  Zulieferungen:** das Paket liefert `UnifiedFloppyTool.pro` und
  `tests/CMakeLists.txt` mit und nennt sie in `README_EINBAU.md`
  „**aktuelle Verdrahtung**". Gemessen sind sie **kleiner als die des
  Baums**: `tests/CMakeLists.txt` **341 230** gegen **361 386** Byte
  (−20 156), `UnifiedFloppyTool.pro` **81 628** gegen **83 293** (−1 665).
  Sie stammen also aus einem **älteren** Stand; sie zu kopieren wäre eine
  **stille Rücknahme** fremder Verdrahtung — die teuerste Fehlerklasse
  dieses Baums. Der Einbau darf diese zwei Dateien **nicht** übernehmen,
  sondern nur die eigenen Zeilen daraus. Zweitens:
  `src/gui/ProtectionAnalysisWidget.cpp/.h` **liegen schon im Baum** — die
  mitgelieferten 35 KB sind eine geänderte Kopie, kein neues Modul.
  Drittens sagt das Paket selbst, der **Qt-Gesamtbau sei nicht gelaufen**.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet, **(b)** je
  Katalogeintrag „belegt / Kandidat / widerlegt" vergibt und dabei gegen
  P3-38/P3-39/P3-79 hält statt gegen die Zulieferung, **(c)** für die
  GUI-Hälfte einen **Zeilendiff** gegen die vorhandene
  `ProtectionAnalysisWidget.cpp` nennt statt eine Dateiübernahme,
  **(d)** ausdrücklich festhält, dass `.pro` und `tests/CMakeLists.txt` des
  Pakets **verworfen** werden, mit den gemessenen Bytezahlen als Grund,
  und **(e)** kein Byte des Pakets nach `src/`, `include/` oder `tests/`
  schreibt. Einbau ist ein eigener Posten.
- **Aufwand:** Begutachtung eine Sitzung (224 Zeilen Auditbericht + 20
  Katalogeinträge + 1 Test + 1 Werkzeug + GUI-Diff). Einbau **nicht
  schätzbar** — die GUI-Hälfte ist ein Diff gegen lebenden Code und der
  Qt-Bau ist unerprobt.
- **Stand:** **Messungen liegen, das Gutachten ist noch nicht geschrieben.**
  Was gemessen ist (je Aussage mit Fundstelle):
  · **Die Kernleistung der Zulieferung liegt im Baum bereits vor.** Die
    Aufnahme-Zeile oben nennt als ihren einen kennzahlwirksamen Teil, die
    RapidLok-Heuristik zu einer neutralen Strukturmeldung herabzustufen.
    Gemessen hat **MF-402** (`22c35a37`) genau das getan, und gründlicher:
    `ufm_cbm_check_vmax()` als tautologisch entfernt,
    `ufm_cbm_check_rapidlok()` in `ufm_cbm_has_half_track_beyond_35()`
    umbenannt, die Klassifikation auf Strukturnamen umgestellt, jeweils mit
    der Begründung **zitiert an Ort und Stelle**
    (`src/protection/ufm_c64_scheme_detect.c:62-78`, `:186-200`;
    `include/uft/protection/ufm_cbm_protection_methods.h:15-27`).
  · **Der produktive Pfad ist ein anderer als der, den die Aufnahme
    vermutet hat — und er ist erreichbar.** Kette gemessen:
    `src/mainwindow.cpp:107` (`new StatusTab()`) → `src/statustab.cpp:458`
    (`new ProtectionAnalysisWidget(dlg)`) →
    `src/gui/ProtectionAnalysisWidget.cpp:215` (`ufm_c64_prot_analyze`) →
    `src/protection/ufm_c64_scheme_detect.c:124`. Ein Dialog, den ein
    Bediener anklickt.
  · **`g64_detect_protection()` ist NICHT erreichbar**, und beide Türen
    sind gemessen tot: `uft_advanced_open()` hat als einzigen Aufrufer
    `tests/test_advanced_guete_ohne_messung.c:274`, und
    `uft_advanced_detect_protection()` hat **nur Prototyp und Definition,
    null Aufrufer**. Deckt sich mit `P3-147`.
  · **BERICHTIGT — hier stand ein Fehler von mir, und er bleibt zitiert
    stehen.** Der Satz lautete: „Zwei Aussagen des Auditberichts tragen
    gegen DIESEN Baum nicht: er führt `ufm_c64_metrics_from_gcr()` als
    ‚produktiv aus ProtectionAnalysisWidget erreichbar / gute Grundlage' —
    der Bezeichner kommt im ganzen Baum **nicht vor** (`git grep`, rc 0,
    0 Treffer)." **Das ist falsch.** Richtig gemessen ohne Pfadliste:
    **43 Treffer in 15 Dateien**, darunter `src/protection/ufm_c64_metrics.c`,
    `include/uft/protection/ufm_c64_metrics.h` und **vier** Stellen in
    `src/gui/ProtectionAnalysisWidget.cpp`. Der Auditbericht hat an dieser
    Stelle **recht**: die Kette G64 → rohe GCR-Spur →
    `ufm_c64_metrics_from_gcr()` → `ufm_c64_prot_analyze()` ist der
    Produktivpfad, so wie er sie beschreibt.
    **Ursache meines Fehlers, benannt statt verschwiegen:** die Messung lief
    als `git grep … -- $(git ls-files) 2>/dev/null`. Die Pfadliste sprengt
    die Argumentlänge, git bricht mit rc≠0 und leerer Ausgabe ab, `2>/dev/null`
    verschluckt das `fatal:`, und ich habe die leere Ausgabe als „0 Treffer"
    gelesen. Das ist die Klasse `grep_exitkode_bricht_die_kette` — **dritter
    Fall an einem Tag**, und der teuerste, weil die Falschaussage in eine
    verfolgte Datei geschrieben wurde. Regel ab sofort: **`rc` je Lauf
    prüfen, nie `2>/dev/null` auf eine Messung, nie `-- $(git ls-files)`.**
    · Die zweite Hälfte des alten Satzes steht weiter: der Bericht nennt
    „mehrere parallele Implementierungen" als unverdrahtet, **ohne zu
    messen, welche** — das ist unverändert richtig und bleibt ein Mangel
    des Berichts.
  · **Die Klasse ist bereits mit einem Tor versehen.**
    `scripts/audit_protection_claims.py` (MF-557, verdrahtet in
    `scripts/check_consistency.py:752`) misst genau diese Lage: 33 Dateien,
    369 `uft_`-Funktionen, **3** von außen gerufen, **353** von keinem Test
    berührt. Sein Kopf warnt wörtlich davor, den Katalog anzuschließen,
    ohne ihn geprüft zu haben. Die 20 Katalogeinträge der Zulieferung
    würden genau dorthin gelegt.
  · **Der eine gemessene Rest ist neu und gehört nicht der Zulieferung:**
    weder `ufm_c64_scheme_detect.c` noch `ufm_c64_metrics.c` kennen
    **Nachbarschaft** oder Kopfbreite (gemessen, 0 Treffer). Damit liefern
    17.0/17.5/18.0 (mit einer 1541 nicht rückschreibbar, weil der
    Schreibkopf eine ganze Spur breit ist) und 1.5/17.5/35.5 (harmlos)
    dieselbe Ausgabe — zwei physikalisch entgegengesetzte Disketten, eine
    Meldung. Das Feld dafür liegt bereit und wird gefüllt:
    `ufm_c64_track_metrics_t.track_x2` (`ufm_c64_metrics.c:142`), also
    Nachbarschaft = `|Δ track_x2| == 1`, **ohne eine neue Konstante**.
    Steht als Kern von `P3-38`; der Einbau ist nach der Zeile
    `Fertig heißt` (e) **ausdrücklich ein eigener Posten** und wurde hier
    bewusst NICHT begonnen.
  · **Verworfen bleibt, was die Aufnahme schon gemessen hat:** die
    mitgelieferten `UnifiedFloppyTool.pro` (81 628 gegen 83 293 Byte) und
    `tests/CMakeLists.txt` (341 230 gegen 361 386) sind **älter** als die
    des Baums; sie zu übernehmen wäre eine stille Rücknahme fremder
    Verdrahtung. Dazu `tools/uft-c64pp-catalog.c` — ein CLI, das gegen die
    GUI-only-Regel des Projekts steht.
  **Das Gutachten ist geschrieben:**
  `tools/uft-scout/out/a008_c64pp_protection_catalog.gutachten.md`, mit
  (a)…(e) der Zeile `Fertig heißt`. Die drei Kernzahlen daraus:
  · **(b) 0 von 20** Katalogeinträgen sind mit dem belegbar, was
    `ufm_c64_track_metrics_t` trägt. **15 von 20** verlangen mindestens
    eines von zwölf Merkmalen, die die Struktur gar nicht hat
    (`FAT_TRACK`, `TRACK_ALIGNMENT`, `CUSTOM_HEADER`, `SLIDING_BITS`,
    `BYTE_COUNT`, `RPM_SENSITIVE`, `SECTOR_PARITY`, `LOADER_TIMING`,
    `SYNC_POSITION`, `MIXED_FORMAT`, `GAP_SIGNATURE`, `EXACT_SIGNATURE`);
    die übrigen fünf verlangen Bytefolgen, Positionen innerhalb der Spur
    oder Mehrfachlesungs-Vergleiche. Widerlegt ist keiner — wer nichts
    messen kann, kann auch nichts widerlegen. **Der Katalog ist damit eine
    Beschaffungsliste, keine Erkennung.**
  · **(c) Zeilendiff GUI:** 712 gegen 885 Zeilen, **17 Blöcke, +185/−22**.
    Unter den 22 entfernten steht ein **gemessener** Kommentar über eine
    behobene Index-Konvention (`g64_get_track()` Index 2 = Spur 1.0 gegen
    `ufm_c64_metrics_from_gcr()` Index 0 = Spur 1.0; ungerechnet trifft es
    genau die drei Zonengrenzen 17→18, 24→25, 30→31 und erzeugte drei
    falsche „long track"-Treffer auf **beiden** sauberen Referenz-
    disketten). Eine Dateiübernahme löschte die Begründung mit.
  · **(d)** `.pro` und `tests/CMakeLists.txt` verworfen, Bytezahlen im
    Gutachten §4; dazu `tools/uft-c64pp-catalog.c` als CLI gegen die
    GUI-only-Regel.
  · **Das Neue, das der Baum wirklich nicht hat:** der Beweisgrad hängt bei
    der Zulieferung von der **Aufnahmequelle** ab
    (`preserves_flux_timing`). Gemessen `rc 1, 0 Treffer` über
    `src/protection/` und `include/uft/protection/` — das ist die
    Tier-Idee, angewandt auf Schutzbefunde.
- **Beleg:** `618ff62b` (MF-1201) (nachgetragen 2026-09-26 beim Aufraeumen: der Commit, der das Gutachten in den Baum gebracht hat, gemessen mit `git log --diff-filter=A`) · Gutachten
  `tools/uft-scout/out/a008_c64pp_protection_catalog.gutachten.md`;
  Commit folgt (der Baum ist durch den unversionierten Block einer zweiten
  Sitzung vorübergehend nicht committierbar, siehe A-017 Nachtrag).

### A-009 · Zulieferung `UFT_Atari_ST_Cartridge_Detection.zip` begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist ein
  eigener Posten) · MF-1201 = `618ff62b` · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen / erstelle mir code Beispiele was besser gemacht werden
  kann / plan verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier.** Ein Cartridge-ROM ist keine Diskette,
  also keine Tier-Stufe, kein Wandlungspfad. **Und das ist hier die Pointe,
  nicht der Einwand:** die Zulieferung baut eine **Absage** — ein erkanntes
  Cartridge bleibt absichtlich `isValid == false`, gekennzeichnet über
  `DiskImageInfo::isNonDiskImage`, damit es „nicht als Diskettenabbild
  fehlklassifiziert oder an einen Disketten-Decoder weitergereicht wird".
  Das ist genau die Bauform von `UFT_CAPS_OS_VOLUME` aus MF-1176 („der
  Eintrag ist eine Absage, keine Zusage"), und Absagen bewegen in diesem
  Baum keine Kennzahl — sie verhindern Falschaussagen.
- **Kanal:** **Nachbau, selbsterklärt — und die Quelle ist nicht
  benannt.** `docs/ATARI_ST_CARTRIDGE_DETECTION.md` sagt: „Die
  Datenstruktur wurde anhand der oeffentlich beschriebenen Atari-ST-
  Cartridge-Header neu implementiert. Es wurde kein Assembler-, Loader-
  oder Entpacker-Code der Referenzseite uebernommen." Welche Seite, welche
  Fassung, welche Stelle — steht nicht da. Nach MF-636 ist eine
  Attribution eine **rechtliche Aussage**, und „nach fremder Doku
  eigenständig implementiert" muss sagen, nach welcher. Dazu gemessen:
  **keine `SPDX`-Zeile und keine Lizenzangabe** in `.c` oder `.h`
  (0 Treffer für `SPDX`, `Licen`) — dieselbe Lücke wie bei A-006.
- **Einfrier-Regel:** **ja, aber nicht das Moratorium.** Gemessen: die
  Datei registriert **kein** Format-Plugin — 0 Treffer für
  `uft_format_plugin_t`, `uft_register_format_plugin`, `DSK_PLUGIN`,
  `.probe`, `uft_probe_konfidenz`. Das Moratorium für neue Plugins greift
  also nicht; die Regel „kein neuer **ungeprüfter** Code im
  Format-/Decoder-Layer" greift sehr wohl, weil die Datei in
  `src/formats/atari/` liegt. Rotbeweis zuerst, Referenz im Header.
- **OPEN_ITEMS:** kein Treffer zu „Cartridge" in der Befundliste. Fachlich
  benachbart und **gegenläufig**: der SCOUT-17b-Block (Zeile 1456 ff.)
  führt `CRT`, `EDD` und `FDX` als „Code liegt im Baum, gebaut, aber nie
  registriert und über `uft_disk_open()` unerreichbar" — die Lage
  MF-446/447 und P3-204. Diese Zulieferung wählt ausdrücklich den anderen
  Weg (kein Plugin, sondern eine Absage im Prüfpfad), und ob das der
  richtige ist, gehört in das Gutachten. Dazu `P3-349` (Formate, die ein
  Werkzeug im Baum schreibt und UFT nicht liest).
- **Bei der Aufnahme gemessen — zweites Mal dasselbe Muster, damit ist es
  eines:** wie A-008 liefert dieses Paket lebende Baudateien mit, und sie
  sind **kleiner als die des Baums**: `tests/CMakeLists.txt` 341 513 gegen
  361 386 (**−19 873**), `UnifiedFloppyTool.pro` 81 746 gegen 83 293
  (−1 547), und — schwerer — `src/mainwindow.cpp` 25 318 gegen 25 457
  (**−139**, lebender GUI-Code). Drei weitere Dateien sind geänderte
  Kopien (`explorertab.cpp` +364, `disk_image_validator.cpp` +2 963,
  `.h` +59). **Wirklich neu sind nur drei Dateien:**
  `src/formats/atari/uft_atari_st_cartridge.c` (341 Zeilen),
  `include/uft/formats/atari/uft_atari_st_cartridge.h`,
  `tests/test_atari_st_cartridge.c`. Ein Kopieren wäre eine stille
  Rücknahme. **Und eine Gegenprobe zu meinen eigenen Gunsten:** die
  Kennung `0xABCDEF42` kommt in der `.c` **nicht** vor — sie steht als
  `UFT_STCART_MAGIC` im Header, neben `0x00FA0000`. Das ist richtig so
  (D3, Wissen nicht doppelt halten); ich notiere es, weil der erste
  Blick auf die `.c` wie eine fehlende Kennung aussah.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet, **(b)** die
  Grundfrage ausdrücklich entscheidet — gehört eine Nicht-Disketten-Absage
  in `src/formats/` oder in den Prüfpfad —, **(c)** die unbenannte Quelle
  als **Nachforderung** stellt (welches Dokument, welche Fassung, welche
  Stelle) statt sie zu ergänzen, **(d)** für die sechs überlappenden
  Dateien einen **Zeilendiff** nennt statt eine Dateiübernahme, mit den
  gemessenen Bytezahlen als Grund, und **(e)** kein Byte des Pakets nach
  `src/`, `include/` oder `tests/` schreibt. Einbau ist ein eigener Posten.
- **Aufwand:** Begutachtung eine Sitzung (341 Zeilen Modul + Header + Test
  + 1413 Byte Doku + sechs Dateidiffs). Einbau **nicht schätzbar** — er
  hängt an der Quellenangabe und an sechs Diffs gegen lebenden Code.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a009_atari_st_cartridge.gutachten.md`, mit (a)…(e).
  · **(b) Die Grundfrage ist entschieden, und zwar gemessen:** eine
    Nicht-Disketten-Absage gehört in den **Prüfpfad**. `DiskImageValidator`
    wird von **8 produktiven GUI-Dateien** benutzt (`decodejob`,
    `explorertab`, `forensictab`, `mainwindow`, `nibbletab`, `toolstab`,
    `xcopytab`, plus `.pro` und ein Test) — der erreichbarste Baustein
    dieser ganzen Begutachtungsreihe. `src/formats/atari/uft_atari.c`
    dagegen registriert **kein Plugin** (0 Treffer für `DSK_PLUGIN`,
    `uft_format_plugin_t`, `.probe`), und die dort liegenden
    **A78-Cartridge-Strukturen sind unerreichbar** (24+35+7 Nennungen,
    Test vorhanden, keine Tür — Klasse `P3-204`). Die Zulieferung wählt
    den richtigen Ort: ihr `disk_image_validator.cpp` ruft den Erkenner
    wirklich (`info.isNonDiskImage = true`), D2 ist auf ihrer Seite
    erfüllt. Bauform wie `UFT_CAPS_OS_VOLUME` (MF-1176): eine Absage,
    keine Zusage.
  · **(d) DER SCHWERSTE BEFUND — ihre `src/mainwindow.cpp` macht MF-1194
    rückgängig.** Gemessen **+17/−50**; unter den 50 entfernten Zeilen
    steht die Verdrahtung von `ProtectionTab`, `ForensicTab`, `NibbleTab`
    und `XCopyTab` samt dem Messprotokoll, das MF-1194 **vor** dem
    Verdrahten angelegt hat. Eine Dateiübernahme hängte **vier GUI-Reiter
    still wieder ab**. Grund ist banal: das Paket ist älter als der Baum.
    Die übrigen Zahlen: `disk_image_validator.cpp` **+58/−0** (rein
    additiv), `.h` +2/−0, `explorertab.cpp` +11/−2,
    `tests/CMakeLists.txt` 341 513 gegen 361 386 B, `.pro` 81 746 gegen
    83 293 B.
  · **(c) Der Kanal trägt nicht.** Die Doku sagt „anhand der oeffentlich
    beschriebenen Atari-ST-Cartridge-Header neu implementiert" und nennt
    **keine** Seite, Fassung oder Stelle; nach MF-636 ist das zu wenig.
    Dazu gemessen: **keine** der drei neuen Dateien trägt eine SPDX-,
    Lizenz- oder Copyright-Zeile (0 Treffer). Bis zur Nachlieferung ist
    der Kanal **Fundus**, nicht Nachbau.
  · **Das Neue:** `DiskImageInfo` kennt heute `isValid` und
    `isFluxFormat`; ein Objekt, das **erkannt und trotzdem abgelehnt**
    wird, hat kein Feld und fällt mit „unbekannt" in einen Topf — Klasse
    `MF-980`/D6. `isNonDiskImage`, `nonDisk`, `STCART` und `0x00FA0000`
    kommen im ganzen Baum nur in dieser Datei hier vor.
  · **(e)** kein Byte nach `src/`, `include/`, `tests/`.
- **Beleg:** `618ff62b` (MF-1201) (nachgetragen 2026-09-26 beim Aufraeumen: der Commit, der das Gutachten in den Baum gebracht hat, gemessen mit `git log --diff-filter=A`) · Gutachten
  `tools/uft-scout/out/a009_atari_st_cartridge.gutachten.md`; Commit folgt.

### A-010 · Zulieferung `FloImg-extrakt.zip` (Ganzdurchläufe · Mediendiagnosen) auseinandernehmen
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist ein
  eigener Posten) · MF-1202 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „nimm den code komplett auseinander , sehr genau / finde
  alles und alles raussuchen was ich übersehen habe , stimme es mit mein
  aktuellen tool ab / - wo können die formate verbessert werden / - ist es
  auf andere formate übertragbar / - welche einstellungen fehlen noch / -
  was habe wir noch nicht / - brauch es eine HAL-Erweiterungen plan
  verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier für die Begutachtung.** Ganzdurchläufe und
  Mediendiagnosen sind keine Tier-Stufe und kein Wandlungspfad. **Ein
  Einbau könnte allerdings P3-284 abtragen** („`adaptive_passes` war ein
  Schalter ohne Schaltung — und der forensische Bericht meldete ihn als
  ‚yes'"), und das ist Rücknahme einer Falschaussage, also die Richtung von
  MF-1077.
- **Kanal:** **Spec.** Die Belegkette der Zulieferung nennt zwei Quellen:
  **FloImg 1.02** (Petari, 8bitchip.info, Hilfe zu v1.02 vom 2011-08-08) für
  „Ganzdurchläufe bringen mehr als sofortige Wiederholungen", und
  `NFORMAT.DOC` (1992) plus cw2dmk `jv3.h` (GPL-2, nur gelesen) für die
  Spurkapazität — **letzteres ist dieselbe Quelle, an der MF-1166 und
  MF-1183 hängen**. Gemessen: **keine `LICENSE`-Datei** im Paket, aber
  `SPDX` steht in `uft_os_volume.c`. Die Paketlizenz ist damit ungeklärt,
  die Dateilizenz nicht.
- **Einfrier-Regel:** **ja.** `src/hal/` und `src/analysis/` grenzen an den
  Decoder-Layer, und `uft_os_volume.c` ist lebender Code aus MF-1176.
  Rotbeweis zuerst.
- **OPEN_ITEMS:** **viele Treffer, und sie sind der Maßstab.** `P3-284`
  (`adaptive_passes` ohne Schaltung), `P3-113` (`distinct_contents` zählte
  nicht, was sein Name sagt — behoben MF-860, `classify_passes()`),
  `P3-88` (Multi-Read-Voting konnte einen Sektor erfinden — behoben
  MF-845), `P3-291` (leere Sektoren werden gezählt, nicht gewählt),
  `P3-429` (HAL-Merkmalstafel ohne Leser).
- **Bei der Aufnahme gemessen — vier Dinge, zwei davon entscheiden schon:**
  **(1)** Das Paket **erweitert**, statt zu ersetzen:
  `include/uft/hal/uft_os_volume.h` Baum 8 145 / Paket 9 625 (**+1 480**),
  `src/hal/uft_os_volume.c` Baum 10 557 / Paket 13 984 (**+3 427**). Das
  ist das **Gegenteil** des A-008/A-009-Musters und gehört gelobt.
  **(2)** Die Wiederholungsinfrastruktur ist **da**: `retries` in **46**
  `.c` und **23** `.h`, `multiread_execute` 7 `.c`, `distinct_contents`
  3 `.c`, `classify_passes` 2 `.c`, `adaptive_passes` 2 `.c`. Ein
  „`--passes` neben `--retries`" trifft also auf einen bestellten Tisch.
  **(3) Und hier liegt ein harter Konflikt: `--passes` ist ein
  CLI-Schalter, und dieses Projekt hat kein CLI.** Gemessen: 0 Dateien
  passen auf `cli\.c|main_cli|uft_cli`, und `CLAUDE.md` sagt „Dies ist ein
  **GUI-only-Projekt**. Es gibt keinen CLI-Modus mehr." Der Vorschlag
  gehört also an die Oberfläche oder an einen Optionssatz, nicht an eine
  Kommandozeile — das ist bei der Begutachtung zu entscheiden, nicht zu
  übernehmen. **(4)** Die Zulieferung stellt selbst eine Frage, die ich
  beantworten kann: „hat das `FloppyDevice`-Subsystem überhaupt einen
  Verteiler?" Sie zählt 79 Dateien, ich zähle **84** über
  `git ls-files | xargs grep -l 'uft_floppy_device.h'` — und der
  Gedächtnisstand `arch_formate_8bit` sagt, **alle** externen Aufrufer
  seien Tests. Die Antwort lautet damit wahrscheinlich „nein", und sie
  gehört gemessen statt zitiert.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede
  öffentliche Funktion der vier Paketdateien einzeln gegen den Baum hält —
  „gibt es / gibt es anders / gibt es nicht", je mit `git ls-files`-Messung,
  weil der Auftrag ausdrücklich „komplett auseinander, sehr genau" und
  „stimme es mit mein aktuellen tool ab" sagt; **(b)** für `uft_os_volume`
  einen **Zeilendiff** gegen den lebenden Stand aus MF-1176 nennt, nicht
  eine Dateiübernahme; **(c)** die fünf Fragen je mit Messung beantwortet;
  **(d)** den CLI-Konflikt ausdrücklich entscheidet; **(e)** die
  Paketlizenz als offene Frage stehen lässt; und **(f)** kein Byte des
  Pakets nach `src/`, `include/` oder `tests/` schreibt.
- **Aufwand:** Begutachtung eine Sitzung (279 Zeilen Bericht + 4
  Quelldateien + 1 Test + 2 Dateidiffs gegen lebenden Code). Einbau
  **nicht schätzbar** — er hängt am CLI-Konflikt und an P3-284.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a010_floimg_passes_diagnosen.gutachten.md`, (a)…(f).
  **Die beste der vier heute begutachteten Zulieferungen**, und das ist
  gemessen, nicht gefühlt:
  · **(a) Jede der 14 öffentlichen Funktionen einzeln, `rc` je Lauf:**
    **10 gibt es** (`uft_osvol_open/close/read/write/query_geometry/
    lba_to_chs/total_sectors/image_size/status_summary`, `uft_osvol_t` —
    alle aus MF-1176), **4 gibt es nicht** (`uft_mdiag_diagnose/report/
    writable`, `uft_mdiag_result_t`, je rc 1 / 0 Treffer). Eine Abweichung:
    `uft_osvol_image_size` steht im Header und im Test, **nicht** in
    `src/hal/uft_os_volume.c`.
  · **(b) Zeilendiff:** `.h` **+29/−3** (2 Blöcke), `.c` **+120/−44**
    (5 Blöcke). **Die 44 entfernten Zeilen sind eine UMSCHREIBUNG von
    `xfer_range()`, keine Rücknahme** — anders als bei A-008 und A-009
    geht dabei nichts Gemessenes verloren. **ABI richtig gelöst, gemessen:**
    `UFT_OSVOL_SEC_OK_LATE_PASS` ist **angehängt**, die vier vorhandenen
    Zustände behalten ihre Zahl.
  · **Der inhaltliche Kern trifft `P3-284`.** Nachgemessen mit
    Kommentarfilter: `adaptive_passes` hat **eine** Deklaration
    (`uft_multiread_pipeline.h:280`), **eine** Zuweisung
    (`uft_multiread_pipeline.c:93`) und **zwei Kommentare** — **null
    lesende Stellen**. Der Begriff „Ganzdurchlauf" aus dieser Zulieferung
    wäre der erste Ort im Baum, an dem er etwas bedeutet.
  · **(d) Der CLI-Konflikt entfällt** — gemessen trägt von den vier
    Quelldateien nur `test_passes.c` ein `int main`, und das ist ein Test.
  · **(e) Die Lizenz ist NICHT offen** — alle vier Dateien tragen
    `SPDX-License-Identifier: GPL-2.0-or-later`, die Projektlizenz. Offen
    bleibt allein, ob die FloImg-Aussage („Ganzdurchläufe bringen mehr als
    sofortige Wiederholungen") belegt ist; sie ist die Behauptung eines
    Werkzeugautors von 2011 und im Baum **nirgends nachgemessen**.
  · **(f)** kein Byte nach `src/`, `include/`, `tests/`.
- **Beleg:** `8b1fad0a` (MF-1202) (nachgetragen 2026-09-26 beim Aufraeumen: der Commit, der das Gutachten in den Baum gebracht hat, gemessen mit `git log --diff-filter=A`) · Gutachten
  `tools/uft-scout/out/a010_floimg_passes_diagnosen.gutachten.md`.

### A-011 · Zulieferung `Metadaten.zip` (Herkunft · Datierung über die VSN) begutachten
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist ein
  eigener Posten) · MF-1202 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „arbeite alles sehr genau aus / finde alles und alles
  raussuchen was ich übersehen habe / - wo können die formate verbessert
  werden / - ist es auf andere formate übertragbar / - welche einstellungen
  fehlen noch / - was habe wir noch nicht / - brauch es eine
  HAL-Erweiterungen"
- **Kennzahl:** **keine der vier.** Metadaten und Datierung sind keine
  Tier-Stufe und kein Wandlungspfad. **Aber es berührt die fünfte Zahl, die
  MF-640 offengelassen hat** („Dateien mit ungeklärter Herkunft") — und
  zwar von der anderen Seite: hier geht es um die Herkunft des
  **Datenträgers**, nicht der Quelldatei. Dieselbe Verschränkung wie
  `A-007`; welche Zahl das ist, gehört zur Antwort.
- **Kanal:** **Spec, und die Belegkette ist die beste dieser Reihe.** Für
  die VSN-Formel `(Sek<<8|Hundertstel)+(Monat<<8|Tag)`,
  `(Std<<8|Min)+Jahr`: Craig Wilson, *Volume Serial Numbers and Format
  Date/Time Verification*, digital-detective.net, plus Ralf Brown. Dazu ein
  **veröffentlichter Prüfvektor** (19.10.2003 22:33:27.01 → `2514-1DF4`),
  den die Zulieferung nachgerechnet hat — „beide Wörter treffen". Weitere
  Aussagen gegen MAME `imd_dsk.cpp`, a8rawconv `rawdiskscp.cpp:380-424`
  und den **eigenen Baum** (`uft_2img.c:6-21`, `uft_scp_writer.c:292`).
  Gemessen: `SPDX` steht in beiden Quelldateien, **keine `LICENSE`-Datei**
  im Paket — wie bei `A-006`.
- **Einfrier-Regel:** **ja, mittelbar.** `src/analysis/` grenzt an den
  Decoder-Layer, und die VSN-Deutung ist eine Aussage über ein
  Dateisystemfeld. Rotbeweis zuerst — und der Prüfvektor **ist** einer,
  weil er von außen kommt.
- **OPEN_ITEMS:** **`P3-387`** ist der nächste Nachbar: „Zwei forensische
  Fähigkeiten, die der Baum nicht hat, und die seine eigene Mission
  verlangt" — darunter „`uft_format_mark_last_missing()` kennzeichnet im
  SPEICHER, und keine Datei trägt es hinaus — der Befund existiert im Lauf
  und überlebt ihn nicht". Das ist dieselbe Klasse wie „gelesen und
  weggeworfen". Dazu `P3-354` (WOZ-CRC stellt eine Beschädigung fest und
  hat keinen Weg, sie zu melden).
- **Bei der Aufnahme gemessen — vier Dinge:** **(1) Die saubersten
  Zulieferungsform bisher:** alle fünf Dateien sind **neu**, keine
  Überlappung mit dem Baum, **keine** mitgelieferten Baudateien. Kein
  Diff gegen lebenden Code nötig. **(2) Die tragende Behauptung trifft
  präzise zu:** „`metadata_count` in keiner `.c` außer MOOFs eigener
  Struktur" — gemessen ist `metadata_count` in **genau einer** `.c`-Datei
  genannt, `src/formats/apple/uft_moof_parser.c`. **(3) VSN als Begriff
  fehlt im Baum vollständig:** der einzige Treffer auf `VSN` ist
  `resload_VSNPrintF` in `src/whdload/`, also unverwandt. `volume_serial`
  wird dagegen gelesen (3 `.c` / 8 `.h`) und über `fat_format_serial()`
  nur **angezeigt** (`%04X-%04X`) — gelesen, nicht gedeutet. **(4) Die
  Zahl „drei unvereinbare Metadatenmodelle" ist womöglich zu niedrig:**
  gemessen nennen mindestens **acht** Header „metadata", darunter
  `uft_disk.h`, `uft_format_registry.h`, `uft_snapshot.h` und
  `uft_flux_meta.h`. Das gehört nachgezählt, nicht übernommen.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** jede der fünf
  Fragen mit einer Messung gegen den Baum beantwortet; **(b)** den
  veröffentlichten Prüfvektor **selbst nachrechnet** statt die Nachrechnung
  zu zitieren — das ist der einzige Rotbeweis, der hier von außen kommt;
  **(c)** die offene Bytefolge-Frage („die VSN-Bytefolge im Sektor ist aus
  den Quellen nicht zweifelsfrei zu entnehmen") als offen stehen lässt und
  messbar macht, statt sie zu entscheiden; **(d)** die Zahl der
  Metadatenmodelle nachzählt; **(e)** die Paketlizenz als offene Frage
  nennt; und **(f)** kein Byte des Pakets nach `src/`, `include/` oder
  `tests/` schreibt.
- **Aufwand:** Begutachtung eine Sitzung (487 Zeilen Bericht + 4
  Quelldateien + 1 Test, alle neu). Einbau **nicht schätzbar** — er hängt
  an der Bytefolge-Frage und daran, ob ein Träger mit bekanntem
  Formatierungsdatum beschafft werden kann (die Zulieferung nennt das
  ausdrücklich als „nicht verifiziert").
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a011_metadaten_vsn_datierung.gutachten.md`, (a)…(f).
  · **(b) Der Prüfvektor ist SELBST nachgerechnet und trifft.**
    `19.10.2003 22:33:27.01 -> 2514-1DF4`, gerechnet statt im Kopf:
    `sekunde*256+hundertstel` = 6913 = `0x1B01`, `monat*256+tag` = 2579 =
    `0x0A13`, **lo = 9492 = `0x2514`**; `stunde*256+minute` = 5665 =
    `0x1621`, `jahr` = 2003 = `0x07D3`, **hi = 7668 = `0x1DF4`**. Beide
    Wörter und alle vier Zwischenwerte treffen. Das ist der einzige
    Rotbeweis dieser Reihe, der von außen kommt.
  · **(c) Die Bytefolge-Frage bleibt offen — ist aber jetzt ENTSCHEIDBAR.**
    Gemessen liest `src/fs/uft_fat12.c:221` `le32(d + 0x27)` (die
    Zulieferung nennt Zeile 191; die Zeile stimmt, die Nummer nicht). Aus
    `25 14 1D F4` wird als `le32` **`0xF41D1425`**, als zwei BE-Wörter mit
    lo zuerst **`0x1DF42514`**. **Der Prüfstein liegt schon im Baum:**
    `tests/test_win98_fdb.c:279` hält `0x27156C21` aus einem ECHTEN
    Win98-Abbild fest — wer die vier Rohbytes daneben legt, hat die
    Bytefolge gemessen statt zitiert. Zweite Probe: ein Zeitstempel muss
    sich in ein plausibles Datum zurückrechnen lassen.
  · **(d) Zahl der Metadatenmodelle: ZWÖLF.** `volume_serial` ist als
    Strukturfeld in zwölf Headern deklariert (`mfm_detect.h`,
    `uft_fat_bootsector.h` dreimal, `formats/uft_fat12.h`, `uft_fdi.h`,
    `fs/uft_fat12.h`, `fs/uft_fat32.h` zweimal, `uft/uft_fat12.h`,
    `uft_xdf_pxdf.h` zweimal) — **plus eine dreizehnte Stelle mit einem
    ANDEREN Namen**, `src/fs/uft_fat12.c:221` (`v->serial`). Das ist
    `MF-1177` in der Metadatenschicht.
  · **(e) Die Lizenz ist NICHT offen** — alle vier Quelldateien tragen
    `SPDX-License-Identifier: GPL-2.0-or-later`.
  · **Neu im Baum:** `uft_vsn_compute`, `uft_vsn_from_bytes` je rc 1 /
    0 Treffer. **Der Baum speichert die VSN und deutet sie nicht.**
  · **(f)** kein Byte nach `src/`, `include/`, `tests/`.
- **Beleg:** `8b1fad0a` (MF-1202) (nachgetragen 2026-09-26 beim Aufraeumen: der Commit, der das Gutachten in den Baum gebracht hat, gemessen mit `git log --diff-filter=A`) · Gutachten
  `tools/uft-scout/out/a011_metadaten_vsn_datierung.gutachten.md`.
- **Stand:** —
- **Beleg:** —

### A-012 · `yas-sim/xm7-related-tools` auseinandernehmen (FM-7: D77 · T77 · FM-Dateisystem · Boot-ROM)
- **Status:** **erledigt** 2026-09-16 (Begutachtung; der EINBAU ist ein
  eigener Posten) · MF-1203 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/yas-sim/xm7-related-tools.git nimm den
  code komplett auseinander , sehr genau / finde alles und alles raussuchen
  was ich übersehen habe , stimme es mit mein aktuellen tool ab / - wo
  können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen"
- **Kennzahl:** **keine der vier für die Begutachtung** — aber dieser Fund
  zielt auf die Achse, die `P3-389` als **die dünnste des Baums** benennt:
  „Die DEKODIERKETTE ist die duennste Achse des Baums … und der FM-Weg hat
  bis heute keinen ERZEUGER." Ein FM-Erzeuger wäre kein Kennzahlwert,
  sondern das **Werkzeug**, mit dem FM-Formate überhaupt eine Prüfspur
  bekommen (MF-864 musste dafür `fluxtoimd` von fremder Hand nehmen).
- **Kanal:** **Port zulässig — und das ist die Ausnahme in dieser Reihe.**
  Gemessen über `gh api`: `license.spdx_id` = **MIT**, 925 KB, C++,
  7 Sterne, letzter Push **2022-09-21**, Standardzweig `main`. Damit ist
  der stärkste Kanal aus MF-695 offen. **Zwei Vorbehalte gehören dazu:**
  die 16 `.zip`-Dateien enthalten vorgebaute Windows-Binärdateien (Kanal
  *Oracle*, nicht *Port*), und für **Prüfdaten** gilt weiter, was
  `SCOUT-5` festgehalten hat — „das Repo ist MIT, der Disketten-INHALT
  hat eigene Urheber".
- **Einfrier-Regel:** **ja.** Jeder Einbau berührt den Format- oder
  Decoder-Layer. Rotbeweis zuerst, benannte Referenz im Header.
- **OPEN_ITEMS:** **drei Treffer, und der erste ist derselbe Autor.**
  **`P3-389`** entstand auf genau diese Eigentümer-Frage („finde was ich
  uebersehen habe") zu `yas-sim/fdc_bitstream` — Behälter und Dateisystem
  wurden bewertet, die **Dekodierkette fehlte in der Bewertung**, und die
  Messung MF-1121 ergab: kein FM-Erzeuger. **`SCOUT-5`** nennt ein
  D77-Wahrheitspaar beim Upstream (`2019FM77AVDemo-4MHz.raw`, `-8MHz.raw`,
  `2019FM77AVDemo.d77`), **blockiert bis Lizenzklärung**. Dazu `P3-218`
  („kein FM-Encoder, deshalb die Fremdabnahme durch `fluxtoimd`").
- **Bei der Aufnahme gemessen — fünf Dinge:** **(1) Es ist ein echtes
  Quellrepo:** 183 Blobs, davon **70 Quelldateien** (42 `.cpp`, 25 `.h`,
  3 `.c`), dazu `CMakeLists.txt`, 9 ROM-Listings (`.lst`), 7
  Assemblerquellen (`.s`/`.src`) — und 16 `.zip` mit Binärdateien.
  **22 Werkzeuge** auf oberster Ebene, darunter `d77enc_dec`, `d77uty`,
  `fmtools` (mit `fmfslib/cfilesys.cpp`), `t77dec`, `t772wav`, `wav2t77`,
  `fdump`, `BootROM`. **(2) `T77` fehlt im Baum vollständig:** 0 Dateien
  für `t77`/`T77`. Gemessen über zitierte Endungen in `.c`-Dateien führt
  der Baum an Bandformaten nur `"cas"` (2×) und `"tzx"` (1×) — die Methode
  ist eng, aber der T77-Nullwert ist eindeutig. **(3) Kein
  FM-7-Dateisystemeintrag** in `docs/VERIFICATION_TIERS_FS.md`.
  **(4) `d77` steht heute auf T1b** mit `test_d77_gegen_hxcfe`,
  `test_d88_header_variants` und `test_oeffentliche_api_am_korpus`, Quelle
  pc98.org — **`SCOUT-5`s Messung „T3 ohne alles" ist damit veraltet**, die
  Lizenzsperre bleibt. **(5) Und der eigentliche Grund, hinzusehen:** das
  Repo enthält **Erzeuger** — `wav2t77` schreibt Bandaudio, `d77enc`
  schreibt D77 —, und P3-389s Befund lautet, dass dem FM-Weg genau das
  fehlt. Ob einer davon **der** fehlende FM-Erzeuger ist, ist die erste zu
  klärende Frage, nicht ihre Antwort.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** die 70
  Quelldateien nach Werkzeug gruppiert durchgeht und je Werkzeug „für UFT
  brauchbar / Oracle-Kandidat / uninteressant" mit Grund vergibt — der
  Auftrag sagt „komplett auseinander, sehr genau"; **(b)** die Frage von
  P3-389 ausdrücklich beantwortet: liefert dieses Repo einen FM-**Erzeuger**,
  und wenn ja, für welche Kodierung; **(c)** je Fund den Kanal nennt (Port
  bei MIT-Quelle, Oracle bei Binärdatei, Spec bei ROM-Listing) und die
  Lizenzfrage der **Prüfdaten** getrennt davon offen lässt (SCOUT-5);
  **(d)** die fünf Fragen je mit Messung gegen den Baum beantwortet;
  **(e)** `T77` und das FM-Dateisystem als gemessene Lücken belegt statt
  behauptet; und **(f)** kein Byte nach `src/`, `include/` oder `tests/`
  schreibt — geklont wird nach `tools/uft-scout/work/`, dem dafür
  vorgesehenen Ort.
- **Aufwand:** Begutachtung **mehr als eine Sitzung** — 70 Quelldateien in
  22 Werkzeugen sind mehr als jede bisherige Zulieferung dieser Reihe
  (A-010 hatte 4). Genauer nicht schätzbar. Einbau **nicht schätzbar** und
  in jedem Fall ein eigener Posten.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a012_xm7_related_tools.gutachten.md`, (a)…(e).
  Repo flach geklont nach `tools/uft-scout/work/xm7-related-tools`
  (gitignoriert); **70 Quelldateien gemessen — exakt die Zahl der
  Aufnahme**, 20 Werkzeugverzeichnisse (die Aufnahme sagte 22; gemessen
  sind es 20, drei davon ohne Quelltext), 8 020 Zeilen.
  · **(b) Die erste Frage ist mit NEIN beantwortet, und zwar gemessen:
    das Repo liefert KEINEN FM-Erzeuger.** `P3-389` meint die
    FM-**Kodierung**; über alle 70 Quelldateien gibt es dafür **zwei**
    Treffer, und beide sind **Kommentare** zu einem Dichte-Byte im
    D77-Behälter (`fmfslib/cfloppy.h:17`, `d77img.h:21`). `wav2t77` ist
    eine **Audio**-Kette (AGC, Komparator, Filter, Tiefpass) — Band-FSK,
    nicht Diskettenfluss. P3-389 bleibt offen.
  · **(a) Je Werkzeug ein Urteil:** **6 brauchbar oder Oracle-Kandidat**
    (`fmtools` 3460 Z., `wav2t77` 920, `t772wav` 519, `d77uty` 341,
    `t77dec` 332, `d77enc_dec` 322), **11 uninteressant** (2 419 Zeilen
    S-Record-/ROM-/Grafikwerkzeuge ohne Diskettenbezug), **3 ohne
    Quelltext** (`BootROM`, `nosys_ipl`, `subtfr` — Kanal *Spec*).
  · **(c) Kanal: PORT zulässig** — `LICENSE.md` ist **MIT, © 2022
    Yasunori Shimura**; MIT in GPL-2-or-later ist permissiv in Copyleft.
    **Einzige Quelle dieser Reihe, bei der ein Port offensteht.** Gemessen
    trägt allerdings **0** der 70 Quelldateien eine eigene SPDX-Zeile.
  · **(d/e) Zwei gemessene Lücken:** das **FM-7-Dateisystem**
    (`fmtools/fmfslib`) — der Baum kennt FM-7 als Plattform (8 Treffer)
    und D77 als Behälter, „DISK BASIC" trifft aber nur **MSX**; und
    **T77**, im Baum **rc 1 / 0 Treffer**, vollständig abwesend. Ob T77
    fehlen SOLL, ist eine Eigentümer-Frage — `SCOPE_DECISION_NON_FLOPPY.md`
    hat Nicht-Disketten-Inhalte 2026-05-25 gelöscht, während CLAUDE.md drei
    Bandformate als unterstützt führt.
  · **Bemerkenswert:** für das FM-7-Dateisystem wäre die Verifikationskette
    **vollständig führbar** — MIT-Quelle als Referenz, `d77enc_dec` als
    Erzeuger, `d77` bereits auf T1b. Das ist selten.
- **Beleg:** `b1e3a6ab` (MF-1203) (nachgetragen 2026-09-26 beim Aufraeumen: der Commit, der das Gutachten in den Baum gebracht hat, gemessen mit `git log --diff-filter=A`) · Gutachten
  `tools/uft-scout/out/a012_xm7_related_tools.gutachten.md`.
- **Beleg:** —

### A-013 · `thomas-luebker/AmigaDiskKit` auseinandernehmen — **das ist `P3-385(b)`**
- **Status:** **erledigt** 2026-09-16 (Begutachtung; Port bleibt gesperrt,
  Orakel ist delegierbar) · MF-1203 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/thomas-luebker/AmigaDiskKit.git nimm den
  code komplett auseinander , sehr genau / finde alles und alles raussuchen
  was ich übersehen habe , stimme es mit mein aktuellen tool ab / - wo
  können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen"
- **Kennzahl:** **keine der vier für die Begutachtung.** Der Punkt, den es
  abtragen könnte, ist `P3-385` — die „ADFlib-unabhängige Zweitmeinung, die
  `docs/ORACLES.md:650` selbst als ‚Offene Lücke' benennt". Ein
  unabhängiges Orakel bewegt keine der vier direkt, ist aber die
  Voraussetzung für Stufenhebungen im Amiga-Zweig.
- **Kanal:** **`P3-385` hat das Urteil bereits gefällt, und ich habe es
  nachgemessen — es gilt, aber es ist nicht ausführbar.** Der Punkt sagt
  wörtlich: „**Lizenz berichtigt:** Apache-2.0, NICHT MIT — mit GPL-2
  unverträglich, also **kein Port**, nur Orakel." Gemessen bestätigt:
  `license.spdx_id` = **Apache-2.0**. **Der offene Widerspruch:** `swift`,
  `swiftc` und `xcodebuild` sind auf diesem Rechner **alle drei nicht
  vorhanden**, und `docs/ORACLES.md` verlangt „Kein Oracle auf Zusicherung
  — ein Werkzeug, das nicht gebaut und ausgeführt wurde, ist kein
  Eintrag". Der einzige Kanal, den P3-385 offenlässt, ist damit hier
  **verschlossen** — nicht grundsätzlich, aber auf dieser Maschine. Das ist
  dieselbe Lage wie die Tier-3-Hardwarebank (MF-310): delegierbar, nicht
  hausintern. Was bleibt: **Spec** (lesen) und **Daten** (die 26 `.bin`).
  Für Apache-2.0 gibt es dabei **zwei entschiedene Präzedenzfälle** —
  `P3-316` ✅ MF-1008 und `P3-353` ✅, beide vom Eigentümer festgestellt.
- **Einfrier-Regel:** **ja** für jeden Einbau; für das Lesen nicht.
- **OPEN_ITEMS:** **`P3-385`** — dieser Posten IST der Auftrag, ihn
  abzutragen; der Befund bleibt dort und wird hier nicht abgeschrieben.
  Berührt `P3-316`/`P3-353` (Apache-Präzedenz) und die Amiga-FS-Stufen.
- **Bei der Aufnahme gemessen — fünf Dinge:** **(1) Die Zahl von P3-385
  stimmt:** **139 Blobs**, genau wie dort notiert; davon **97 `.swift`**,
  26 `.bin`, 11 `.txt`. Oberste Ebene `Sources`, `Tests`, `Package.swift`
  — eine Swift-Paketstruktur, kein Werkzeugkasten. **(2) Der Umfang ist
  breiter als „Diskette":** die Beschreibung nennt „RDB/MBR layouts,
  FFS/OFS/FFS2, PFS3, FAT32, ADF & flux floppies, LHA" und ausdrücklich
  „No external tools or Python". **(3) Vier dieser Achsen fehlen dem Baum
  gemessen:** `rdb` 1 Datei / `RDB` 0 / `rigid_disk` 0, `pfs3`/`PFS3`
  **0**, `ffs2`/`FFS2` **0**, `lha` 0 / `LHA` 1. **(4) Die Amiga-Seite des
  Baums ist ungleich belegt:** `uft_amigados` steht auf **FS-T2**,
  `uft_amigados_extended`, `uft_bootblock_scanner` und
  `uft_amiga_virus_db` auf FS-T1 — und `uft_fs_amigados_driver` auf
  **FS-T0** mit der Bemerkung „kein Test nennt ein Symbol dieses Lesers".
  **(5) Es ist aktiv:** letzter Push **2026-07-29**, nicht archiviert —
  anders als der Nachbar `amigadx` (letzter Commit 2014-07-18, GPL,
  vendort ADFlib 0.7.10), der laut P3-385 gerade **nicht** unabhängig ist.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** die 97
  Swift-Dateien nach Achse gruppiert durchgeht (RDB/MBR · FFS/OFS/FFS2 ·
  PFS3 · FAT32 · ADF · Fluss · LHA) und je Achse „fehlt im Baum / ist da
  und schwächer / ist da und gleichwertig" mit `git ls-files`-Messung
  vergibt — der Auftrag sagt „komplett auseinander, sehr genau" und
  „stimme es mit mein aktuellen tool ab"; **(b)** die Orakel-Frage
  ausdrücklich entscheidet: ohne Swift-Werkzeugkette ist kein Eintrag nach
  `docs/ORACLES.md` möglich, also ist zu sagen, **was genau delegiert
  werden müsste** (Bau auf welcher Plattform, welche Ausgabe wäre der
  Beleg); **(c)** die 26 `.bin` als möglichen **Daten**-Kanal prüft und
  ihre Herkunft/Lizenz getrennt von der Apache-Frage nennt (die Lage aus
  `SCOUT-5`: Repolizenz ≠ Disketteninhalt); **(d)** die fünf Fragen je mit
  Messung beantwortet; **(e)** `P3-385` mit dem Ergebnis fortschreibt statt
  einen neuen Punkt anzulegen; und **(f)** kein Byte nach `src/`,
  `include/` oder `tests/` schreibt — geklont wird nach
  `tools/uft-scout/work/`.
- **Aufwand:** Begutachtung **mehr als eine Sitzung** — 97 Swift-Dateien
  in einer Sprache, für die hier keine Werkzeugkette existiert, also
  reines Lesen ohne Ausführen. Genauer nicht schätzbar. Einbau
  **entfällt** unter dem heutigen Lizenzurteil; was bliebe, wäre ein
  **Nachbau** nach `docs/QUARANTINE_PROCESS.md` §5 — und das ist ein
  eigener Posten mit eigener Eigentümer-Entscheidung.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a013_amigadiskkit.gutachten.md`, (a)…(f). Repo flach
  geklont nach `tools/uft-scout/work/AmigaDiskKit`; **97 `.swift`, 26
  `.bin`, 11 `.txt` — genau die Zahlen aus `P3-385`**, Apache-2.0
  bestätigt.
  · **(e) DREI ZAHLEN VON `P3-385` SIND FALSCH**, case-sensitive
    nachgemessen mit `rc` je Lauf: `RDB` **11 Treffer in 2 Dateien** statt
    0 — und **umgesetzt**, nicht erwähnt (`uft_hdf_parse_rdb()`,
    `calc_rdb_checksum()`, BE-Felder bei Versatz 128/132, Schranke gegen
    „malformed RDB"); `FFS2` **1** statt 0, aber nur als Kommentarzeile;
    `LHA` **8** statt 1. Richtig bleiben `rigid_disk` 0 und `PFS3` 0.
  · **(a) Vierzehn Achsen je mit Urteil.** Die eine, die dem Baum wirklich
    fehlt, ist **`PFS3`** — 6 Dateien, **3207 Zeilen**, die größte
    Einzelachse des Repos, im Baum **rc 1 / 0 Treffer**. Gleichwertig sind
    Floppy/ADF, FAT, MBR, ImageIO; schwächer sind RDB, Preview/IFF — und
    **LHA**.
  · **LHA ist der schärfste Einzelbefund:** der Baum trägt eine Format-ID
    (`UFT_FMT_LHA = 203`), drei Funktionsdeklarationen — und
    `src/fs/uft_amigados_extended.c:683` „LHA Archive - Stubs" mit **drei
    Rümpfen `return -1; /* Not implemented */`**. Klasse `P3-204`. Das
    fremde Repo hat 1797 Zeilen davon, und Apache-2.0 verbietet den Port;
    LHA ist aber dokumentiert, also wäre es ein **Nachbau**, kein Port.
  · **(b) Die Orakel-Frage ist entschieden: auf dieser Maschine
    verschlossen.** Keine Swift-Werkzeugkette, und `docs/ORACLES.md`
    verlangt Bau UND Ausführung. Delegierbar wie die Tier-3-Bank (MF-310).
    Was delegiert werden müsste, steht im Gutachten §4 — inklusive der
    Gegenprobe auf **ADFlib-Unabhängigkeit**, ohne die auch ein
    erfolgreicher Bau nichts belegt (MF-1033).
  · **(c) Daten-Kanal: 26 Dateien, 5 097 472 Byte**, gezielte Ausschnitte
    ECHTER Aufbauten (`classic-8g`, `mister-8g`, `pistorm-29g`, `hst-`,
    `pfs3-2g/8g`) — RDB-Bereiche, Bootblöcke, FAT32-Systembereiche.
    Ausgezeichnetes Fixture-Material; **Herkunft je Datei fehlt**, und die
    Repolizenz deckt den Disketteninhalt nicht (`SCOUT-5`).
  · **(f)** kein Byte nach `src/`, `include/`, `tests/`.
- **Beleg:** `b1e3a6ab` (MF-1203) (nachgetragen 2026-09-26 beim Aufraeumen: der Commit, der das Gutachten in den Baum gebracht hat, gemessen mit `git log --diff-filter=A`) · Gutachten
  `tools/uft-scout/out/a013_amigadiskkit.gutachten.md`.

### A-014 · `SecurityRonin/disk-forensic` auseinandernehmen — und die Vorfrage lautet: ist das Diskettenarbeit?
- **Status:** **erledigt** 2026-09-16 (Begutachtung; Urteil **Fundus** —
  die Vorfrage ist mit Nein beantwortet) · MF-1204 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/SecurityRonin/disk-forensic.git nimm den
  code komplett auseinander , sehr genau / finde alles und alles raussuchen
  was ich übersehen habe , stimme es mit mein aktuellen tool ab / - wo
  können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen"
- **Kennzahl:** **keine der vier.** Und hier ist die Zeile ausnahmsweise
  ein echter Einwand, nicht nur Buchhaltung: **der Gegenstand ist keine
  Diskette.** E01, VMDK, VHDX, VHD, QCOW2 und DMG sind Festplatten- und
  VM-Behälter, MBR/GPT/APM sind Partitionsschemata für Medien, die größer
  sind als eine Diskette, und ISO 9660 ist optisch. Eine 3,5-Zoll-Diskette
  hat keine Partitionstabelle. **Genau ein Teil trägt** — siehe die
  Aufnahme-Messung.
- **Kanal:** **Spec, und mehr ist hier nicht zu holen.** Gemessen:
  `license.spdx_id` = **Apache-2.0**, Sprache **Rust**, 574 KB, angelegt
  2026-06-05, letzter Push 2026-08-26, 4 Sterne, kein Fork. Apache-2.0
  gegen GPL-2 ist unverträglich — dieselbe Lage wie `A-013`/`P3-385`, und
  auch hier gibt es die zwei Präzedenzfälle `P3-316` ✅ und `P3-353` ✅.
  **Dazu verschlossen: `cargo` und `rustc` sind auf diesem Rechner nicht
  vorhanden**, also ist auch *Orakel* nach `docs/ORACLES.md` hausintern
  unmöglich („Kein Oracle auf Zusicherung"). Es bleibt **Lesen**.
- **Einfrier-Regel:** **nein für das Lesen.** Ein Einbau wäre eine neue
  Behälterschicht und fiele voll unter das Moratorium — als *neues Format*,
  nicht als Bugfix.
- **OPEN_ITEMS:** **`P3-387`** ist der tragende Anker — „Zwei forensische
  Fähigkeiten, die der Baum nicht hat, und die seine eigene Mission
  verlangt: ein stückweiser Hash und ein Fehlerprotokoll, das den Lauf
  überlebt." Dazu `P3-380` (ATX kann exportieren, niemand bietet es an),
  `P3-381` (darf ein Schreiber die Identitätsdatei überschreiben) und
  `KI-6.1` (keine CI-Prüfung durch echte Emulatoren).
- **Bei der Aufnahme gemessen — und ein Fund ist nicht offensichtlich:**
  **(1) Nichts davon ist im Baum.** Fünfzehn Begriffe gesucht, alle **0
  Dateien**: `E01`, `ewf`/`EWF`, `vmdk`, `vhdx`, `qcow`, `dmg`,
  `iso9660`, `GPT`, `APM`. **Mit einer Ausnahme:** `mbr` hat **2 Dateien**
  — `include/uft/formats/uft_fat32_mbr.h` und
  `src/formats/fat32/uft_fat32_mbr.c` —, und `partition` hat 0. MBR ist
  also punktuell da, als FAT32-Anhang, nicht als Achse.
  **(2) Der eine Teil, der trägt, ist `E01` — und zwar als SCHREIBZIEL,
  nicht als Leser.** Das EnCase Expert Witness Format führt eine **CRC je
  32-KiB-Block** und einen Bereichsvermerk für fehlerhafte Sektoren. Das
  ist wörtlich, was `P3-387` als fehlend benennt: „ein SHA-256 je
  (Zylinder, Kopf) statt einer Summe über die ganze Datei" und „`uft_format
  _mark_last_missing()` kennzeichnet im SPEICHER, und keine Datei trägt es
  hinaus". **E01 ist der bestehende Industriestandard für genau diese zwei
  Fähigkeiten** — und der Wert dieses Repos liegt darin, seinen Aufbau zu
  zeigen, nicht darin, VM-Behälter zu lesen. **(3) Die übrigen sieben
  Behälter sind für dieses Werkzeug Fundus**, nicht Auftrag: sie bewegen
  keine Kennzahl und gehören nicht zur Mission. Das ist eine Einordnung,
  keine Abwertung des Repos.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** die Vorfrage
  ausdrücklich beantwortet — welche der acht Behälter und drei
  Partitionsschemata für ein **Disketten**werkzeug überhaupt in Betracht
  kommen, je mit Begründung, und welche als Fundus notiert werden;
  **(b)** für `E01` den Aufbau aus dem Rust-Code **liest** und daraus
  benennt, was ein UFT-Schreiber bräuchte (Blockgröße, CRC-Stelle,
  Fehlerbereichs-Satz, Fallmetadaten) — als **Spec**, ohne eine Zeile zu
  übernehmen, und mit dem Hinweis, dass die kanonische Quelle
  libewf/ASR-Dokumentation ist und nicht dieses Repo; **(c)** die Brücke
  zu `P3-387` herstellt oder begründet verwirft; **(d)** die fünf Fragen je
  mit Messung beantwortet; **(e)** festhält, dass `cargo`/`rustc` fehlen
  und ein Orakel damit delegiert werden müsste — samt der Angabe, was
  genau zu bauen und welche Ausgabe der Beleg wäre; und **(f)** kein Byte
  nach `src/`, `include/` oder `tests/` schreibt.
- **Aufwand:** Begutachtung **eine Sitzung, wenn sie sich auf `E01`
  beschränkt** — genau das ist zu entscheiden, nicht zu unterstellen. Eine
  Durcharbeitung aller acht Behälter ist **nicht schätzbar** und wäre nach
  MF-640 Fundus. Einbau **entfällt** unter dem heutigen Lizenzurteil; ein
  E01-Schreiber wäre ein **Nachbau** nach benannter Spezifikation und ein
  eigener Posten mit eigener Eigentümer-Entscheidung.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a014_disk_forensic.gutachten.md`, (a)…(e).
  · **(b) DIE ANNAHME DIESER AUFNAHME FÄLLT — und das ist der Befund.**
    Sie setzte darauf, dass das Repo den **E01-Aufbau zeigt**. Gemessen
    über alle **2 146** Zeilen: **es implementiert E01 überhaupt nicht.**
    Es vergleicht die Kennung (`ewf::EVF1_SIGNATURE`, `EVF2`, `LEF2`) und
    ruft dann `::ewf::EwfReader::open(path)` — eine **externe Kiste**. Der
    Quelltext sagt es selbst an `container.rs:123-124`: „the decoder is the
    external `ewf` crate". **Keine Blockgröße, keine CRC-Stelle, kein
    Fehlerbereichs-Satz, keine Fallmetadaten.** Als *Spec*-Quelle für E01
    taugt es nicht; die kanonische Beschreibung liegt bei **libewf** bzw.
    der ASR-Dokumentation — was die Aufnahme zur Hälfte selbst
    vorweggenommen hatte.
  · **(a) Die Vorfrage ist mit NEIN beantwortet.** Von zehn Gegenständen
    ist keiner Diskettenarbeit: VMDK/VHDX/VHD sind VM-Behälter, DMG ist
    macOS, ISO 9660 optisch, GPT/APM Festplatten-Partitionsschemata. **Eine
    3,5-Zoll-Diskette hat keine Partitionstabelle** — und GPT (48), MBR
    (40) und APM (25) sind mit 113 Nennungen der größte Einzelblock des
    Repos. Einzige Berührung ist MBR, und dort ist der Baum punktuell
    versorgt (`uft_fat32_mbr.h/.c`).
  · **Eine weitere Zahl der Aufnahme fällt:** sie zählte acht Behälter
    auf, darunter **QCOW2** — gemessen **0 Treffer** im Quelltext. Das Repo
    behandelt die Behälter nicht, es **erkennt und delegiert**; einzige
    eigene Umsetzung ist `vhd.rs` (244 Z.).
  · **(c) Die Brücke zu `P3-387` ist begründet VERWORFEN**, nicht
    hergestellt: sie führt richtig über E01 (CRC je Block,
    Bereichsvermerk für fehlerhafte Sektoren) — nur nicht über dieses Repo.
  · **(e) Orakel verschlossen** (kein `cargo`/`rustc`) und **es lohnt
    nicht**: ein Orakel ohne gemeinsame Gegenstände vergleicht nichts.
  · **Urteil: FUNDUS.** Kein Port (Apache-2.0), kein Orakel, kein
    Spec-Gewinn.
- **Beleg:** `ecba5d17` (MF-1204) (nachgetragen 2026-09-26 beim Aufraeumen: der Commit, der das Gutachten in den Baum gebracht hat, gemessen mit `git log --diff-filter=A`) · Gutachten
  `tools/uft-scout/out/a014_disk_forensic.gutachten.md`.
- **Stand:** —
- **Beleg:** —

### A-015 · `programandala-net/mkmgt` auseinandernehmen — MGT steht auf T1, die Frage ist Beta DOS
- **Status:** **erledigt** 2026-09-16 (Begutachtung; **zwei neue Befunde**,
  `P3-465` und `P3-466`) · MF-1205 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/programandala-net/mkmgt.git nimm den
  code komplett auseinander , sehr genau / finde alles und alles raussuchen
  was ich übersehen habe , stimme es mit mein aktuellen tool ab / - wo
  können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen"
- **Kennzahl:** **keine der vier, und hier ist der Grund ungewöhnlich
  günstig: der Behälter ist schon fertig.** Gemessen steht `mgt` auf
  **T1** — die höchste Stufe — mit **sechs** Tests
  (`test_durchschreibprobe`, `test_kopflose_sonden_sind_erreichbar`,
  `test_mgt_gegen_mame`, `test_mgt_schreibt_in_die_datei`,
  `test_mgt_verzeichnis_vollstaendig`, `test_oeffentliche_api_am_korpus`).
  **Damit korrigiere ich meine eigene Erwartung bei der Aufnahme:** ich
  hatte MF-1006 im Kopf („`mgt` von T3 auf T2, Feldabgleich gegen MAMEs
  `coupedsk.cpp`, und er fand KEINEN Fehler") und daraus geschlossen, ein
  fremder **Schreiber** würde die Stufe heben. Er würde sie nicht heben —
  sie ist schon oben.
- **Kanal:** **Spec.** Gemessen: `license.spdx_id` = **GPL-3.0**, Sprache
  **Forth**, 40 KB, letzter Push **2020-05-14**, 2 Sterne. GPL-3 ist mit
  diesem Baum verträglich (er trägt GPL-3-Bestandteile), ein *Port* wäre
  also rechtlich offen — aber **Forth nach C ist eine Neuschreibung, kein
  Port**. Und *Orakel* ist hausintern verschlossen: `gforth`, `pforth`,
  `sf` und `swiftforth` sind **alle vier nicht vorhanden**. Es bleibt
  Lesen. **Das ist die vierte fehlende Werkzeugkette in Folge** —
  `mkfs.fat`/`mtools` (A-005), Swift (A-013), Rust (A-014), Forth (hier).
- **Einfrier-Regel:** **nein für das Lesen.** Ein Beta-DOS-Leser wäre eine
  neue Dateisystemvariante und fiele unter das Moratorium.
- **OPEN_ITEMS:** kein Treffer zu `mkmgt`, `GDOS` oder `Beta DOS`. Der
  Nachbar ist die Namensrolle/Variantenfrage: `mgt` ist ein **Behälter**,
  und was darin liegt, ist GDOS, G+DOS oder Beta DOS — die Achse, für die
  `uft-variants` zuständig ist („wo sagen wir etwas Falsches, ohne dass es
  auffällt?").
- **Bei der Aufnahme gemessen — und ein Wert bleibt:** **(1) Der Behälter
  ist belegt, das Dateisystem nicht vollständig.** Das Repo nennt
  ausdrücklich **drei** DOSe: „ZX Spectrum GDOS, G+DOS and Beta DOS".
  Gemessen im Baum: `DISCiPLE` steht in **5** Dateien, darunter
  `include/uft/formats/uft_mgt.h`, `src/formats/mgt/uft_mgt.c`,
  `include/uft/uft_format_plugin.h`, `include/uft/xdf/uft_xdf_zxdf.h` und
  `src/analysis/profiles/uft_profile_uk.c` — der Baum kennt also die
  DISCiPLE/+D-Herkunft. **`gdos` und `plusd` stehen dagegen nur in zwei
  TESTS** (`test_kopflose_sonden_sind_erreichbar.c`,
  `test_zx_und_pc98_echte_abbilder.c`), sehr wahrscheinlich als
  Korpus-Dateinamen und nicht als unterschiedene Dateisysteme.
  **`Beta DOS`/`BetaDOS`/`betados`: 0 Treffer, überall.** Das ist die
  Lücke, und sie ist klein und benennbar. **(2) Zwei MGT-Leser liegen
  schon im Baum:** `src/formats/mgt/uft_mgt.c` und `src/samdisk/mgt.cpp`.
  Ein dritter wäre die Lage aus P3-147 (G64 hat drei Leser), nicht ein
  Zugewinn. **(3) Eine Messung ist unbrauchbar und wird nicht verwendet:**
  `grep -F "+D"` meldete 12 Dateien, trifft aber jedes `x+D` in
  C-Ausdrücken. Für „+D" gibt es damit **keine** belastbare Zahl in dieser
  Aufnahme.
- **Fertig heißt:** ein Gutachten als Dokument, das **(a)** ausdrücklich
  feststellt, was ein Schreiber für `mgt` **nicht** mehr leisten kann,
  weil das Format auf T1 steht — und damit die Erwartung korrigiert, statt
  sie zu bedienen; **(b)** die drei DOSe **einzeln** gegen den Baum hält:
  liest UFT GDOS, G+DOS und Beta DOS, oder nimmt es eines an und schweigt?
  Je mit `git ls-files`-Messung und, wo möglich, an einem Korpus-Abbild;
  **(c)** die Forth-Quelle als **Spec** liest — Verzeichnisaufbau,
  Sektorbelegung, Namensfelder — und daraus einen Prüfauftrag formt statt
  eines Ports; **(d)** die fünf Fragen je mit Messung beantwortet;
  **(e)** die vierte fehlende Werkzeugkette in Folge als eigenen
  Vorschlag für `docs/OPEN_ITEMS.md` benennt, weil ein Orakel-Kanal, der
  regelmäßig an der Werkzeugkette scheitert, eine Entscheidung braucht
  (delegieren wie die Tier-3-Bank, oder die Kanalregel präzisieren); und
  **(f)** kein Byte nach `src/`, `include/` oder `tests/` schreibt.
- **Aufwand:** Begutachtung **eine Sitzung** — 40 KB Forth sind der
  kleinste Gegenstand dieser Reihe, und die Frage ist eng (drei DOSe, ein
  Behälter, der schon belegt ist). Einbau: für Beta DOS **nicht
  schätzbar** und ein eigener Posten unter dem Moratorium.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a015_mkmgt.gutachten.md` — **und der Posten hat
  etwas gefunden, wonach er nicht gesucht hat.**
  · **(b) Die gestellte Frage, beantwortet:** UFT benennt **keines** der
    drei DOSe im Quelltext. `GDOS`, `gdos`, `G+DOS`, `BetaDOS`,
    `Beta DOS`, `betados`, `plusd` — **alle sieben rc 1 / 0 Treffer** in
    `src/` und `include/`; nur `DISCiPLE` hat 9 Treffer in 5 Dateien. Der
    Baum kennt die **Hardware**, nicht das Dateisystem. Steht als
    `P3-466`.
  · **DER EIGENTLICHE FUND — `mgt_dir_entry_t` ist ab Versatz 11 um ein
    Byte verschoben, und `mgt` steht auf T1.** Der Schreiber `mkmgt.fs`
    kommentiert jede Position: Sektorzahl **16 Bit Big-Endian auf 11-12**,
    Spur auf **13**, Sektor auf **14**, Karte auf **15-209**, GDOS-Kopf auf
    **210-219**. UFT führt `uint8_t sectors_used` (11), `track` (12),
    `sector` (13), `sector_map[195]` (14-208). **An einem ECHTEN
    Korpus-Abbild gemessen** (`zxfd_plusd_gdos_tools.mgt`): alle vier
    geprüften Einträge melden nach UFTs Lesart `sectors_used = **0**` —
    eine vorhandene Datei mit null Sektoren gibt es nicht. Dazu geht die
    Feldsumme nicht auf: **255 gegen `MGT_DIR_ENTRY_SIZE` 256**. Steht als
    `P3-465`.
  · **Warum keiner der sechs Tests anschlägt, ist mitgemessen:**
    `test_mgt_verzeichnis_vollstaendig` baut seine Daten selbst und prüft
    nur Versatz 0 und 1-10 — genau die zwei Felder, die stimmen; die vier
    verschobenen fasst er nie an. `test_mgt_gegen_mame` (MF-1006) verglich
    den **Behälter**. **Die T1-Stufe ist korrekt vergeben und sagt über das
    Verzeichnis nichts.**
  · **(a) Ein Schreiber hebt bei `mgt` nichts** — das Format steht auf T1.
    Und `mkmgt` wäre ohnehin verschlossen: Forth, und `gforth`, `pforth`,
    `sf`, `swiftforth` fehlen alle vier. **Vierte fehlende Werkzeugkette in
    Folge** nach A-005 (mkfs.fat/mtools), A-013 (Swift), A-014 (Rust).
  · **Urteil: REFERENCE.** Aus dem Repo ist nichts zu übernehmen — sein
    Wert waren die Feldlagen, und die haben einen Defekt aufgedeckt.
- **Beleg:** `bf3bc534` (MF-1205) (nachgetragen 2026-09-26 beim Aufraeumen: der Commit, der das Gutachten in den Baum gebracht hat, gemessen mit `git log --diff-filter=A`) · Gutachten `tools/uft-scout/out/a015_mkmgt.gutachten.md`;
  Befunde `P3-465` und `P3-466`.

### A-016 · `ChrisBertrandDotNet/ST-Recover` — **das ist `P3-63`**, und `P3-59` hat seine Vorbedingung umgekehrt
- **Status:** **erledigt** 2026-09-17 — Gutachten (2026-09-16) **plus**
  Teil (d) entschieden: **nicht gebaut, aus drei Messungen**, und die
  Begründung der Zurückstellung ist dabei gefallen (die Brücke existiert
  — `P3-473`). Der öffnende Schritt ist benannt: ein **Leser**, und
  `P3-453` bringt ihn mit · MF-1206, MF-1217 ·
  **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/ChrisBertrandDotNet/ST-Recover.git nimm
  den code komplett auseinander , sehr genau / finde alles und alles
  raussuchen was ich übersehen habe , stimme es mit mein aktuellen tool ab
  / - wo können die formate verbessert werden / - ist es auf andere formate
  übertragbar / - welche einstellungen fehlen noch / - was habe wir noch
  nicht / - brauch es eine HAL-Erweiterungen , **erweite das tool**, plan
  verstanden, was kannst du besser machen ??"
- **Kennzahl:** **keine der vier.** Beide Größen aus `P3-63` sind
  forensische Aussagen je Sektor, keine Tier-Stufe und kein
  Wandlungspfad — sie berühren die fünfte, offene Zahl (MF-640) und
  `P3-387` (ein Fehlerprotokoll, das den Lauf überlebt).
- **Kanal:** **Spec — und hier hilft `P3-452` ausdrücklich NICHT.** Gemessen:
  das Repo führt `Source/Ms-RL License.htm` und `License.htm`, also die
  **Microsoft Reciprocal License**; `gh api` liefert `NOASSERTION`, weil
  GitHub sie nicht einordnet. **Ms-RL ist mit GPL in JEDER Fassung
  unverträglich** — anders als Apache-2.0, das nur gegen GPL-2 scheitert.
  Die GPL-3-Bindung aus MF-698 öffnet den Port-Kanal hier also nicht.
  Was bleibt: Werte lesen, keine Zeilen nehmen — und **das ist schon
  geschehen**: `P3-63` führt vier belegte Bestätigungen aus dieser Quelle
  (32 µs je Rohbyte aus `200000/6250`, „11 Sektoren/Spur in 2 Umdrehungen",
  das 50-Byte-Fenster zwischen ID- und Datenmarke, und `128 << (n & 3)`).
- **Einfrier-Regel:** **ja.** Beide Größen hängen an `uft_sector_t`, der
  kanonischen Struktur aus `include/uft/uft_types.h` („This is the ONE
  definition … used across the entire project"). Rotbeweis zuerst, und eine
  ABI-Frage steht daneben.
- **OPEN_ITEMS:** **`P3-63`** — dieser Posten IST der Auftrag, ihn
  abzutragen; der Befund bleibt dort. Vorbedingung war `P3-59`, dazu
  `P3-387` (Fehlerprotokoll mit Position), `P3-51(1)` (die 11 Sektoren) und
  `P3-453` (Schreibnaht — siehe unten).
- **Bei der Aufnahme gemessen — und die Vorbedingung ist der eigentliche
  Befund:** `P3-63` sagt „die Reihenfolge steht: **nach P3-59**".
  **`P3-59` ist ✅ erledigt (MF-832) — aber seine Auflösung war eine
  Umkehrung:** „Vier weitere Felder auf vier tote zu setzen macht die
  Struktur **irreführender**, nicht reicher." Gemessen hatte MF-831, dass
  von den fünf Positionsfeldern auf `uft_sector_t` **genau eines** je
  gefüllt wird — `angular_position`, von `uft_atx.c:366`, dem einzigen
  Format, das es kann. `id_offset` hat **eine** Fundstelle im ganzen Baum,
  nämlich die Deklaration; `gap_before` einen Treffer auf einer **fremden**
  Struktur; `data_offset` und `bit_offset` haben 44 bzw. 14 Schreibstellen,
  die **alle** anderen Strukturen gehören. Dazu heute nachgemessen:
  `trouve_par_le_controleur`, `gap_us` und `duree_espace` haben je **0**
  Treffer. **`P3-63` ist damit in der Reihenfolge frei und in der Sache
  blockiert:** seine zwei Größen wären das fünfte und sechste tote Feld.
  Der Punkt sagt das selbst — „die zwei Groessen setzen gefuellte
  Feldgrenzen voraus; heute gibt es keinen Erzeuger dafuer".
- **Und eine Verbindung, die den Weg abkürzt:** `duree_espace_libre_en_1er`
  ist die **Lücke als Messgröße in Mikrosekunden statt als Restmenge** —
  und genau die entsteht bei `P3-453`/Phase 2 des laufenden Plans
  (Schreibnaht auf IBM-MFM **aus den Lückenwerten**). Beide Punkte wollen
  dieselbe Messung von zwei Seiten. Wer Phase 2 baut, erzeugt die Hälfte
  von `P3-63` mit.
- **Fertig heißt:** ein Gutachten **plus** die Erweiterung, in dieser
  Reihenfolge, weil der Auftrag „erweite das tool" sagt:
  **(a)** die 13 Quelldateien durchgehen und je Fund „schon in `P3-63`
  genannt / neu / trifft nicht zu" vergeben — `P3-63` ist vom Eigentümer
  gelesen, ein zweites Lesen muss also **etwas Neues** liefern oder das
  sagen; **(b)** die Ms-RL-Grenze einhalten: Werte und Verfahren
  beschreiben, **keine Zeile übernehmen**, und im Header benennen;
  **(c)** die Vorbedingung ausdrücklich entscheiden — einen **Erzeuger** für
  die Feldgrenzen bauen (dann sind die zwei Größen sinnvoll) oder die zwei
  Größen zurückstellen, mit Begründung; **(d)** wenn gebaut wird: Rotbeweis
  zuerst, ein Aufrufer im selben Commit (D2), und die ABI-Frage zu
  `uft_sector_t` benannt; **(e)** die fünf Fragen je mit Messung;
  **(f)** kein Byte des Repos nach `src/`, `include/` oder `tests/`.
- **Aufwand:** Begutachtung **eine Sitzung** (13 Quelldateien, 127 KB, und
  `P3-63` liefert die Vorarbeit). Die **Erweiterung** ist nicht schätzbar —
  sie hängt daran, ob ein Erzeuger für die Feldgrenzen entsteht, und das
  ist Decoder-Arbeit am Spurmodell, nicht ein Feld anhängen.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a016_st_recover.gutachten.md`, (a)…(f).
  · **(c) DIE VORBEDINGUNG IST GEFALLEN — durch eigene Arbeit, seit der
    Aufnahme.** `P3-63` war blockiert, weil seine zwei Größen „gefüllte
    Feldgrenzen voraussetzen" und es „heute keinen Erzeuger dafür" gebe.
    **Seit MF-1190 gibt es ihn:** `uft_mfm_sector_t` trägt `id_sync_bit`,
    `data_start_bit`, `gap2` und `lead_gap` — im **Produktivpfad**
    `uft_mfm_decode_track()`. Die Aufnahme hatte es fast gesehen („wer
    Phase 2 baut, erzeugt die Hälfte von P3-63 mit"); gebaut ist Phase 2
    als MF-1190 = `e22e1b78`, und sie hat **beide** Hälften gebracht.
  · **Entscheidung: nicht zurückgestellt, sondern benannt.** Gebaut wird
    hier nicht, und der Grund ist die Bauform: die Größen hängen an
    `uft_sector_t`, der kanonischen Struktur — das ist eine **ABI-Frage**,
    braucht Rotbeweis-zuerst und einen Aufrufer im selben Commit. Die
    Gestalt des Auftrags steht im Gutachten §1.4 und als Fortschreibung an
    `P3-63` selbst, samt der Lösung, die der Baum sich bereits notiert hat
    (`uft_types.h:397`: Flag `has_bit_positions` nach dem Muster von
    `has_angular_position`).
  · **(a) Ein zweites Lesen liefert nichts Neues von Gewicht** — und das
    ist die ehrliche Antwort. 13 Dateien, 3 222 Zeilen; `P3-63` hat die
    Quelle bereits ausgewertet (vier belegte Werte). Neu sind allein
    `Analyse_disque.*` (138 Z.), und das ist **Lesestrategie**, kein
    Formatwissen — es berührt A-010.
  · **(b) Ms-RL ist härter als Apache-2.0:** unverträglich mit GPL in
    **jeder** Fassung, `P3-452` hilft hier ausdrücklich nicht. Das
    Gutachten zitiert keine Zeile Quelltext.
  · **(f)** kein Byte nach `src/`, `include/`, `tests/`.
- **Nachtrag MF-1217 — (d) ist jetzt ENTSCHIEDEN statt verschoben, und
  die Begründung der Zurückstellung ist dabei gefallen.**
  · **Die Brücke existiert.** `include/uft/uft_types.h` stellte das Flag
    mit dem Satz zurück, der Parser „erzeugt `uft_mfm_sector_t`, nicht
    `uft_sector_t` — **ein Erzeuger dafür wäre die Brücke zwischen
    beiden; die gibt es nicht**". Gemessen gibt es sie:
    `src/formats/86box/uft_86f_plugin.c:309-323` läuft über
    `uft_mfm_sector_t recs[]` und ruft
    `uft_format_add_sector_with_id(…)`, der in
    `include/uft/uft_format_common.h:82` ein **`uft_sector_t`** anlegt
    und über `uft_track_add_sector()` kopiert. Sie **verwirft** nur
    `id_sync_bit`, `data_start_bit`, `gap2`, `lead_gap`. Das ist
    **`P3-473`**.
  · **Gebaut wird trotzdem nicht — aber aus DREI Messungen, nicht aus
    einer Verschiebung:**
    **(1)** Die Hälfte von Größe 1 gibt es schon in UFT-Idiom:
    `UFT_SECTOR_MISSING` (`uft_types.h:294`) samt Konvention
    (`uft_format_common.h:186`) und Setzer (`:522`) — das ist `MF-980`.
    Die andere Hälfte (controller-gemeldet vs. **rekonstruiert mit
    Platzhalter**) hat **keinen Erzeuger**: UFT setzt keine Platzhalter,
    das Feld sagte immer dasselbe.
    **(2)** Größe 2 liegt nicht in ihrer Einheit vor: `uft_mfm_gap_t`
    (`uft_mfm_sector_parser.h:131-148`) führt **nur** Bits und Wörter,
    keine Mikrosekunden; die Umrechnung bräuchte eine Stelle, die es
    nicht gibt (`MF-1177`).
    **(3)** Der harte Grund: **kein Produktivleser** für sektorbezogene
    Positionen im ganzen Baum — sogar `angular_position`, das EINZIGE
    gefüllte Feld (`uft_atx.c:366`), hat außerhalb seines Erzeugers **2**
    Treffer, **beide in einem Kommentar**. Der Sektor-Editor rechnet
    seinen Versatz selbst (`uft_sector_editor.cpp:730`) und hat **0**
    Treffer auf die Felder.
  · **Was den Punkt öffnet, ist benannt und klein:** ein **Leser**.
    `P3-453` (Schreibnaht auf IBM-MFM aus den Lückenwerten) braucht
    Bitlagen ohnehin — wer ihn baut, bekommt Erzeuger, Brücke und Leser
    in einem Zug, und Flag plus Füllung sind dann ein Anhängsel statt
    eines Vorrats. Dann gilt (d) vollständig: Rotbeweis zuerst,
    Aufrufer im selben Commit, ABI-Frage zu `uft_sector_t` benannt.
  · **Kein Byte des Repos ist nach `src/`, `include/` oder `tests/`
    geschrieben worden** — (f) gilt unverändert.
- **Beleg:** `be19fb59` (MF-1206) (nachgetragen 2026-09-26 beim Aufraeumen: der Commit, der das Gutachten in den Baum gebracht hat, gemessen mit `git log --diff-filter=A`) · Gutachten `tools/uft-scout/out/a016_st_recover.gutachten.md`;
  `P3-63` fortgeschrieben (MF-1206), (d) entschieden und `P3-473` neu
  (MF-1217).

### A-017 · Die Befunde der Zulieferungen EINBAUEN (Plan, Phasen 1–5)
- **Status:** **erledigt** 2026-09-16 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „bau alles ein mach einen plan und zetzte es um" · später
  „mach A-005 ungestopt" · „beginne phase 1"
- **Kennzahl:** **keine der vier** für Phase 1 (Trägerprovenienz ist eine
  forensische Aussage, keine Tier-Stufe). Phase 4/5 berühren
  `T3 runter` mittelbar über den Abgleicher.
- **Kanal:** **Port.** Gemessen (MF-1188): alle fünf gelieferten
  Quelldateien sind `GPL-2.0-or-later`, also eigener Code unter der
  Projektlizenz — für die Phasen 1–4 gibt es keine Lizenzfrage. Die
  **Datenbestände** dahinter sind eine andere Sache und einzeln geprüft.
- **Einfrier-Regel:** **ja** für jede Phase, die den Format-/Decoder-Layer
  berührt → Rotbeweis zuerst, benannte Referenz im Header.
- **OPEN_ITEMS:** `P3-454` (Phase 1) · `P3-453` (Phase 2) · `P3-455`
  (Phase 3) · `P3-449`/`P3-451` (Phase 4) · `P3-445` (Phase 5)
- **Fertig heißt:** jede Phase mit eigenem Commit, Rotbeweis belegt,
  Produktivaufrufer im **selben** Commit (D2), Vollsuite grün.
- **Stand:** **Phase 1 und Phase 2 fertig.**
  · **Phase 1** (`P3-454`): `uft_bootstrap.{h,c}` + Aufruf in
    `fat_analyze_boot_sector()` + 9 Zusagen; D2-Probe gemessen (Aufruf weg →
    genau die zwei Verdrahtungs-Zusagen rot, 7/9, rc 1). Suite **494/494**.
    Nebenbefund gemessen und berichtigt: die Bootstrap-Datenbank **liegt**
    im gitignorierten Fremdklon (379 Schlüssel, 490 Namen, 223 verifiziert)
    — der leere Bestand ist **S3** (GPL-3.0 + Datenbankherstellerrecht),
    nicht Abwesenheit.
  · **Phase 2** (`P3-453`): die Lückenmessung sitzt **im** Produktivpfad
    `uft_mfm_decode_track()` (2 Produktivaufrufer), kein viertes Modul und
    kein dritter Wortleser. Rotbeweis zuerst, Mutationsmatrix **13 von 13
    im ersten Lauf**, Suite **495/495**. Zwei Befunde erst im Lauf: das
    erste Lückenwort weicht in 5 von 9 Fällen ab — **führende Taktzelle**,
    `0x9254 ^ 0x1254 = 0x8000`, und genau dort, wo das letzte Bit davor
    eine 1 war; und die Encoder-Lage ist von der Leseseite byteweise
    bestätigt (146 = 80+12+4+50). **S1 gehalten:** keine der fünf
    unbelegten Zahlen der Zulieferung ist übernommen, es gibt kein Feld
    `splice` und kein Herkunfts-Urteil.
  · **Phase 3** (`P3-455`, PC-Schutz aus Fehlsektormustern): **ANGEHALTEN
    MF-1192, und der Halt ist das Ergebnis.** Kein Code. Drei Gründe, jeder
    einzeln hinreichend, alle gemessen: (1) im Korpus liegen **103**
    Dateien und **0** geschützte PC-Abbilder — ein Rotbeweis für einen
    Herstellernamen könnte nur gegen eine selbst nach der Tafel gebaute
    Diskette prüfen, der geschlossene Kreis von MF-1009/MF-1028; (2) `P0-2`
    ist mit der ausdrücklichen Begründung geschlossen, dass Verdrahten die
    falsche Antwort ist; (3) `uft_pc_disk_if_t.is_bad(lba)` hat im Baum
    keine Antwort — der Header dafür ist ein Phantom (9 von 9 Funktionen
    ohne Rumpf, `P3-228`). **Die Lizenz sperrt hier NICHT** (Nachbau,
    9 Sektorlisten sind keine Datenbank, MF-698 deckt die Bindung).
    Vierter Befund: zwei der neun Muster sind über die Fehlsektoren nicht
    unterscheidbar und müssten „mehrdeutig" melden. Nächster Handgriff
    steht in `P3-455` und ist **nicht** die Tafel.
  · **Phase 4** (`P3-449`/`P3-451`, Bitkonfidenz aus A-006): **ANGEHALTEN
    MF-1193.** Kein Code, dafür drei Messungen und zwei neue Befunde.
    Gemessen: das Ziel des Ports — `src/algorithms/advanced/
    uft_multi_rev_fusion.c` (469 Z., 4 öffentliche Funktionen) — hat **0**
    Produktivaufrufer, **jede** Aufrufstelle liegt in einem Test; die
    Vorbedingung der Weak/Fuzzy-Trennung (Phasenlage je Bit) liegt in
    `src/algorithms/uft_kalman_pll.c` (399 Z., 6 öffentliche Funktionen)
    mit **0** Aufrufstellen überhaupt; und der erreichbare
    Abstimmungspfad (`multiread_*`, 1 Produktivaufrufer) stimmt
    **byteweise** ab, kann eine bitweise Karte also nicht tragen. Ein Port
    hätte eine **vierte** „weak bit"-Definition hinter eine Tür gelegt, die
    niemand öffnet. Neu eingetragen: **`P3-457`** (die Berichtigung aus
    MF-950 ist da und unerreichbar) und **`P3-458`** („weak" wird dreimal
    verschieden gerechnet, fünf Zahlen ohne Quelle). `P3-448`, `P3-449`
    und `P3-451` sind um je eine Messung erweitert.
  · **Und der Fehler liegt hier auch bei MIR, nicht nur an den
    Zulieferungen.** Ich habe fünf Phasen aus den Gutachten geordnet, ohne
    vorher die **Erreichbarkeit** ihrer Ziele zu messen. Phasen 1 und 2
    trugen, weil ihre Ziele Produktivaufrufer hatten
    (`fat_analyze_boot_sector`, `uft_mfm_decode_track`); Phasen 3 und 4
    fallen an genau dieser Frage. Ein Plan, der D2 erst beim Bauen prüft,
    plant die Hälfte seiner Posten gegen eine Wand. **Für Phase 5 wird die
    Erreichbarkeit zuerst gemessen.**
  · **Phase 5** (`P3-445`, A-005): **FERTIG MF-1195.** Und die
    vorgezogene Erreichbarkeitsmessung hat getragen — sie fiel erstmals
    seit Phase 2 positiv aus: `uft_probe_ranking_t` liegt in **3**
    Produktivdateien, `analyze_quality()` in `uft_smart_open.c` hält
    Zylinder/Köpfe/Sektoren/Sektorgröße zusammen. Deshalb wurde gebaut
    statt angehalten.
    Behoben und eingebaut: `uft_floppy_ranking_t` mit `tied`/`ambiguous`/
    `kandidaten` und den drei Feldzählern; ein Feld wird nur verglichen,
    wenn **beide** Seiten etwas sagen. 113 Referenzsätze als Tafel,
    Namensnennung (CC BY-SA 4.0) im Header, `docs/ORACLES.md` um den
    Nachtrag ergänzt, damit die dortige „nicht übernehmen"-Regel nicht
    im Widerspruch steht. **12 Zusagen, Mutationsmatrix 14/14 im zweiten
    Lauf** (der erste 8/14 — alle sechs Entkommenen waren echte Lücken
    in meinen Gegenproben), D2-Probe am Aufrufer gemessen mit geprüfter
    Rücknahme, Suite **497/497**.
    **Drei Erwartungen von mir sind dabei gefallen** (1,44 M hat 5 statt
    3 Gleichrangige und der PC ist jetzt dabei; Amiga DD 2 statt 1; die
    beiden Victor/Commodore-Sätze sind GCR, nicht MFM) — und die beiden
    Zahlen des Rotbeweises in `P3-445` waren beide falsch, weil sie an
    der defekten Fassung gemessen waren.
  · **Der Plan ist damit abgearbeitet:** Phasen 1, 2 und 5 gebaut,
    Phasen 3 und 4 mit Ergebnis angehalten. Offen bleiben die Posten
    `A-008`…`A-016` und `A-018`…`A-024` in der Warteschlange.
- **Beleg:** Phase 1 = **MF-1189** = `a27fb2d0` (9 Dateien, 894 Einfügungen)
  · Phase 2 = **MF-1190** = `e22e1b78` (6 Dateien, 856 Einfügungen)
  · Phase 3 = **MF-1192** = `32685609` (Halt, reine Doku)
  · Phase 4 = **MF-1193** = `503522c0` (Halt, reine Doku)
  · Phase 5 = **MF-1195** = `38d20378` (12 Dateien, 1607 Einfügungen).
  Alle mit Pre-Commit-Toren grün und `mcp=0`.
  **Gepusht** 2026-09-16 als Welle von 12 Commits (`76c56535..38d20378`),
  alle drei pre-push-Tore bestanden, danach `origin/main..HEAD` = 0.
- **Abschluss mit einer Einschränkung, die nicht verschwiegen wird:** die
  Zeile `Fertig heißt` verlangt für **jede** Phase Commit, Rotbeweis,
  Produktivaufrufer im selben Commit und grüne Vollsuite. Erfüllt ist das
  von den Phasen **1, 2 und 5**. Die Phasen **3 und 4** sind als
  **gemessener Halt** abgeschlossen — ein Halt ist nach dem AUFTRAG ein
  Ergebnis, aber er ist nicht das, was diese Zeile beschreibt. Der Posten
  gilt als erledigt, weil der PLAN abgearbeitet ist, nicht weil fünf
  Phasen gebaut wurden.
- **Nachtrag 2026-09-16 (MF-1199 = `70a940af`):** der Push von MF-1195 hat
  die CI rot gemacht — `Consistency check`, **1** Befund,
  `[STAND.md stale]`. Ursache gemessen: `gen_stand.py::offen()` zählt
  `docs/OPEN_ITEMS.md` aus dem **Arbeitsbaum**, in dem die Parallelsitzung
  unversionierte Zeilen liegen hatte. Versioniert 7796, Arbeitsbaum 7798,
  eingecheckt stand **7797** — ein Zwischenstand einer fremden Datei, den
  es in keinem Commit gab. Behoben mit einer Zeile (Befund `P3-463`),
  gepusht, `origin/main` = `70a940af`. Die Lehre steht als `P3-463`; der
  Umweg über einen `git worktree` ist dort **verworfen**, weil er im
  Hauptdepot zweimal `core.bare = true` hinterlassen hat.

---

`A-007` ist **erledigt** (Gutachten, MF-1186); sein Eintrag steht
unverändert an seinem Platz in der Warteschlange unten und trägt dort Status,
Stand und Beleg. Er wird hier **nicht** wiederholt — zwei Fassungen desselben
Postens wären zwei Wahrheiten (D3).

Erledigt: `A-005` (MF-1184 = `2aa7bda6`), `A-006` (MF-1185 = `6dbb4ebf`),
`A-007` (MF-1186 = `87abb0b3`), **`A-017`** (Phasen 1–5, MF-1189…MF-1195,
gepusht; Nachtrag MF-1199), **`A-008`** und **`A-009`** (MF-1201 =
`618ff62b`), **`A-010`** und **`A-011`** (MF-1202). Alle vier
Begutachtungen liegen als Dokument in `tools/uft-scout/out/`; der EINBAU
ist bei jeder ein eigener, noch nicht aufgenommener Posten.

**Der Begutachtungsblock `A-008`…`A-016` ist abgearbeitet** (neun Posten,
neun Gutachten unter `tools/uft-scout/out/`). `/aufgabe weiter` zieht als
Nächstes **`A-018`** hoch (Apple DOS 3.3).

> **Was die neun Begutachtungen zusammen ergeben haben, gezählt:**
> · **Vier Zulieferungen** (A-008…A-011): drei von ihnen liefern `.pro` und
>   `tests/CMakeLists.txt` als „aktuelle Verdrahtung" mit, jedes Mal
>   **kleiner als die des Baums**. Bei `A-009` hätte eine Dateiübernahme
>   **vier verdrahtete GUI-Reiter still wieder abgehängt** (MF-1194).
>   Nur `A-010` überschreibt nichts Gemessenes.
> · **Fünf Fremdrepos** (A-012…A-016): **vier fehlende Werkzeugketten in
>   Folge** — Swift, Rust, Forth, und schon bei A-005 `mkfs.fat`/`mtools`.
>   *Orakel* ist damit viermal hausintern verschlossen und **delegierbar**,
>   wie die Tier-3-Bank (MF-310).
> · **Lizenzlage:** einmal Port zulässig (**A-012**, MIT), zweimal
>   Apache-2.0 (A-013/A-014, kein Port), einmal **Ms-RL** (A-016, härter —
>   unverträglich mit GPL in jeder Fassung), einmal GPL-3 (A-015, Port
>   rechtlich offen, aber Forth nach C ist eine Neuschreibung).
> · **Drei Annahmen der Aufnahmen sind gefallen**, alle drei gemessen:
>   A-014 (E01 ist im Repo gar nicht umgesetzt), A-013 (drei Zahlen von
>   `P3-385` falsch), A-010 (kein CLI-Konflikt, Lizenz nicht offen).
> · **Zwei neue Befunde**, beide aus A-015: `P3-465` (MGT-Verzeichnis um
>   ein Byte verschoben, **an einem echten Korpus-Abbild gemessen**, in
>   einem Format auf **T1**) und `P3-466`.
> · **Eine Blockade ist gefallen:** `P3-63` war auf einen Erzeuger
>   gewartet, den **MF-1190 längst gebaut hatte** — bemerkt hat es erst
>   A-016.
>
> **Die Regel, die sich durch alle vier Zulieferungen zieht: nie die Datei,
> immer die Zeilen.**

> **Was die vier Begutachtungen zusammen ergeben haben — ein Muster, kein
> Einzelfall.** Drei der vier Zulieferungen liefern `UnifiedFloppyTool.pro`
> und `tests/CMakeLists.txt` als „aktuelle Verdrahtung" mit, und sie sind
> jedes Mal **kleiner als die des Baums**. Bei `A-009` hätte eine
> Dateiübernahme **vier verdrahtete GUI-Reiter still wieder abgehängt**
> (MF-1194), bei `A-008` einen gemessenen Kommentar über einen behobenen
> Defekt gelöscht. Nur `A-010` überschreibt nichts Gemessenes — dort ist die
> Überlappung eine echte Umschreibung.
>
> **Die Regel daraus, für jede künftige Zulieferung: nie die Datei, immer
> die Zeilen.** Sie kostet beim Einbau mehr und ist der einzige Weg, der
> keine stille Rücknahme erzeugt.

> **Warteschlange, gezählt statt geschätzt (Stand 2026-09-16):** `A-008`
> … `A-016` (neun Begutachtungen), dazu die sieben heute aufgenommenen
> `A-018` Apple DOS 3.3, `A-019` TR-DOS, `A-020` Amiga-Medienklassifikation,
> `A-021` FAT12-Robustheit, `A-022` diskstack, `A-023` Apple-Sektorordnung,
> `A-024` Audit-Umfang, und am Ende `A-004` (Punkt 7, angehalten am
> vereinbarten Schnitt) — **17 wartende Posten** bei einem laufenden.
>
> Nach Art sortiert, damit die Reihenfolge entscheidbar ist:
> · **Defekt im erreichbaren Pfad behauptet:** `A-019` (TR-DOS-Sonde liest
>   angeblich drei falsche Felder), `A-020` (Amiga-Klassierung),
>   `A-021` (FAT12 liefert bei Schaden erfundene Daten)
> · **Lücke:** `A-018` (Apple DOS 3.3 fehlt auf der FS-Achse ganz),
>   `A-023` (Apple-Sektorordnung, offener Punkt aus MF-714)
> · **Begutachtung fremden Codes:** `A-008`…`A-016`, `A-022`
> · **Methode statt Code:** `A-024`
>
> Vorgezogen wird nichts von selbst — das ist `/aufgabe vor`.

> **Ein Befund aus `A-007` betrifft zwei wartende Posten.** MF-698 bindet
> die verteilbare Fassung an GPL-3, und Apache-2.0 ist GPL-3-verträglich —
> die Kanal-Zeilen von `A-013` und `A-014` messen dagegen gegen GPL-2 und
> führen den Port als geschlossen. Sie bleiben unverändert stehen, bis der
> Eigentümer entscheidet; das Gutachten `a007_…` §8 nennt die Messung.

---

### A-022 · `Booyaka101/diskstack` auseinandernehmen — mehrere Abzüge EINER Diskette zusammenstimmen
- **Status:** **erledigt** 2026-09-16 (Begutachtung; Orakel-Kanal fast
  offen, ein Paket fehlt) · MF-1210 · **Aufgenommen:** 2026-09-16
- **Wortlaut:** „https://github.com/Booyaka101/diskstack.git nimm den code
  komplett auseinander , sehr genau / finde alles und alles raussuchen was
  ich übersehen habe  , stimme es mit mein aktuellen tool ab / - wo können
  die formate verbessert werden / - ist es auf andere formate übertragbar /
  - welche einstellungen fehlen noch / - was habe wir noch nicht / - brauch
  es eine HAL-Erweiterungen verbessere damit mein tool"
- **Kennzahl:** **keine der vier** unmittelbar. Mittelbar `leckende Tests
  null` unberührt; der eigentliche Bezug ist die **fünfte, offene Zahl**
  (MF-640) und `P3-387`.
- **Kanal:** **Oracle — und das ist stärker als der Port.** Gemessen über
  `gh api`: **MIT**, Python, 225 KB, 0 Sterne, letzter Push 2026-09-12,
  nicht archiviert. MIT erlaubt den Port, aber es ist **Python** und UFT
  ist C/Qt — ein Port wäre eine Neuschreibung. Ausgeführt ist es dagegen
  eine **fremde Hand**, die `.scp`, KryoFlux-`.raw`, `.hfe` und
  Sektorabbilder liest und sektorweise **abstimmt**: genau das, was
  `src/recovery/uft_multiread_pipeline.c` tut (MF-473).
- **Einfrier-Regel:** **ja**, sobald etwas im Decoder-/Recovery-Pfad
  entsteht → Rotbeweis zuerst. Die Begutachtung selbst ist
  Verifikationsarbeit.
- **OPEN_ITEMS:** `P3-387` (ein Verlustprotokoll, das den Lauf überlebt)
- **Fertig heißt:** ein Gutachten mit Lizenzurteil, Inventar-Abfrage und
  Differenzlauf-Plan; jeder Fund mit Kennzahl und Kanal; die Frage nach
  der HAL-Erweiterung **beantwortet**, nicht offen gelassen.
- **Aufwand:** **nicht schätzbar.**
- **Der Fund, der schon in der Beschreibung steht:** *„…and say what is
  still missing … prints the `gw read --tracks=` command for what is still
  bad."* Das ist ein Verlustbericht, der den **nächsten Befehl** nennt —
  und genau diese Gestalt hat `src/core/uft_loss_report.c` heute nicht.
  Der zweite Teil des Auftrags („wo können die Formate verbessert werden")
  trifft damit nicht die Formatschicht, sondern die Berichtsschicht.
- **Stand:** **Gutachten geschrieben**,
  `tools/uft-scout/out/a022_diskstack.gutachten.md`. **Der vorhergesagte
  Fund ist gemessen bestätigt.**
  · **Umfang:** MIT, Python, **85 Dateien, 13 036 Zeilen**, letzter Push
    2026-09-12. Port rechtlich offen, praktisch eine **Neuschreibung** —
    dieselbe Lage wie `mkmgt` (Forth).
  · **Der Fund, im Quelltext statt in der Beschreibung:**
    `report.py:99` erzeugt „Greaseweazle `--tracks=` specs covering every
    unresolved sector", `:119` ganze `gw read`-Befehlszeilen, `:76` den
    Satz „`{still_bad}` sectors are still bad. Another pass at …".
    **Dagegen gemessen:** `src/core/uft_loss_report.c` sind **123 Zeilen**
    mit fünf Funktionen, die **JSON schreiben** — kein nächster Schritt,
    keine Spurliste, kein Befehl. Und `--tracks=` steht im ganzen Baum
    **nur** in `fluxengine_provider_v2.cpp`, dort als **Argumentbau** für
    einen Lesevorgang, nicht als Vorschlag an einen Bediener.
  · **Der Unterschied ist gedanklich:** UFTs Bericht sagt, was verloren
    ist; `diskstack`s Bericht sagt, **was als Nächstes zu tun ist** — und
    das ist genau die Gestalt, die `P3-387` sucht („ein Fehlerprotokoll,
    das den Lauf überlebt"). Ein Protokoll, das den nächsten Befehl trägt,
    überlebt ihn notwendigerweise.
  · **Die Abstimmung selbst ist KEIN Zugewinn** — `stack.py` und
    `candidates.py` haben in `src/recovery/uft_multiread_pipeline.c`
    (MF-473) ihre Entsprechung, und `P3-88` hat dort bereits gemessen,
    dass Voting einen Sektor erfinden konnte.
  · **Orakel-Kanal: zum ersten Mal in dieser Reihe fast offen.** Gemessen
    je Abhängigkeit: `click` **vorhanden**, `crcmod` **vorhanden**,
    `bitarray` **fehlt**. Nach fünf vollständig verschlossenen
    Werkzeugketten (Swift, Rust, Forth, mkfs.fat/mtools, Borland) fehlt
    hier **ein Paket**. **Nicht installiert** — eine Paketinstallation
    verändert die Umgebung des Eigentümers und ist dessen Entscheidung.
  · **HAL-Frage beantwortet: nein.** `diskstack` liest Dateien und
    **druckt** den `gw read`-Befehl, statt ihn auszuführen. Genau diese
    Trennung ist übertragbar, ohne die HAL zu berühren — der Bericht muss
    nicht lesen können, um zu sagen, was zu lesen wäre.
- **Beleg:** `3bad0e48` (MF-1210) (nachgetragen 2026-09-26 beim Aufraeumen: der Commit, der das Gutachten in den Baum gebracht hat, gemessen mit `git log --diff-filter=A`) · Gutachten `tools/uft-scout/out/a022_diskstack.gutachten.md`;
  Befund folgt als Fortschreibung an `P3-387`.

### A-024 · AUFTRAG Audit-Umfang: komplettes Repository statt CMake-Liste (Mengen A/B/C)
- **Status:** **erledigt** 2026-09-17 — Mengen **abgeleitet** statt
  gepflegt (`scripts/audit_bauliste_umfang.py`, Selbsttest 12/12),
  Menge C gemessen **fünf** Dateien (nicht acht — die Zahl fiel
  dreimal, `P3-470`), jede mit einer der sechs Dispositionen, **kein
  DELETE**, und die eine Eigentümerfrage ist entschieden und ausgeführt
  („verdrahten" → MF-1216) · MF-1211, MF-1215, MF-1216 ·
  **Aufgenommen:** 2026-09-16
- **Wortlaut:** „Nur die Dateien aus den CMake-Listen reichen nicht. …
  Gerade die nicht in CMake stehenden Dateien sind beim UFT wichtig. Dort
  können fertige, aber unerreichbare Parser, alte Versionen, Stubs oder
  nicht verdrahtete Algorithmen liegen. … Danach wird in drei Mengen
  getrennt: A: Von CMake gebauter Produktcode / B: Nur in Tests, Tools
  oder Sonderkonfigurationen gebauter Code / C: Im Repository vorhandener,
  aber nicht erreichbarer Code … Menge C wird nicht automatisch
  repariert, sondern zuerst klassifiziert: REGISTER / MERGE / REFERENCE /
  PARTIAL / BLOCKED / DELETE … **Keine Datei allein deshalb löschen, weil
  sie nicht in CMake steht.** … Am Ende sollten drei getrennte Änderungen
  entstehen"
  <br>Ein Satz im Auftrag steht ohne Bezug und wird **zitiert, nicht
  ausgelegt**: „Damit würdest du das Repository ziemlich sicher
  beschädigen."
- **Kennzahl:** **keine der vier** — es ist die Verlässlichkeit des
  Prüfstands, wie `P3-421` und `P3-426`.
- **Kanal:** **entfällt** (eigener Baum, eigene Methode).
- **Einfrier-Regel:** **nein** für das Klassifizieren. **Ja**, sobald aus
  einem REGISTER ein registriertes Plugin würde — dann greift das
  Moratorium (1 neues Format = 2 Hebungen).
- **OPEN_ITEMS:** `P3-379` (Waisenrolle: Register statt Sweep) ·
  `P3-421` (`repo_scope` meldete einmal „git ls-files nicht verfügbar"
  und prüfte daraufhin den ganzen Verzeichnisbaum) · `P3-426` (zwölf
  GLOBs im Prüfstand)
- **Fertig heißt:** die drei Mengen **abgeleitet** erzeugt (nicht
  gepflegt), jede Datei der Menge C mit **einer** der sechs Dispositionen
  belegt, und drei getrennte Commits — wobei jede Löschung die
  `Ruecknahme:`-Zeile trägt, die der `commit-msg`-Haken erzwingt.
- **Aufwand:** **nicht schätzbar.**
- **Fünf Messungen, weil vier Teile der Vorgabe hier anders liegen:**
  · „nicht nur CMake, sondern `git ls-files`" ist **schon Grundsatz** —
    MF-636 mit `scripts/repo_scope.py` und **vier** belegten Vorfällen
    veralteter Aufzählungen (MF-567, MF-578, MF-598, MF-633).
  · `git submodule update --init --recursive` ist hier ein Leerlauf:
    **`.gitmodules` existiert nicht**.
  · `git lfs pull` ebenso: **0** `filter=lfs`-Zeilen in `.gitattributes`.
  · „~155 Einträge unter `src/formats/`" — gemessen sind es **354** `.c`
    und **11** `.h` (von **2889** versionierten Dateien im Baum). Woher
    die 155 stammt, ist **offen**; keine der beiden Zahlen wird benutzt,
    bevor das geklärt ist.
  · Menge C ist **teils schon da**: `docs/orphan_baseline.txt` (328
    Zeilen), `scripts/audit_orphan_modules.py`, die Waisenrolle. **Neu**
    sind die sechs Dispositionen und die drei getrennten Commits.
- **Stand (MF-1211): der erste, tragende Schritt ist getan — die drei
  Mengen sind ABGELEITET, und die naive Zahl ist dabei gefallen.**
  · **Roh gemessen** über `git ls-files` (2902 versionierte Dateien,
    **1301** Übersetzungseinheiten `.c`/`.cpp`), Nennung des Basisnamens in
    irgendeiner `CMakeLists.txt`/`.cmake`/`.pro`/`.pri`:
    **605** Übersetzungseinheiten stehen in **keiner** Bauliste.
  · **Diese 605 sind aber KEINE Menge C, und das ist der eigentliche
    Befund dieses Schrittes.** Zwei Klassen fallen heraus, beide belegt:
    **(a) 465 sind Tests**, die CMake per **GLOB** findet —
    `tests/CMakeLists.txt:46` `file(GLOB TEST_SOURCES_C "test_*.c")` und
    `:48` für `.cpp`; sie sind verdrahtet, nur nicht *genannt*.
    **(b) 132 sind Fremdcode und Werkzeuge** (`src/samdisk/`,
    `src/a8rawconv/`, `tools/`).
  · **Die echte Menge C sind ACHT Dateien**, alle klein:
    | Zeilen | Datei |
    |---|---|
    | 66 | `audit/applesauce/test_applesauce_vectors.c` |
    | 57 | `audit/fc5025/test_fc5025_vectors.c` |
    | 78 | `audit/greaseweazle/test_greaseweazle_vectors.c` |
    | 94 | `audit/scp/test_scp_vectors.c` |
    | 51 | `audit/usbfloppy/test_usbfloppy_vectors.c` |
    | 63 | `audit/xum1541/test_xum1541_vectors.c` |
    | 146 | `cli/uft-decode/main.c` |
    | 186 | `src/gui/UftParameterIntegration_example.cpp` |
  · **Mengen A und B, gezählt:** A (in CMake genannt) **455**, B (in `.pro`
    genannt) **796**; Schnitt **368**, nur A **87**, nur B **428**. Die
    428 „nur B" sind der qmake-Produktivbau, den CMake nicht baut — das
    ist Absicht, kein Befund.
  · **Symbolprüfung begonnen, Ergebnis so weit:** `parse_format_name`
    (`cli/uft-decode/main.c`) **0** fremde Dateien;
    `uft_audit_scp_vectors_ok` (`audit/scp/`) **0**. `main` und `usage`
    sind Allerweltsnamen und taugen nicht als Beleg.
  · **KEINE Disposition lautet DELETE**, und das ist kein Zögern:
    der Auftrag sagt „Keine Datei allein deshalb löschen, weil sie nicht in
    CMake steht. Löschen erst nach CMake-, Symbol-, Registry-, Test-,
    Plattform- und Laufzeitprüfung", und `MF-1077` verlangt für jede
    Löschung eine ausdrückliche Eigentümerentscheidung. Zwei der sechs
    Prüfungen sind gemacht (CMake, Symbol); vier stehen aus.
  · **Vorläufige Einordnung, damit die Wahl vorliegt:** die sechs
    `audit/*/test_*_vectors.c` sind **je Controller ein Prüfvektorsatz** —
    das ist Bestand, der zu den Emulatoren gehört (9 von 9 fertig), und
    gehört eher **verdrahtet als entfernt**. `cli/uft-decode/main.c` ist
    ein **CLI** und steht damit gegen die GUI-only-Regel — das ist der
    einzige echte Kandidat für eine Eigentümerentscheidung.
    `UftParameterIntegration_example.cpp` heißt „example" und ist
    vermutlich Vorlagencode.
  · **Offen bleiben:** die vier restlichen Prüfungen je Datei, die sechs
    Dispositionen als Urteil, und die drei getrennten Commits.
- **Und ein Punkt, der die empfohlene Vorgehensweise selbst betrifft:** ein
  frischer `git clone` von GitHub würde die laufende Arbeit **nicht**
  enthalten — gemessen liegen **9** Commits nur lokal (MF-1182 … MF-1190,
  C3). Ein Audit auf dem Klon liefe gegen einen Stand ohne Phase 1 und 2.
  Quelle ist deshalb der Arbeitsbaum, oder es wird vorher gepusht.
- **Stand (MF-1215/MF-1216): abgeschlossen — und die Zahl ist auf dem
  Weg DREIMAL gefallen, jedes Mal aus demselben Grund.**
  · **1107 → 8 → 12 → 6 → 5.** Jede dieser Zahlen scheiterte an einer
    **Aufzählung von Bauliste-Arten**: `1107` an den CMake-GLOBs
    (MF-1211), `8` an den **CI-Arbeitsablauf-GLOBs**
    (`.github/workflows/audit.yml:67`), `12` an absoluten Pfaden in
    meiner eigenen ersten Fassung des Ableiters, `6` daran, dass eine
    Schale in CI von der **Wurzel** läuft, und `5` daran, dass ein
    `conftest.py`, das selbst übersetzt, eine Bauliste ist.
    Der Befund steht als **`P3-470`**: Erreichbarkeit kommt hier aus
    mindestens **sechs** Arten von Ort.
  · **Die Mengen sind jetzt ABGELEITET** (`scripts/audit_bauliste_umfang.py`,
    Selbsttest **12/12**, Grundlinie Menge C = **5**, darf nur sinken).
    Seine tragende Zusage ist eine Absage: ein GLOB-Muster, das es nicht
    auflöst, lässt es **abbrechen** statt freizusprechen — und genau
    daran hat es seinen eigenen Pfad-Fehler gemeldet.
  · **Sieben Dateien haben Menge C verlassen, weil sie verdrahtet sind:**
    die sechs `audit/*/test_*_vectors.c` (CI-GLOB; `gcc -fsyntax-only`
    vollstreckt ihre `_Static_assert` — rotgeprüft: falsche Zusage rc 1)
    und `tests/differential/uft_flux_decode.c` (`conftest.py` baut es).
  · **Die fünf Dispositionen, je mit der Prüfung, die sie entschied:**
    | Datei | Disposition | entschieden durch |
    |---|---|---|
    | `cli/uft-decode/main.c` | **REGISTER** | Eigentümerentscheidung „verdrahten" → MF-1216 |
    | `src/gui/UftParameterIntegration_example.cpp` | REFERENCE | 0 Nennungen im Baum; zweites `class MainWindow` neben `src/mainwindow.h:27` |
    | `tests/external_audits/gw/test_encode_match.c` | REFERENCE | ersetzt durch `tests/test_gw_encoder.c` (sagt es selbst, `audit.yml:15` bestätigt) |
    | `tests/flux_gen/xcopy/gen_xcopy_fixtures.c` | REFERENCE | dokumentierter Handbau, `docs/nachbau/XCOPY_EMULATIONSSITZUNG.md:34,46` |
    | `tests/kalibrierung/sanitizer_kann_rot.c` | REFERENCE | `scripts/kalibriere_sanitizer.sh:15` übersetzt es; das Skript ruft niemand automatisch |
  · **KEIN DELETE** — alle sechs vom Auftrag verlangten Prüfungen
    (CMake, Symbol, Registry, Test, Plattform, Laufzeit) sind je Datei
    durchgeführt, und keine endet dort. Geliefert ist die **Auswahl**,
    nicht die Tat (`MF-1077`).
  · **Das Verdrahten hat zwei eigene Befunde gebracht**, beide hätte ein
    reiner Link-Erfolg verdeckt: das Binärprogramm **band, lief und
    konnte nichts** (`uft_register_all_formats()` fehlte → Exit 5 für
    jede Datei; jetzt 92 160 Byte byteidentisch mit dem
    atrcopy-Erzeugnis), und **`CMakeLists.txt:99` verlangt Qt
    unbedingt** — das Programm ist Qt-frei, CMake nicht, womit der
    Qt-freie Behälter als sein Existenzgrund versperrt bleibt
    (**`P3-471`**). Nebenbei fiel **`P3-472`** ab:
    `CONFIG+=kalman_pll` kann nicht bauen, weil
    `uft/algorithms/uft_kalman_pll.h` 0 Mal existiert.
  · **Was ausdrücklich NICHT getan ist:** das neue Tor hängt in keiner
    Kette, und `tests/CMakeLists.txt` ist nicht angefasst — beide Wege
    führen durch `scripts/check_consistency.py` bzw. jene Datei, und
    beide tragen die unversionierte Arbeit einer anderen Sitzung. Die
    Abhängigkeitsschließung der CLI ist deshalb **abgeleitet** (acht
    Verzeichnisse, 467 Einheiten, `-lz`) statt als **14.** Handliste
    abgeschrieben — `tests/CMakeLists.txt:205` nennt die 13. bereits
    „genau der Fehler, den dieser Baum viermal bezahlt hat".
  · **Eine Verfahrensbeobachtung, weil sie wiederkehrt:** der Auftrag
    verlangt drei getrennte Commits, der Vor-Commit-Haken prüft aber den
    **ganzen Arbeitsbaum** (`P3-463`). Zwei Versuche fielen —
    `[STAND.md stale]`, dann `[Erzeugte Doku eingecheckt]` (MF-1043) —,
    weil `docs/OPEN_ITEMS.md` und `docs/STAND.md` nicht in verschiedene
    Commits können. Die Reihenfolge wurde daraufhin gedreht.
- **Beleg:** `c2b9eddc` (MF-1215, 1/3 — Befund `P3-470`) ·
  `4ba7d7ba` (MF-1215, 2/3 — `scripts/audit_bauliste_umfang.py`,
  Selbsttest 12/12) · `aaf5c001` (MF-1216 — Verdrahtung,
  ctest `uft_decode_cli_wandelt_atr_nach_xfd` grün, Mutante rot) ·
  `a7f8e9fe` (MF-1215, 3/3 — Abschluss, `P3-471`, `P3-472`).

### A-030 · Formatfilter aus der Registry, Familienachse, Versand
- **Status:** **erledigt** — alle drei Punkte belegt
  · **Aufgenommen:** 2026-09-18 · **Abgeschlossen:** 2026-09-18

> **BERICHTIGT MF-1258 — die Statuszeile war veraltet, und sie hat
> die Ein-Schloss-Regel verletzt.** Hier stand bis heute: „**in
> Arbeit** — Punkt 1 fertig und abgenommen, Punkt 2 gemessen mit
> widerlegter Annahme, Punkt 3 **blockiert**". Gemessen nennt die
> Beleg-Zeile weiter unten **alle drei** Punkte mit Commit, und
> alle sechs genannten Hashes sind vorhanden und stehen auf
> `origin/main` (`git merge-base --is-ancestor`): `422ca4aa`
> (Punkt 3), `117808c7` + `0bafecfa` (Punkt 1), `61cb0d01` +
> `0da4b0a1` + `b8d2419a` (Punkt 2). Die Familienachse laeuft mit
> Selbsttest **18/18**, ihr Frische-Tor ist gruen.
>
> Das ist dieselbe Gestalt wie bei `A-031`, nur andersherum: dort
> stand ein wartender Posten unter „Erledigt", hier ein erledigter
> unter „in Arbeit". Eine Statuszeile ist eine Behauptung wie jede
> andere — sie gehoert gegen den Beleg gehalten, nicht
> fortgeschrieben.
- **Wortlaut:** „ja, sehr gut, mach 1. 2. & 3." — auf meinen Vorschlag
  „1. Formatfilter aus `plugin.extensions` erzeugen — 93 Formate
  erreichbar machen. 2. Familienachse als Feld, danach die
  Variantenkampagne neu zählen. 3. MF-1244 und MF-1243 gehen raus,
  sobald der Baum wieder committierbar ist." Ergänzt vom Eigentümer:
  „‚aus plugin.extensions erzeugen statt pflegen' ist K4 in Reinform:
  eine Quelle, ein abgeleiteter Verbraucher, kein Drift möglich."
- **Kennzahl:** keine der vier — **und das ist begründet**, nicht
  übergangen: der Eigentümer nennt es „Fähigkeit, nicht Buchhaltung".
  85 geprüfte Formate sind für den Bediener erst jetzt erreichbar. Eine
  fünfte Zahl („Formate, die die Oberfläche anbietet") wäre begründbar
  und wird nur benannt.
- **Kanal:** entfällt — eigener Baum.
- **Einfrier-Regel:** berührt die Formatschicht (`uft_format_plugin.c`)
  → **Rotbeweis zuerst**. Eingehalten mit einer Einschränkung, die
  unten steht.
- **OPEN_ITEMS:** `P3-495` (behoben), `P3-496`, `P3-497`, `P3-498`
  (neu benannt).
- **Fertig heißt:** kein Dateidialog des Baums hält mehr eine
  handgepflegte Endungsliste, und ein Test fällt, wenn eine Endung der
  Registry im Filter fehlt.
- **Aufwand:** nicht schätzbar für Punkt 2 und 3.
- **Stand Punkt 1 — FERTIG, MF-1245:**
  · **7 Listen → 1 abgeleitete.** Gemessen hielten sieben Dialoge je
    eine eigene Liste: 16, 14, 13, 8, 8, 7 und 6 Endungen, alle
    verschieden. Die Registry beansprucht **105**; **85** standen in
    KEINEM Dialog.
  · Neu: `uft_ext_naechste()` (die Trennregel an EINER Stelle — nötig,
    weil 14 Einträge mit Komma und 29 mit Semikolon trennen, obwohl der
    Header nur `;` zusagt) und `uft_format_endungen_sammeln()`, beide
    in `src/core/uft_format_plugin.c`. Die Qt-Umhüllung einmal in
    `src/uft_format_filter_qt.h`.
  · Rotbeweis **25 grün / 0 rot** über die GANZE Registry (137 Plugins,
    193 beanspruchte Endungen, 0 fehlen, 0 Dubletten);
    **Mutationsmatrix 4 von 4**. Vollbau 18021/18021 mit **0
    Compiler-Warnungen**, Vollprobe **515/515**.
  · **ZWEI eigene Fehler, beide sofort bezahlt:** (a) ich habe die
    Umsetzung VOR dem ersten Testlauf geschrieben — „bindet nicht" ist
    ein schwächerer Rotbeweis als „Zusage fällt", die Matrix holt das
    nach, ersetzt es aber nicht; (b) ich habe ein **Heredoc** benutzt,
    um ein Skript zu ändern, und es hat meine Escapes in echte
    Umbrüche verwandelt — die Falle aus §MF-1096, zwanzigmal bezahlt
    und von mir zum einundzwanzigsten Mal.
  · Die Gegenprobe fand zwei Nacharbeiten: `forensictab.cpp` hielt
    dieselbe Kette **dreimal** (ersetzt war nur die erste), und
    `kFileFilter` in `uft_compare_dialog.cpp` ist jetzt verwaist —
    bleibt nach MF-1077 stehen, mit Kommentar.
  · Unberührt: `src/mainwindow.cpp` (13 Endungen), Fremdarbeit.
- **Stand Punkt 2 — GEMESSEN, und die Annahme ist widerlegt:**
  · Der Eigentümer korrigierte die Körnung: „137 Einzeluntersuchungen
    sind die falsche Körnung. Varianten bündeln sich in Familien … Bei
    2 von 137 ist die Zahl also nicht der Fortschritt; die Frage ist,
    wie viele Familien die zwei schon abdecken."
  · **Die Achse gibt es nicht als Daten.** `UFT_FCLASS_*` ist eine
    TRÄGERklasse mit 5 Werten und deckt 19 von 137 Formaten (in einer
    Tafel, die `ATR` und `XFD` doppelt führt, `P3-498`). `PLATFORM_*`
    hat 67 Begriffe mit mindestens 8 Dublettenpaaren und steht in **8**
    von 137 Plugins. Verzeichnisse unter `src/formats/` sind **164**,
    also feiner als 137 — keine Familienachse.
  · Als Prosa gibt es sie: 8 Familien in `CLAUDE.md` — dieselbe Liste,
    die MF-1064 schon einmal berichtigen musste.
  · **Und die zwei Übergaben decken je EIN Format, keine Familie:**
    `hfe.uebergabe.md` (341 Zeilen) nennt `hfe` 28-mal und
    `scp`/`a2r`/`kryoflux`/`woz`/`nib`/`g64` **0**-mal;
    `dim_atari.uebergabe.md` nennt `dim` 31-mal und hat einen Abschnitt
    „**Abgrenzung** X68000" — sie grenzt ab, statt mitzunehmen. Der
    Agent arbeitet laut eigener Beschreibung „ein Format je Zyklus".
  · **Folgerung:** die Familienzählung verkürzt die Kampagne nicht,
    solange nicht die QUELLE mehrere Formate trägt. Das ist die
    messbare Frage, nicht die Zähleinheit — und der erste Schritt wäre
    die Achse als Feld, nicht ein drittes Format.
- **Stand Punkt 3 — BLOCKIERT, beide Wege gemessen zu:**
  · **Aus dem Arbeitsbaum:** die zwei neuen Fähigkeits-Tore melden 4
    Befunde an der laufenden WOZ/TD0-Arbeit der zweiten Sitzung. Nicht
    umgangen, kein `--warn-only`.
  · **Aus einem sauberen Auscheck:** `check_consistency.py` meldet dort
    **214** Befunde bei identischen 2942 verfolgten Dateien — `P3-496`.
  · **UND DAS WAR SCHON GEMESSEN. Der Umweg war vermeidbar.** Die
    Gedächtnisnotiz `stand_md_zuletzt_erzeugen.md` hält seit MF-1230
    (einem Tag vorher) BEIDES fest: dass der Vor-Versand-Haken stirbt,
    solange ein zweiter Arbeitsbaum registriert ist („also: vor jedem
    Push `git worktree list` prüfen"), und dass ein frischer Auscheck
    **214 Befunde** ergibt, weil `build-tests-ci` und die Scout-Klone
    fehlen. Ich habe die Notiz nicht gelesen, den Weg trotzdem genommen
    und danach in `P3-496` geschrieben, die Ursache sei ungemessen.
    Beides berichtigt. **Die Lehre ist nicht der Worktree, sondern die
    Reihenfolge: erst das eigene Gedächtnis befragen, dann messen.**
  · **Und dieser Weg hat Schaden angerichtet, der gehört gesagt:**
    meine `git worktree add`/`remove`-Kette hat `core.bare = true` in
    die Konfiguration des Hauptrepositoriums geschrieben. Danach
    verweigerte git jede Arbeitsbaum-Operation. Repariert mit
    `git config core.bare false`; alle 29 geänderten Dateien
    unversehrt, HEAD unverändert. **Der Worktree-Weg ist
    zurückgenommen und wird nicht wieder benutzt.**
  · Gefangen hat den Schaden nicht das Skript, sondern dass ich nach
    dem Versand `git status` und `git ls-remote` befragt habe statt dem
    Rückgabewert zu glauben.
- **Stand Punkt 3, FORTSETZUNG — `P3-496` zu Ende gemessen, und es ist
  eine Rücknahme (MF-1246):**
  · Auftrag: „P3-496 zu Ende messen", dann „erst den Fix, dann P3-496
    und die Notiz berichtigen".
  · **Die Ursache war der PFAD, nicht der Commit.** Acht Tore prüften
    `any(s in p.parts for s in SKIP_DIRS)`, und `p.parts` sind ALLE
    Pfadstücke — auch die oberhalb des Repositoriums. Mein
    Auftragsverzeichnis liegt unter `~/.claude/jobs/…`, und `.claude`
    steht in der Sperrliste. Jedes Tor übersprang jede Datei und
    meldete danach Massenbefunde gegen den Baum.
  · **Messkette**, derselbe Commit `422ca4aa`, dieselben Prüfer:
    worktree unter `.claude` **214** · `git archive` unter `.claude`
    **191** · dasselbe Archiv unter `AppData/Local/Temp` **0** ·
    **echter `git clone` dorthin: rc=0, null Befunde in JEDER
    Kategorie**.
  · **Zwei eigene Erklärungen sind damit widerlegt:** „die 20259
    gitignorierten Fremdklone" (geprüft, die Symbole stehen nicht
    darin) und „die Ursache ist nicht gemessen" (sie war es, nur
    falsch). Und die Zahl 214 stand seit MF-1230 in meiner
    Gedächtnisnotiz **mit der falschen Ursache** — eine Zahl ohne
    geprüfte Ursache ist eine halbe Messung, und sie hat einen ganzen
    Tag in die falsche Richtung gelenkt.
  · **Fix MF-1246:** die Regel liegt einmal in
    `scripts/repo_scope.py::uebersprungen(repo, pfad, skip)` und
    rechnet relativ zur Repo-Wurzel; acht Tore, neun Aufrufstellen
    umgestellt; die unverankerte Fassung steht in keiner Codezeile
    mehr (gemessen). Die MENGE der Sperrnamen bleibt bei jedem Tor,
    weil sie sich begründet unterscheidet.
  · **Rotbeweis** `python scripts/repo_scope.py --selbsttest` **6/6** —
    seine erste Zusage führt die ALTE Regel wörtlich vor und verlangt,
    dass sie an derselben Stelle falsch liegt; drei Gegenproben halten
    fest, dass „nicht übersprungen" nicht einfach immer gilt.
    **Nachweis am Objekt:** derselbe Klon unter `.claude` geht von 191
    auf **0**, der Arbeitsbaum bleibt bei 0.
  · **Mein Heuristik-Fehler im Umstellskript** ist erwähnenswert, weil
    er richtig ausgegangen ist: die Einfügestelle des Imports war an
    „kein `def ` davor" geknüpft, und drei Tore haben `def ` im
    Kopfkommentar. Sie fielen durch — und wurden dabei **nicht
    geschrieben**, blieben also unversehrt. Berichtigt auf „die erste
    Zeile, die eine Definition oder Konstante beginnt".
  · Neu benannt: **`P3-499`** — `SKIP_DIRS` enthält `build`, trifft
    aber nur ein Verzeichnis dieses Namens; sieben weitere
    Bauverzeichnisse mit über 128 000 erzeugten Dateien werden
    mitgelesen, und die Grundlinien hängen daran.
- **Beleg:** **Punkt 1 und Punkt 3 erledigt, 2026-09-18.**
  · `422ca4aa` **MF-1243** — K4-Vorzeigefund als Fehlalarm zurückgenommen
  · `117808c7` **MF-1246** — verankerte Sperrregel, 8 Tore / 9 Aufrufstellen,
    Selbsttest 6/6, Klon unter `.claude` von 191 auf 0
  · `0bafecfa` **MF-1245** — Dateifilter aus der Registry, 10 Dateien,
    +553/−25, Mutationsmatrix 4/4, ctest 516/516
  · **Versand belegt, nicht behauptet:** `6d3a62f3..0bafecfa main -> main`,
    und alle drei Quellen nennen denselben Wert — `git rev-parse HEAD`,
    `git rev-parse origin/main` und `git ls-remote` gegen den Server.
  · **Der erste Versuch wurde dabei ABGEWIESEN, und nur die zweite
    Aufzeichnung hat es gezeigt:** `git push` meldete 1, die äußere
    Hülle meldete `exited with code 0` (Pipe durch `tail`), und die
    drei Werte wichen ab. Ursache war meine: das Beiseitelegen der
    Bauartefakte stand in den Commit-Skripten und fehlte im
    Versandskript — der `pre-push`-Haken urteilt über den ARBEITSBAUM
    (Klasse `tor_misst_arbeitsbaum_nicht_commit`), und `ctest` erzeugt
    die 17 `ui_*.h` über AUTOUIC neu, also gehört die Probe VOR das
    Wegräumen.
  · **Punkt 2 erledigt — MF-1248, und die Achse hat beim ersten Lauf
    zwei Fehler gefunden, davon einen von mir.** Bauabsicht vorgelegt,
    freigegeben mit „ja, bau es". `scripts/gen_familien.py` leitet die
    Familien aus `docs/erzeuger_kanaele.json` ab statt sie zu pflegen;
    Selbsttest **12/12** mit drei Rot-Proben, Frische-Tor in
    `check_consistency.py` (feuert auf eine gekippte Ziffer, schweigt
    bei Gleichstand).
    · **Die Antwort auf deine Korrektur:** 14 Formate mit gemessenem
      Erzeuger verteilen sich auf **5** Quellen — `hxcfe` 6,
      `dsktrans` 5, dazu je eine.
    · **BERICHTIGT MF-1249, und die Berichtigung kam von CI.** Hier
      stand: „Die Variantenkampagne berührt **2 von 5** Quellen und
      damit **11 von 14** Formaten. Nach Plugins gezählt sind es 9 von
      137; nach Quellen gezählt sind vier Fünftel der belegten Fläche
      angefasst." Das beschrieb meinen **Arbeitsbaum**. Gemessen im
      Commit: **1** Variantentafel, **0** mit gemessenem Erzeuger,
      **0 von 5** Quellen. Acht der neun Tafeln sind die uncommittete
      Arbeit der zweiten Sitzung — die Zahl wird mit deren Commit zu
      2 von 5, heute ist sie null. Die Aussage „so gezählt ist die
      Kampagne kürzer" bleibt richtig; nur steht sie noch am Anfang.
    · **Mein Fehler, von der Messung gefangen:** der erste Abgleich
      lief über Namensanfänge und zählte `adf` gegen `adf_ext` — zwei
      verschiedene Plugins (MF-1222 nennt genau diese Verwechslung).
      Aus 3 Treffern wurden richtig **2**. Die Zuordnung ist jetzt
      zeichengleich, Beinahe-Treffer werden AUSGEGEBEN statt
      verschwiegen.
    · **Der zweite Fehler war im Zensus, und er ist ableitbar:**
      `kanal: "keiner"` steht 16 von 30 Mal, und **8** dieser Formate
      stehen auf T1/T1b — einer Stufe, die eine fremde Hand VERLANGT.
      Der Zensus probierte `hxcfe`/`floptool`/`SAMdisk`, gehoben wurde
      mit anderen Werkzeugen. „Es gibt keinen" war eine Aussage über
      den probierten Aufruf. Benannt als `P3-502`.
  · **Zwei abgewiesene Läufe an derselben Stelle, und erst der zweite
    hat die richtige Frage gestellt (MF-1249, MF-1250).** CI fiel mit
    `FORMAT_FAMILIEN.md stale`, weil die Tafel den **Arbeitsbaum**
    zählte. MF-1249 stellte auf den **Index** um — und der nächste
    Push fiel ebenfalls, weil die zweite Sitzung laufend Dateien
    einlegt. Gemessen im selben Augenblick: **Arbeitsbaum 9, Index 9,
    HEAD 1.**
    · Der Fehler war nicht die Wahl der Sicht, sondern dass ein
      **committetes Dokument** überhaupt an Quelltext hing, den ein
      Dritter zwischen Erzeugen und Prüfen bewegt. Seit MF-1250 liest
      das Dokument nur noch committete Dokumente (Zensus, Stufentafel);
      der Kampagnen-Fortschritt ist eine **Abfrage**
      (`gen_familien.py --fortschritt`, misst gegen HEAD und nennt den
      SHA). Selbsttest **18/18**, vier davon halten mechanisch fest,
      dass die Fortschrittsbegriffe nicht ins Dokument zurückkehren.
    · **Am Verfahren geändert:** meine Commit-Skripte machten
      `git reset -q` und warfen damit die **eingelegten** Dateien der
      zweiten Sitzung aus dem Index (`A src/core/uft_copy_plan.c`,
      `A tests/test_varianten_aus_dem_plugin.c`). Ab MF-1250 wird
      `git commit -- <pfade>` benutzt: es committet genau die genannten
      Pfade und lässt den Index unberührt.
  · **MF-1247** (Teilstring-Falle in zwei Fähigkeits-Toren) liegt
    fertig im Arbeitsbaum und wartet nach der Ordnungsregel auf den
    Tor-Commit der zweiten Sitzung.

### A-031 · Vier Dinge: Sondendoktrin, eine Größenrechnung, Bauabbruch, Körnung
- **Status:** **erledigt** (alle vier Punkte, CI grün) · **Aufgenommen:** 2026-09-19 · **Abgeschlossen:** 2026-09-19
- **Wortlaut:** „Vier Dinge, in dieser Reihenfolge — und das vierte
  macht aus zwei offenen Aufgaben eine. **1.** Die Sonde muss ihrer
  eigenen Doktrin gehorchen … bei Gleichstand gibt die Sonde keinen
  Sieger zurück, sondern die Kandidatenliste, und der Öffnungspfad
  verlangt dann `force_format` … Die Warnung darf nicht in einem
  Protokoll stehen, das niemand liest. Sie ist die Auswahl. **2.** Eine
  Zahl, eine Stelle … Welche der beiden Zahlen stimmt, ist S5 … Also
  darf keine der beiden gewinnen … Raten war der Fehler; ihn zu
  beheben heißt nicht, richtig zu raten. **3.** `pad[1024]` wird ein
  Bauabbruch, kein Zufall … wobei `UFT_GEOM_TABLE_MAX_SECTOR` aus der
  Tafel erzeugt wird, nicht daneben gepflegt. **4.** Die Körnung
  entscheidet, was „Stufe" überhaupt bedeutet … Was nicht passieren
  darf: den 38 eine Stufe geben, weil die Spalte leer aussieht. Die
  leere Spalte war ehrlicher als jede Zahl, die dort stünde."
  · Nachtrag desselben Tages: „die Endung als engeren Anspruch nehmen,
  dann weiter mit 2 und 3"
- **Kennzahl:** keine der vier unmittelbar; Punkt 4 berührt
  **ungeprüfte Formate (T3)** und wird sie voraussichtlich **erhöhen**,
  weil 49 Plugins heute gar nicht gemessen sind
- **Kanal:** entfällt (eigener Baum)
- **Einfrier-Regel:** berührt den Format-/Decoder-Layer — **ja**, also
  Rotbeweis zuerst. Eingehalten: der Rotbeweis stand vor der Umsetzung
  und fiel 7/9
- **OPEN_ITEMS:** `P3-503`…`P3-505` (neu, siehe unten)
- **Fertig heißt:** (1) die Sonde gibt bei Gleichstand keinen Sieger
  und der Öffnungspfad hat einen benannten Ausweg; (2) beide Pfade
  rufen EINE Größenfunktion, und `DSK_RC`/`DSK_HP` öffnen nur mit
  Zwang; (3) der Bau bricht, wenn eine Geometriezeile den Puffer
  übersteigt, und die Konstante ist erzeugt; (4) die Stufentafel führt
  Parser, eindeutige Zeilen und Gleichstände getrennt
- **Stand Punkt 1 — ERLEDIGT, und die Messung hat drei eigene
  Fehlannahmen umgeworfen:**
  · **Rotbeweis zuerst**, `tests/test_sonde_sagt_ab_bei_gleichstand.c`:
    vor der Änderung **7/9**, danach **12/12**.
  · **Fehlannahme 1:** ich wählte 327 680 Byte, weil die
    Geometrietafel dort neun Zeilen führt. Gemessen gewinnt dort `TRD`
    mit 45 gegen die DSK-Zeilen mit 40 — **kein** Gleichstand. Die
    Tafel sagt, was mehrdeutig IST; die Registry sagt, was daraus
    FOLGT.
  · **Fehlannahme 2:** meine Gegenprobe suchte „nur EIN Beansprucher".
    Das gibt es in dieser Registry nicht — selbst `Micropolis` mit 70
    hat fünf Mitbewerber unter sich. Entscheidend ist `tied == 1`.
  · **Fehlannahme 3:** „38 von 49 sind nicht unterscheidbar" war eine
    Aussage über die **Tafel**. An der Registry gemessen sind **10 von
    20** Größen durch Registrierungsreihenfolge entschieden, und die
    Gleichstände laufen quer durch die Systeme: 204 800 an `IMG`,
    während `DSK_ACE`/`DSK_EIN`/`DSK_LYN` gleich berechtigt sind;
    92 160 an `XFD`, während TRS-80 `JVC` daneben steht.
  · **Umgesetzt:** die Leiter liegt an EINER Stelle,
    `uft_probe_file_entschieden()` — höchste Konfidenz → bei
    Gleichstand die **Endung** → sonst NULL. Dazu `uft_disk_open_as()`
    (Zwang) und `uft_disk_open_ranked()` (Absage **plus**
    Kandidatenliste). `uft_disk_open()` behält Signatur, damit die 96
    Aufrufstellen übersetzbar bleiben und ein Test, der ein
    mehrdeutiges Abbild öffnet, **sichtbar** fällt.
  · **Die Endung wird nie zum Beleg.** Sie verengt nur eine Menge, die
    die Sonden bereits gleichrangig beansprucht haben; bleibt mehr als
    einer übrig, gilt weiter „keiner". Die Messung weist `tied > 1`
    weiterhin aus — wer ein Plugin UND `tied > 1` sieht, weiß, dass
    der Name verengt hat und nicht die Evidenz. `uft_probe_ranking_t`
    ist dabei unangetastet, kein Layout verschiebt sich.
  · **Die Folge, gemessen:** die blosse Absage machte **18 von 129**
    Korpus-Abbildern unerreichbar, darunter eine von VICE erzeugte
    `.d81`. Mit der Endungsverengung bleiben **10**, und sie sind
    ehrlich benannt: `gw_po.img`, `gw_sam.img`, `gw_ssd.img`,
    `gw_trd.img` heißen alle `.img`, `hxcfe_pc160.dsk` und
    `samdisk_edsk.dsk` beide `.dsk`. Was ihren Gleichstand bricht, ist
    **Inhalt** — genau Punkt 4.
  · **Acht Abbilder gehen nicht mehr an ein fremdes Plugin.** Die
    Bilanz von `test_oeffentliche_api_am_korpus` bewegt sich von
    72/52/20 auf **72/60/12**; die Zahl ist die FOLGE der Messung und
    nicht ihr Ziel (MF-1077).
  · **Fünf benannte Rennen sind überholt** (die Sonde sagt ab, statt
    `edk` an D81, `edsk` an DSK **bei Konfidenz 95**, `jv1`/`tan` an
    IMG, `po` an DO zu geben), **eines gewonnen** (`adl` durch `.adl`),
    **eines neu besetzt**: `pdp` geht an `HardSector` statt
    `DSK_X820`, weil die Korpusdatei `gw_pdp.img` heißt. Die alte
    Fußnote „Geometrie identisch, folgenlos" ist dabei **nicht
    übernommen, sondern am neuen Sieger nachgemessen** —
    `uft_hardsector.c:543` führt `77 x 1 x 26 x 128 = 256 256`.
  · **Volle Probe: 517/517**, Bau rc=0.
- **Stand Punkt 2 — ERLEDIGT (MF-1254), und die Behebung hat einen
  Nebenbefund erzeugt, den ich festhalte statt ihn zu übergehen:**
  · **Rotbeweis zuerst**, `tests/test_dsk_groesse_eine_rechnung.c`:
    **12/14** vor der Änderung, **14/14** danach.
  · Der Widerspruch, durch die öffentliche API gemessen: „DSK_RC
    oeffnet 634880 Byte und erklaert davon 630784 (Differenz 4096)".
    `DSK_RC` und `DSK_HP` tragen beide `{77, 2, 16, 256, 634880}` —
    gerechnet 630 784, angegeben 634 880, genau eine Spur Unterschied.
  · **Eine Rechnung, beide Pfade:** `dsk_gen_groesse(idx)` gibt beide
    Werte **und** das Kennzeichen `unentschieden` zurück. Die Sonde
    beansprucht eine unentschiedene Zeile **nicht**; `open` — dann nur
    über `uft_disk_open_as()` erreichbar — nimmt die **kleinere**
    Geometrie und nennt den Befund über den vorhandenen
    `[UFT] note:`-Kanal, mit beiden Zahlen und dem Satz, dass keine
    belegt ist.
  · **Keine der beiden Zahlen wurde geändert.** Es gibt keine Quelle
    für RC702/Piccoline oder HP LIF im Baum — `S5`. Benannt als
    `P3-506`.
  · **Nebenbefund:** als die zwei ihren Anspruch aufgaben, verlor
    634 880 Byte seinen Gleichstand (vorher 3 Bewerber), und `IMG`
    gewinnt dort jetzt **allein**. Eine Zusage, die vorher grün war,
    kippte dadurch — und die Lehre steht im Test: **wenn Bewerber
    wegfallen, kann aus einem gemeldeten Gleichstand ein stiller
    Alleingewinner werden.**
  · Volle Probe **518/518**, Bau rc=0.
- **Stand Punkt 3 — ERLEDIGT (MF-1255):**
  · `scripts/generators/gen_dsk_geom_max.py` erzeugt
    `UFT_GEOM_TABLE_MAX_SECTOR` **aus** der Tafel (heute `1024u`,
    getragen von `DSK_KC` und `DSK_RLD`); Selbsttest **4/4** mit einer
    Rot-Probe, die eine 2048er-Zeile einträgt und verlangt, dass die
    Konstante steigt, und einer Gegenprobe, dass eine Tafel **daneben**
    nicht mitgelesen wird.
  · `_Static_assert(sizeof(pad) >= UFT_GEOM_TABLE_MAX_SECTOR, …)` in
    `dsk_gen_write_track()`. **Rot-Probe am Objekt:** mit `2048u`
    **bricht der Bau**, danach wiederhergestellt.
  · Frische-Tor `check_dsk_geom_max_fresh()`; gemessen feuert es auf
    eine gekippte Zahl und schweigt bei Gleichstand. Ohne das wäre die
    Konstante eine Zahl, die beim Erzeugen einmal gestimmt hat.
- **Stand Punkt 4 — ERLEDIGT (MF-1256), und die Messung hat eine
  vierte eigene Fehlannahme umgeworfen — diesmal meine Zeile, nicht
  die leere Spalte:**
  · **Die Körnung steht jetzt im erzeugten Dokument**, in der
    Gliederung des Auftrags: der Parser `dsk_generic` **1** · Zeilen
    mit eigener Größe in der Tafel **11** · Zeilen im Gleichstand
    **38 — „T3 — Größe allein, mehrdeutig"**. Die Zahlen sind
    abgeleitet (`dsk_koernung()` über `gen_dsk_geom_max.tafel()`),
    nicht getippt; die Summe geht auf und wird im Selbsttest geprüft
    (11 + 38 = 49).
  · **Die Summe „88" trägt einen Hinweis**, dass sie Plugins mit
    eigener Beweislage zählt und nicht alle 137 — vorher las sie sich
    wie die Gesamtzahl.
  · **Fehlannahme 4, und sie war der Spiegelfehler zum verbotenen:**
    die Zeile der 11 stand zuerst mit „je Zeile möglich" da — also
    einer Zusage. An der laufenden Registry gemessen
    (`tests/test_sonde_sagt_ab_bei_gleichstand.c`, neu 16/16 statt
    12/12) stehen von den 11 nur **3** oben (`DSK_VIC`, `DSK_CRO`,
    `DSK_RLD`); **5** liegen im Gleichstand mit einem Plugin außerhalb
    der Tafel, und **3 werden überboten** — `DSK_NS` gegen
    `NorthStar` (65 : 40), `DSK_NAS` gegen `Micropolis` (70),
    `DSK_MZ` gegen `TRD`/`ADL` (45). „In der Tafel eindeutig" ist
    **nicht** „erreichbar", und die Zelle sagt das jetzt.
  · **Rot-Probe (Mutation):** „Tafel = Registry" lässt genau die drei
    neuen Messzusagen fallen (13/16, rc 1); die Invarianten-Zusage
    „die drei Klassen sind erschöpfend" bleibt richtig grün — der Test
    zeigt damit, welche Zusage welche Behauptung trägt.
  · **Rot-Probe (Frische-Tor):** 38 auf 37 gekippt → Tor feuert und
    `--check` endet 1; zurückerzeugt → Tor schweigt, Datei byteweise
    wie vorher.
  · **Anti-Tautologie, gemessen statt behauptet:** hätte ich einen der
    beiden `render_md(`-Aufrufer beim alten einstelligen Aufruf
    gelassen, verglichen 377 Zeilen gegen 436 geschriebene — das Tor
    hätte **dauerhaft** gefeuert. Beide Aufrufer sind umgestellt.
  · **Der Generator hatte keinen Selbsttest und hat jetzt einen
    (9/9).** Das Frische-Tor kann ihn nicht ersetzen: es prüft
    Gleichschritt zwischen Generator und Dokument, nicht Richtigkeit —
    ein falscher Generator schriebe seinen Fehler beim nächsten
    `--write` ins Dokument, und das Tor schwiege weiter.
  · **Die S5-Frage aus Punkt 2 leckt nicht in die Stufenaussage.**
    Gruppiert man nach der Geometrie statt nach der Tafelzahl, kommen
    **dieselben** Mengen heraus — 11 allein, 38 im Gleichstand,
    namensgleich. Das steht als abgeleitete Fahne `s5_robust` im
    Dokument, mit einer Gegenprobe im Selbsttest, dass sie falsch
    werden **kann**.
  · **Keine Zahl im erzeugten Dokument, die es nicht ableiten kann:**
    die registryweite Dreiteilung steht ausdrücklich **nicht** dort,
    sondern im Test — eine von Hand nachgezogene Zahl in einem
    erzeugten Dokument wäre genau die Drift aus MF-541.
  · `docs/OPEN_ITEMS.md` `P3-503` um den Befund erweitert.
- **Beleg:**
  | Punkt | MF | Commit |
  |---|---|---|
  | 1 · Sondendoktrin, Gleichstand, Endung | MF-1251/MF-1252 | `7abf0499` |
  | 2 · eine Größenrechnung für beide Pfade | MF-1254 | `e3d038f8` |
  | 3 · `pad[1024]` wird ein Bauabbruch | MF-1255 | `e3d038f8` |
  | 4 · die Körnung der Stufentafel | MF-1256 | `56e9716f` |

  Vorbedingung im selben Zug: **MF-1253** (`3640212b`) — der
  Flut-Wächter musste allein und zuerst committet werden, bevor er
  urteilen durfte, und sein Commit musste das eigene Tor passieren.

  **Versand belegt, nicht behauptet:** `b8d2419a..56e9716f`, und die
  zweite Aufzeichnung nennt dreimal denselben Wert — `git rev-parse
  HEAD`, `git rev-parse origin/main` und `git ls-remote` beim Server.
  **CI grün** auf `56e9716f`: alle sechs Abläufe (CI-Matrix mit
  Linux c++17, Linux c++20, macOS, **build-windows**, Consistency
  check, gw parity · Sanitizer · Coverage · Audit · Emulator-CI ·
  Teilstring-Fallen).

  **Was die Aufgabe NICHT erledigt hat, und das gehört hierher:**
  · **`P3-504`** — die Gleichstand-Warnung erreicht weiterhin keinen
    Bediener. Der Eigentümer hat den Dialog ausdrücklich verlangt
    („Ein Dialog mit den acht Kandidaten … ist keine Belästigung,
    sondern die Wahrheit über die Datei"); gebaut ist der **Baustein**
    dafür (`uft_disk_open_ranked()` füllt `tied_with[]`), nicht der
    Dialog.
  · **`P3-503`** — was den Gleichstand der 38 bricht, ist Inhalt. Der
    Eigentümer hat das mit der Variantenkampagne zusammengelegt; die
    Arbeit steht aus.
  · **`P3-506`** — welche der beiden DSK-Größen stimmt, ist weiter
    offen (S5). Keine wurde geändert.


## Zurückgenommen

*(mit Grund. Eine Liste, die durch Schrumpfen produktiv aussieht, ist
der Spiegelfehler aus MF-1077.)*
