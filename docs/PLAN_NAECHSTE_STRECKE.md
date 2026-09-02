# Die nächste Strecke — ein Plan mit gemessener Wirbelsäule

**Stand 2026-09-02, zweite Fassung (MF-803).** Jede Zahl hier ist
gemessen, nicht geschätzt; wo eine fehlt, steht das da.

> **Warum eine zweite Fassung.** Die erste (MF-786) ist in einer einzigen
> Sitzung überholt worden: drei ihrer fünf Schritte sind erledigt, einer
> ist **gesperrt**, und ihre Leitannahme zu den Containern war falsch.
> Ein Plan, den eine Messung widerlegt, wird geändert, nicht verteidigt
> (Konfliktordnung, Vorrang 1). Der alte Text hätte die nächste Sitzung
> an Schritt 3 geführt — und dort ist eine Wand.

---

## Der eine Satz

> **Das Moratorium endet nicht, und der Grund ist gemessen: es fehlt eine
> Hand, nicht ein Werkzeug.**

`MF-363/498` hebt die Einfrier-Regel auf, wenn **ATR, D64, ADF, FDI und
NFD-r0** auf T1/T1b stehen. Vier davon stehen:

| Format | Stufe | |
|---|---|---|
| `fdi` | **T1** | ✔ |
| `adf` | **T1b** | ✔ |
| `atr` | **T1b** | ✔ |
| `d64` | **T1b** | ✔ |
| **`nfd`** | **T2** | ✘ — gesperrt, siehe B1 |

Der Unterschied zur ersten Fassung: dort stand „ein Format entfernt",
als wäre es eine Frage von Arbeit. Es ist eine Frage von **Beschaffung**,
und das ändert die Reihenfolge von allem darunter.

---

## Wo wir stehen, in vier Zahlen

| Kennzahl | Stand | Bewegung seit MF-786 |
|---|---|---|
| **ungeprüfte Formate (T3)** | **37** von 88 | 39 → 37 (`edsk` gehoben, plus eine Zahlenkorrektur, s. u.) |
| angebotene Wandlungspfade | 16 Matrix-Einträge, 6 verlustfrei | unverändert |
| leckende Tests | 0 | gehalten |
| Bench-Alter je Controller | kein Gerät (MF-310) | unverändert |

Dazu, nicht als Kennzahl geführt, aber tragend: **Oracles 4 von 15
verfügbar** — `gw` 1.23, `samdisk` 4.0, `hxcfe` 2.16.13, `mformat`
(mtools 4.0.49).

> **Die Zahl selbst war viermal verschieden.** T3 stand als **39**
> (P3-1), **38** (CLAUDE.md), **37** (abgeleitet) und **50** (README,
> zweimal) im selben Baum. Das Tor prüfte **eine** Schreibweise mit
> `re.search` und übersah die anderen zwei in derselben Datei — der
> sechzehnte Fall von Aufzählung statt Messung, und diesmal war die
> Aufzählung ein regulärer Ausdruck. Seit MF-796 prüft es jede
> Fundstelle in drei Schreibweisen, streng für README und CLAUDE.md,
> als Liste für die Verlaufsdokumente.

---

## Was blockiert, gemessen statt vermutet

### B1 — `nfd` scheitert an der fünften Frage, nicht am Werkzeugmangel

Gemessen (MF-795): **keines** der drei verfügbaren Oracles schreibt NFD.
`gw` kennt PC-98 nur als Rohgeometrie (`pc98.2d/2dd/2hd/2hs`), `samdisk`
nennt es nicht, `hxcfe` führt `NEC_FDI` und `NEC_D88` — beide **ohne**
NFD.

Die zwei Werkzeuge, die NFD beherrschen, sind **FIVEC** (pc98.org) und
**d88split** (tomari). Und genau diese zwei stehen im Kopf von
`src/formats/nfd/uft_nfd_plugin.c` als die Quellen, gegen die MF-358 den
Leser geschrieben hat. Ein Abbild von ihnen bestätigte nur, wovon UFT
abgeleitet ist — dieselbe Lage, die MF-780 bei `openMSX`/`msx_disk`
erkannt und verworfen hat.

