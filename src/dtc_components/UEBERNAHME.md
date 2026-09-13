# `src/dtc_components/` — uebernommener Fremdbestand (MF-1099)

**Diese Datei gehoert UFT. Alle uebrigen Dateien in diesem Verzeichnis
sind unveraendert uebernommen und werden nicht angefasst** — wer sie
aendert, macht aus einem Beleg eine Ableitung.

---

## Herkunft

| | |
|---|---|
| Quelle | `neue-ideen/code-schnipsel.zip` (nicht versioniert, `.gitignore`) |
| Paketname | „DTC-derived analysis components (independent C reconstruction)" |
| Angegebener Urheber | EMUUAC, 2026 |
| Angegebene Lizenz | MIT — Volltext in [`LICENSE`](LICENSE) |
| Uebernommen am | 2026-09-13, Zustand des Archivs von **20:13** |
| SHA-256 des Archivs | `d90109037634a3c488999a1aea2f3d68aaa2c617b2f3b0ac3ab2526c96fb41bc` |
| Uebernommene Dateien | **18**, vollstaendig, byteweise |

## Die Lizenzlage, und sie ist nicht einfach

Der MIT-Text traegt einen **ausdruecklichen Ausschluss**, woertlich:

> This license covers the independently authored files in this package.
> It does not grant rights in any third-party DTC/KryoFlux/SPS binary,
> **decompiler output**, format database, trademark, or proprietary
> file-format specification.

Und das Paket sagt in [`RECONSTRUCTION_STATUS.md`](RECONSTRUCTION_STATUS.md)
ueber sich selbst:

> The supplied **decompiler output** recovers 116 named routines from a
> much larger ARM64 text section.

Die Erteilung soll also „die unabhaengig verfassten Dateien" decken —
verfasst wurden sie nach der eigenen Darstellung **aus** dem Material,
das dieselbe Lizenz ausnimmt. Das ist die Gestalt von **P0-5** in
diesem Baum (`SPDX: MIT` auf einem GPLv2+-Nachbau, hat ein Release
blockiert), nur mit proprietaerer statt copyleft-Herkunft: `dtc` ist in
[`docs/ORACLES.md`](../../docs/ORACLES.md) als **Oracle** gefuehrt —
*Ausfuehrung frei, Weitergabe nicht*.

**Die Uebernahme ist deshalb eine ausdrueckliche
Eigentuemer-Entscheidung**, getroffen am 2026-09-13, nach Vorlage genau
dieses Befundes. Sie ist als solche festgehalten und nicht als
Ergebnis einer Lizenzpruefung; die Zeile steht in
[`docs/QUARANTINE.md`](../../docs/QUARANTINE.md).

## Eine Datei ist aus dem Archiv verschwunden, und das ist gemessen

Das Archiv lag um **20:01** in einer anderen Fassung vor als um
**20:13**:

| | 20:01 | 20:13 (uebernommen) |
|---|---|---|
| Archivgroesse | 15 490 B | 14 581 B |
| Dateien | **19** | **18** |
| unkomprimiert | 23 056 B | 21 773 B |
| `SOURCE_MAP.md` | vorhanden | **nicht mehr enthalten** |
| `LICENSE`, SHA-256 | `2f0375582cb9b2a4…` | `f924d7ed3358cc54…` |

Die mitgelieferte [`SHA256SUMS`](SHA256SUMS) fuehrt weiterhin den
**alten** LICENSE-Hash. Bei der Uebernahme stimmten deshalb **16 von 17**
gelisteten Dateien exakt; die einzige Abweichung ist die Lizenzdatei
selbst. Der Ausschlusssatz oben ist in beiden Fassungen **wortgleich** —
was sich sonst geaendert hat, ist von hier aus nicht feststellbar, weil
die 20:01-Fassung nicht aufbewahrt wurde.

`SOURCE_MAP.md` war die **spezifischste Herkunftsangabe** des Pakets.
Sie ordnete die wiedergewonnenen Symbolfamilien den Modulen zu; ihr
tragender Satz lautete:

> The supplied decompiler output exposed 116 named functions. The
> reusable technical areas were mapped as follows: `CBitBuffer::*` →
> `src/bitbuffer.c`, `MakeCRCTable`/`CalcCRC*` → `src/crc.c`,
> `CDiskEncoding::InitFM/InitMFM` → `src/encoding.c`, …

