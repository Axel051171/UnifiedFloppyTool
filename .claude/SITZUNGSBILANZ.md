# Sitzungsbilanz — Pflichtabschnitt am Ende jeder Sitzung

Eigentümerentscheidung 2026-09-28 (MF-1505).

**Ohne Bilanz ist die Sitzung offen.** Nicht als Formalie: die Rückschau
(`.claude/STEHENDE_AUFTRAEGE.md` §Rückschau) findet nur, was in Bilanzen
steht — eine Sitzung ohne Bilanz ist für sie unsichtbar, und damit
wiederholt sich ihr Fehler ungesehen.

Der Anlass ist gemessen. Am 2026-09-28 standen fünf Regeln als Satz im
Baum, und **dieselbe Sitzung hat eine davon im selben Atemzug verletzt**
— die 24. Kopie der CBM-Zonenlängen, eingebaut in einen Test, dessen
Kopfkommentar „eine Größe, eine Rechnung" zitierte. Gefangen hat es das
**Tor**, nicht der Agent. Daraus folgt die Kennzahl **K0**: Anteil der
vom Tor gefangenen Fehler. Steigt sie, halten die Regeln; bleibt sie
niedrig, ist jede weitere Regel als Prosa verschwendet.

## Vorlage

```
## Sitzungsbilanz <Datum> <Rolle>

Kennzahlen bewegt:   K3 23 -> 23 (jetzt Manifest) | ...
Kennzahlen unbewegt: begruendet oder Fundus

Fehler dieser Sitzung — je Zeile: was | gefangen von | Folge
  1. <Fehler>   | Tor <Nr> / Mensch / niemand | Rotbeweis <Test> / Tor <Nr> / OFFEN
  2. ...

K0 dieser Sitzung: <Tor-Faenge> von <Fehler gesamt>

Fundus (nicht gebaut, weil keine Kennzahl bewegt): ...
```

**OFFEN heißt: die Sitzung endet nicht.** Jeder vom Menschen gefangene
Fehler bekommt vor dem Ende ein Tor oder einen Rotbeweis — oder eine
Zeile im Fundus mit der Begründung, warum kein Tor möglich ist (meist:
die Messung fehlt noch).

**Die Bilanz wird beim Auftreten geführt, nicht am Ende rekonstruiert.**
Gemessen am 2026-09-28, dem Tag ihrer Einführung: zwischen dem ersten
Schreiben und dem Sitzungsende kamen drei Fehler dazu, und einer davon
wäre ohne eine Nachfrage von außen unbemerkt geblieben — die erste
Fassung nannte 7, die vollständige 10. Wer sie am Schluss
zusammensucht, misst sein Gedächtnis, nicht die Sitzung.

**Der Bauer bewertet nicht.** Ob eine Kennzahl sich wirklich bewegt hat,
prüft die Widerspruchs-Rolle in eigener Sitzung mit der einen Frage:
*welche Kennzahl behauptet dieser Commit bewegt zu haben, und stimmt der
Messbefehl?* Kennzahlen laden zum Spielen ein — „gültige Sektoren" lassen
sich auch durch eine weichere CRC-Prüfung vermehren. Deshalb hat jede
Kennzahl einen Messbefehl **im Baum**, keinen im Kopf.

## Die Regelliste: höchstens 30 Sätze

