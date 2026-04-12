---
name: quality-fixer
description: >
  Behebt Code-Qualitätsprobleme aus findings.json: Exception-Logging,
  bare excepts, stille Catches, fehlende Fehlerklassen-Hierarchie, TODOs.
  Liest Findings mit fix_agent=quality-fixer aus findings.json und
  implementiert die Fixes direkt in der Zieldatei. Use when: audit-router
  hat quality-fixer Findings zugewiesen, oder Nutzer will Exception-Coverage
  verbessern.
model: claude-sonnet-4-20250514
tools:
  - type: bash
  - type: text_editor
---

# Quality Fixer

## Phase 0 — Findings laden

```python
import json
from pathlib import Path

def my_findings(findings_path='findings.json'):
    report = json.loads(Path(findings_path).read_text())
    return [
        {**f, 'filepath': fa['file'], 'metrics': fa['metrics']}
        for fa in report['files']
        for f in fa['findings']
        if f['fix_agent'] == 'quality-fixer'
    ]

findings = my_findings()
for f in findings:
    print(f"[{f['severity'].upper()}] {f['filepath']}: {f['title']}")
```

---

## Fix 1 — bare except → except Exception as e

```python
import re

def fix_bare_excepts(filepath: str) -> int:
    src = Path(filepath).read_text()
    # bare except: → except Exception as e:
    new_src, n = re.subn(
        r'^(\s*)except:\s*$',
        r'\1except Exception as e:',
        src, flags=re.MULTILINE)
    if n:
        Path(filepath).write_text(new_src)
        print(f"  {filepath}: {n} bare excepts gefixt")
    return n
```

---

## Fix 2 — stille Exceptions mit Logging versehen

```python
def fix_silent_exceptions(filepath: str, logger_name: str = 'log') -> int:
    """
    Findet except-Blöcke ohne log/raise und fügt log.debug ein.
    Konservativ: nur bei except Exception as e und except SomeError as e.
    """
    src = Path(filepath).read_text()
    lines = src.splitlines(keepends=True)
    result = []
    i = 0
    fixes = 0

    while i < len(lines):
        line = lines[i]
        m = re.match(r'^(\s*)except\s+\w[\w.]*\s+as\s+(\w+)\s*:', line)
        if m:
            indent   = m.group(1)
            exc_var  = m.group(2)
            result.append(line)
            i += 1
            # Nächste nicht-leere Zeile prüfen
            if i < len(lines):
                next_line = lines[i]
                stripped  = next_line.strip()
                has_log   = any(x in next_line for x in ['log.', 'logger.', 'print(', 'raise', 'return'])
                if not has_log and stripped:
                    # log.debug einfügen
                    log_line = f"{indent}    {logger_name}.debug('%s: %s', type({exc_var}).__name__, {exc_var})\n"
                    result.append(log_line)
                    fixes += 1
        else:
            result.append(line)
        i += 1

    if fixes:
        Path(filepath).write_text(''.join(result))
        print(f"  {filepath}: {fixes} stille Exceptions mit log.debug versehen")
    return fixes
```

---

## Fix 3 — Fehlerklassen-Hierarchie hinzufügen

```python
HIERARCHY_TEMPLATE = '''
# ── Fehlerklassen ─────────────────────────────────────────────────
class ToolError(Exception):
    """Basis-Ausnahme für alle Tool-Fehler."""
    def __init__(self, msg: str, ctx=None):
        super().__init__(msg)
        self.ctx = ctx or {}

class FileFormatError(ToolError):
    """Dateiformat unbekannt oder beschädigt."""

class ValidationError(ToolError):
    """Daten sind strukturell valide aber inhaltlich inkonsistent."""

class SaveError(ToolError):
    """Speichern fehlgeschlagen."""

class InternalToolError(ToolError):
    """Unerwarteter interner Fehler (Bug)."""
# ──────────────────────────────────────────────────────────────────
'''

def add_error_hierarchy(filepath: str) -> bool:
    src = Path(filepath).read_text()
    if 'class ToolError' in src:
        print(f"  {filepath}: Fehlerklassen bereits vorhanden")
        return False
    # Nach den Imports einfügen
    import_end = max(
        src.rfind('\nimport '), src.rfind('\nfrom '),
        src.rfind('\n# ==='))
    if import_end < 0:
        import_end = 0
    pos = src.find('\n', import_end) + 1
    new_src = src[:pos] + HIERARCHY_TEMPLATE + src[pos:]
    Path(filepath).write_text(new_src)
    print(f"  {filepath}: Fehlerklassen-Hierarchie hinzugefügt")
    return True
```

---

## Fix 4 — TODOs dokumentieren (nicht automatisch entfernen)

```python
def report_todos(filepath: str) -> list:
    src = Path(filepath).read_text()
    todos = []
    for i, line in enumerate(src.splitlines(), 1):
        m = re.search(r'#.*(TODO|FIXME|HACK|XXX)(.*)', line, re.I)
        if m:
            todos.append({'line': i, 'type': m.group(1), 'text': m.group(2).strip()})
    if todos:
        print(f"\n  {filepath}: {len(todos)} offene Punkte")
        for t in todos:
            print(f"    Z.{t['line']:>5} [{t['type']}] {t['text']}")
    return todos
```

---

## Hauptlauf

```python
def run(findings_path='findings.json'):
    findings = my_findings(findings_path)
    if not findings:
        print("Keine quality-fixer Findings. Nichts zu tun.")
        return

    # Dateien die gefixt werden müssen
    files_to_fix = list(set(f['filepath'] for f in findings))

    for fpath in files_to_fix:
        print(f"\n{'─'*50}")
        print(f"  {fpath}")
        print(f"{'─'*50}")

        # Welche Findings für diese Datei?
        file_findings = [f for f in findings if f['filepath'] == fpath]
        ids = [f['id'] for f in file_findings]

        # Backup
        import shutil, datetime
        bak = fpath + '.' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S') + '.bak'
        shutil.copy2(fpath, bak)
        print(f"  Backup: {bak}")

        if 'bare_except' in ids:
            fix_bare_excepts(fpath)

        if any(x in ids for x in ['exc_logging_zero', 'exc_logging_low']):
            # Logger-Name aus Datei erkennen
            src = Path(fpath).read_text()
            log_name = 'log' if 'log = logging' in src else 'logger'
            fix_silent_exceptions(fpath, log_name)

        if 'no_error_hierarchy' in ids:
            add_error_hierarchy(fpath)

        if 'has_todos' in ids:
            report_todos(fpath)

    print(f"\n{'='*50}")
    print(f"Quality-Fix abgeschlossen. Empfehlung: code-auditor erneut ausführen.")

if __name__ == '__main__':
    import sys
    run(sys.argv[1] if len(sys.argv) > 1 else 'findings.json')
```

---

## Checkliste

- [ ] Backup vor jedem Fix (`file.bak`)
- [ ] bare excepts gefixt (kein `except:` mehr)
- [ ] Stille Exceptions mit `log.debug` versehen
- [ ] Fehlerklassen-Hierarchie eingefügt (wenn fehlte)
- [ ] TODOs dokumentiert (nicht blind gelöscht)
- [ ] code-auditor danach ausführen und Coverage-% messen