Diese Datei wird hier **nicht nachgebaut** — ein Beleg, den man selbst
schreibt, ist keiner. Festgehalten ist, dass es sie gab und was sie
sagte; die Aussage ueber die Herkunft steht ohnehin unveraendert in
`RECONSTRUCTION_STATUS.md`.

## Bauzustand: zuschaltbar, Vorgabe AUS (MF-1100)

**Im Vorgabebau wird keine einzige dieser Dateien uebersetzt.** Der
Eintrag in `NOT_BUILT_BY_DESIGN` (`scripts/verify_build_sources.py`)
bleibt deshalb richtig, und `verify_build_sources.py` meldet **0 neue
Abweichungen**.

Zugeschaltet wird ueber je ein Flag, auf beiden Bauwegen:

```
cmake -S . -B build -DUFT_WITH_DTC_COMPONENTS=ON
qmake CONFIG+=uft_dtc_components
```

**Gegenprobe, gemessen:** mit ausgeschalteter Option nennt der
CMake-Lauf `dtc_components` **null Mal**, und `ctest -N` fuehrt **kein**
Ziel dieses Namens. Eingeschaltet entsteht die statische Bibliothek
`dtc_components` (10 Module) und das ctest-Ziel `test_dtc_components`.

### Was dabei gemessen wurde, und was nicht

| Prüfung | Ergebnis |
|---|---|
| 10 Module, `-Wall -Wextra -Wpedantic -Werror`, gcc 13.1.0 | **10 gruen, 0 rot** — keine einzige Warnung |
| Testreihe mit `-UNDEBUG` | „all tests passed", Kode 0 |
| Testreihe mit `-DNDEBUG` (CMake-Release) | druckt dasselbe und **prueft nichts**, Kode 0 |
| Rotbeweis: ein CRC-Erwartungswert verfaelscht, `-UNDEBUG` | bricht ab, **Kode 3** |
| Release-Typ: kommt `-UNDEBUG` am Ziel an? | Befehlszeile traegt `-DNDEBUG -O3 -UNDEBUG` — das `-U` gewinnt |
| ctest, zugeschaltet | `test_dtc_components` **bestanden** |
| libm noetig? | unter MinGW nein (gemessen); fuer glibc als **Link-Option** `-lm` gesetzt, nicht als Bibliothek — ein roher Bibliotheksname macht die Coverage- und Sanitizer-Laeufe rot (`LINK_LIBRARIES_ONLY_TARGETS`) |
| **ASan / UBSan** | **hier NICHT messbar** — die MinGW-Toolchain hat weder `libasan` noch `libubsan` (`cannot find -lasan`). Deshalb schalten die beiden Linux-Jobs in `.github/workflows/sanitizers.yml` die Option ein; **gemessen wird dort, nicht hier** |

Der `NDEBUG`-Befund ist der wichtigste: die Reihe prueft ausschliesslich
mit `assert()`, und ohne `-UNDEBUG` waere sie im gatenden Release-Lauf
ein leerer Ausdruck, der Erfolg meldet. Das ist woertlich die Lage aus
**MF-830** (21 Testdateien, die in genau dem Job nicht rot werden
konnten) und die Form aus **MF-596**.

**Was die Reihe nicht leistet und was deshalb hier steht:** sie meldet
„all tests passed" ohne Nenner. Die fremde Datei wird dafuer **nicht**
umgeschrieben — wer sie aendert, macht aus einem Beleg eine Ableitung.
Der Nenner steht stattdessen im Kommentar der CMake-Option: **sieben**
Pruefgruppen (CRC-32 und CRC-16/CCITT gegen die Referenzeingabe
`"123456789"`, MFM-Rundlauf, Commodore-GCR-Rundlauf, Bitpuffer,
Flussstatistik, Formaterkennung, CT-Raw-Rundlauf).

### Was damit noch NICHT geschehen ist

Der Bestand ist **baubar und gepruefbar**, aber **kein Produktpfad ruft
ihn**. Ein Modul in den Lese- oder Dekoderpfad zu haengen ist ein
eigener Schritt und faellt unter die EINFRIER-REGEL: benannte Referenz
oder Rotbeweis zuerst, jede Zahl gemessen, Referenz im Header. Die
Orakel dafuer liegen bereit und stehen je Scheibe in
`docs/QUARANTINE.md`.

## Sicherheitsdurchsicht bei der Uebernahme