Jede Regel trägt den Vorfall, der sie erzeugt hat, in einer Zeile daneben
(„24. Kopie, 28.9."). **Satz 31 verdrängt den ältesten Satz *ohne* Tor —
oder wird selbst ein Tor.** Eine Regelliste, die nur wächst, wird nicht
gelesen; eine, die einen Preis hat, zwingt zur Entscheidung zwischen
Werkzeug und Verzicht.

---

## Sitzungsbilanz 2026-09-28 — Oracle-Eichung und Tore

```
Kennzahlen bewegt:
  K0  neu eingefuehrt                    -> 1 von 7
  K3  Zonen: Zahl 23 -> Manifest 23      (MF-1504; eine Zahl konnte die
      Regel nicht halten, vorgefuehrt an einem Wegwerf-Baum)
  K4  hxcstream berichtigt (CHKH statt Plugin-ID), C64FRZ als P3-661
      registriert                        (MF-1494/1498)
  K6  neu gezaehlt                       -> 2

Kennzahlen unbewegt:
  K1/K2  Tierstand unveraendert (T1=11, T1b=68, T2=7, T3=1). Der
         Differenzlauf MF-1503 hat `g64` nicht gehoben, weil es bereits
         T1 war — er hat das ORACLE geeicht, nicht das Format.
  K5     199 Waisen, unangetastet.
  K7-K9  nicht beruehrt.

Fehler dieser Sitzung — was | gefangen von | Folge
  1. `git commit | tail` meldete 0, obwohl der Haken abgewiesen hatte
     | Mensch (git nachgeprueft) | scripts/commit_verified.py, Selbsttest 5/5
  2. 24. Kopie der CBM-Zonenlaengen im eigenen neuen Test
     | TOR audit_cbm_zonen | Tafel entfernt, Aussage ueber nibscans
       Ausgabe gefuehrt statt ueber seinen Quelltext
  3. `python -c` mit Backticks in Doppelquotes -> bash fuehrte fremde
     Befehle aus | Mensch (Datei nachgemessen) | Tor 71 erweitert, 39/39
  4. Eigener Parser verwarf 42 von 71 Spurzeilen STILL, weil er
     `(density:0!=3?)` nicht kannte | Mensch (grep -c Gegenprobe)
     | Abbruch-Zusage im Parser — aber nur im Scratchpad: FUNDUS, siehe unten
  5. Gleichungspruefung meldete 44 Abweichungen, die keine waren
     (`[E2Sn]` fragt nach Sollgeometrie, `sector_count` zaehlt Header)
     | Mensch (Quelltext beider Seiten gelesen) | Richtungs-Zusage im
       Test, Mutationsmatrix 5/5
  6. Fast nibscans 244022 „bad GCR bytes" gegen UFTs bad_gcr_count
     gehalten (Byte gegen <=1 je Sync-Block) | Mensch, VOR der Messung
     | P3-665 + t_bad_gcr_zaehler_ist_keine_bytezahl
  7. T2=14/T3=2 gemessen und CLAUDE.md fast als „gedriftet" korrigiert —
     die Datei fuehrt jedes Format ZWEIMAL, die gepflegte Zahl stimmte
     | Mensch (MF-1177 Satz 1 befolgt) | keine Aenderung; der Beinahe-
       Fehler steht hier, weil er sonst unsichtbar bliebe
  8. Warteschleife pruefte „Datei nicht leer" statt das ENDE des Laufs —
     die Datei fuellt sich waehrenddessen, also meldete der Warter
     Vollzug, als das Urteil noch fehlte. Dieselbe Form wie Fehler 1: ein
     Zwischenzustand als Endergebnis gelesen | Mensch | auf `rc=`
     umgestellt; kein Tor moeglich, das ist Werkzeugbedienung
  9. K4 als „Messbefehl fehlt" in CLAUDE.md eingetragen, obwohl
     `gen_erzeuger_zensus.py` genau diese Frage seit MF-1087 misst und
     bei jedem Commit laeuft. Die Kennzahl war nicht unmessbar, sie war
     nur nie mit ihrem Messbefehl VERBUNDEN | Mensch (erst auf Nachfrage)
     | K4 verbunden, Stand 8 von 89 offen
 10. Nach dem Merge drei Generatoren gelaufen und `gen_stand.py`
     ausgelassen — obwohl genau das in der eigenen Gedaechtnisnotiz
     `stand_md_zuletzt_erzeugen.md` steht | TOR (pre-commit,
     `[STAND.md stale]`) | Generator nachgezogen, Merge liegt als 7a1d6b07

 11. Der Betreff von MF-1505 sagt „1 of 7", sein eigener Rumpf sagt
     „2 von 10" — dieselbe Zahl zweimal von Hand, zu verschiedenen
     Zeiten geschrieben. Dass sie auseinanderliefen, ist kein Versehen,
     sondern das, was zwei Kopien immer tun (MF-1177) | Mensch
     | berichtigt in MF-1506; die DAUERHAFTE Behebung ist MF-1507:
       `commit_verified.py` liest K0 aus DIESER Datei und haengt sie an,
       und eine abweichende Zahl im Text weist es ab (Selbsttest 10/10).
       Die zweite Stelle ist damit abgeschafft, nicht geprueft.

 12. Der Teststrom fuer die GCR-Erkennung war 64 Byte lang — das geht
     durch 5 Bit nicht auf, die letzten Bytes blieben 0x00 und wurden als
     ungueltige Quintette gezaehlt. Der Test mass damit seinen eigenen
     AUFBAU, nicht den Codec | TEST (wurde rot, 10 von 11) | 60 Byte =
       genau 96 Quintette, plus `ASSERT(aus == STROM_BYTE && bits == 0)`
       als Sperre. Der Code war richtig, die Messung falsch — geprueft
       nach MF-1177 Satz 1, bevor irgendetwas am Register geaendert wurde.

 13. Der erste Entwurf des GCR-Registers legte eine eigene 16-Byte-Tafel
     an — die **13.** Kopie der CBM-Encode-Tafel. Ein Register, das die
     Kopien zusammenfuehren soll und dabei eine neue anlegt
     | TOR audit_konstantenfamilien (drei Stunden nach seinem Bau)
     | die Tafel wird jetzt aus `include/uft/uft_cbm_gcr.h` GELESEN statt
       wiederholt; sie wandert erst mit dem letzten Umhaengen ins
       Register (Schritt 3 von P3-666). Das Tor hat damit nicht nur einen
       Fehler gefangen, sondern die bessere Bauform erzwungen.

K0 dieser Sitzung: 4 von 13

**Befund ueber diese Bilanz selbst, und er gehoert hierher:** zwischen
ihrem ersten Schreiben und dem Sitzungsende kamen die Fehler 8, 9 und 10
dazu — und Fehler 9 waere ohne eine Nachfrage von aussen unbemerkt
geblieben. **Eine am Ende rekonstruierte Bilanz ist unvollstaendig.** Sie
gehoert beim Auftreten gefuehrt, nicht nachtraeglich zusammengesucht;
sonst haengt ihre Vollstaendigkeit daran, dass man sich an den
Sitzungsanfang erinnert. Gemessen am 2026-09-28: die erste Fassung nannte
7 Fehler, die vollstaendige nennt 10.

Und zwei Beobachtungen zu K0, damit die Zahl nicht falsch gelesen wird.
**Sie ist von 1/7 auf 2/10 gestiegen, weil MEHR Fehler gefunden wurden,
nicht weniger** — eine ehrliche Fehlerzahl steigt zuerst. Und die beiden
Tor-Faenge (Fehler 2 und 10) haben beide etwas gefangen, das als Satz
bereits im Baum stand: „eine Groesse, eine Rechnung" und „gen_stand.py
zuletzt". Das ist der ganze Grund fuer diese Kennzahl.

Fundus (nicht gebaut, weil keine Kennzahl bewegt):
  * Fehler 4 bekommt KEIN Tor: der Parser war Wegwerf-Code im
    Scratchpad, ein CI-Tor dafuer waere ein Tor ohne Gegenstand. Statt
    dessen Regelsatz Nr. 2 unter K6: „Wer eine Menge parst, zaehlt sie
    gegen." Ein Tor dafuer braucht erst eine Messung: an welchen Stellen
    im Baum wird eine fremde Ausgabe geparst?
  * K3 fuer die uebrigen Konstantenfamilien (Kapazitaeten, Lueckenmasse,
    CRC-Polynome) — es gibt kein Audit. Der naechste Schritt ist die
    MESSUNG (welche Familien werden mehrfach gefuehrt?), nicht das Tor.
  * Zwei Tragbarkeitsbefunde aus der t4-Zulieferung (`memmem`,
    `system("./...")`) — bewegen keine Kennzahl dieses Baums, dem Autor
    gemeldet, nicht registriert.
```

---

## Sitzungsbilanz 2026-09-29 — GCR-Codec-Register und die Umhaengungen (P3-666)

```
Kennzahlen bewegt:
  K3  113 -> 97 ueberzaehlige Kopien, 58 -> 56 Familien
      (MF-1513/1515/1522/1523/1524/1525; Messbefehl
      `python scripts/audit_konstantenfamilien.py`, Ausgabe im Commit)
  K0  4 von 13 (28.9.)                   -> 5 von 8 (siehe unten)
  K6  unveraendert 2 — die Heimatort-Regel ist KEIN neuer Satz ohne Tor,
      sie ist ein Tor (`REGISTERORTE` in audit_konstantenfamilien.py)

Kennzahlen unbewegt:
  K1/K2  Tierstand unveraendert (T1=11, T1b=68, T2=7, T3=1). Das Register
         hebt kein Format — es fuehrt Tafeln zusammen, die alle schon
         gelesen wurden.
  K4     nicht beruehrt.
  K5     199 Waisen, unangetastet — und das ist eine ENTSCHEIDUNG, keine
         Unterlassung: drei der fuenf verbleibenden Encode-Kopien liegen in
         Waisen, ihre Umhaengung senkte K3 um 3, ohne dass ein Aufrufer
         davon profitiert. Das waere „die Zahl als Motiv" (P3-338, MF-1077).
  K7-K9  nicht beruehrt.

Fehler dieser Sitzung — je Zeile: was | gefangen von | Folge
  1. MF-1513 hat `uft_gcr.c` aus den Quellenlisten geloest, in denen die
     Register-Aufrufer standen, und damit 89 Link-Ziele zerbrochen — eine
     Aenderung, die im eigenen Ziel gruen war | TOR (Vollbau, Linker)
     | MF-1515 nimmt `uft_gcr.c` in `CORE_SOURCES`: die eine Stelle statt
       der 89
  2. Fuenf Musterfehler beim Umhaengen, jeder mit derselben Form — eine
     Struktur angenommen, die der Baum nur MEISTENS hat: `sed` mit
     `$`-Anker (zwei CMake-Zeilen enden auf `)`), Gegenprobe ueber
     `elseif`-Bloecke (die Sammelbibliothek ist keiner), Tafelblock-Regex
     mit `/* */` (die Datei benutzt `//`), geschachtelte Klammern
     (`[^\]]+` zerriss `gcr_encode_table[(data[0] >> 4) & 0x0F]` an acht
     Stellen), Funktionskopf-Regex (schnitt `const` in `con`+`st`)
     | Compiler bzw. Gegenprobe | `scripts/gcr_umhaengen.py` mit
       KLAMMERZAEHLUNG statt Mustersuche, Selbsttest 10/10
  3. Das eigene Umhaenge-Skript schrieb, BEVOR es gegenprueft hat — der
     fehlende Bezeichner `gcr_decode_high` wurde gemeldet, die Datei war
     aber schon zerbrochen | Mensch | Reihenfolge umgedreht: Gegenproben
       (kein Bezeichner mehr im Code, Klammerbilanz unveraendert) laufen
       VOR dem Schreiben
  4. `test_gcr_praedikat_trifft_die_tafel` verlor seinen Zeugen, als die
     CBM-Tafel ins Register wanderte — der Abgleich waere eine
     Selbstpruefung geworden | TOR (Vollbau, der Test wurde rot)
     | `tests/oracles/gcr_cbm_tafel_87345aab.c` eingefroren; der Test hat
       dadurch ZWEI Zusagen bekommen, die vorher zirkulaer gewesen waeren
  5. Das Blastradius-Skript suchte nur `uft_gcr.c.obj` im Link-Block und
     sah nicht, dass `libuft_core.a` das Objekt ebenfalls liefert — es
     meldete 98 unversorgte Ziele, gemessen waren es 9. Eine Messung, die
     die falsche KLASSE meldet, ist schlimmer als keine | TOR (Vollbau:
     genau ein Ziel fiel, nicht 98) | Skript berichtigt, danach 9
       eingetragen und 0 unversorgt gemessen
  6. Neun Link-Ziele riefen das Register, ohne es zu bekommen | TOR
     (Linker, `undefined reference to uft_gcr_dekodieren`) | in
       `tests/CMakeLists.txt` eingetragen, Blastradius 0, Vollbau 0 Fehler
  7. Ein Heredoc fuer ein siebenzeiliges Python-Skript — die Regel steht
     seit MF-1096 im Baum und ich habe sie im Vorbeigehen gebrochen
     | TOR 71 (PreToolUse, wies VOR der Ausfuehrung ab) | mit `sed`
       einzeilig erledigt; kein weiterer Bedarf
  8. Der Heimatort-Selbsttest war in der ersten Fassung 18/18 gruen, aber
     Fall (iii) fiel an der SCHRUMPF-Bedingung statt an der Einmal-Klammer,
     und die ORTSKLAMMER war ganz ohne Fall — entschaerft blieb der Test
     gruen. Eine Ausnahme, die nur in ihrer guten Richtung geprueft ist,
     laesst alles durch | MUTATIONSPROBE (3 Klammern einzeln entschaerft)
     | Fall (iii) auf vier Ausgangsstellen umgebaut, Fall (iv) ergaenzt;
       19/19, Mutationsmatrix 3/3

