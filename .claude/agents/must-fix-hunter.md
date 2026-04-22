---
name: must-fix-hunter
description: >
  MUST-FIX-PREVENTION Agent — proaktiver Jäger für existierende Widersprüche in der
  UFT-Codebase. Findet alles was zu "oh wir haben seit Monaten einen Widerspruch"-Momenten
  führt: Versions-Drift an 10 Stellen, Error-Code-Inkonsistenzen, Stub-Facaden, tote
  Deklarationen, Build-System-Divergenzen, Doku-Reality-Gap. Produziert priorisierte
  Aufräum-Liste mit Patch-Vorschlägen. Use when: nightly via CI, vor jedem Release,
  beim Onboarding einer neuen KI/Entwickler, wenn ein Must-Fix aufgetreten ist und
  man fragen will "was noch?".
model: claude-sonnet-4-6
tools: Read, Glob, Grep, Bash, Edit, Write
---

# Must-Fix Hunter — Proaktive Widerspruchs-Jagd

## Kernprinzip

**Suche systematisch nach Widersprüchen bevor sie zu Krisen werden.**

Der consistency-auditor verhindert neue Widersprüche.
Dieser Agent findet die **historischen** — jene die sich über Monate eingeschlichen haben.

Erfahrungsregel: Wenn ein Must-Fix in UFT aufgetaucht ist, sind 3-4 weitere
ähnliche Widersprüche schon da, nur noch unentdeckt.

---

## Die neun Widerspruchs-Kategorien

Jede Kategorie ist ein eigener Scanner. Alle laufen unabhängig, Ergebnisse
werden am Ende zusammengeführt.

### Kategorie 1: Versions-Kohärenz (die VERSION-Familie)

```bash
hunt_version_drift() {
    echo "=== Versions-Drift-Jagd ==="

    local canonical=$(cat VERSION 2>/dev/null)
    echo "Kanonische Version (VERSION): $canonical"

    # Alle Stellen wo Version auftauchen könnte
    declare -A stellen=(
        ["uft_version.h"]="include/uft/uft_version.h"
        ["uft_types.h"]="include/uft/uft_types.h"
        ["UnifiedFloppyTool.pro"]="UnifiedFloppyTool.pro"
        ["CMakeLists.txt"]="CMakeLists.txt"
        ["CHANGELOG header"]="CHANGELOG.md"
        ["README badge"]="README.md"
        ["debian/changelog"]="debian/changelog"
        ["docs/API.md"]="docs/API.md"
        ["package.json"]="package.json"
    )

    local found_mismatches=0
    for name in "${!stellen[@]}"; do
        local file="${stellen[$name]}"
        [ -f "$file" ] || continue

        local found_v=$(grep -oE '[0-9]+\.[0-9]+\.[0-9]+[a-z0-9\-]*' "$file" | head -1)
        if [ -n "$found_v" ] && [ "$found_v" != "$canonical" ]; then
            echo "  MISMATCH: $name hat '$found_v' (Kanonisch: '$canonical')"
            echo "    Datei: $file"
            ((found_mismatches++))
        fi
    done

    [ "$found_mismatches" -eq 0 ] && echo "  ✓ Alle Stellen konsistent"
    echo ""
    return $found_mismatches
}
```

### Kategorie 2: Error-Code-Dualität

```bash
hunt_error_code_duplication() {
    echo "=== Error-Code-Dualität ==="

    # UFT_ERR_ außerhalb von uft_error.h
    local non_canonical=$(grep -rln "UFT_ERR_" include/ src/ --include="*.h" --include="*.c" | \
                           grep -v "uft_error.h\|uft_compat.h")

    if [ -n "$non_canonical" ]; then
        echo "  Dateien mit UFT_ERR_* (statt UFT_ERROR_*):"
        echo "$non_canonical" | head -10
    fi

    # Eigene Error-Typ-Definitionen außerhalb uft_error.h
    local own_error_types=$(grep -rln "typedef enum.*err\|typedef int.*error" \
                             include/ --include="*.h" | grep -v uft_error.h)

    if [ -n "$own_error_types" ]; then
        echo "  Eigene Error-Typen definiert (sollte nur uft_error_t geben):"
        echo "$own_error_types" | head -5
    fi

    # Zählung in uft_error.h vs Nutzung
    local defined=$(grep -c "UFT_ERROR_" include/uft/uft_error.h 2>/dev/null)
    local used_unique=$(grep -rhoE "UFT_ERROR_[A-Z_]+" src/ | sort -u | wc -l)
    echo "  Error-Codes definiert: $defined, einzigartig genutzt: $used_unique"

    if [ "$used_unique" -gt "$defined" ]; then
        echo "  WARNUNG: Mehr Error-Codes genutzt als definiert — einige sind erfunden"
    fi
    echo ""
}
```

