# Integrationsplan: die dreizehn Formate der SAMdisk-Zulieferung

Stand: 2026-09-21 (MF-1332). Zulieferung:
`neue-ideen/UFT_missing_formats_integration_plan.zip`, 22 220 Byte,
21 Eintraege, Eigentuemer-Beitrag. Urteil im Register:
[`neue_ideen_urteile.json`](neue_ideen_urteile.json).

Dieses Dokument ist **kein** Uebernahmebeschluss. Es haelt fest, was die
Zulieferung vorschlaegt, was davon am Baum **gemessen** zutrifft, und in
welcher Reihenfolge die Arbeit unter der EINFRIER-REGEL ueberhaupt
zulaessig ist.

---

## 1. Die Sperre zuerst: das Moratorium haelt

Die EINFRIER-REGEL (MF-363, praezisiert MF-498) verbietet neue
Format-Plugins, bis **ATR, D64, ADF, FDI und NFD-r0** auf T1/T1b stehen.
Gemessen am erzeugten `docs/VERIFICATION_TIERS.md`, nicht an Prosa:

| Format | Stufe | Zeile |
|---|---|---|
| `atr` | **T1b** | 139 |
| `d64` | **T1b** | 179 |
| `adf` | **T1b** | 134 |
| `fdi` | **T1** | 101 |
| `nfd` | **T2** | 443 |

**Vier von fuenf sind erfuellt. `nfd` ist es nicht.** Damit gilt das
Moratorium unveraendert, und **neun der dreizehn Formate dieser
Zulieferung sind gesperrt** — nicht aus Vorsicht, sondern weil eine
Registrierung ein neues Plugin ist und die Regel das ausdruecklich
nennt: „neues Format-Plugin, neue Format-Variante, neuer Decoder, **neue
Registrierung**".

Stufenstand gesamt zum selben Zeitpunkt: T1 = 8, T1b = 70, T2 = 7,
T3 = 2, n/a = 2, **Summe 89**. Die 89 schliesst das unversionierte
`src/formats/a2r/uft_a2r_plugin.c` ein; gegen HEAD sind es 88.

---

## 2. Ist-Zustand, gemessen statt uebernommen

Die Statusmatrix der Zulieferung (§1.2) nennt je Format einen
Ist-Zustand. Jede dieser Zeilen ist eine **Aussage ueber den Baum** — in
diesem Projekt die Sorte Aussage, die zuletzt dreimal falsch war
(MF-1064, MF-930, MF-1226). Sie wurde deshalb nachgemessen: je Format
vier getrennte Fragen — Quelldateien, Plugin-Symbol, Registry-Eintrag,
Tier-Zeile.

| Format | UFT-eigene Dateien | Plugin-Symbol | Registry | Stufe |
|---|---|---|---|---|
| `dti` | **0** (nur `src/samdisk/dti.cpp`) | 0 | nein | – |
| `cwtool` | **0** (nur `src/samdisk/cwtool.cpp`) | 0 | nein | – |
| `dfi` | `include/uft/formats/dfi.h`, `src/formats/flux/dfi.c` | **1 — nur `extern`** | nein | – |
| `d2m` | **0** (nur `src/samdisk/d2m.cpp`) | 0 | nein | – |
| `d4m` | **0** (nur `src/samdisk/d4m.cpp`) | 0 | nein | – |
| `lif` | `include/uft/formats/lif.h`, `src/formats/hp/lif.c` | 0 | nein | – |
| `qdos` | `include/uft/formats/qdos.h`, `src/formats/ql/qdos.c` | 0 | nein | – |
| `opd` / `opus` | 7 Dateien | 8 | **ja** | **T1b** |
| `mbd` | **0** (nur `src/samdisk/mbd.cpp`) | 0 | nein | – |
| `pdi` | **0** (nur `src/samdisk/pdi.cpp`) | 0 | nein | – |
| `s24` | **0** (nur `src/samdisk/s24.cpp`) | 0 | nein | – |
| `ds2` | **0** (nur `src/samdisk/ds2.cpp`) | 0 | nein | – |
| `dsc` | **0** (nur `src/samdisk/dsc.cpp`) | 0 | nein | – |

Gemessen mit vier getrennten Fragen je Name, wobei der Name als eigenes
Wort im Pfad gesucht wurde und nicht als Teilstring — `lif` steckt sonst
in `qualified`, `ds2` in `ds2x` (Klasse MF-1247).

### 2.1 Drei Berichtigungen an der Zulieferung