Gemessen ueber alle `.c`/`.h`: **kein** Treffer auf `system(`, `exec*`,
`popen(`, `socket`, `http`, `CreateProcess`, `ShellExecute`, `dlopen`,
`LoadLibrary`, `__asm`, `fork(`, `unlink(`, `remove(`, `gets(`,
`strcpy(`, `sprintf(`. Eingebunden werden ausschliesslich
`assert.h`, `math.h`, `stddef.h`, `stdint.h`, `stdio.h`, `stdlib.h`,
`string.h` und der eigene Header.

**Formale Eigenart:** der Code ist **minifiziert** — ganze Funktionen
stehen auf einer Zeile. Das ist kein Fehler, aber es heisst, dass jede
Pruefung hier am Verhalten ansetzen muss und nicht am Lesefluss.

## Was hier liegt

| Datei | Byte | SHA-256 (Blob) |
|---|---|---|
| `CMakeLists.txt` | 584 | `794765bc42b3d38a3935da4c7c1ce7ea1c459ee53c6a37b397f884196d3be65c` |
| `LICENSE` | 1292 | `f924d7ed3358cc54b1cb643471604b5af040ff71384b15a41f98bda334822897` |
| `Makefile` | 593 | `a70a5ecf3f5f9e10d27559f465058239e4836ceb114a31062fa320c5fe551944` |
| `README.md` | 1216 | `12b295014ffeaff88da3339c58470ea87f6a0edf8065407b5ea11d6e888a23da` |
| `RECONSTRUCTION_STATUS.md` | 1326 | `e7972806795af0c77a51584407f7390cd256f598dc6cb3ed38856d87ecf6fb5e` |
| `SHA256SUMS` | 1440 | `316e1d0b556778af8ba9f1ad176665e44a8fc2d88e42b2d18809c1034d86dcf0` |
| `include/dtc_components.h` | 3320 | `4050c4eec4b38736ba57ccd3ad1eeb26f7705d91ecb9c512f6ea70c54798dd8b` |
| `src/bitbuffer.c` | 1376 | `b0b71520c1752133d62f05a1fa13f3c4eb1b6bbd44edeb1591f863bcad37478a` |
| `src/crc.c` | 809 | `d09d3cac9395db05f19169762193ed238d4d94c719b76ebe5e4dd6518c702772` |
| `src/ctraw.c` | 1387 | `466d5c2f3af02c2c36583ad06eac9cd12e254ea12db72d7add98ce17fab912c2` |
| `src/detect.c` | 865 | `7e441a471d2220c9be62daeaf3936cef63ca844bf2834ce925bb7e738a549d46` |
| `src/encoding.c` | 2952 | `fa53025b10023501eff258db67aeb9d2b62a53d72676e093f4e4d44bbe5a24f2` |
| `src/flux.c` | 802 | `b422c9aa8543a797c819ff249eae11bf1b32109a70c9afba3110d9b546a3706f` |
| `src/ipf.c` | 660 | `15b29663a2567a74f1a37d6b1b1afdeefd14cf28d2ffc7af6cc1adb6cbe01ec1` |
| `src/match.c` | 813 | `ccfaf90aded2b9d225154e4eb9b872f90e296e3996fc7801e06a4384d4412836` |
| `src/protection.c` | 303 | `f5b5247a70486b8876deb0b89bb69365779bcb4317ab2e5c789a3551b81e86ce` |
| `src/track.c` | 641 | `b933711c21e75a6345e383792e62b2038ba1904339b615bc1b629c60fb4f30e1` |
| `tests/test_main.c` | 1394 | `8e5623b196f126da6054eff5e4a67388797a59f01fc3eb4135ada63f83429398` |

Die Summen sind ueber das **git-Objekt** gebildet, nicht ueber den
Arbeitsbaum (MF-1096, Sperre 3); `.gitattributes` fuehrt
`src/dtc_components/** -text`, damit sie auf jedem Rechner dieselben
sind. Nachrechnen:

```
git show HEAD:src/dtc_components/src/crc.c | sha256sum
```

## Was das Paket ueber sich selbst NICHT behauptet

Woertlich aus seinem [`README.md`](README.md):

> Not claimed: SPS-compatible CT Raw compression, complete IPF
> encode/decode, or DTC's proprietary format/protection database.

und:

> `DTCCTRW` is this package's independent interchange format. It is not
> asserted to be compatible with SPS/KryoFlux CT Raw.

Beides gehoert in jede Verdrahtung uebernommen: ein Modul, das hier
einzieht, darf nicht mehr versprechen, als sein Ursprung beansprucht.
Das ist in diesem Baum die Klasse MF-509 — die Liste nennt, was
gelesen werden SOLL, nicht was geprueft ist.
