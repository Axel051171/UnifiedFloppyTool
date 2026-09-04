# FluxEngine-Drahtautomat — Abweichungen von echter Hardware

Register der forensischen Ehrlichkeit für
`tests/emulators/fluxengine/wire_state_machine.{c,h}` (MF-857).

**Abgrenzung:** `DIVERGENCES.md` im selben Verzeichnis gehört zum
**CLI**-Emulator (`firmware_state_machine.c`), der das externe
`fluxengine`-Binary modelliert. Dieses Register gehört zum **Draht**-
Automaten, der das Rahmenprotokoll auf den USB-Endpunkten modelliert.
Die beiden bestehen nebeneinander und beantworten verschiedene Fragen.

**Quelle:** `davidgiven/fluxengine`, Commit `909fac72` (2026-06-18),
GPL-2.0-only. Gutachten: `tools/uft-scout/out/fluxengine.gutachten.md`
(MF-856). Eigenständige Umsetzung, kein Port.

Schwere: **HOCH** könnte einen Fehler verdecken oder an echter Hardware
brechen · **MITTEL** plausibel, aber unbelegt · **NIEDRIG** kosmetisch.

---

## FEW-0 — Eine Quelle, keine zweite

| | |
|---|---|
| **Schwere** | **HOCH** |

Der Scout hat zweifach gesucht und **keine unabhängige Umsetzung des
Board-Protokolls gefunden** (Gutachten §7). Es gibt keinen zweiten
Client, keine Reimplementierung, keine Dokumentation außerhalb des
Projekts. `pid.codes` bestätigt nur VID/PID.

Für ein Wire-Protokoll ist das Projekt selbst die *definierende* Hand —
Firmware und Client übersetzen denselben `protocol.h`. Aber es ist
**nicht** dasselbe wie zwei unabhängige Zeugen, und die
Zwei-Quellen-Regel dieses Baums ist damit **nicht erfüllt**.

Alles Weitere in diesem Register hängt an dieser einen Zeile: wo die
Quelle irrt, irrt der Automat mit, und niemand hier kann es merken.

---

## FEW-1 — Die ARM-Seite ist abgeleitet, nicht gemessen

| | |
|---|---|
| **Schwere** | **HOCH** |

`protocol.h` setzt **kein** `#pragma pack` (0 Treffer im ganzen Repo).
Die Rahmengrößen entstehen also aus der natürlichen Ausrichtung des
jeweiligen Compilers, und **Füllbytes gehen mit über die Leitung**.

Gemessen wurden sie auf **x86-64 mit MinGW gcc 13.1.0** (Gutachten §2):
`read_frame` = 8 mit `milliseconds` @ 4, `write_frame` = 12 mit
`bytes_to_write` @ 4. Die PSoC-Seite (Cortex-M3, ARM-EABI) wurde **nie
gemessen**. Da alle Feldtypen ≤ 4 Byte sind, richtet sie nach der ABI
identisch aus — das ist eine **Ableitung**.

Der Automat und die Rahmenschicht schreiben die Versätze deshalb
ausdrücklich aus, statt Structs zu kopieren. Damit ist die Annahme
sichtbar und prüfbar; widerlegen kann sie nur ein Gerät.

---

## FEW-2 — „Keine Diskette" ist modelliert, nicht belegt

| | |
|---|---|
| **Schwere** | **MITTEL** |

Der Automat antwortet auf `MEASURE_SPEED` ohne Medium mit
`F_FRAME_ERROR`. Was die echte Firmware tut, wenn der Indexpuls
ausbleibt, ist im Quelltext nicht eindeutig ablesbar — sie könnte auch
warten (es gibt clientseitig kein Zeitlimit, siehe FEW-4) oder einen
Messwert von 0 liefern.

Die Wahl ist die **forensisch sichere**: ein Fehler ist ehrlicher als
eine 0, die wie eine Messung aussieht. Aber sie ist eine Wahl, keine
Beobachtung, und an echter Hardware zu bestätigen.

---

## FEW-3 — Debug-Rahmen sendet die heutige Firmware nie

| | |
|---|---|
| **Schwere** | **NIEDRIG** |