K0 dieser Sitzung: 16 von 31

Diese Zeile ist die EINE Stelle, an der die Zahl steht — `commit_verified.py`
liest sie hier und haengt sie an jede Commit-Nachricht an (MF-1507). Sie hat
im Lauf der Sitzung 5 von 8, dann 5 von 10, dann 5 von 11 gesagt; die
Fehler 9 bis 12 kamen nach dem ersten Schreiben dazu, und die Nachtraege
unten erzaehlen, wann. **Wer sie fortschreibt, aendert diese Zeile** und
schreibt keine zweite mit demselben Wortlaut irgendwo darunter — genau das
ist Fehler 12 gewesen.

**Zur Lesart von K0, damit die Zahl nicht mehr sagt, als sie misst.** Sie
steigt von 4/13 auf 5/8, und der groessere Teil davon ist der NENNER: acht
Fehler statt dreizehn, bei aehnlich viel Arbeit. Das ist kein Beleg fuer
Besserung — es kann auch heissen, dass weniger gefunden wurde. Belastbar
ist nur der Zaehler, und dort steht eine Beobachtung: **vier der fuenf
Tor-Faenge sind der VOLLBAU** (Fehler 1, 4, 5, 6). Der Vollbau ist damit
das wirksamste Tor dieses Baums und wird zugleich am seltensten gefahren,
weil er zehn Minuten dauert. Fehler 5 zeigt, warum das teuer ist: ein
Messskript hatte die falsche Klasse gemeldet, und nur der Linker hat
widersprochen.

Und Fehler 8 ist die einzige Zeile, die kein bestehendes Tor gefangen hat,
sondern eine **Mutationsprobe** — also die Frage „faengt mein Tor
ueberhaupt etwas". Sie hat eine Klammer als Zierde entlarvt, die beim
Lesen richtig aussah. Das gehoert in die Rueckschau: ein neues Tor ohne
Mutationsprobe ist ungemessen.

Fundus (nicht gebaut, weil keine Kennzahl bewegt):
  * Die drei Waisen-Kopien (`uft_d64_parser_v3.c`, `uft_g64_parser_v2.c`,
    `uft_c64_protection_enhanced.c`) und die zwei Viterbi-Kopien bleiben
    stehen — siehe K5 oben. Naechster Schritt ist NICHT ihre Umhaengung,
    sondern die Frage, ob die Waisen einen Weg bekommen (P3-666).
  * P3-678 (Brother-Tafel: keine Quelle, keine Aufrufer, Kommentar sagt
    „5-to-8", aber kein Wert hat Bit 7) und P3-675 (`uft_vorpal_decode()`
    liest 4 von 8 Quintetten, null Aufrufer) — beide registriert, beide
    ohne Kennzahl.
  * Zwei Dateien mit 0 Byte im Wurzelverzeichnis (`Was`, `Werkzeuge,`,
    28.9. 18:24) sind Reste eines zerbrochenen Shell-Aufrufs. NICHT
    angetastet: unversioniert, fremder Herkunft, und Loeschen ohne
    Auftrag ist in diesem Baum eine eigene Fehlerklasse.
```

### Nachtrag zur Bilanz 2026-09-29 — zwei Fehler nach dem ersten Schreiben

Genau das, was die Vorlage vorhersagt: die Bilanz war geschrieben, dann
kamen Fehler 9 und 10 dazu. Sie stehen hier, nicht oben, damit die
Reihenfolge erkennbar bleibt.

```
  9. **Eine MF-Nummer war zweimal vergeben, und zwar auf beiden Seiten
     schon committet.** MF-1515 liegt auf `87345aab` (main, Apple-Tafeln
     ins Register) UND auf `d39834ff` im Arbeitsbaum `wt-dtc` (zweite
     CBM-Verzeichnistuer). Beide Seiten hatten `git log` gefragt, nur
     nicht denselben: `wt-dtc` haelt MF-1515…1520 committet und MF-1521
     unvorgemerkt. | Mensch (beim Suchen der naechsten freien Nummer)
     | **TOR 72 gebaut** (`scripts/audit_mf_nummer.py`, im
       `commit-msg`-Haken): die LETZTE MF-Nummer des Betreffs ist die
       beanspruchte, und sie darf in `git log --all` nicht schon stehen.
       Selbsttest 14/14, Mutationsmatrix 5/5, Einspeiseprobe am echten
       Baum: der Betreff `(MF-1515, P3-666)` faellt mit rc 1 und nennt
       `d39834ff`, `(MF-1526)` geht durch. MF-1515 selbst bleibt doppelt —
       eine committete Kollision wird nicht still ueberschrieben, sie
       braucht eine Eigentuemerentscheidung.

 10. Dieselbe Ursache, zweite Haelfte: meine noch nicht committeten Zeilen
     benutzten MF-1516/1517/1518/1519/1521 und P3-669/670 — alle sieben
     von `wt-dtc` belegt, alle mit anderem Inhalt. Und die fuenf MF-Zahlen
     waren fuer EINEN Commit gedacht, also fuenf Anker ohne Commit
     | Mensch (dieselbe Messung) | je Zeile umgehaengt, nie dateiweit
       (Gedaechtnisnotiz `mf_nummer_von_origin`): 41 Ersetzungen, danach
       auf eine Nummer zusammengezogen — **MF-1522** fuer den Commit,
       P3-674/675 fuer die zwei Fundus-Zeilen (P3-674 ist beim Merge auf 678 gewandert, zweimal — siehe Nachtrag 6 und 7). P3-673 bleibt frei, die
       Nachbararbeit hat sie vorgesehen.
```

**K0 wandert weiter** (die kanonische Zeile oben traegt den Stand). Die Zahl oben
war richtig, als sie geschrieben wurde, und ist es zwei Fehler spaeter
nicht mehr — das ist der Grund, warum die Vorlage „beim Auftreten fuehren"
verlangt.

Und eine Beobachtung, die sich mit Fehler 8 trifft: **Fehler 9 und 10
haette kein bestehendes Tor gefangen**, weil es keines gab. Gemessen ueber
`git ls-files`: von **76** Tor-Skripten (`scripts/audit_*.py`) haben **54**
einen Selbsttest, **22** keinen — und **3** eine Mutationsprobe (eines
davon ist Tor 72, gebaut heute). Ein Tor ohne Selbsttest ist eine
Behauptung; ein Tor ohne Mutationsprobe ist ungemessen. Das ist ein
Rueckstand mit Zahl, aber ohne Kennzahl: er gehoert in den Fundus, bis
entschieden ist, ob daraus eine eigene Zahl wird.

Zusaetzlich zum Fundus oben:
  * Die 22 Tore ohne Selbsttest, und die 73 ohne Mutationsprobe. Der
    naechste Schritt ist KEINE Nachruestung ins Blaue, sondern die Frage,
    welche dieser Tore ueberhaupt eine Entscheidung treffen (manche
    berichten nur). Messung vor Arbeit.
  * MF-1515 doppelt: `main` und `wt-dtc` muessen beim Zusammenfuehren
    entscheiden, welcher Commit die Nummer behaelt. Kein stiller Umschrieb
    — beide sind committet.

### Nachtrag 2 zur Bilanz 2026-09-29 — ein Fundus-Eintrag aus dem Wartezimmer

Waehrend auf einen Nachbar-Commit gewartet wurde, ist eine Sache gemessen
worden, die **kein Fehler dieser Sitzung** war und deshalb nicht in der
Fehlerliste steht:

**`.git/hooks/` ist allen Arbeitsbaeumen GEMEINSAM und von keiner Sperre
gedeckt.** Die Commit-Sperre liegt pro Arbeitsbaum — der Haken schreibt
sie nach `git rev-parse --absolute-git-dir`, in einem Arbeitsbaum also
nach `.git/worktrees/<name>/uft-commit.lock`, und `scripts/commit_lock.py`
loest genauso auf (gemessen: es folgt der `.git`-DATEI zum
Arbeitsbaum-Verzeichnis). Das ist in sich stimmig, weil jeder Arbeitsbaum
seinen eigenen Dateibaum hat.

Nicht gedeckt ist damit aber der **gemeinsame** Teil. Gemessen am
2026-09-29: ein Commit aus `wt-dtc` lief von 06:05:38Z an, seine Sperre lag
unter `.git/worktrees/wt-dtc/`, und `.git/uft-commit.lock` im Hauptbaum
existierte nicht — ein Generator im Hauptbaum haette „frei" gelesen. Fuer
seine eigenen Dateien ist das richtig. Fuer `.git/hooks/` nicht: bash liest
ein Skript in Stuecken, und wer eine laufende Hakendatei ueberschreibt,
kann den Rest eines halb gelesenen Skripts zerreissen.

**Hier ist nichts passiert, und das ist gemessen, nicht gehofft:** der
`commit-msg`-Haken wurde um 07:58:20 installiert, der Nachbar-Commit begann
um 08:05:38 — sieben Minuten spaeter. Was bleibt, ist die Folge: dieser
Commit wird von **Tor 72** beurteilt, einem Tor, das sein Autor nie gesehen
hat. Das ist bei einem gemeinsamen Haken normal und nur dann harmlos, wenn
das Tor keine falschen Treffer hat.

**Kein Auftrag (MF-640):** eine Abweichung zwischen Vorlage und
Installation gibt es heute nicht — `commit-msg`, `pre-commit` und
`pre-push` sind je identisch (gemessen mit `diff -q`). Ein Installier-Tor
gegen ein Problem, das nicht vorliegt, waere Vorratsbau. Der Eintrag steht
hier mit dem, was ihn oeffnen wuerde: die erste gemessene Abweichung, oder
der erste Fall, in dem ein Haken WAEHREND eines Commits geschrieben wird.

### Nachtrag 3 zur Bilanz 2026-09-29 — Fehler 11, und was ihn gefunden hat

```
 11. **Ein Ziel blieb unversorgt, das meine Messung gar nicht sehen
     konnte.** `bench_decode_hotpath` linkt `src/flux/uft_flux_decoder.c`,
     das seit dem Umhaengen `uft_gcr_dekodieren()` ruft — und nicht
     `src/core/uft_gcr.c`. Es haengt an
     `option(UFT_ENABLE_BENCHMARKS ... OFF)`, stand deshalb NIE in
     `build.ninja`, und die Blastradius-Messung ueber die Link-Bloecke war
     blind dafuer. Dasselbe Muster wie Fehler 5: die Messung meldete die
     falsche Klasse, diesmal nicht zu viel, sondern zu wenig
     | Mensch (beim Nachlesen des Bench-Ziels, waehrend auf einen
       Nachbar-Commit gewartet wurde) | zweite Messung ueber den
       CMake-TEXT statt ueber `build.ninja`: von 2 Zielen mit einem
       Register-Aufrufer war genau dieses unversorgt, danach 0.
       Rotbeweis von Hand gelinkt: OHNE das Register rc 1 mit
       `undefined reference to uft_gcr_dekodieren`, MIT ihm rc 0.