**Was es öffnen würde:** ein PC-98-Emulator, der NFD selbst schreibt —
der Format-Urheber **T98-Next** oder ein gleichwertiger.
Beschaffungsentscheidung des Eigentümers, wie Amiga Forever bei X-Copy.

### B2 — die Container-Prämisse trug nicht *(erledigt, mit Berichtigung)*

Die erste Fassung sagte: SAMdisk und hxcfe **raten** die Eingangsgeometrie
und legen den Inhalt neu aus, deshalb überlebe die Prüfmarke nicht.

Gemessen (MF-794): mit **expliziter** Geometrie
(`SAMdisk copy -c80 -s10 -z2 -b1`) kommt für `sad` eine Datei heraus,
die mit der geratenen **byteidentisch** ist. Das Raten war folgenlos.
Für `mgt` und `edsk` schließt sich der Rundlauf `img → Container → img`
ebenfalls byteidentisch.

**Die Werkzeuge legen den Inhalt nach dem Zielformat ab. Sie zerstören
nichts.** Falsch war die **Erwartung** — der Test verglich gegen eine
lineare Anordnung.

Was der Differenzlauf stattdessen fand, sind **zwei echte Lesefehler**:

* `sad` rechnete zylinder-dur, SAD ist **kopf-dur** — **158 von 160
  Spuren** kamen von der falschen Stelle, bei einem Format auf **T1b**.
  Richtig lagen nur (0,0) und (79,1), und (0,0) war die Spur, die der
  Test las (MF-794).
* `edsk` lieferte **nie einen Sektor**, für keine Datei: `uft_edsk.c`
  hatte die Parser-Typen von Hand nachdeklariert — gemessen 40 gegen
  32 Byte je Element, Datenzeiger bei +16 gegen +8. `sector_count` lag
  zufällig gleich, deshalb meldete das Plugin „9 Sektoren" und verwarf
  jeden (MF-796).

Beide waren nur sichtbar, weil ein Abbild vorlag, dessen Sektoren ihre
eigene Nummer tragen. **Auf T3 hatte nie ein Test eine Spur gelesen.**

### B3 — FAT12 gegen eine fremde Hand *(erledigt)*

`mtools` 4.0.49 unter WSL gebaut, `mformat` im Oracle-Register,
`uft_fat12` von FS-T1 auf FS-T2 (MF-789).

### B4 — was nur der Eigentümer kann

| | wartet auf |
|---|---|
| **`nfd`** | T98-Next oder gleichwertiger PC-98-Emulator (B1) |
| **P3-12** | ein Hardwaretag (deckt P6/P3-7, P1 und P7 zugleich) |
| **P3-8/P3-11** | Lizenz-Entscheidung zu den Aminet-Paketen |
| **X-Copy** | Amiga Forever + WinUAE |
| **`ci/action-major-bumps`** | ein RC-Durchlauf, bevor der Branch nach `main` geht |

Keiner davon blockiert die Strecke unten.

---

## Die Strecke, nach Hebelwirkung geordnet

### Schritt 1 — SCP-Statuscodes durchreichen *(eine Stunde)*

`uft_scp_direct.c:157-161` wirft das Statusbyte weg und meldet
`UFT_ERR_IO`, mit dem Verweis auf eine *„future status-query API"* — die
es nicht gibt. Die Firmware liefert **19 unterscheidbare Codes**
(`PR_NODISK`, `PR_WPENABLED`, `PR_NOINDEX`, `PR_NOTRK0`, `PR_NOMOTORSEL`,
`PR_CHECKSUM`, `PR_BADRAM`, …).

Für den Benutzer ist der Unterschied zwischen „keine Diskette eingelegt"
und „Spur 0 nicht gefunden" der ganze Unterschied zwischen einem Werkzeug
und einem Rätsel. Für dieses Projekt ist es dieselbe Ehrlichkeitsfrage
wie überall sonst: **die Information ist da und wird weggeworfen.**

**Kennzahl:** keine direkt. **Warum trotzdem zuerst:** transportunabhängig,
und ab da sagt jeder Fehlschlag am SCP, was er ist. `uft_scp_direct.c`
ist **nicht** geschützt. Siehe P3-29.

