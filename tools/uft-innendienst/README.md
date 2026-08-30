# uft-innendienst

Sechs Rollen für die wiederkehrenden Muster der Baumarbeit. Jede findet
ihre Information selbst, belegt sie mit Fundstelle und übergibt an den
MF-Workflow. **Keine Rolle erzeugt Produktcode.** Regeln: [`AGENT.md`](AGENT.md).

Die dritte Aufklärungs-Rolle: `uft-scout` sieht nach draußen in die
Breite, `uft-variants` nach draußen in die Tiefe — der Innendienst sieht
nach innen.

## Stand — gemessen auf `bd2d5616`, 2026-08-30

| Rolle | Werkzeug | Lauf |
|---|---|---|
| 1 Tür-Sucher | `scripts/tuersucher.py` | Selbsttest **13/13**. Baum: **4069** Exporte — OK 240 · WAISE 2661 · NUR_TESTS 898 · NUR_EIGENES_VERZEICHNIS 270 · **ANGEBOT_OHNE_ABNEHMER 289** |
| 2 Zensus-Schmied | `playbook/zensus.md` | Ritual + Checkliste der 9 gemessenen Zensus-Fehler (kein Skript, siehe AGENT.md) |
| 3 Oracle-Kalibrierer | `scripts/kalibrierer.py` | Register **8** (seit MF-693 mit `xdftool`) · **6 Befunde**: 4 ungeeicht (`cpmls`, `hxcfe`, `samdisk`, `lsatr`) + 2 unregistriert (`a8rawconv`, `adfrescue`, seit MF-693 gleichrangig) · 2 Widersprüche (`gw`, `dtc`) |
| 4 Fixture-Beschaffer | `scripts/beschaffung.py` | **9 Posten** (1 Version, 8 Format), 0 ohne konkreten Weg |
| 5 Widerspruchs-Finder | `scripts/widerspruch.py` | Selbsttest **3/3**. Baum: **0** — die drei bekannten Klassen sind geheilt |
| 6 Tore-Sekretär | `scripts/sekretaer.py` | **21** wartende Entscheidungs-Posten |

Alle Zahlen aus einem Lauf am selben Commit. Sie driften; der Befehl ist
die Wahrheit, nicht diese Tabelle.

## Bedienung

```bash
python tools/uft-innendienst/scripts/tuersucher.py --selbsttest
python tools/uft-innendienst/scripts/tuersucher.py .          # -> out/tueren.md
python tools/uft-innendienst/scripts/kalibrierer.py erzeugen  # -> work/kalib_127.bin
python tools/uft-innendienst/scripts/kalibrierer.py pruefen .
python tools/uft-innendienst/scripts/beschaffung.py .         # -> out/korb.md
python tools/uft-innendienst/scripts/widerspruch.py .
python tools/uft-innendienst/scripts/sekretaer.py .           # -> out/sitzung.md
```

`out/` und `work/` sind gitignored — die Berichte sind regenerierbar,
und ein eingecheckter Bericht wäre die nächste driftende Zahl.

## Was bei der Übernahme in den Baum geändert wurde (MF-693)

Der gelieferte Werkzeugkasten lief; sechs Messungen zeigten, wo er die
eigenen Regeln nicht einhielt. Der Reihe nach, mit Beleg:

| | Befund | gemessen | Folge |
|---|---|---|---|
| **A** | `widerspruch.py --selbsttest` lief `git ls-files` in einem Verzeichnis, das **kein Repository** ist. Leere Dateiliste, **0 von 3** gepflanzten Fällen. Das README behauptete daneben „Selbsttest 3/3". | `fatal: not a git repository`, rc=3 | Dateimenge aus `scripts/baum.py` mit sichtbarem Rückfall; Selbsttest **3/3**, läuft jetzt **vor** jedem echten Lauf und bricht ab |
| **B** | Die Tür-Sucher-Abnahme nannte **vier historische Symbole des echten Baums**. Drei davon waren beim ersten Lauf schon gelöscht. Eine Abnahme, die sich selbst löscht, je gesünder der Baum wird. | 1 von 4 Fällen noch im Baum | gepflanzter Fixture-Baum `data/tuer_fixtures/`, 9 Prüfungen, verfällt nie. Historische Fälle bleiben — **nachrichtlich, nicht blockierend** |
| **C** | `git ls-files src include tests cli` — `git ls-files` **mit gepflegter Verzeichnisliste davor**, ohne `--others --exclude-standard`. Genau die Aufzählung, die dieser Baum viermal veralten sah, und in der eigenen Checkliste verboten. | — | `baum.py` nach der Regel aus `scripts/repo_scope.py` |
| **D** | 75 Symbole aus `src/samdisk/` und `src/a8rawconv/` im Bericht — **vendorte Fremdbäume**, die sechs bestehende Tore ausnehmen. | 26 WAISE + 1 NUR_TESTS + 48 NUR_DIR | Ausnahme über **Import** von `audit_spdx_policy.py:AUSGENOMMEN`, nicht abgeschrieben. Rotbeweis: Ausnahme abgeschaltet ⇒ `uft_fix_fremd` erscheint |
| **E** | Der Kalibrierer las aus der Sammelzeile ``​`lsatr`, `a8rawconv`, `gw`, …​`` nur den **ersten** Namen und meldete für `cpmls`/`hxcfe`/`samdisk` „**keine** Kalibrierzeile", obwohl eine dasteht. Urteil zufällig richtig, Begründung falsch. | 3 falsch begründete Befunde | Zelle wird an Kommas zerlegt. Nebenbei: `` `floptool` (`flophashes`) `` erzeugte ein erfundenes Werkzeug `flophashes` — auch weg |
| **F** | `beschaffung.py` führte ein **eigenes** `soll_versionen` und meldete „HFE **v1** fehlt", während der Korpus-Zensus dieselbe Fassung als **`rev0`** zählte. Abgleich über den Namen statt über die Identität — Zeile 8 der eigenen Checkliste. | Posten 1 von 7 war erfunden | Soll kommt jetzt aus `tools/uft-variants/config.json` → `erkennungen`, derselben Quelle wie die Ist-Namen |

Dazu drei kleinere:

* **G — README-Zahlen ohne Messung.** „Selbsttest 3/3" (war 0/3),
  „6 Posten" (waren 7), „42 wartende Posten" (waren 47). Alle Zahlen
  oben stammen jetzt aus einem Lauf.
* **H — der Agent fehlte.** Es gab nur einen Werkzeugkasten, keine
  Registrierung. `.claude/agents/uft-innendienst.md` ist neu.
* **I — `scripts/zensus_vorlage.py`** stand in `AGENT.md` und existierte
  nicht: eine Vereinbarung ohne Leser im eigenen Haus, also genau die
  Klasse, die Rolle 5 zum Tor macht. Die Erwähnung ist weg, mit
  Begründung.

Und zwei Abgrenzungen, die vorher fehlten und ohne die zwei Zahlen
nebeneinander gedriftet wären:

* Rolle 1 vs. `scripts/audit_orphan_modules.py` — **Symbol**- gegen
  **Modul**-Frage. Steht jetzt im Kopf beider Berichte.
* Rolle 6 vs. `scripts/scout_stand.py` — der Sekretär hatte einen
  eigenen, schwächeren Ersatz („Gutachten ohne `<!-- stufe: -->`-Marke").
  Gemessen tragen **4 von ~30** Gutachten diese Marke; der Ersatz hätte
  rund 25 erledigte Vorgänge als offene Entscheidung auf den Zettel
  gesetzt. Er importiert jetzt `ohne_spur()`. Postenzahl: 47 → **21**.

Selbst gefunden hat der Werkzeugkasten dabei zwei Dinge, die stehen
bleiben: `uft_convert_memory()` ist eine öffentliche C-API, deren
Aufrufer außerhalb von Kommentaren **alle unter `tests/`** liegen; und
`a8rawconv`, `adfrescue`, `amitools` sind kalibriert, aber nicht im
Oracle-Register — sie zählen damit für kein T1b-Manifest.