```

**K0 wandert weiter — die kanonische Zeile oben traegt den Stand.** Der Zaehler steht, weil kein
Tor den Fall fangen konnte — es gab keines, das optionsgeschaltete Ziele
sieht.

**Und eine Frage, die vorher UNGEMESSEN war und es jetzt nicht mehr ist.**
Aus Tafelzugriffen in einer inline-Funktion sind Aufrufe in eine andere
Uebersetzungseinheit geworden. Der vorhandene Bench deckt MFM-PLL und
Sync-Suche, nicht GCR — er konnte die Frage nicht beantworten. Gemessen mit
einem Wegwerf-Programm im Kratzverzeichnis (gegen eine lokale Tafel im
selben Programm, also als OBERE Schranke), drei Laeufe, MinGW gcc 13.1.0
`-O3`: **1,0–1,1 ns je Aufruf**, hochgerechnet auf die ~470 000 Aufrufe
einer 1541-Diskette (42 Spuren x ~1400 Fuenferbloecke x 8 Quintette)
**0,50–0,52 ms**. Als Bereich, nicht als Faktor.

Kein Auftrag daraus: ein GCR-Bench im Baum waere Vorratsbau fuer eine
Differenz von einer halben Millisekunde. Die Zahl steht hier, damit
niemand sie spaeter schaetzen muss.

### Nachtrag 4 zur Bilanz 2026-09-29 — Fehler 12, gefangen vom Tor aus MF-1507

```
 12. **Ich habe K0 an drei Stellen geschrieben, und sie sind auseinander-
     gelaufen** — genau der Fehler, den MF-1505 als Nummer 11 der letzten
     Sitzung verzeichnet und den MF-1507 dauerhaft abschaffen sollte. Die
     kanonische Zeile sagte weiter „5 von 8", waehrend die Nachtraege
     „berichtigt: 5 von 10" und „endgueltig: 5 von 11" trugen und die
     Commit-Nachricht „5 of 11". Die Nachtragsformen trafen den Anker
     `^K0 dieser Sitzung: N von M` nicht, also blieb die alte Zahl die
     gueltige. | TOR (`commit_verified.py`: „die Nachricht nennt K0 als
     5 von 11, die Sitzungsbilanz sagt 5 von 8" — KEIN NEUER COMMIT)
     | die kanonische Zeile wird jetzt FORTGESCHRIEBEN statt ergaenzt, mit
       einem Satz daneben, der genau das verlangt; die Zahl steht nicht
       mehr in der Commit-Nachricht, das Werkzeug haengt sie an.
```

**Das ist der beste Fang dieser Sitzung, und er verdient einen Satz.** Tor
1507 wurde gebaut, weil derselbe Fehler am Vortag durch die Finger ging
(„1 of 7" im Betreff gegen „2 von 10" im Rumpf). Einen Tag spaeter hat es
ihn gefangen — bei mir, in derselben Form, an derselben Kennzahl. Die
Begruendung von MF-1507 lautete: „Die zweite Stelle ist damit abgeschafft,
nicht geprueft." Gemessen war sie nicht abgeschafft, sondern nur bewacht —
ich habe eine dritte angelegt. Ein Tor ersetzt keine Disziplin, es
ueberlebt sie.

**K0 steht damit auf 6 von 12** (der Fang selbst ist der sechste). Die Zahl
ist ueber die Sitzung von 5/8 auf 6/12 gewandert, und beide Bewegungen
kamen aus ehrlicherem Zaehlen, nicht aus besserer Arbeit.

### Nachtrag 5 zur Bilanz 2026-09-29 — Fehler 13, im eigenen frischen Tor

```
 13. **Tor 72 ist abgestuerzt, statt ein Urteil zu faellen** — und zwar in
     der Fassung, die eine Stunde vorher committet wurde. Die Nachricht wird
     mit `Path(...).read_text(...).splitlines()[0]` gelesen: eine FEHLENDE
     Datei ergibt `FileNotFoundError`, eine LEERE `IndexError`, ein
     VERZEICHNIS ebenfalls einen Fehler. In einem `commit-msg`-Haken haette
     das den Commit mit einem Traceback abgebrochen — aus einem Grund, der
     mit dem Inhalt der Nachricht nichts zu tun hat. Klasse MF-1000 /
     Tor 64: „ein Absturz ist kein Urteil"; dieselbe Zusage macht
     `scripts/c_literal.py` seit MF-1171 ausdruecklich, und mein Tor hat sie
     nicht gemacht. | Mensch (Abschlusspruefung NACH dem Commit: `rc=1` sah
     wie eine Abweisung aus und war ein Traceback — genau die
     Verwechslung, vor der MF-1000 warnt)
     | `betreff_lesen()` gibt `None` zurueck und wirft NIE; der Aufrufer
       meldet „NICHTS geprueft" und laesst durch, weil eine unlesbare Datei
       nichts ueber die Nummer sagt. Vier Faelle dazu (fehlend, leer,
       Verzeichnis, erste-gegen-zweite-Zeile), Selbsttest 14 -> 18.

     Und die Mutationsprobe hat eine ZWEITE Haelfte gefunden: mit
     entschaerftem Abfangen starb der SELBSTTEST an der ersten fehlenden
     Datei, also liefen die uebrigen Faelle nicht mehr — ein zweiter Defekt
     waere dahinter unsichtbar geblieben. Die vier Faelle fangen jetzt
     selbst ab und melden ROT statt abzubrechen. Mutationsmatrix 5 -> 7,
     alle 7 rot.
```

**K0 bleibt bei 6, der Nenner geht auf 13.** Kein Tor konnte diesen Fall
fangen, weil es das Tor selbst war — und das ist die Lehre: **ein neues Tor
gehoert gegen seine eigenen Fehleingaben geprueft, nicht nur gegen die
richtigen.** Der Satz dazu steht seit heute in den stehenden Auftraegen
(„jede Klammer einzeln entschaerfen"); er muss um eine Zeile erweitert
werden, und das ist mit MF-1523 geschehen: **auch gegen fehlende, leere und
falsch geformte Eingaben.**

Bemerkenswert bleibt die Reihenfolge: das Tor war 14/14 gruen und
5/5 mutationsfest, als es committet wurde. Beides stimmte — und beides
bezog sich nur auf gueltige Eingaben. Ein Selbsttest prueft, was er kennt.

### Nachtrag 6 zur Bilanz 2026-09-29 — Fehler 14, und dieses Mal hat das Tor NICHT gefeuert

Beim Zusammenfuehren von `origin/main` (11 fremde Commits) ist derselbe
Zusammenstoss ein zweites Mal aufgetreten, in die andere Richtung:

```
 14. **MF-1522 und P3-674 sind DOPPELT vergeben** — mein `0a66fb21`
     (08:22:12) und `e36346ce` (08:39:53) aus dem Arbeitsbaum, siebzehn
     Minuten spaeter, beide mit derselben MF- und derselben P3-Nummer.
     | Mensch (beim Merge, nicht vom Tor) | P3-674 ist mein Eintrag
       gewichen — gepusht schlaegt nicht-gepusht: die Brother-Tafel heisst
       jetzt **P3-676**, und die Zeile sagt das selbst, weil die Nachricht
       von `0a66fb21` sie noch P3-674 nennt. MF-1522 bleibt doppelt: beide
       Commits sind geschrieben, einer davon gepusht. Wie bei MF-1515
       braucht das eine Eigentuemerentscheidung, kein stilles Umschreiben.
```

**Und das ist der unangenehme Teil: Tor 72 haette es fangen muessen.**
Gemessen, gerade nachgeprueft:

* der Haken liegt installiert (`.git/hooks/commit-msg`, 07:58:20, also
  24 Minuten vor `e36346ce`), er enthaelt den Aufruf, und `core.hooksPath`
  zeigt in BEIDEN Baeumen genau dorthin;
* mit ihrem Betreff gefuettert weist er ab: „MF-1522 ist bereits
  beansprucht von: e36346ce, 0a66fb21";
* ihr Reflog sagt `commit:` — kein Rebase, kein Cherry-pick, bei denen
  `commit-msg` nicht laeuft.

**Warum es trotzdem durchging, ist NICHT gemessen**, und diese Luecke wird
hier nicht mit einer plausiblen Geschichte gefuellt (dieselbe Zurueckhaltung
wie bei `repo_scope` in MF-1171: „einmal beobachtet, nicht reproduziert").
Denkbar bleiben ein `--no-verify`, eine andere Python-Umgebung im
Nachbarprozess, oder ein Weg, den ich nicht kenne. **Bis das gemessen ist,
gilt Tor 72 als ungeprueft IM BETRIEB** — sein Selbsttest (18/18) und seine
Mutationsmatrix (7/7) sagen etwas ueber den Klassifizierer, nicht ueber den
Haken. Dieselbe Unterscheidung fuehrt Tor 71 ausdruecklich („ob der Haken
wirkt, misst `--protokolle`"), und mein Tor hat dieses Gegenstueck nicht.

Das ist die ehrliche Bilanz dieses Baus: **ein Tor, das im Klassifizierer
belegt und im Betrieb unbelegt ist, ist kein Tor, sondern ein Skript mit
einem Haken davor.** Eintrag dafuer ist die Zeile unten im Fundus; er
bewegt K0, sobald er messbar ist.

**K0: 6 von 14.**

Zusaetzlich zum Fundus:
  * **Eine Betriebsprobe fuer Tor 72.** Tor 71 misst seine Wirkung ueber
    die Sitzungsprotokolle (`--protokolle --seit <Einbauzeit>`) und meldet
    dabei, wie viele Aufrufe es GESEHEN hat. Tor 72 braucht das Gegenstueck:
    eine Messung, die sagt, bei wie vielen Commits seit dem Einbau der
    Haken gelaufen ist — sonst ist „es hat nicht gefeuert" nicht von „es
    lief nicht" zu unterscheiden. Der erste Schritt ist die MESSUNG, nicht
    das Tor.

### Nachtrag 7 zur Bilanz 2026-09-29 — Fehler 15, und Tor 72 hat gefangen

```
 15. **Der Merge-Commit selbst trug eine belegte Nummer.** Ich hatte
     MF-1524 als frei gemessen (`git log --all --format=%s | grep -c
     "MF-1524"` ergab 0) und die Nachricht damit geschrieben. Waehrend die
     Konflikte aufgeloest, die Generatoren gelaufen und 601 Tests gebaut
     wurden — knapp eine Stunde — hat die Nachbarsitzung `ea1f2436` mit
     **MF-1524 und P3-676** committet. P3-676 war die Nummer, auf die mein
     Brother-Eintrag in Nachtrag 6 gerade ausgewichen war; er ist damit
     ZWEIMAL gewichen. | **TOR 72** (im commit-msg-Haken: „MF-1524 ist
     bereits beansprucht von: ea1f2436" — KEIN NEUER COMMIT)
     | Merge auf **MF-1526**, Brother-Eintrag auf **P3-678**. Frei gemessen
       diesmal ueber committete UND unvorgemerkte Nummern beider Baeume:
       MF-1525 und P3-677 liegen unvorgemerkt in `wt-dtc`.
```

**Damit ist Tor 72 auch im BETRIEB belegt — und zwar an genau der Klasse,
fuer die es gebaut wurde.** Nachtrag 6 sagt, es gelte im Betrieb als
ungeprueft, weil `e36346ce` unerklaert durchgegangen ist. Das bleibt so;
aber jetzt steht daneben ein Fall, in dem es gefeuert hat, in demselben
Haken, an demselben Tag. Was von Nachtrag 6 stehen bleibt, ist also die
praezisere Aussage: **es wirkt, aber es ist nicht bewiesen, dass es bei
JEDEM Commit laeuft** — und die Betriebsprobe, die diese Frage beantworten
wuerde, fehlt weiter.

**Und die eigentliche Lehre ist nicht das Tor, sondern die HALTBARKEIT
einer Nummernmessung.** Zwischen „MF-1524 ist frei" und dem Commit lagen
Konfliktaufloesung, fuenf Generatoren, ein Vollbau und 601 Tests. In dieser
Zeit sind vier fremde Commits entstanden. Die Gedaechtnisnotiz
`mf_nummer_von_origin` sagt schon „Nummer direkt vor dem Commit bestimmen";
gemessen heisst „direkt" hier **weniger als eine Stunde**, und bei einem
Vollbau im Haken ist das nicht einzuhalten. Deshalb ist das Tor die
richtige Stelle: es prueft die Nummer in dem Augenblick, in dem der Commit
entsteht, nicht in dem, in dem die Nachricht geschrieben wurde.

**K0: 7 von 15.**

### Nachtrag 8 zur Bilanz 2026-09-29 — die Betriebsprobe, fünf Fehler beim Bauen, und K0 hatte keine Definition

Die Fundus-Zeile aus Nachtrag 6 ist abgearbeitet: Tor 72 hat seit MF-1528
eine Betriebsprobe (`python scripts/audit_mf_nummer.py --betrieb`).

**Zuerst die Messung, die den Bau gerechtfertigt hat — und ihr Ergebnis war
ein Nein.** Die Frage „warum ging `e36346ce` durch" ist rueckblickend
**nicht** entscheidbar: gemessen ueber **754** Sitzungsprotokolle im
Zeitfenster 05:50–06:50Z steht dort **kein einziger** `git commit`-Aufruf
einer fremden Sitzung. Der Weg, auf dem jener Commit entstand, hinterlaesst
in `~/.claude/projects` keine Spur. **Damit ist auch Tor 71s Betriebsprobe
(`--protokolle`) fuer solche Commits blind** — das steht in ihrer
Beschreibung nicht, und es gehoert dort hin.

Deshalb misst die neue Probe vorwaerts: jeder Lauf vermerkt sich in
`$GIT_COMMON_DIR/uft-tor72-laeufe.log` (Zeit, beanspruchte Nummer, Urteil,
HEAD), und `--betrieb` zaehlt, welcher Commit seit dem ersten Vermerk
**keinen** hat. „Der Haken lief nicht" ist damit von „der Haken hat nichts
gefunden" unterscheidbar — und genau deren Verwechslung war der Befund.
Erster Lauf: Laufbuch seit 09:09:57Z, 58 Vermerke, 0 Commits ohne Vermerk.

**Grenze, benannt:** das Laufbuch liegt in `.git`, ist also pro Klon und
unversioniert, wie der Haken selbst. Es sagt nichts ueber Commits vor
seiner Einfuehrung und nichts ueber einen anderen Klon. CI kann es nicht
pruefen; was `check()` prueft, ist der Klassifizierer und die Zeile in der
Haken-Vorlage.

```
Fehler beim Bauen dieser Probe — je Zeile: was | gefangen von | Folge
 16. Das Muster `(?<!\w)-n(?=\s|$)` fuer „--no-verify" trifft `tail -n`,
     `grep -n`, `sort -n`. Die Zahl „3 Commits mit --no-verify" war
     Rauschen | Mensch (beim Lesen der Ausgabe) | nur noch `--no-verify`
       und `commit -n`; und die zweite Haelfte desselben Fehlers: eine
       Datei, die HEUTE angefasst wurde, enthaelt nicht nur heutige
       Zeilen — die ersten „Treffer" stammten vom 20.9. Jetzt wird nach
       ZEITSTEMPEL gefiltert, nicht nach Dateidatum
 17. Der Zeitfilter der Probe verglich nur DATEN. Am Starttag des
     Laufbuchs haette damit jeder fruehere Commit desselben Tages
     faelschlich „ohne Vermerk" bekommen | Mensch (beim Schreiben des
     Falls dafuer — er liess sich nicht schreiben, weil die Rechnung nicht
     trug) | `utc()` rechnet `%cI` samt Zeitzone auf UTC um und vergleicht
       exakt; der grobe Vergleich bleibt nur als grosszuegiger Rueckfall
 18. Mein Schutzhelfer gab bei einem Wurf `None` zurueck — und der Fall
     „`utc()` wirft bei Unsinn nicht" prueft genau auf `None`. Er konnte
     „hat None geliefert" nicht von „hat geworfen" unterscheiden
     | MUTATIONSPROBE | ein MERKMAL statt `None`
 19. Die Abfangklammer in `lauf_vermerken()` war von KEINEM Fall
     erreichbar: ohne git liefert `laufbuch()` schon vorher `None`, das
     `except` kam nie dran | MUTATIONSPROBE (die einzige von acht, die
     nicht fiel) | ein Fall, der es erreicht — das Laufbuch ist ein
       VERZEICHNIS, dann muss `False` kommen und kein Wurf
 20. Drei `betrieb()`-Aufrufe im Selbsttest waren ungeschuetzt, also starb
     bei entschaerfter Abfangklammer der ganze Selbsttest statt einen Fall
     rot zu melden | MUTATIONSPROBE | alle drei ueber `meldung()`; jetzt
       meldet jede der acht Mutationen einen roten FALL, keinen Absturz
```

Stand des Baus: Selbsttest **35/35**, Mutationsmatrix **8 von 8**,
`check()` leer, Konstantenfamilien unveraendert 56/97.

---

**Und jetzt das Unangenehme: K0 hatte keine Definition, und meine eigene
Zaehlung war widerspruechlich.** Gemessen an der Liste oben: Fehler 8 wurde
von der Mutationsprobe gefangen und im Zaehler NICHT gezaehlt; Fehler 2 vom
Compiler, ebenfalls nicht; Fehler 12 und 15 von Toren, gezaehlt. Eine
Kennzahl ohne Definition ist kein Mass — genau der Satz, der in CLAUDE.md
ueber Messbefehle steht, gilt fuer die Zaehlregel ebenso.

**Definition, ab hier verbindlich:** ein **Tor**-Fang ist jede
MECHANISCHE Pruefung, die anschlaegt, ohne dass ich beschliesse
hinzusehen — CI- und Commit-Haken, Audit-Skripte, der Compiler und Linker
ueber den Bau, ein Test, eine Mutationsmatrix. Ein **Mensch**-Fang ist
einer, bei dem ich durch Lesen oder Nachdenken darauf gekommen bin.

Danach neu gezaehlt, Fehler 1–20:

| | Faenger | Nummern |
|---|---|---|
| **Tor** | Vollbau/Linker 1·4·5·6 · Compiler 2 · Tor 71 7 · Mutation 8·18·19·20 · commit_verified 12 · Tor 72 15 | **12** |
| **Mensch** | 3 · 9 · 10 · 11 · 13 · 14 · 16 · 17 | **8** |

**K0: 12 von 20** — vorher stand 7 von 15.

**Und der Anstieg ist NICHT verdient.** Er kommt aus der Definition, nicht
aus besserer Arbeit: dieselben Faenge, anders gezaehlt. Nach der alten,
unausgesprochenen Zaehlung waeren es 9 von 20. Die Zahl steht hier mit
beiden Werten, weil eine Kennzahl, die durch eine Umdefinition steigt,
genau die Bewegung ist, die MF-1077 verbietet — sie ist hier zulaessig, weil
die alte Zaehlung in sich widerspruechlich WAR und das ein Befund ist, aber
sie ist kein Fortschritt und wird nicht als solcher berichtet.

Was die Zahl weiter sagt: von 12 Tor-Faengen sind **4 der Vollbau** und
**4 die Mutationsprobe**. Beide sind teuer und werden selten gefahren —
der Vollbau zehn Minuten, die Mutationsprobe gibt es an 3 von 76 Toren.
Das ist die Stelle, an der K0 am billigsten steigt.

### Nachtrag 9 zur Bilanz 2026-09-29 — der Commit MF-1528 wurde abgewiesen, und die Ursache war eine Regel dieses Baums

```
 21. **Mein Selbsttest war nur AUSSERHALB eines Hakens gruen.**
     `check_consistency.py` hat MF-1528 abgewiesen: der Fall „Arbeitsbaum
     liess sich anlegen" war im Haken rot (34/35), direkt aufgerufen gruen
     (35/35). Ursache ist die Regel, die dieser Baum eigens dokumentiert
     (MF-1282, `scripts/git_env.py`): **Git exportiert `GIT_DIR` in jeden
     Haken, und `cwd=` ueberstimmt das nicht.** `git init` bekam
     `env=git_umgebung()`, die fuenf folgenden Aufrufe nicht — sie liefen
     also gegen das UMGEBENDE Depot. | TOR (check_consistency im
     pre-commit) | alle git-Aufrufe der Wegwerf-Depots mit `env=_gu()`,
       Gegenprobe im Aenderungsskript VOR dem Schreiben

 22. **Und dahinter lag ein gefaehrlicherer Fall, den erst die Gegenprobe
     zeigte.** `laufbuch()` fragt git selbst — unter gesetztem `GIT_DIR`
     gab `laufbuch(<wegwerf-depot>)` den Pfad des **ECHTEN** Laufbuchs
     zurueck. Der Fall „Laufbuch ist ein Verzeichnis" haette damit ein
     Verzeichnis ueber die echte Datei gelegt und sie dauerhaft zerstoert.
     | Mensch, aber nur weil der Selbsttest mit `GIT_DIR=…/.git`
       ausgefuehrt wurde statt gelesen | `laufbuch()`, `lauf_vermerken()`
       und `betrieb()` nehmen eine optionale `umgebung`; produktiv bleibt
       sie None (dort SOLL `GIT_DIR` erben, so findet
       `--git-common-dir` bei einem Arbeitsbaum-Commit den gemeinsamen
       Ort), im Selbsttest ist sie `git_umgebung()`. Nachgewiesen in DREI
       Umgebungen: ohne `GIT_DIR`, mit `GIT_DIR=.git`, und mit `GIT_DIR`
       auf einen Arbeitsbaum — je 35/35. Laufbuch danach unversehrt
       (Datei, 3380 Byte, 78 Vermerke), Mutationsmatrix weiter 8 von 8.

 23. Fuenfmal in dieser Sitzung ein `python -` bzw. `python - <<'X'` mit
     LEERER Eingabe abgeschickt — der Prozess wartet dann auf stdin und
     laeuft in die Zeitgrenze; zweimal musste er mit TaskStop beendet
     werden, einmal WAEHREND eine Mutation angewandt war. | Mensch
     | Verzicht: `python -` wird in diesem Baum nicht mehr benutzt,
       Skripte gehen ueber `Write` in eine Datei. Tor 71 faengt das
       Heredoc-MUSTER, aber nicht den leeren Rumpf — eine Erweiterung
       waere moeglich und steht im Fundus.

 24. Mein Aenderungsskript prueft, dass kein git-Aufruf ohne `env=`
     bleibt — mit `[^)]*`, und das bricht an der Klammer in „(MF-4000)"
     ab: vier Fehltreffer, kein Schreiben. Genau die Falle, die heute
     Morgen `gcr_encode_table[(data[0] >> 4) & 0x0F]` an acht Stellen
     zerrissen hat | GEGENPROBE (sie hat vor dem Schreiben abgebrochen)
     | Klammerzaehlung statt Zeichenklasse
```

**K0: 15 von 24** (neu dazu: 21 Tor, 22 Mensch, 23 Mensch, 24 Tor).

---

### Nachtrag 10 — meine eigene Fundus-Zahl war falsch, und zwar zu pessimistisch

In Nachtrag 6 steht committet: *„von 76 Tor-Skripten haben 54 einen
Selbsttest, 22 keinen — und 3 eine Mutationsprobe."* Das ist berichtigt,
nicht entfernt (MF-1077). Gemessen mit dem richtigen Kriterium:

| | |
|---|---|
| Tor-Skripte (`git ls-files scripts/audit_*.py`) | **77** |
| in `check_consistency.py` eingebunden | 61 |
| im **Pruefstand** `audit_selbsttest.py` (MF-735) eingetragen | 24 |
| geprueft — eigener Selbsttest **oder** Pruefstand | **69** |
| faellt ein Urteil und ist **nirgends** geprueft | **1** |
| berichtet nur, faellt kein Urteil | 10 |
| nennt eine Mutationsprobe | 4 |

Zwei Musterfehler, und beide sind dieselbe Bauform wie fuenfmal heute — ein
Muster, das eine Struktur annimmt, die der Baum nur MEISTENS hat:

1. `--selftest|--selbsttest|_selbsttest` uebersah `audit_heredoc.py`, dessen
   Funktion `def selbsttest(` heisst, ohne Unterstrich.
2. Viel schwerer: **ein Tor braucht seinen Selbsttest nicht IN SICH.**
   `audit_selbsttest.py` ist ein Pruefstand — er pflanzt je Werkzeug einen
   kleinen Baum mit bekannter Antwort und ruft dessen `check()` in BEIDE
   Richtungen (`treffer` muss gemeldet werden, `sauber` darf nicht). **18
   der 20 angeblich ungepruefen** stehen darin.

Das eine echte ist `audit_plugin_compliance.py` — ein **Release**-Tor
(gerufen aus `.claude/skills/uft-release/scripts/pre_release_check.sh`,
genannt in drei Skills), nicht im CI, ohne Probe.

**Die Lehre ist nicht die Zahl, sondern die Richtung des Irrtums.** Eine
falsche Fundus-Zahl, die einen Rueckstand ERFINDET, kostet Arbeit an einer
Stelle, an der nichts fehlt — und sie stand schon in einem Commit. Wer eine
Rueckstandszahl schreibt, prueft zuerst, ob es fuer die Sache im Baum
bereits eine EINRICHTUNG gibt.

Neuer Fundus-Eintrag an ihrer Stelle:
  * **Die Mutationsproben liegen AUSSERHALB des Baums.** 4 von 15
    Tor-Faengen dieser Sitzung stammen von ihnen, und `git ls-files | grep
    -i mutation` findet genau eine Datei: den Skill, der die METHODE
    beschreibt (`.claude/skills/uft-mutationsmatrix`). Damit ist
    „Mutationsmatrix 8 von 8" im Commit-Rumpf **nicht nachrechenbar** —
    dieselbe Klasse, gegen die `docs/ORACLES.md` „Quellstand + Baurezept +
    Ausgabe-SHA" verlangt. Und die Proben mutieren die ECHTE Datei mit
    Ruecknahme im `finally`; ein Abbruch laesst ein mutiertes Tor im Baum
    stehen, was heute einmal eingetreten ist (Fehler 23). Der Weg heraus:
    Proben arbeiten auf einer KOPIE in einem Wegwerf-Verzeichnis und liegen
    im Baum. Das ist ein Bau und braucht eine Vorlage.
  * `audit_plugin_compliance.py` — ein Release-Tor ohne Probe. Es in den
    Pruefstand einzutragen ist billig; der erste Schritt ist zu messen, ob
    sein `check()` die Signatur des Pruefstands hat (es hat kein `check()`,
    gemessen).

### Nachtrag 11 zur Bilanz 2026-09-29 — der letzte ungeprüfte Urteilsträger, und warum er NICHT gebaut wird

Nachtrag 10 nennt `audit_plugin_compliance.py` als das einzige der 77
Tor-Skripte, das ein Urteil fällt und nirgends geprüft ist. Der
angekündigte nächste Schritt lautete wörtlich: „ihm die Signatur des
Prüfstands geben".

```
 25. **Der angekuendigte Weg hat nicht getragen, und die Messung hat es
     gezeigt statt einer Diskussion.** Der Pruefstand
     `audit_selbsttest.py` pflanzt je Werkzeug einen Baum mit BEKANNTER
     Antwort und ruft `check(pflanzung)`. `audit_plugin_compliance.py` hat
     aber (a) kein `check()` und (b) eine GRUNDLINIEN-relative Entscheidung
     (`--min-pass`), nicht eine fallweise — ein gepflanzter Baum mit einem
     Plugin beantwortet das nur mit ausdruecklichen Schwellen. | Mensch
     (Messung vor Plan, Konfliktordnung 1) | Plan verworfen, bevor eine
       Zeile geschrieben war; der richtige Ort waere ein Selbsttest IM
       Skript, und ob er gebaut wird, entscheidet die Kennzahlfrage unten
```

**Drei Zerleger-Lücken, je einzeln an gepflanzten Quellen gemessen.**
`extract_plugin_body()` zählt Klammern über **rohen** C-Text:

| Fall | gemessene Folge |
|---|---|
| Klammer in einer Zeichenkette (`.name = "B { mit Klammer"`) | Tiefe kehrt nie auf 0 zurück → **Plugin wird gar nicht gefunden**, still übersprungen |
| Klammer in einem Kommentar (`/* Vorsicht: } */`) | Rumpf endet zu früh → `UFT_SPEC_UNKNOWN` → **falsches FAIL** |
| `.features = &x` | `(\w+)` trifft das `&` nicht → **falsches FAIL** |

Der erste ist der schlimmste: ein übersprungenes Plugin kann beliebig
unvollständig sein, ohne gezählt zu werden — ein Tor, das an dieser Stelle
schweigt statt zu melden.

**Und alle drei sind heute LATENT.** Über `git ls-files` unter
`src/formats/`: **89** Definitionen im Text, **89** vom Zerleger gelesen;
`.features` mit einem Wert, der kein blosses Wort ist: **0**. Der Zerleger
ist richtig — aus Glück, nicht aus Bauart.

**Deshalb wird nichts gebaut.** Nach MF-640 bewegt der Fund keine der
Kennzahlen K0–K9, ist also **Fundus, nicht Auftrag**: eingetragen als
**P3-679**, mit dem, was ihn öffnet (der erste Plugin-Name oder
Beschreibungstext mit einer geschweiften Klammer, oder der erste
`.features`-Wert, der kein blosses Wort ist — beides eine gewöhnliche
Änderung, die niemand als riskant erkennen würde). Und `scripts/c_lex.py`
kann Zeichenketten und Kommentare bereits überspringen; es wird hier nur
nicht benutzt.

**K0: 15 von 25.**

Was dieser Nachtrag über die Sitzung sagt: die Reihenfolge hat gehalten.
Erst die Messung, dann das Urteil — und die Messung hat sowohl meinen
angekündigten Plan verworfen als auch den Bau verhindert, der sich sonst
plausibel angefühlt hätte. Drei gemessene Defekte sind ein guter Grund
hinzusehen und kein Grund zu bauen, solange sie nicht zuschlagen.

### Nachtrag 12 zur Bilanz 2026-09-29 — die Betriebsprobe hat geliefert, und die Antwort ist unbequem

Die Probe aus MF-1528 war zwei Stunden alt, als sie ihren ersten echten
Befund gemeldet hat:

```
Laufbuch seit 09:09:57Z, 107 Vermerke
Commits mit Anspruch seither: 4, davon ohne Vermerk: 2
   ohne Vermerk: 97b2888a  MF-1601
   ohne Vermerk: cc204479  MF-1602
```

Beide sind HEAD des Arbeitsbaums `C:/Users/Axel/wt-dtc`; meine eigenen zwei
(`0d50177e`, `04b8a400`) tragen je einen Vermerk. **Tor 72 bindet gemessen
nur eine Seite.** Eingetragen als **P3-680**.

Die Arithmetik der Probe ist dabei nachprüfbar richtig: die zwei anderen
fremden Commits des Tages (10:17 und 11:00 Ortszeit) liegen **vor** dem
Laufbuch und werden korrekt ausgenommen — genau der Fall, für den `utc()`
gebaut wurde, nachdem der Tagesvergleich ihn verfehlt hätte.

**Und die naheliegenden Ursachen sind ausgeschlossen, nicht vermutet:**
`core.hooksPath` liefert in **beiden** Bäumen denselben Pfad, die Datei dort
ist vorhanden und ruft Tor 72 (3 Nennungen), das Reflog sagt `commit:` (also
kein Rebase, bei dem `commit-msg` ohnehin nicht liefe). Seit MF-1530
schreibt zusätzlich die **Bash** des Hakens selbst eine Zeile, wenn sie kein
Python findet oder das Skript fehlt — beide Zeilen fehlen ebenfalls. Es
bleiben `--no-verify` oder git-Plumbing; **welches, ist NICHT gemessen** und
wird nicht mit einer Geschichte gefüllt.

```
 26. **„Kein Vermerk" hatte drei Ursachen, und die Probe konnte sie nicht
     unterscheiden** — der Haken lief nicht · der Haken lief und fand kein
     Python · das Anhaengen scheiterte. Mein eigener Bau von MF-1528 hat
     also einen Befund geliefert, dessen Deutung offen blieb | Mensch
     (beim Lesen des ersten echten Befunds) | MF-1530: die Bash des Hakens
       schreibt in den zwei Zweigen selbst eine Zeile, je einzeln
       nachgestellt und belegt — `ungeprueft-kein-python` (mit git im PATH,
       Python entfernt) und `ungeprueft-skript-fehlt` (Wegwerf-Depot ohne
       `scripts/`). Der gewoehnliche Weg schreibt weiterhin genau eine
       Zeile, von Python. `--betrieb` sagt die verbleibende
       Zweideutigkeit jetzt AUS statt sie zu verschweigen.

 27. Meine erste Probe des Zweigs „kein Python" nahm `PATH=/usr/bin:/bin`
     — darin fehlt auch `git`, und ohne git findet der Haken das Laufbuch
     gar nicht. Gemessen 101 -> 101 Zeilen, und ich haette daraus
     geschlossen, der Zweig schreibe nicht | Mensch (die Zahl passte nicht
     zur Erwartung, also wurde der Aufbau geprueft, nicht der Code —
     MF-1177 Satz 1) | Probe mit `PATH="$(dirname $(command -v
     git)):/usr/bin:/bin"`, danach 102 -> 103 mit
     `ungeprueft-kein-python`. Der vierte Fall (kein git ueberhaupt) ist
     damit ebenfalls gemessen und unvermeidlich.

 28. Ein schliessendes `"` in `„der Haken lief nicht"` hat mitten in einer
     Python-Zeichenkette die Zeichenkette BEENDET; der Em-Dash danach war
     ein freies Zeichen -> `SyntaxError` | TOR (der Selbsttest lief nicht
     mehr, drei Aufrufe hintereinander rot) | ohne Anfuehrungszeichen
       geschrieben. In Kommentaren sind `„…"` harmlos, in Zeichenketten
       nicht — und die Datei tut beides.
```

**K0: 17 von 28** (26 Mensch, 27 Mensch, 28 Tor).

**Was dieser Nachtrag über die Probe sagt.** Sie war zwei Stunden alt und
hat in dieser Zeit: einen echten Befund gemeldet (zwei Commits ohne
Hakenlauf), eine Lücke in sich selbst gezeigt (drei Ursachen, nicht
unterscheidbar), und nach deren Schließen eine Aussage geliefert, die ohne
sie nicht zu haben war — *dass* der Haken nicht lief, mit ausgeschlossenen
Alternativen. Das ist der Unterschied zwischen einem Tor und einem Skript
mit einem Haken davor.

**Und die richtige Lesart des grünen Tores ist damit enger als gedacht:**
„meine Nummern kollidieren nicht mit bekannten" — nicht „Nummern
kollidieren nicht".

### Nachtrag 13 zur Bilanz 2026-09-29 — Eigentümerentscheidung: Tor 72 ist herabgestuft

Der Eigentümer hat einen Widerspruch benannt, den ich selbst gebaut hatte:
**P3-680 und „verbindliches Tor" können nicht beide gelten.** Ein Tor, das
als verbindlich *gilt* und nur eine Seite bindet, ist kein P3, sondern
mindestens ein P1. Entscheidung vom 2026-09-29: **Tor 72 ist eine lokale
Vorprüfung gegen den bekannten Stand**, kein verbindliches Tor.

```
 29. **Ich hatte Tor 72 eine Zusage gegeben, die es technisch nicht halten
     kann** — und den Widerspruch dann als P3 eingetragen, statt ihn
     aufzulösen. Der Kopf des Skripts sagte „Eine MF-Nummer gehoert genau
     einem Commit"; gemessen bindet es nur die Seite, die den Haken
     durchläuft | Mensch (Eigentümer) | herabgestuft an VIER Stellen:
       Skriptkopf, Hakenkommentar, CI-Kategorie (jetzt „MF-Nummer:
       Vorpruefung intakt"), CLAUDE.md samt Aufgabenteilung-Tafel; P3-680
       umgeschrieben statt entfernt (MF-1077), Gegenstand ist jetzt die
       FEHLENDE Serverprüfung samt zehn Abnahmekriterien

 30. **Ein Selbsttestfall war auf die SEKUNDE flackernd.** „mit Vermerk: 1
     Commit betrachtet, 0 ohne" war einmal rot und einmal grün. Ursache:
     ich schrieb im Test den Vermerk NACH dem Commit — in Wirklichkeit
     läuft der Haken davor, also ist die Commit-Zeit nie kleiner als die
     Vermerk-Zeit. Bei Sekundengleichheit grün, sonst rot, weil
     `ohne_vermerk()` alles vor dem ersten Vermerk ausnimmt
     | TOR (der Fall wurde rot) | Reihenfolge umgedreht, dreimal
       hintereinander 45/45. Und der Kommentar sagt jetzt, WARUM die
       Reihenfolge so ist — sonst dreht sie der nächste zurück

 31. Sechstes Mal `python -` mit leerer Eingabe (diesmal als
     Platzhalter `python - --nichts`), Prozess wartet auf stdin, per
     TaskStop beendet | Mensch | dieselbe Folge wie bei Fehler 23 und
       dieselbe Lehre; die Wiederholung selbst ist der Befund und steht
       hier, statt sie mit Fehler 23 zu verrechnen
```

**K0: 16 von 31.**

### Was gebaut ist

**Die Nachprüfung** (`--nachpruefen`), nach dem Feldsatz des Eigentümers und
in einer **eigenen** Datei neben dem Laufbuch:

```
status: nachgeprueft
commit: 97b2888a
claim: MF-1601
urspruenglicher-hook-nachweis: fehlt
ursache: nicht-gemessen
nachpruefung: bestanden
nachpruefung-zeitpunkt: 2026-09-29T11:53:18Z
```

Drei Sätze angelegt (`97b2888a`/MF-1601, `cc204479`/MF-1602,
`16880094`/MF-1603), **alle bestanden** — und die Prüfung ist echt, nicht
ein Stempel: sie stellt dieselbe Frage wie der Haken und wird von einer
Mutation entlarvt, die sie zum Stempel macht.

**Die Gegenrichtung ist die wichtigste Zusage:** `--betrieb` führt diese
Commits weiter als „ohne Vermerk" und nennt die Nachprüfung getrennt. Eine
Mutation, die den Nachweis als Hakenlauf zählt, lässt den Selbsttest fallen
(43/45). Ein nachgetragener Eintrag im **Laufbuch** würde eine historische
Sicherheit vortäuschen und findet nicht statt.

Selbsttest **35 → 45**, Mutationsmatrix **6 von 6** für die neuen Klammern
(zusammen mit den acht aus MF-1528/1530: 14 von 14). Und `ohne_vermerk()`
ist jetzt die EINE Rechnung, die Probe und Nachprüfung gemeinsam benutzen —
zweimal gerechnet driftet sie (MF-1177).

### Was NICHT gebaut ist, und warum

Punkte 2 und 3 des Auftrags — **Serverprüfung und Merge-Serialisierung** —
kann diese Sitzung nicht herstellen: sie brauchen Depotverwaltung und einen
Push, und beides habe ich nicht. Sie stehen mit allen zehn Abnahmekriterien
und den drei Serialisierungswegen in P3-680, dazu der Ausnahmeweg mit
seinen acht Pflichtfeldern. **Eine reine CI-Prüfung genügt nicht** — zwei
Zweige können dieselbe Nummer beanspruchen und beide gegen denselben alten
Zielstand grün werden.

Und die Bedingung des Eigentümers für MF-1530 ist erfüllt: dass Tor 72
vorläufig nur eine lokale Vorprüfung ist, steht jetzt ausdrücklich im Baum.
Gepusht ist weiterhin nichts.