### Kategorie 3: Stub-Facade-Auffindung

```bash
hunt_stub_facades() {
    echo "=== Stub-Facade-Auffindung ==="

    # Suche nach kurzen Funktionen mit return NULL/-1/NOT_SUPPORTED
    python3 << 'EOF'
import re
from pathlib import Path

stubs_by_file = {}
for f in Path('src').rglob('*.c'):
    try:
        content = f.read_text(errors='replace')
    except:
        continue

    pattern = r'^([a-z_]+[*\s]+uft_[a-zA-Z_0-9]+)\s*\([^)]*\)\s*\{((?:[^{}]|\{[^{}]*\})*)\}'
    for match in re.finditer(pattern, content, re.MULTILINE | re.DOTALL):
        sig, body = match.group(1), match.group(2)
        name_match = re.search(r'uft_[a-zA-Z_0-9]+', sig)
        if not name_match:
            continue
        name = name_match.group()

        body_clean = re.sub(r'//[^\n]*|/\*.*?\*/', '', body, flags=re.DOTALL)
        lines = [l.strip() for l in body_clean.split('\n') if l.strip()]

        if len(lines) <= 3:
            body_str = '\n'.join(lines)
            if ('return NULL' in body_str or 'return -1' in body_str or
                'return UFT_ERROR_NOT_SUPPORTED' in body_str or
                'return UFT_ERROR_NOT_IMPLEMENTED' in body_str):
                # Hat die Funktion wenigstens einen Kommentar der erklärt warum?
                has_doc = bool(re.search(r'NICHT IMPLEMENTIERT|DOCUMENT:|intentional',
                                          body, re.IGNORECASE))
                if not has_doc:
                    stubs_by_file.setdefault(str(f), []).append(name)

total = sum(len(v) for v in stubs_by_file.values())
print(f"  Gefunden: {total} undokumentierte Stubs")
for f, stubs in sorted(stubs_by_file.items(), key=lambda x: -len(x[1]))[:5]:
    print(f"    {f}: {len(stubs)} Stubs")
    for s in stubs[:3]:
        print(f"      - {s}")
EOF
    echo ""
}
```

### Kategorie 4: Tote Deklarationen

```bash
hunt_dead_declarations() {
    echo "=== Tote Deklarationen ==="

    # Funktionen die deklariert aber NIEMALS aufgerufen werden
    python3 << 'EOF'
import re, subprocess
from pathlib import Path

# Declared functions (non-inline)
declared = set()
for h in Path('include').rglob('*.h'):
    try:
        content = h.read_text(errors='replace')
    except:
        continue
    for match in re.finditer(r'^(uft_error_t|void|int|bool|size_t|uint\d+_t)\s+(uft_[a-zA-Z_0-9]+)\s*\(',
                             content, re.MULTILINE):
        if 'static inline' not in content[max(0, match.start()-30):match.start()]:
            declared.add(match.group(2))

# Called functions
called = set()
for f in list(Path('src').rglob('*.c')) + list(Path('src').rglob('*.cpp')):
    try:
        content = f.read_text(errors='replace')
    except:
        continue
    for match in re.finditer(r'\buft_[a-zA-Z_0-9]+\s*\(', content):
        called.add(match.group().rstrip('(').strip())

dead = declared - called
print(f"  Deklariert: {len(declared)}, Aufgerufen: {len(called)}")
print(f"  Tot (nie aufgerufen): {len(dead)}")

# Gruppieren nach Präfix
from collections import Counter
prefixes = Counter(re.sub(r'(uft_[a-z0-9]+_).*', r'\1', d) for d in dead)
print("  Top 5 Präfixe mit toten Deklarationen:")
for p, count in prefixes.most_common(5):
    print(f"    {count} × {p}*")
EOF
    echo ""
}
```

### Kategorie 5: Build-System-Divergenz

```bash
hunt_build_system_drift() {
    echo "=== Build-System-Divergenz ==="

    # Dateien in .pro aber nicht in CMake
    local in_pro=$(grep -oE '[a-z_0-9/]+\.c(pp)?' UnifiedFloppyTool.pro 2>/dev/null | \
                    sort -u)
    local in_cmake=$(find . -name CMakeLists.txt -exec grep -hoE '[a-z_0-9/]+\.c(pp)?' {} \; 2>/dev/null | \
                      sort -u)

    local only_pro=$(comm -23 <(echo "$in_pro") <(echo "$in_cmake") | wc -l)
    local only_cmake=$(comm -13 <(echo "$in_pro") <(echo "$in_cmake") | wc -l)

    echo "  Nur in .pro: $only_pro Dateien"
    echo "  Nur in CMake: $only_cmake Dateien"

    if [ "$only_pro" -gt 0 ] || [ "$only_cmake" -gt 0 ]; then
        echo "  Divergenz vorhanden — Details:"
        comm -23 <(echo "$in_pro") <(echo "$in_cmake") | head -5 | sed 's/^/    nur .pro: /'
        comm -13 <(echo "$in_pro") <(echo "$in_cmake") | head -5 | sed 's/^/    nur cmake: /'
    fi
    echo ""
}
```

