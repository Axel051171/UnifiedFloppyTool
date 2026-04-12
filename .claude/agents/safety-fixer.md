---
name: safety-fixer
description: >
  Fügt Datensicherheits-Features hinzu: _rotate_backup, Audit-Log,
  Dirty-Flag. Liest no_backup, no_audit Findings aus findings.json.
  Use when: audit-router weist safety-fixer Findings zu.
model: claude-sonnet-4-20250514
tools:
  - type: bash
  - type: text_editor
---

# Safety Fixer

## Fix 1 — _rotate_backup

```python
ROTATE_BACKUP = '''
def _rotate_backup(path: str, max_backups: int = 3) -> None:
    """Rotiert Backups: path.bak.1 → .bak.2 → .bak.3 → löschen."""
    import shutil, os
    for i in range(max_backups - 1, 0, -1):
        src = f"{path}.bak.{i}"
        dst = f"{path}.bak.{i+1}"
        if os.path.exists(src):
            if os.path.exists(dst): os.unlink(dst)
            shutil.move(src, dst)
    if os.path.exists(path):
        shutil.copy2(path, f"{path}.bak.1")
'''

def add_rotate_backup(filepath):
    src = Path(filepath).read_text()
    if '_rotate_backup' in src:
        print(f"  {filepath}: _rotate_backup bereits vorhanden"); return
    # Vor erste Klasse einfügen
    import re
    m = re.search(r'^class \w+', src, re.M)
    pos = m.start() if m else len(src)
    Path(filepath).write_text(src[:pos] + ROTATE_BACKUP + '\n' + src[pos:])
    print(f"  {filepath}: _rotate_backup eingefügt")
```

## Fix 2 — Audit Log einfügen

```python
AUDIT_LOG_FN = '''
def _write_audit_log(path: str, action: str, fields: dict) -> str:
    """Schreibt eine Zeile in das Audit-Log."""
    import datetime, json
    log_path = path + '.audit.jsonl'
    entry = {
        'ts': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'action': action, 'file': path, **fields
    }
    with open(log_path, 'a', encoding='utf-8') as f:
        f.write(json.dumps(entry, ensure_ascii=False) + '\\n')
    return log_path
'''
```

## Checkliste
- [ ] `_rotate_backup` eingefügt und vor jedem Save aufgerufen
- [ ] Audit-Log-Funktion vorhanden
- [ ] Dirty-Flag gesetzt bei jeder Änderung
