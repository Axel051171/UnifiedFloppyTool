# Namensrolle der Formateintraege

**Zweck (MF-1077):** diese Rolle prueft die **Abwesenheit**. Eine
Plugin-Tafel kann ueber sich selbst nicht sagen, dass sie einmal da
war; die Rolle kann es. Sie wird **gepflegt, nicht erzeugt** — ein
Generator, der sie jedes Mal neu schreibt, wuerde eine Loeschung
stillschweigend mitschreiben und damit genau das verdecken, wogegen
sie steht.

**Regeln** (`scripts/audit_namensrolle.py`, im Konsistenzlauf):

1. `aktiv` verlangt eine lebende Plugin-Tafel.
2. `zurueckgenommen` verlangt einen `MF-`/`P3-`-Beleg.
3. Kein Neuzugang ohne Zeile.
4. **Keine ID verschwindet** — verglichen wird gegen `HEAD`.

`zurueckgenommen` heisst: der **Anspruch** ist zurueckgezogen, nicht
unbedingt der Code. `rcpmfs` steht so da und ist trotzdem vorhanden — Sonde, `open()` und Schreiber sagen seit
MF-1035 ab, und
`tests/test_rcpmfs_ist_kein_dateiformat.c` haelt die Messung fest.
Der Grabstein IST der Beleg; eine geloeschte Datei belegt nichts.

`Art` ist dieselbe Einteilung wie `.kind` in der Plugin-Tafel:
`behaelterformat` (Datei mit eigenem Aufbau), `treiber_quelle`
(Konvention ueber rohe Sektoren), `geometriekatalog` (Tafel, kein
Dateiaufbau).

| ID | Art | Status | Beleg |
|---|---|---|---|
| 2img | behaelterformat | aktiv |  |
| a2r | behaelterformat | aktiv | MF-1322 — Tuer zum vorhandenen Leser (`src/parsers/a2r/uft_a2r_parser.c`, vorher 13 Aufrufer, alle in Tests). Registrierung auf Eigentuemer-Entscheidung 2026-09-21; Begruendung in `scripts/format_freeze_baseline.json`, Feld `whitelist_begruendung`. Nur lesend: kein `write_track`, kein `create`, kein `flush`. |
| 86f | behaelterformat | aktiv |  |
| adf | behaelterformat | aktiv |  |
| adf_arc | behaelterformat | aktiv |  |
| adf_ext | behaelterformat | aktiv |  |
| adl | behaelterformat | aktiv |  |
| akai_s900 | behaelterformat | aktiv |  |
| apridisk | behaelterformat | aktiv |  |
| atr | behaelterformat | aktiv |  |
| atx | behaelterformat | aktiv |  |
| cas | behaelterformat | aktiv |  |
| cfi | behaelterformat | aktiv |  |
| cpm | behaelterformat | aktiv |  |
| cqm | behaelterformat | aktiv |  |
| d13 | behaelterformat | aktiv |  |
| d64 | behaelterformat | aktiv |  |
| d67 | behaelterformat | aktiv |  |
| d71 | behaelterformat | aktiv |  |
| d77 | behaelterformat | aktiv |  |
| d80 | behaelterformat | aktiv |  |
| d81 | behaelterformat | aktiv |  |
| d82 | behaelterformat | aktiv |  |
| d88 | behaelterformat | aktiv |  |
| dc42 | behaelterformat | aktiv |  |
| dcm | behaelterformat | aktiv |  |
| dim | behaelterformat | aktiv |  |
| dim_atari | behaelterformat | aktiv |  |
| dmk | behaelterformat | aktiv |  |
| dms | behaelterformat | aktiv |  |
| do | behaelterformat | aktiv |  |
| dsk_cpc | behaelterformat | aktiv |  |
| edk | behaelterformat | aktiv |  |
| edsk | behaelterformat | aktiv |  |
| fdi | behaelterformat | aktiv |  |
| fdi_pc98 | behaelterformat | aktiv |  |
| fds | behaelterformat | aktiv |  |
| g64 | behaelterformat | aktiv |  |
| g71 | behaelterformat | aktiv |  |
| hardsector | geometriekatalog | aktiv |  |
| hfe | behaelterformat | aktiv |  |
| imd | behaelterformat | aktiv |  |
| img | behaelterformat | aktiv |  |
| ipf | behaelterformat | aktiv |  |
| jv1 | behaelterformat | aktiv |  |
| jv3 | behaelterformat | aktiv |  |
| jvc | behaelterformat | aktiv |  |
| kfx | behaelterformat | aktiv |  |
| korg_dss1 | behaelterformat | aktiv |  |
| lisa_twiggy | behaelterformat | aktiv |  |
| logical | treiber_quelle | aktiv |  |
| mfi | behaelterformat | aktiv |  |
| mgt | behaelterformat | aktiv |  |
| micropolis | behaelterformat | aktiv |  |
| msa | behaelterformat | aktiv |  |
| msx_disk | behaelterformat | aktiv |  |
| myz80 | behaelterformat | aktiv |  |
| nanowasp | behaelterformat | aktiv |  |
| nfd | behaelterformat | aktiv |  |
| nib | behaelterformat | aktiv |  |
| northstar | behaelterformat | aktiv |  |
| opus | behaelterformat | aktiv |  |
| pdp | behaelterformat | aktiv |  |
| po | behaelterformat | aktiv |  |
| posix | treiber_quelle | aktiv |  |
| pri | behaelterformat | aktiv |  |
| pro | behaelterformat | aktiv |  |
| qrst | behaelterformat | aktiv |  |
| rcpmfs | treiber_quelle | zurueckgenommen | MF-1035 / P3-338 |
| sad | behaelterformat | aktiv |  |
| sam | behaelterformat | aktiv |  |
| sap_thomson | behaelterformat | aktiv |  |
| scl | behaelterformat | aktiv |  |
| scp | behaelterformat | aktiv |  |
| ssd | behaelterformat | aktiv |  |
| st | behaelterformat | aktiv |  |
| stx | behaelterformat | aktiv |  |
| syn | behaelterformat | aktiv |  |
| t1k | behaelterformat | aktiv |  |
| tan | behaelterformat | aktiv |  |
| td0 | behaelterformat | aktiv |  |
| trd | behaelterformat | aktiv |  |
| udi | behaelterformat | aktiv |  |
| v9t9 | behaelterformat | aktiv |  |
| vdk | behaelterformat | aktiv |  |
| victor9k | behaelterformat | aktiv |  |
| woz | behaelterformat | aktiv |  |
| xdm86 | behaelterformat | aktiv |  |
| xfd | behaelterformat | aktiv |  |

## Zurueckgenommene Schnittstellen

- P3-233: `include/uft/formats/c64/uft_c64_bam.h` ist zurueckgenommen.
  Der unbenutzte zweite BAM-Entwurf hatte keine einbindende Quelldatei
  und sagte 17 Funktionen ohne Umsetzung zu. Die aktive BAM-API bleibt
  `include/uft/formats/c64/uft_bam_editor.h` mit ihrer Umsetzung und
  `tests/test_bam_editor.c`; die CBM-Formatunterstuetzung bleibt aktiv.