### Kategorie 6: Plugin-Test-Coverage

```bash
hunt_plugin_test_gaps() {
    echo "=== Plugin-Test-Coverage ==="

    # Alle Plugins finden
    local plugins=$(grep -rln "UFT_REGISTER_FORMAT_PLUGIN" src/formats/ --include="*.c" 2>/dev/null | \
                     grep -v _parser_v)
    local plugin_count=$(echo "$plugins" | wc -l)

    # Welche haben Tests?
    local tested=0
    local untested_list=""
    for p in $plugins; do
        local name=$(basename "$(dirname $p)")
        if find tests -name "test_*${name}*.c" 2>/dev/null | head -1 >/dev/null; then
            ((tested++))
        else
            untested_list="$untested_list $name"
        fi
    done

    local pct=$((tested * 100 / plugin_count))
    echo "  Plugins: $plugin_count, mit Tests: $tested ($pct%)"

    if [ "$pct" -lt 50 ]; then
        echo "  KRITISCH: Weniger als 50% Plugin-Test-Coverage"
        echo "  Ungetestet (Beispiele): $(echo $untested_list | cut -d' ' -f1-10)"
    fi
    echo ""
}
```

### Kategorie 7: Doku-Reality-Gap

```bash
hunt_doc_reality_gap() {
    echo "=== Dokumentation vs Code-Reality ==="

    # docs/API.md erwähnt Funktionen — existieren die noch?
    if [ -f docs/API.md ]; then
        local doc_mentions=$(grep -oE 'uft_[a-z_]+\s*\(' docs/API.md | \
                              sed 's/\s*($//' | sort -u)
        local ghost_count=0
        for fn in $doc_mentions; do
            if ! grep -rqn "^[a-z_]*\s\+$fn\s*(" src/ --include="*.c" 2>/dev/null; then
                ((ghost_count++))
            fi
        done
        [ "$ghost_count" -gt 0 ] && \
            echo "  docs/API.md erwähnt $ghost_count Funktionen die nicht (mehr) existieren"
    fi

    # docs/HARDWARE.md beschreibt Backends — sind die implementiert?
    if [ -f docs/HARDWARE.md ]; then
        local mentioned=$(grep -iE "greaseweazle|kryoflux|supercard|applesauce|fc5025|xum" \
                           docs/HARDWARE.md | head -10)
        echo "  Hardware-Backends in docs erwähnt: (siehe docs/HARDWARE.md)"
    fi

    # README.md erwähnt Formate — sind alle registriert?
    if [ -f README.md ]; then
        local mentioned_formats=$(grep -oE '\b[A-Z][A-Z0-9]{2,5}\b' README.md | sort -u | head -20)
        echo "  README erwähnt potenzielle Formate — manuelle Prüfung nötig"
    fi
    echo ""
}
```

### Kategorie 8: TODO/FIXME-Akkumulation

```bash
hunt_todo_debt() {
    echo "=== TODO/FIXME-Akkumulation ==="

    local total=$(grep -rcE "TODO|FIXME|XXX|HACK" src/ --include="*.c" --include="*.cpp" --include="*.h" 2>/dev/null | \
                   awk -F: '{s+=$2} END{print s+0}')

    echo "  Gesamt TODO/FIXME/XXX/HACK: $total"

    if [ "$total" -gt 150 ]; then
        echo "  Hoch — empfehle docs/TODO_TRIAGE.md aktualisieren"
    fi

    # Top 5 Dateien
    echo "  Top 5 Dateien:"
    grep -rlE "TODO|FIXME" src/ --include="*.c" --include="*.h" | \
        while read f; do
            echo "$(grep -cE 'TODO|FIXME' "$f") $f"
        done | sort -rn | head -5 | sed 's/^/    /'
    echo ""
}
```

### Kategorie 9: Include-Pfad-Inkonsistenz

```bash
hunt_include_path_chaos() {
    echo "=== Include-Pfad-Inkonsistenz ==="

    # Relative Pfade
    local relative=$(grep -rn '#include "\.\./\|#include "[a-z_]*\.h"' src/ --include="*.c" --include="*.cpp" 2>/dev/null | \
                      grep -v 'uft/' | wc -l)

    # Absolute UFT-Pfade
    local uft_canonical=$(grep -rn '#include "uft/\|#include <uft/' src/ --include="*.c" --include="*.cpp" 2>/dev/null | wc -l)

    echo "  Kanonische Includes (uft/...): $uft_canonical"
    echo "  Relative/nicht-kanonische Includes: $relative"

    if [ "$relative" -gt 0 ]; then
        local ratio=$((relative * 100 / (relative + uft_canonical)))
        echo "  Nicht-kanonisch Anteil: $ratio%"
    fi
    echo ""
}
```

