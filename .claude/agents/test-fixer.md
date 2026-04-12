---
name: test-fixer
description: >
  Fügt Selftests, Roundtrip-Validation und Golden Tests für Python-Tools
  hinzu. Liest no_selftest und no_roundtrip Findings aus findings.json.
  Use when: audit-router weist test-fixer Findings zu.
model: claude-sonnet-4-20250514
tools:
  - type: bash
  - type: text_editor
---

# Test Fixer

## Phase 0 — Findings laden

```python
import json
from pathlib import Path
findings = [
    {**f, 'filepath': fa['file']}
    for fa in json.loads(Path('findings.json').read_text())['files']
    for f in fa['findings'] if f['fix_agent'] == 'test-fixer'
]
```

## Fix 1 — Selftest-Gerüst

```python
SELFTEST_TEMPLATE = '''
def _run_selftest(extra_dirs=None):
    """Eingebauter Selftest. Gibt Exit-Code zurück (0=OK)."""
    import tempfile, os
    errors = 0

    def test(name, fn):
        nonlocal errors
        try:
            fn()
            print(f"  ✓ {name}")
        except Exception as e:
            print(f"  ✗ {name}: {e}")
            errors += 1

    print("=== Selftest ===")

    # Roundtrip-Test
    def test_roundtrip():
        # TODO: datei laden → speichern → neu laden → vergleichen
        pass
    test("roundtrip_save_load", test_roundtrip)

    # Schema-Test
    def test_schema():
        # TODO: Mindestfelder prüfen
        pass
    test("schema_validation", test_schema)

    print(f"{'OK' if errors==0 else f'{errors} Fehler'}")
    return errors

if __name__ == '__main__':
    import sys
    if '--selftest' in sys.argv:
        sys.exit(_run_selftest())
'''

def add_selftest(filepath):
    src = Path(filepath).read_text()
    if '_run_selftest' in src:
        print(f"  {filepath}: Selftest bereits vorhanden"); return
    src += SELFTEST_TEMPLATE
    Path(filepath).write_text(src)
    print(f"  {filepath}: Selftest-Gerüst eingefügt")
```

## Checkliste
- [ ] `_run_selftest()` eingefügt
- [ ] `--selftest` CLI-Flag registriert  
- [ ] Mindestens Roundtrip-Test implementiert
- [ ] Golden Test für bekannte Eingabe vorhanden
