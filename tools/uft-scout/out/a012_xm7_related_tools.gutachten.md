# A-012 · Gutachten `yas-sim/xm7-related-tools` — FM-7-Werkzeuge

**Stand:** 2026-09-16 · **Posten:** `A-012` · **Lizenz:** **MIT**, © 2022 Yasunori Shimura
**Prüfstand:** `origin/main` = `70a940af` · Klon: `tools/uft-scout/work/xm7-related-tools`

---

## Ergebnis in drei Sätzen

**Die erste Frage des Postens ist mit Nein zu beantworten, und zwar
gemessen: dieses Repo liefert KEINEN FM-Erzeuger.** `P3-389` meint die
FM-**Kodierung**, und die kommt hier nirgends vor — die einzigen beiden
Treffer für `FM`/`MFM` in 70 Quelldateien sind Kommentare zu einem
**Dichte-Byte** im D77-Behälter. Was das Repo stattdessen hat, ist wertvoll
und im Baum vollständig abwesend: eine **FM-7-Dateisystem-Bibliothek** und
das **Bandformat T77**.

**Empfehlung: REGISTER für zwei Teile, uninteressant für elf.** Die Lizenz
ist MIT — **Port ist zulässig**, als einzige dieser Reihe.

---

## 1. Die Frage von P3-389, beantwortet (b)

`P3-389` sagt wörtlich: *„Kein FM-Encoder, und das ist benannt: `P3-218`
schliesst woertlich mit ‚kein FM-Encoder (wahr, deshalb die Fremdabnahme
durch `fluxtoimd`)'."* Gemeint ist die **Kodierung** — Taktzellen auf der
Diskette —, nicht der Rechner FM-7.

**Gemessen über alle 70 Quelldateien des Repos**, Suche nach `MFM`,
`FM encod`, `clock bit`, `IDAM`, `0x4489`, `cell`:

| Treffer | Art |
|---|---|
| `fmtools/fmfslib/cfloppy.h:17` | Kommentar: `uint8_t m_fDensity; /* 00:MFM 40:FM */` |
| `fmtools/fmfslib/d77img.h:21` | Kommentar zum Statusbyte |

**Zwei Treffer, beide Kommentare, beide über ein Feld im Behälter.** Es gibt
im ganzen Repo keine Stelle, die Taktzellen erzeugt.

**Und `wav2t77` ist nicht die Ausnahme, die es zu sein scheint.** Sein
Dateibestand ist eine **Audio**-Kette: `cagc` (Verstärkungsregelung),
`ccomparator`, `cfilter`, `clpf` (Tiefpass), `cwave`. Das demoduliert
**Band-FSK** aus einer WAV-Datei — eine andere Modulation, ein anderes
Medium, ein anderer Zweck. `t772wav` geht die Gegenrichtung, aber ebenfalls
zu **Bandaudio**, nicht zu Diskettenfluss.

**Antwort auf (b): nein.** `P3-389` bleibt offen, und dieses Repo trägt
nichts dazu bei. Das ist ein Ergebnis, kein Fehlschlag — die Frage war
ausdrücklich „die erste zu klärende Frage, nicht ihre Antwort".

---

## 2. Die zwanzig Werkzeuge, je mit Urteil (a)

Gemessen je Verzeichnis (Dateien und Zeilen über `*.c`, `*.cpp`, `*.h`):

