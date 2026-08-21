# `src/samdisk/` — Referenzbestand, kein Baubestandteil

**Herkunft:** [SAMdisk](https://github.com/simonowen/samdisk) 4.0 ALPHA,
© 2002–2024 Simon Owen.
**Lizenz:** MIT — Volltext in [`License.txt`](License.txt).

---

## Was das hier ist

Diese 145 Dateien werden **von keinem Build kompiliert**. Nachgeprüft
(MF-458): weder `UnifiedFloppyTool.pro` noch `CMakeLists.txt` führen eine
Quelldatei daraus; beide binden lediglich `src/samdisk` als **Include-Pfad**
ein. Im Build-Verzeichnis liegt keine einzige Objektdatei dazu.

Der Bestand dient als **Referenz-Orakel**: eine unabhängig geschriebene,
funktionierende Implementierung derselben Formate, gegen die UFTs eigene
Parser geprüft werden. Der Baum zitiert sie mit Datei und Zeile:

| Wo | Was |
|---|---|
| `src/formats/td0/uft_td0.c:15` | „Authority: `src/samdisk/td0.cpp:10` compares the signature as the byte string…" |
| `src/formats/fdi/uft_fdi_plugin.c:10` | gegen SAMdisks `ReadFDI` verifiziert (MF-359) |
| `include/uft/hal/uft_scp_direct.h:101` | „`src/samdisk/scp.cpp:132` `flux_times.push_back(total_time * 25)`" |
| `src/hardware_providers/scp_provider_v2.h:48` | gegen `samdisk/scp.cpp` abgeglichen |

## Warum es nicht gelöscht werden darf

Ein Aufräum-Durchgang, der nach „wird nicht gebaut" filtert, hält diesen
Ordner für tot. Er ist es nicht — er ist die Beweisgrundlage für mehrere
Format-Entscheidungen. In MF-271 wurden aus genau diesem Muster heraus
`tests/test_switch.c` und `tests/test_provider_switch.cpp` fälschlich
gelöscht, weil sie das Wort „switch" trugen; sie mussten zurückgeholt werden.

`scripts/verify_build_sources.py` kennt den Ordner seit MF-458 als
`NOT_BUILT_BY_DESIGN` und meldet ihn nicht mehr als Lücke. Vorher machten
diese 100 Dateien den Großteil einer 160 Einträge langen „akzeptierten
Abweichungen"-Liste aus und ließen die Zahl bedeutungslos wirken.

## Was die Lizenz erlaubt

MIT — also mehr als bei den anderen Fremdbeständen im Umfeld:

| Bestand | Lizenz | Was erlaubt ist |
|---|---|---|
| **SAMdisk** (hier) | **MIT** | Lesen, **Code übernehmen**, verändern — solange Copyright-Vermerk und Lizenztext mitgeführt werden |
| X-Copy (`~/Github/xcopy`) | keine | nur lesen und Verfahren neu implementieren |
| `xvs.library` | „All Rights Reserved" | nur lesen |

Bis MF-458 fehlte `License.txt` in unserer Kopie. MIT verlangt ausdrücklich,
dass Vermerk und Lizenztext „in all copies or substantial portions"
mitgeführt werden — die Datei ist damit Pflicht, nicht Höflichkeit.

Wer Code von hier übernimmt, vermerkt das an der Zielstelle mit Datei und
Zeile, so wie es die Zitate oben tun.

## Wofür es als Nächstes nützlich ist

Von den 62 Formaten, die `docs/VERIFICATION_TIERS.md` als **T3**
(unverifiziert) führt, hat SAMdisk für **17** einen eigenen Handler:

```
adf_arc  cfi  cpm  cqm  do  fdi_pc98  ipf  mfi  mgt  msa
sad  sap_thomson  scl  st  td0  trd  udi
```

Für die lässt sich ein Cross-Check durchführen, **ohne eine reale
Referenzdiskette zu besitzen** — und Referenzmaterial ist der Engpass beim
T3-Abbau, nicht Code. Siehe `docs/XCOPY_COMPARISON.md` zum selben Ansatz mit
`keirf/disk-utilities`.