**(a) „D2M/D4M: Leser/Writer vorhanden" trifft nicht zu.** Die einzigen
Dateien sind `src/samdisk/d2m.cpp` und `d4m.cpp` — der **vendorte fremde
SAMdisk-Quelltext** (MIT, `src/samdisk/License.txt`), kein UFT-Code. Die
Zulieferung widerspricht damit ihrem eigenen §1.1, das woertlich sagt:
*„Eine C-Datei im Baum ist noch keine Produktfaehigkeit."* Beide gehoeren
in die Gruppe **wirklich neu**, nicht in **vorhanden**.

**(b) `dfi` ist schaerfer kaputt als beschrieben.** Die Zulieferung sagt
„Grundcode vorhanden, Pluginadapter fehlt". Gemessen gibt es zusaetzlich
eine **Deklaration ohne Definition**:

```
include/uft/uft_format_plugin.h:1502:
    extern const uft_format_plugin_t uft_format_plugin_dfi;
```

Das ist der einzige Treffer im ganzen Baum — **keine Definition**, **kein
Registry-Eintrag**. Eine Tuer mit Namensschild ohne Raum dahinter, die
Klasse Phantom-API (MF-366). Wer `dfi` verdrahten will, findet ein
Symbol vor, das aussieht, als gaebe es das Plugin schon.

**(c) `opd`/`opus` ist besser dran, als die Zulieferung weiss.** Sie
fuehrt es als „falsch katalogisiert". Gemessen steht es seit MF-1084 auf
**T1b**, mit fuenf Tests und einem Abbild aus MAMEs `floptool`. Der
Katalogpunkt bleibt richtig — zwei Namen, ein Plugin —, aber es ist
Feinarbeit an einem belegten Format, keine Reparatur.

---

## 3. Reihenfolge, nach Zulaessigkeit sortiert

Die Zulieferung empfiehlt eine Reihenfolge nach Aufwand (README). Unter
der EINFRIER-REGEL ist die bindende Sortierung eine andere: **was ist
erlaubt, was ist gesperrt.**

### 3.1 Erlaubt und ohne Vorbedingung

| # | Arbeit | Kennzahl | Warum erlaubt |
|---|---|---|---|
| 1 | **`dfi`-Phantom aufloesen** — die `extern`-Zeile entfernen *oder* das Plugin definieren und registrieren | Fundus | Behebung eines Defekts an Bestehendem; die Regel erlaubt Bugfixes ausdruecklich |
| 2 | **`opd`/`opus`-Katalog** — ein Plugin, zwei Namen und Endungen | Fundus | Katalogkorrektur, kein neuer Parser |
| 3 | **CopyPlan auf getrennte Quell-/Zielfaehigkeiten** (§03 der Zulieferung) | Fundus | beruehrt die Formatschicht nicht |

Punkt 1 ist der einzige, der eine **Auswahl statt einer Entscheidung**
verlangt (MF-1077): Entfernen waere eine Loeschung in der Formatschicht
und braucht Eigentuemer-Zustimmung samt `Ruecknahme:`-Zeile; Definieren
waere neuer Code und faellt unter 3.3; Stehenlassen ginge nur
beschriftet (MF-699).

### 3.2 Der Schluessel: `nfd` auf T1/T1b

**Solange `nfd` auf T2 steht, bleiben neun Formate gesperrt.** Diese eine
Hebung oeffnet alles andere, und die Lage dazu hat sich gemessen
veraendert.

`docs/OPEN_ITEMS.md` (P3-1) begruendet die Sperre damit, die beiden
NFD-faehigen Werkzeuge **FIVEC** und **d88split** stuenden „im Kopf von
`src/formats/nfd/uft_nfd_plugin.c` als die Quellen, gegen die MF-358 den
Leser geschrieben hat" — ein Abbild von ihnen belege also nur, wovon UFT
abgeleitet ist.

Gemessen je Bezeichner traegt das **nur zur Haelfte**: `FIVEC` kommt im
Kopf des UFT-Lesers **nicht vor**. Es ist damit eine unabhaengige Hand,
und die Abstammungs-Absage gilt fuer `d88split`, nicht fuer FIVEC.

**Das ist keine Freigabe, sondern ein geoeffneter Weg.** Was fehlt, ist
ein NFD-Abbild aus FIVEC — eine Beschaffungsentscheidung des
Eigentuemers, wie seinerzeit Amiga Forever bei X-Copy.

### 3.3 Gesperrt, mit Grund