| Werkzeug | Dateien | Zeilen | Urteil | Grund |
|---|---|---|---|---|
| **`fmtools`** (mit `fmfslib`) | **24** | **3460** | **brauchbar** | FM-7-DISK-BASIC-Dateisystem; im Baum gibt es das nicht (§3) |
| **`wav2t77`** | 15 | 920 | **Oracle-Kandidat** | erzeugt T77 aus Bandaudio; T77 fehlt im Baum ganz (§4) |
| **`t772wav`** | 5 | 519 | **Oracle-Kandidat** | Gegenrichtung; zusammen ein Rundlauf |
| **`t77dec`** | 1 | 332 | **brauchbar** | T77-Zerleger, kleinster Einstieg in das Format |
| **`d77enc_dec`** | 2 | 322 | **Oracle-Kandidat, hebt aber nichts** | D77 steht bereits auf **T1b** |
| **`d77uty`** | 1 | 341 | **brauchbar** | D77-Dienstprogramm, zweite Hand auf denselben Behälter |
| `mot2bin2` | 5 | 864 | uninteressant | S-Record ↔ Binär, keine Diskettenarbeit |
| `dmp2mot` | 3 | 679 | uninteressant | dito |
| `mot2bin` | 3 | 627 | uninteressant | dito |
| `bin2mot` | 4 | 334 | uninteressant | dito |
| `krom` | 1 | 137 | uninteressant | ROM-Werkzeug |
| `fontp` | 1 | 137 | uninteressant | Zeichensatz |
| `seven2av` | 1 | 117 | uninteressant | Grafik |
| `fdump` | 1 | 90 | uninteressant | Hexdump |
| `romcut` | 1 | 67 | uninteressant | ROM schneiden |
| `bincut` | 1 | 52 | uninteressant | Datei schneiden |
| `dmygen` | 1 | 22 | uninteressant | Füllbytegenerator |
| `BootROM` | **0** | 0 | **Spec-Kandidat** | nur Listings/Binärdateien — der Kanal wäre *Spec* |
| `nosys_ipl` | **0** | 0 | Spec-Kandidat | dito |
| `subtfr` | **0** | 0 | Spec-Kandidat | dito |

**Gezählt: 6 brauchbar oder Oracle-Kandidat, 11 uninteressant, 3 ohne
Quelltext.** Die 11 uninteressanten machen 2 419 von 8 020 Zeilen aus und
haben mit Disketten nichts zu tun — S-Record-Umwandler für
6809-Entwicklung.

---

## 3. `fmtools/fmfslib` — das FM-7-Dateisystem

Bestand: `cfilesys.cpp/.h`, `cfdimg.cpp/.h`, `cfloppy.h`, `d77img.h`,
`fmerr.h`, `fmfslib.h`, `sysdef.h`.

**Gegen den Baum gemessen:**

| Suche | rc | Treffer |
|---|---|---|
| `FM-7` | 0 | 8 — u. a. `src/formats/d77/uft_d77.c`, `include/uft/core/uft_encoding.h` |
| `DISK BASIC` | 0 | 2 — **beide MSX**, keiner FM-7 |