Der Automat kann auf Anforderung `F_FRAME_DEBUG` senden, damit die
Behandlung im Client geprüft werden kann. **Die aktuelle Firmware sendet
nie welche** — `print()` geht auf UART (`main.c:127-138`); die
Client-Behandlung (`fluxengineusb.cc:106-123`) ist Altbestand.

Der Fall wird trotzdem geprüft: unbewachter Altbestand ist der Zustand,
aus dem P3-89 kam (drei Selbsttest-Blöcke, die nie übersetzt wurden).

---

## FEW-4 — Wir haben ein Zeitlimit, die Vorlage nicht

| | |
|---|---|
| **Schwere** | **NIEDRIG** (bewusste Abweichung) |

Der Original-Client setzt **kein** Zeitlimit: kein `set_timeout` im
FluxEngine-Pfad, libusbp wartet unbegrenzt. Bei einer READ-Antwort über
1 MiB droht laut Quelltext eine wechselseitige Blockade
(`fluxengineusb.cc:261`).

`uft_fluxengine.c` setzt 5000 ms als Vorgabe. Ein forensisches Werkzeug
darf nicht hängen. Die Abweichung ist gewollt und hier benannt, damit
sie nicht später als Fehler gelesen wird.

---

## FEW-5 — Wir prüfen die Länge, die Firmware nicht

| | |
|---|---|
| **Schwere** | **NIEDRIG** (bewusste Abweichung) |

`handle_command` (`main.c:820-875`) liest bis zu 64 Byte und verzweigt
**nur** über `f->f.type` — weder Länge noch das `size`-Feld werden
geprüft. `send_reply` schickt `f.size` Byte (`main.c:191-196`), der
Client liest immer 64 (`fluxengineusb.cc:21,111`).

Die Rahmenschicht hier weist eine Antwort ab, die kürzer ist als der
Rahmen, den sie ankündigt. Ohne diese Prüfung läse
`uft_fe_get_version()` seine Fassung aus einem Byte, das nie gesendet
wurde.

**Folge für die Modelltreue:** ein Gerät, das zu kurze Rahmen sendet,
würde von der echten Vorlage akzeptiert und von uns abgewiesen. Das ist
strenger, nicht laxer — die sichere Richtung.

---

## FEW-6 — Nur fünf Kommandos modelliert

| | |
|---|---|
| **Schwere** | **MITTEL** |

Modelliert: `GET_VERSION`, `SEEK`, `RECALIBRATE`, `SET_DRIVE`,
`MEASURE_SPEED`. Nicht modelliert: `READ`, `WRITE`, `ERASE`,
`MEASURE_VOLTAGES`, die beiden Bulk-Tests.

Das ist **Absicht**, kein Versäumnis: jedes weitere Kommando kommt mit
seinem eigenen Prüffall, nicht als Vorrat. Der Automat **zählt**
unbekannte Rahmentypen (`unknown_cmds`), damit die Lücke sichtbar wird
statt geerbt.

Vor allem `READ` fehlt und ist der eigentliche Zweck: der
Flussdatenstrom über `DATA_IN`, seine 64-Byte-Pakete und das
Zero-Length-Paket als Ende (`main.c:513`). Dazu gehört die
Fortsetzungsmarke — **Schwelle 63, nicht 62**; die 62 stammt aus dem
Client-Encoder (`fluxmap.cc:42-47`), die Hardware sendet
Event-Bytes mit 63 Ticks (`Sampler.v:70-76`, `0xBF` ist gültig).

---

## Was NICHT abweicht

- Die Rahmentypen und ihre Werte stammen aus `protocol.h:44-70`,
  Commit `909fac72`, und sind Zeile für Zeile abgeglichen.
- Die Byteordnung auf der Leitung ist little-endian; das ist im Client
  ausdrücklich vermerkt („the board always operates in little-endian
  mode", `fluxengineusb.cc:11-16`). Der Automat und die Rahmenschicht
  lesen `period_ms` **immer** als LE — der Original-Client tut das an
  dieser einen Stelle **nicht** (`fluxengineusb.cc:164`), was auf einem
  Big-Endian-Host sein Fehler wäre.
- Die Protokollfassung ist **17** und seit 2022-03-26 unverändert.