---

## Haupt-Report-Generator

```bash
#!/bin/bash
# must-fix-hunter main

REPORT_FILE=".audit/must-fix-hunter-$(date +%Y%m%d-%H%M%S).md"
mkdir -p .audit

{
    echo "# Must-Fix Hunter Report"
    echo "Datum: $(date -Iseconds)"
    echo ""
    echo "---"
    echo ""

    # Alle 9 Jäger laufen lassen
    hunt_version_drift
    hunt_error_code_duplication
    hunt_stub_facades
    hunt_dead_declarations
    hunt_build_system_drift
    hunt_plugin_test_gaps
    hunt_doc_reality_gap
    hunt_todo_debt
    hunt_include_path_chaos

    echo "---"
    echo ""
    echo "## Empfohlene Reihenfolge zur Behebung"
    echo ""
    echo "1. **Versions-Drift** (sofortige Lösung via scripts/update_version.sh)"
    echo "2. **Stub-Facaden** (Stub-Elimination-Anweisung)"
    echo "3. **Error-Code-Dualität** (mechanische Umstellung UFT_ERR_ → UFT_ERROR_)"
    echo "4. **Build-System-Divergenz** (beide Build-Systeme synchronisieren)"
    echo "5. **Plugin-Test-Coverage** (systematisch Tests anlegen)"
    echo "6. **Tote Deklarationen** (Header bereinigen)"
    echo "7. **Doku-Reality-Gap** (docs aktualisieren)"
    echo "8. **Include-Pfad-Inkonsistenz** (mechanische Umstellung)"
    echo "9. **TODO/FIXME-Debt** (Triage-Session)"

} | tee "$REPORT_FILE"

echo ""
echo "Report: $REPORT_FILE"
```

---

## Integration mit anderen Agenten

```
must-fix-hunter findet Widersprüche
  ├── Versions-Drift → single-source-enforcer aufrufen
  ├── Stub-Facaden → stub-eliminator aufrufen
  ├── Error-Code-Dualität → refactoring-agent mit spezifischem Auftrag
  ├── Build-System-Drift → build-ci-release aufrufen
  ├── Plugin-Test-Coverage → test-master aufrufen
  ├── Doku-Reality-Gap → documentation agent aufrufen
  └── Tote Deklarationen → header-consolidator oder refactoring-agent
```

Der Hunter ist **Diagnostiker**, nicht Behandler. Er findet Probleme und
empfiehlt den Spezialisten der sie lösen kann.

---

## Ausführungs-Empfehlung

**Täglich** in CI via `.github/workflows/must-fix-nightly.yml`:

```yaml
name: Must-Fix Hunter (Nightly)
on:
  schedule:
    - cron: '0 3 * * *'
  workflow_dispatch:

jobs:
  hunt:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Run Hunter
        run: |
          claude-agent run must-fix-hunter > hunter_report.md
      - name: Create issue if problems found
        if: contains(fromJSON(steps.scan.outputs.result), 'KRITISCH')
        uses: actions/github-script@v7
        with:
          script: |
            const fs = require('fs');
            const report = fs.readFileSync('hunter_report.md', 'utf8');
            github.rest.issues.create({
              owner: context.repo.owner,
              repo: context.repo.repo,
              title: 'Must-Fix Hunter: Konsistenz-Probleme gefunden',
              body: report,
              labels: ['audit', 'must-fix']
            });
```

---

## Nicht-Ziele

- **Kein Verhaltens-Test** — das ist test-master
- **Kein Architektur-Audit** — das ist architecture-guardian
- **Keine Security-Scans** — das ist security-robustness
- **Keine Performance-Analyse** — das ist performance-memory

Dieser Agent hat genau **einen** Fokus: **Widersprüche im Code-Base finden
bevor sie Must-Fix-Krisen auslösen**.

---

## Unterschied zu code-auditor

| Aspekt | code-auditor | must-fix-hunter |
|--------|--------------|-----------------|
| Fokus | Statische Qualität + Cross-Platform + CI-Readiness | Widersprüche zwischen Code-Stellen |
| Frequenz | Vor Release | Nightly + on-demand |
| Output | findings.json für andere Agenten | Priorisierte Must-Fix-Liste |
| Problem | "Ist dieser Code gut?" | "Sind diese zwei Stellen konsistent?" |

Beide sind komplementär — nicht überlappend.
