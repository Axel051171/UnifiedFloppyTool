---
name: threading-fixer
description: >
  Wraps GUI-blockierende Operationen in Thread(target=worker) Pattern.
  Liest gui_blocking Findings aus findings.json und fügt _run_in_thread()
  Helper ein, dann kapselt alle blockierenden Export/Import/Batch-Operationen.
  Use when: audit-router weist gui_blocking Findings zu.
model: claude-sonnet-4-20250514
tools:
  - type: bash
  - type: text_editor
---

# Threading Fixer

## Phase 0 — Findings laden

```python
import json
from pathlib import Path

findings = [
    {**f, 'filepath': fa['file'], 'metrics': fa['metrics']}
    for fa in json.loads(Path('findings.json').read_text())['files']
    for f in fa['findings']
    if f['fix_agent'] == 'threading-fixer'
]
```

---

## Fix — _run_in_thread Helper einfügen

```python
THREAD_HELPER = '''
    def _run_in_thread(self, func, on_done=None, on_error=None, label='Bitte warten…'):
        """Führt func() in Background-Thread aus. GUI bleibt responsiv."""
        import threading

        def worker():
            try:
                result = func()
                if on_done:
                    self.root.after(0, lambda: on_done(result))
            except Exception as e:
                log.error('_run_in_thread: %s', e)
                if on_error:
                    self.root.after(0, lambda: on_error(e))
                else:
                    self.root.after(0, lambda: messagebox.showerror('Fehler', str(e)))
            finally:
                self.root.after(0, lambda: self.root.config(cursor=''))

        self.root.config(cursor='watch')
        threading.Thread(target=worker, daemon=True).start()
'''

def add_thread_helper(filepath: str, class_name: str) -> bool:
    src = Path(filepath).read_text()
    if '_run_in_thread' in src:
        print(f"  {filepath}: _run_in_thread bereits vorhanden")
        return False

    # Nach __init__ der GUI-Klasse einfügen
    import re
    pattern = rf'(class {class_name}.*?def __init__.*?\n)(    def )'
    m = re.search(pattern, src, re.DOTALL)
    if m:
        pos = m.start(2)
        new_src = src[:pos] + THREAD_HELPER + '\n' + src[pos:]
        Path(filepath).write_text(new_src)
        print(f"  {filepath}: _run_in_thread eingefügt")
        return True
    print(f"  {filepath}: Konnte Einfügepunkt nicht finden — manuell ergänzen")
    return False
```

---

## Fix — Blockierende Ops umschreiben

Für jede Funktion in `metrics['gui_blockers']`:

```python
BEFORE = '''    def export_csv(self):
        # ... langer Export-Code ...
        self._do_export_csv()'''

AFTER  = '''    def export_csv(self):
        self._run_in_thread(
            func=self._do_export_csv,
            on_done=lambda _: self._sv.set("Export abgeschlossen"),
            label="CSV wird exportiert…"
        )

    def _do_export_csv(self):
        # ... langer Export-Code ... (unverändert)'''
```

**Muster für alle blockierenden Ops:**
1. Aktuelle Funktion in `_do_FUNC()` umbenennen
2. Neue `FUNC()` schreibt nur `self._run_in_thread(self._do_FUNC)`
3. Progress-Feedback im `on_done`-Callback

---

## Checkliste

- [ ] `_run_in_thread` in GUI-Klasse eingefügt
- [ ] Alle `gui_blockers` aus findings.json abgearbeitet
- [ ] Jede blockierende Op in `_do_FUNC` + `FUNC` aufgeteilt
- [ ] `on_error` Callback vorhanden (Fehlermeldung im Hauptthread)
- [ ] `cursor='watch'` während Ausführung
- [ ] Keine direkten GUI-Zugriffe aus dem Worker-Thread