| Format | Gruppe | Sperrgrund |
|---|---|---|
| `dti` | wirklich neu | Moratorium (`nfd` T2) |
| `cwtool` | wirklich neu | Moratorium |
| `d2m`, `d4m` | wirklich neu (berichtigt, s. 2.1a) | Moratorium |
| `mbd` | wirklich neu | Moratorium |
| `pdi` | wirklich neu | Moratorium |
| `lif` | Verdrahtung vorhandenen Codes | Moratorium — es gibt **kein** Plugin-Symbol, eine Registrierung waere neu |
| `qdos` | Verdrahtung vorhandenen Codes | dito |
| `s24`, `ds2`, `dsc` | unklar belegt | **doppelt** gesperrt: Moratorium **und** fehlende Spezifikation/Korpus |

Nach dem Fallen des Moratoriums gilt zusaetzlich **1:2** — ein neues
Format kostet zwei Hebungen.

---

## 4. Sachbefunde der Zulieferung — uebernommen, nicht gemessen

Die Zulieferung macht am gelieferten SAMdisk-Code vier Sachbefunde
(§1.3). Sie sind hier **nicht nachgemessen** und stehen ausdruecklich als
uebernommene Behauptungen:

* **DTI** — der Writer legt die Nutzlast hinter einem 3-Byte-Spurkopf ab,
  der Reader beginnt nach 2 Byte. Wahrscheinlicher Off-by-one; **darf
  nicht uebernommen werden**. DTI ist kein Flussformat, sondern ein
  Container fuer ACE-kodierte Spurdaten mit Fehlerflag.
* **CWTool v3** — der Code nimmt nur Version 3 an, weist nicht
  index-ausgerichtete Bilder ab, schaetzt die Kopfzuordnung aus der
  Satzzahl und deutet mehrere Saetze je Spur als Umdrehungen. Das sind
  **Einschraenkungen der Umsetzung**, keine Formateigenschaften, und
  gehoeren als solche gemeldet.
* **PDI** — der RLE-Entpacker hat kein Ausgabelimit; eine kleine Datei
  kann eine sehr grosse Ausgabe erzeugen. Ein hartes, formatabhaengiges
  Limit ist Pflicht.
* **MBD** — Geometrie aus dem Bootsektor; drei einzelne Marker sind nach
  der Sonden-Doktrin keine hinreichende Kennung.

Der letzte Punkt deckt sich mit der Hausregel aus
[`SONDEN_DOKTRIN.md`](SONDEN_DOKTRIN.md): **ohne Kennung ist die
Obergrenze 45.**

## 5. Was ausdruecklich nicht uebernommen wird

Aus §1.4 der Zulieferung, hier bestaetigt und um die Baumregeln
ergaenzt:

* keine mitgelieferten CAPSImage- oder FTDI-Binaerdateien;
* kein SAMdisk-internes C++-Diskmodell;
* keine pauschalen Geometrieschaetzungen — der Baum hat dafuer einen
  frisch gemessenen Fall: `uftc_convert_img_to_imd()` waehlt fuer eine
  819 200-Byte-Datei die Geometrie 80x2x15, waehrend die Datei
  80x2x10 ist, und schreibt 800 nicht vorhandene Sektoren als **gueltige**
  0xE5-Sektoren;
* keine starren Maximalwerte, die Daten still abschneiden;
* kein Parsercode ohne reproduzierbaren Fremdtest.

Die C-Beispiele unter `examples/` sind **eigenstaendige Geruste unter
GPL-2.0-or-later**, kein SAMdisk-Code, und mit dem GPL-2-Baum vereinbar.
Uebernehmbar sind sie trotzdem erst nach dem Fallen des Moratoriums —
ein Geruest ist ungeprueft im Sinne von MF-498.

---

## 6. Offene Entscheidungen fuer den Eigentuemer

1. **`dfi`-Phantomzeile**: entfernen (Loeschung in der Formatschicht,
   braucht Zustimmung), definieren (neuer Code, gesperrt) oder
   beschriftet stehen lassen (MF-699).
2. **FIVEC beschaffen?** Der gemessen offene Weg, `nfd` und damit das
   Moratorium zu loesen.
3. **`s24`/`ds2`/`dsc`**: zurueckgestellt lassen, oder Spezifikation
   suchen lassen (Auftrag an `uft-scout`).

Solange keine davon entschieden ist, bleibt von den dreizehn Formaten
genau das bearbeitbar, was in Abschnitt 3.1 steht.