### Schritt 2 — `msa` auf T1b *(eine Stunde)*

`msa` steht auf **T2**, und seine einzige Spec-Quelle ist **SAMdisk**
(`src/samdisk/msa.cpp`) — MF-785 führt `msa` ausdrücklich auf SAMdisks
Sperrliste, es ist dieselbe Hand.

**hxcfe 2.16.13 führt `ATARIST_MSA` als RW** und steht für MSA **nicht**
auf der Sperrliste (dort nur `dim_atari`, `hfe`, `hxcstream`). Damit ist
der Weg derselbe wie bei `edsk`: hxcfe erzeugt, SAMdisk liest zurück,
`gen_container_corpus.py` schreibt nur bei byteidentischem Rundlauf.

**Kennzahl:** ungeprüfte Formate ↓. Siehe P3-27.

### Schritt 3 — PLL-Kaskade und Re-Read-Fusion *(die teuerste Lücke)*

`greaseweazle/track.py` hält **zwei** PLL-Profile — aggressiv
(`period=5:phase=60`) für lange und ratenvariable Spuren, konservativ
(`period=1:phase=10`) gegen Hochfrequenzrauschen von Schimmel und Dreck.
`read.py` fährt sie kaskadiert und mischt neue Sektoren in **dasselbe**
Codec-Objekt. Nach zwanzig Leseversuchen ist die Spur vollständig, auch
wenn kein einzelner Versuch es war.

UFT: `flux_pll_init()` setzt feste Gains (`freq_gain=0.02`,
`phase_gain=0.5`), es gibt kein zweites Profil.
`uft_recovery_fusion.c:74` sagt es selbst — *„Flux-level fusion requires
PLL pipeline integration — out of scope."* `GW_READ_RETRIES` fängt nur
`UFT_GW_ERR_OVERFLOW`, also USB-Fehler, **nicht fehlende Sektoren**.

Auf einer gesunden Diskette merkt das niemand. Auf der einen Diskette,
für die es dieses Werkzeug gibt, gehen Sektoren verloren, die ein
anderes Werkzeug rettet. Für ein Programm mit dem Grundsatz „Kein Bit
verloren" ist das die schwerste offene Stelle.

**Zwei Schritte, in dieser Reihenfolge:** erst `flux_pll_t` um ein
Profilfeld erweitern und die zwei Parametersätze übernehmen — vier
Zahlen, eine Stunde. Dann den Dekodierpfad auf **Akkumulation statt
Ersetzung** umbauen: Dekodierläufe schreiben nur in noch leere
Sektorplätze. Der zweite Schritt ist der eigentliche Umbau.

**Kennzahl:** keine der vier — und das ist ein Fall, in dem Regel 9 an
ihre Grenze kommt. Der Kernauftrag ist keine der vier Zahlen. Notiert,
damit die Lücke sichtbar bleibt.

**Lizenz:** greaseweazle steht unter der **Unlicense** (Public Domain,
so im Oracle-Register geführt). Kein Clean-Room, keine Zwei-Hände-
Brandmauer, keine Spec-Route: das ist eine **Übersetzung mit
vorliegender Vorlage**, kein Rückbau.

### Schritt 4 — `P3-10`, die dritte Fundstelle *(Innendienst)*

43 CMake-Blöcke **plus** `tests/differential/conftest.py` pflegen
denselben Quellensatz von Hand. Dreimal in einer Sitzung hat das den Bau
gebrochen; der Hook fängt es seit MF-773, aber er behandelt das Symptom.

**Kennzahl:** keine direkt — er schützt alle vier davor, auf einem Baum
gemessen zu werden, der nicht baut.

### Schritt 5 — Formatwissen als Daten, als Pilot *(strukturell)*

Der einzige Vorschlag, der die T3-Halde **strukturell** angreift statt
einzeln. 88 Plugins verlangen 88 Verifikationen. Wäre IBM-FM/MFM **ein**
verifizierter, parametrisierter Codec und `olivetti.m20`, `kaypro`,
`epson` nur Parameterzeilen, wäre es **eine** Verifikation statt dreißig
— und die Zahl fiele durch Architektur statt durch Arbeit.

