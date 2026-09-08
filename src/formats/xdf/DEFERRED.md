# XDF — Stand, gemessen (MF-959, 2026-09-08)

**Diese Datei stand 4½ Monate falsch.** Sie erklärte
`uft_xdf_adapter.c` und `uft_xdf_api_impl.c` für *„intentionally NOT
restored"* und nannte `<fnmatch.h>` als Windows-Blocker. **Am Tag nach
ihrer Entstehung** lieferte `1535dfab` (2026-04-25) beides — der
Commit-Titel sagt wörtlich *„fnmatch shim + xdf adapter"*.

`docs/KNOWN_ISSUES.md` hielt schon in MF-459 fest, das Dokument sei
veraltet; korrigiert wurde es nicht. Das ist dieselbe Klasse wie MF-938:
eine Behauptung wird weitergetragen statt nachgemessen.

## Was heute wirklich im Baum liegt

| Datei | Zeilen | in `SOURCES` |
|---|---|---|
| `uft_xdf_core.c` | 1006 | ja |
| `uft_xdf_api.c` | 941 | ja |
| `uft_xdf_api_impl.c` | 1124 | ja |
| `uft_xdf_adapter.c` | 254 | ja |

Der `fnmatch`-Blocker ist gelöst: `uft_xdf_api_impl.c:39` bindet
`uft/compat/uft_fnmatch.h` ein, hinter `#ifdef _WIN32`.

## Was wirklich offen ist

**Nicht die Restaurierung — die Erreichbarkeit.** Der Zweig wird gebaut
und ruft niemand:

* die öffentliche API (`xdf_api_batch_create`, `xdf_api_batch_process`,
  `xdf_api_compare`) hat **null** Aufrufer in `src/` und `tests/`
  (ARCH-18)
* `import_d64` liest D64 mit **eigener** Zonentabelle — eine zweite
  Format-Schicht neben dem D64-Plugin (ARCH-6)
* vier Header sagen zusammen **39 Funktionen** zu, die es nicht gibt:
  `uft_xdf_{dxdf,pxdf,txdf,zxdf}.h`, 900 Zeilen, **0** Umsetzungen,
  **0** Aufrufe. `uft_xdf_api_impl.c` bindet alle vier ein und ruft
  keine. Sie stehen seit dem v4.1.0-Release (2026-02-08). **P3-254** —
  ihr fünfter Bruder `uft_xdf_mxdf.h` wurde in PH-2/MF-549 aus
  derselben `#include`-Gruppe entfernt, die vier blieben stehen.

## Was hier NICHT entschieden ist

Ob der Zweig verdrahtet oder entfernt wird. Verdrahten hieße, eine
zweite Format-Schicht scharf zu schalten, die ARCH-6 gerade abbaut;
Entfernen ist ein eigener Schritt mit der MF-369-Beweispipeline. Beides
gehört zu **ARCH-6**, nicht hierher. Diese Datei sagt nur noch, was
gemessen ist.