**Der Baum kennt FM-7 als Plattform und D77 als Behälter — das Dateisystem
darin kennt er nicht.** Das deckt sich mit der Aufnahme („kein
FM-7-Dateisystemeintrag in `docs/VERIFICATION_TIERS_FS.md`").

**Kanal: Port.** MIT nach GPL-2-or-later ist permissiv in Copyleft,
unproblematisch. Das ist die **einzige** der bisher begutachteten Quellen,
bei der ein Port offensteht — A-008 war MIT, aber inhaltlich erledigt,
A-009 hatte gar keine Lizenzzeile, A-010/A-011 sind hauseigen.

**Aber die EINFRIER-REGEL steht davor.** Ein neues Dateisystem ist neuer
Code im Format-Layer; er braucht Rotbeweis-zuerst, eine benannte Referenz
im Header und ein Abbild von fremder Hand. Das Repo liefert die Referenz
(den Quelltext) und über `d77enc_dec` auch einen Erzeuger — **die
Verifikationskette wäre also vollständig führbar**, und das ist selten.

---

## 4. T77 — im Baum vollständig abwesend

| Suche | rc | Treffer |
|---|---|---|
| `t77` | **1** | **0** |
| `T77` | **1** | **0** |

**Null.** Das Bandformat des FM-7 gibt es im Baum nicht — weder als Plugin
noch als Erwähnung.

**Ob das eine Lücke ist, ist eine Eigentümer-Frage, keine technische.** UFT
ist ein Disketten-Werkzeug; CLAUDE.md führt zwar `TAP`, `T64` und `TZX` als
unterstützt, aber `docs/SCOPE_DECISION_NON_FLOPPY.md` hält fest, dass
Nicht-Disketten-Inhalte 2026-05-25 per Eigentümer-Entscheidung **gelöscht**
wurden (Option C, ausgeführt MF-271).

**Das Gutachten entscheidet das nicht.** Es hält fest: die Mittel lägen
bereit (drei Werkzeuge, 1 771 Zeilen, MIT, mit Erzeuger **und** Leser für
einen Rundlauf), und die Einordnung ist eine Scope-Entscheidung.

---

## 5. Die fünf Fragen (c)

### 5.1 „wo können die formate verbessert werden"

`d77` steht auf **T1b** (`test_d77_gegen_hxcfe`, `test_d88_header_variants`,
`test_oeffentliche_api_am_korpus`). `d77enc_dec` wäre eine **zweite fremde
Hand** auf denselben Behälter — das hebt keine Stufe, härtet aber den
Beleg. Nach Regel 9 bewegt es keine Kennzahl: **Fundus**.

### 5.2 „ist es auf andere formate übertragbar"

**Das FM-7-Dateisystem nicht** — es ist ein Einzelfall. **Der
Rundlauf-Aufbau schon:** Erzeuger (`wav2t77`) und Leser (`t77dec`) aus
derselben Hand, dazu `t772wav` als Rückweg. Das ist die Konstellation, die
MF-1033 bei `nanowasp` zum Beleg gemacht hat — mit der Einschränkung, dass
zwei Werkzeuge derselben Hand **keine** zwei unabhängigen Hände sind.

### 5.3 „welche einstellungen fehlen noch"

Keine, die dieses Repo beantwortet. Seine Werkzeuge sind
Kommandozeilenumwandler ohne Einstellungsebene.

### 5.4 „was habe wir noch nicht"

Zwei Dinge, beide gemessen: das **FM-7-Dateisystem** (§3) und **T77** (§4).

### 5.5 „brauch es eine HAL-Erweiterungen"

**Nein.** Alles hier arbeitet auf Dateien.

---

## 6. Disposition

| Teil | Urteil | Kanal |
|---|---|---|
| `fmtools/fmfslib` (FM-7-Dateisystem) | **REGISTER** | **Port** (MIT), unter EINFRIER-REGEL |
| `t77dec`, `wav2t77`, `t772wav` | **REGISTER** als Fundus | Port/Oracle — Scope-Frage zuerst (§4) |
| `d77enc_dec`, `d77uty` | **PARTIAL** | Oracle; hebt keine Stufe, härtet den Beleg |
| `BootROM`, `nosys_ipl`, `subtfr` | **REFERENCE** | *Spec* (Listings, kein Quelltext) |
| die elf S-Record-/ROM-/Grafikwerkzeuge | **uninteressant** | — |

**Kein Byte dieses Repos ist nach `src/`, `include/` oder `tests/`
geschrieben worden.** Der Klon liegt unter `tools/uft-scout/work/`, das ist
gitignoriert.

---

## 7. Vorschläge für `docs/OPEN_ITEMS.md`

1. **`P3-389` bleibt offen, und dieses Repo trägt nichts bei** — gemessen
   über 70 Quelldateien: zwei `FM`/`MFM`-Treffer, beide Kommentare zu einem
   Dichte-Byte (§1). Nachtrag zu `P3-389`, kein neuer Befund.
   *Kennzahl:* keine der vier.
2. **Das FM-7-Dateisystem ist portierbar, und die Verifikationskette wäre
   vollständig** — MIT-Quelle als Referenz, `d77enc_dec` als Erzeuger,
   `d77` bereits auf T1b (§3). Das ist selten genug, um es zu führen.
   *Kennzahl:* mittelbar **FS-Stufen**, wenn es gebaut wird.
3. **T77 fehlt vollständig (rc 1, 0 Treffer), und ob es fehlen SOLL, ist
   eine Scope-Entscheidung** — `docs/SCOPE_DECISION_NON_FLOPPY.md` hat
   Nicht-Disketten-Inhalte 2026-05-25 gelöscht, während CLAUDE.md drei
   Bandformate als unterstützt führt (§4). *Kennzahl:* keine der vier; es
   ist eine Eigentümer-Entscheidung.

---

## 8. Quellen

* `https://github.com/yas-sim/xm7-related-tools`, flach geklont 2026-09-16
  nach `tools/uft-scout/work/xm7-related-tools`
* **MIT License, © 2022 Yasunori Shimura** (`LICENSE.md`); **0** der 70
  Quelldateien trägt eine eigene SPDX-Zeile
* Umfang gemessen: 70 Quelldateien, 20 Werkzeugverzeichnisse, davon 3 ohne
  Quelltext; 8 020 Zeilen gesamt
* Baum: `origin/main` = `70a940af`
* Gegengehalten: `P3-389`, `P3-218`, `MF-271` /
  `docs/SCOPE_DECISION_NON_FLOPPY.md`, `MF-1033`, EINFRIER-REGEL