Nicht alles umstellen. **Pilot: die IBM-FM/MFM-Familie.** Die
`diskdefs`-Dateien von greaseweazle sind gemeinfrei und wörtlich
übernehmbar.

**Kennzahl:** ungeprüfte Formate ↓↓, mittelfristig. **Warum zuletzt:**
er verlangt einen belegten Codec als Fundament, und den liefert
Schritt 3.

---

## Was wir **nicht** tun

* **Keine neuen Format-Plugins.** Das Moratorium bleibt in Kraft, und
  seine Bedingung ist jetzt eine Beschaffung (B1), keine Arbeit.
* **Kein Erzwingen halber Belege.** Fünf Container-Einträge wurden
  zurückgenommen, obwohl sie das Tor formal bestanden hätten. Ein halb
  belegtes T1b ist schlimmer als ein ehrliches T3: es sieht aus wie
  Fortschritt. Genau diese Rücknahme ist der Grund, warum der SAD-Fehler
  überhaupt noch auffindbar war.
* **Keine Hardware-Vorschläge** (MF-310).
* **Keine Sichtung ohne Kennzahl.** Ein Fund, der keine Zahl bewegt, ist
  Fundus — eingetragen, nicht eingeplant. Mit der in Schritt 3 notierten
  Ausnahme: der Kernauftrag ist keine der vier Zahlen.
* **Keine Änderung an `src/hal/uft_greaseweazle_full.c` ohne Rückfrage.**
  MF-799 hatte eine ausdrückliche Freigabe für genau zwei Punkte; die
  vier weiteren bestätigten Befunde stehen in P3-28 und sind nicht
  angefasst.

---

## Die Lehren, damit sie nicht verlorengehen

**Größengleichheit ist keine Geometriegleichheit.** `v9t9` und `cpm`
liefen byteidentisch durch den Rundlauf und waren trotzdem falsch
gepaart. Beide hätten das Tor formal bestanden.

**Ein Skip, der einen Bruch verdeckt, ist schlimmer als ein roter Test.**
Die gw-Paritätstests meldeten jahrelang „6 skipped". Seit MF-798 macht
ein Skip den CI-Job rot — `pytest` gibt bei durchweg übersprungenen Tests
sonst 0 zurück, und genau daran ist die Aussage vorbeigelaufen.

**Eine Herkunftsangabe ist eine Behauptung wie jede Zahl.** „Das ist
X-Copys Code 3" war abgeleitet, nicht gemessen — und widerlegt. Ebenso
hätte `openMSX` als Oracle für `msx_disk` nur bestätigt, wovon UFT
abgeleitet ist. Bei `nfd` sperrt genau dieselbe Frage den letzten Schritt
des Moratoriums (B1).

**Der Test prüft oft genau die Stelle, an der der Fehler unsichtbar ist.**
Spur (0,0) ist die einzige, bei der zylinder-dur und kopf-dur denselben
Versatz ergeben. `sad` stand deshalb auf T1b und las 98,75 % der
Diskette falsch. Seit MF-795 prüft der Korpus-Test vier Spuren; der
Gegenbeweis an `trd` zeigt, dass er die Klasse fängt.

**Ein Werkzeug, das nicht liefert, was man erwartet, ist erst der zweite
Verdächtige.** Fünf Container galten als von SAMdisk zerstört. Gemessen
war die Erwartung falsch und der Fehler lag im eigenen Baum.

**Die Rechtfertigung im Kommentar ist keine Prüfung.** MF-793 baute die
FZS-Arbeitsliste aus Umdrehung 0 und schrieb die Begründung daneben.
Die Begründung trug nicht: ein Sektor, der dort fehlt, erzeugte weder
Befund noch Skip — Stille, in der Datei, die gegen Stille geschrieben
ist (MF-797).

---

## Der nächste Griff

**Schritt 1**, dann **Schritt 2**. Beide zusammen ein Vormittag, beide
unblockiert. Schritt 3 ist der große Brocken und will einen eigenen
Anlauf; Schritt 5 wartet auf ihn.
