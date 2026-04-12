---
name: code-auditor
description: >
  Universeller Code-Qualitäts-Audit für Python-Tools. Misst exakte Zahlen zu
  Stabilität, Architektur, Features, Datensicherheit und Code-Qualität.
  Produziert eine strukturierte findings.json die von anderen Agenten
  konsumiert werden kann. Use when: Nutzer gibt eine oder mehrere .py-Dateien
  und will wissen wie gut sie sind und was fehlt. IMMER wenn ein Audit
  angefragt wird — dieser Agent läuft zuerst, danach übernehmen Fix-Agenten.
  Unterstützt beliebige Python-Tools, nicht nur die drei Messtechnik-Tools.
model: claude-sonnet-4-20250514
tools:
  - type: bash
  - type: text_editor
---

# Code Auditor

## Mission

Jede Aussage wird durch Messwerte belegt. Kein Raten, nur Zählen.
Output ist immer `findings.json` — maschinenlesbar für nachgelagerte Agenten.

---

## Phase 0 — Dateien finden

```python
import os, sys, glob
from pathlib import Path

def find_targets(args):
    """
    Akzeptiert: Pfad, Verzeichnis, Glob, oder mehrere davon.
    Gibt Liste von .py-Dateien zurück.
    """
    targets = []
    for arg in args:
        p = Path(arg)
        if p.is_file() and p.suffix == '.py':
            targets.append(p)
        elif p.is_dir():
            targets.extend(p.rglob('*.py'))
        else:
            targets.extend(Path('.').glob(arg))
    return sorted(set(targets))
```

---

## Phase 1 — Messung (alle Metriken)

```python
import re, ast, tokenize, io

def audit_file(path: Path) -> dict:
    """
    Vollständige Messung einer Python-Datei.
    Gibt strukturiertes dict zurück.
    """
    src = path.read_text(encoding='utf-8', errors='replace')
    lines = src.splitlines()

    result = {
        'file':    str(path),
        'name':    path.stem,
        'lines':   len(lines),
        'metrics': {},
        'findings': [],   # Liste von Finding-Dicts
        'score':   0,
    }

    m = result['metrics']

    # ── Grundstruktur ──────────────────────────────────────────────
    m['functions']   = len(re.findall(r'^(?:def |    def )\w+', src, re.M))
    m['classes']     = len(re.findall(r'^class \w+', src, re.M))
    m['imports']     = len(re.findall(r'^import |^from ', src, re.M))

    # ── Qualitäts-Metriken ─────────────────────────────────────────
    m['todos']       = len(re.findall(r'#.*(TODO|FIXME|HACK|XXX)', src, re.I))
    m['bare_except'] = len(re.findall(r'except:\s*$', src, re.M))
    m['except_pass'] = len(re.findall(r'except[^:]*:\s*\n\s*pass', src))
    m['exc_total']   = len(re.findall(r'\bexcept\b', src))
    m['exc_logged']  = len(re.findall(r'except.*:\s*\n[^\n]*(?:log\.|logger\.)', src))
    m['exc_silent']  = m['exc_total'] - m['exc_logged']
    m['exc_coverage_pct'] = int(m['exc_logged'] / m['exc_total'] * 100) if m['exc_total'] else 100

    # ── Logging ────────────────────────────────────────────────────
    m['log_calls']   = len(re.findall(r'\blog\.\w+\(|\blogger\.\w+\(', src))
    m['has_logging'] = 'import logging' in src or 'getLogger' in src

    # ── Datensicherheit ────────────────────────────────────────────
    m['has_undo']    = '_undo_stack' in src or 'undo_stack' in src
    m['has_redo']    = '_redo_stack' in src or 'redo_stack' in src
    m['max_undo']    = int(re.search(r'MAX_UNDO\s*=\s*(\d+)', src).group(1)) \
                       if re.search(r'MAX_UNDO\s*=\s*(\d+)', src) else None
    m['has_backup']  = '_rotate_backup' in src or '.bak' in src
    m['backup_calls']= len(re.findall(r'_rotate_backup', src))
    m['has_audit']   = 'audit_log' in src or '_write_audit' in src
    m['has_dirty']   = '_dirty' in src or 'dirty' in src.lower()

    # ── Fehler-Architektur ─────────────────────────────────────────
    m['error_classes']    = len(re.findall(r'^class \w+Error\(', src, re.M))
    m['has_error_hier']   = bool(re.search(r'class \w+Error\(\w+Error\)', src))
    m['has_handle_error'] = 'handle_error' in src or 'gui_run' in src

    # ── Threading ─────────────────────────────────────────────────
    m['thread_count']   = len(re.findall(r'Thread\(target=', src))
    m['has_threading']  = m['thread_count'] > 0
    m['has_after']      = 'root.after(' in src or 'self.after(' in src

    # Schwere Ops ohne Thread
    heavy_ops = ['export_csv','export_excel','export_html','export_pdf',
                 'import_flw','batch_analyze','normalize_all','batch_convert',
                 'merge_','Pipeline','sor_batch','project_analyze']
    blockers = []
    for op in heavy_ops:
        if op in src:
            surrounding = src[max(0,src.find(op)-200):src.find(op)+200]
            if 'Thread' not in surrounding:
                blockers.append(op)
    m['gui_blockers'] = blockers

    # ── Tests & Validation ─────────────────────────────────────────
    m['has_selftest']   = bool(re.search(r'_selftest|_run_golden|_run_selftest', src))
    m['has_roundtrip']  = 'roundtrip' in src.lower()
    m['has_validation'] = 'validate' in src.lower()
    m['test_functions'] = len(re.findall(r'def test_\w+', src))

    # ── Code-Qualität ──────────────────────────────────────────────
    m['type_hint_fns'] = len(re.findall(r'def \w+\([^)]*: \w', src))
    m['dataclasses']   = len(re.findall(r'@dataclass', src))
    m['docstrings']    = len(re.findall(r'"""', src)) // 2

    # ── UI-Features ────────────────────────────────────────────────
    m['has_dark_mode']  = 'dark_mode' in src or '_apply_dark' in src
    m['has_dnd']        = 'DND_FILES' in src or 'tkinterdnd' in src
    m['has_settings']   = 'SettingsDialog' in src or 'DEFAULT_SETTINGS' in src
    m['has_progress']   = 'ProgressDialog' in src or 'progressbar' in src.lower()
    m['has_recent']     = '_recent_files' in src or 'recent' in src.lower()
    m['has_cli']        = 'argparse' in src

    # ── Lange Funktionen ───────────────────────────────────────────
    long_fns = []
    fn_name, fn_start = '', 0
    for i, line in enumerate(lines):
        mm = re.match(r'^(def |    def )(\w+)', line)
        if mm:
            if fn_start and (i - fn_start) > 80:
                long_fns.append({'name': fn_name, 'start': fn_start+1,
                                  'length': i - fn_start})
            fn_name, fn_start = mm.group(2), i
    long_fns.sort(key=lambda x: -x['length'])
    m['long_functions'] = long_fns[:10]
    m['fn_over_200']    = sum(1 for f in long_fns if f['length'] > 200)
    m['fn_over_500']    = sum(1 for f in long_fns if f['length'] > 500)

    # ── Export/Import Formate ──────────────────────────────────────
    formats = {
        'csv':   any(x in src for x in ['export_csv', 'to_csv', 'csv.writer']),
        'json':  any(x in src for x in ['export_json', 'json.dump']),
        'excel': any(x in src for x in ['openpyxl', '.xlsx', 'xlwt']),
        'html':  any(x in src for x in ['export_html', 'export_pdf_html']),
        'xml':   any(x in src for x in ['export_xml', 'ElementTree']),
        'pdf':   any(x in src for x in ['fpdf', 'reportlab', 'weasyprint',
                                          'export_pdf']),
    }
    m['export_formats'] = [k for k, v in formats.items() if v]

    return result
```

---

## Phase 2 — Findings generieren

```python
def generate_findings(audit: dict) -> list:
    """
    Wandelt Metriken in strukturierte Findings um.
    Jedes Finding:
      - id:        eindeutiger Bezeichner (für Router)
      - severity:  'critical' | 'high' | 'medium' | 'low'
      - category:  'stability' | 'safety' | 'arch' | 'ui' | 'quality'
      - title:     Kurzbeschreibung
      - detail:    Messwert + Kontext
      - fix_agent: welcher Agent soll das fixen
      - effort_h:  geschätzter Aufwand in Stunden
    """
    m = audit['metrics']
    findings = []

    def finding(fid, severity, category, title, detail, fix_agent, effort):
        findings.append({
            'id': fid, 'severity': severity, 'category': category,
            'title': title, 'detail': detail,
            'fix_agent': fix_agent, 'effort_h': effort,
            'file': audit['name'],
        })

    # ── CRITICAL ──────────────────────────────────────────────────
    if m['bare_except'] > 0:
        finding('bare_except', 'critical', 'stability',
            f"bare except: {m['bare_except']} Stück",
            "bare except fängt ALLES ab inkl. SystemExit/KeyboardInterrupt",
            'quality-fixer', 0.5)

    if m['exc_coverage_pct'] == 0 and m['exc_total'] > 10:
        finding('exc_logging_zero', 'critical', 'stability',
            f"0% Exception-Logging ({m['exc_total']} Catches, 0 geloggt)",
            "Alle Fehler verschwinden lautlos — kein Debugging möglich",
            'quality-fixer', 2.0)

    if not m['has_error_hier'] and m['exc_total'] > 20:
        finding('no_error_hierarchy', 'critical', 'stability',
            "Keine Fehlerklassen-Hierarchie",
            "Alle Fehler als generische Exception — kein gezieltes Catching",
            'quality-fixer', 1.0)

    # ── HIGH ──────────────────────────────────────────────────────
    if m['exc_coverage_pct'] < 40 and m['exc_total'] > 10:
        finding('exc_logging_low', 'high', 'stability',
            f"Exception-Logging nur {m['exc_coverage_pct']}%",
            f"{m['exc_logged']}/{m['exc_total']} Catches geloggt, "
            f"{m['exc_silent']} stumm",
            'quality-fixer', 1.5)

    if m['gui_blockers']:
        finding('gui_blocking', 'high', 'arch',
            f"GUI-blockierende Ops: {len(m['gui_blockers'])}",
            f"Kein Thread-Wrapper für: {', '.join(m['gui_blockers'][:5])}",
            'threading-fixer', 3.0)

    if not m['has_undo'] and m['has_dirty']:
        finding('no_undo', 'high', 'safety',
            "Kein Undo/Redo trotz Bearbeitungs-Modus",
            "Nutzer kann Änderungen nicht rückgängig machen",
            'undo-fixer', 3.0)

    if not m['has_selftest'] and m['lines'] > 2000:
        finding('no_selftest', 'high', 'quality',
            "Kein Selftest bei großem Tool",
            f"{m['lines']} Zeilen ohne automatisierten Test",
            'test-fixer', 2.0)

    if not m['has_roundtrip'] and m['lines'] > 2000:
        finding('no_roundtrip', 'high', 'quality',
            "Keine Roundtrip-Validation",
            "Save→Load Konsistenz wird nicht geprüft",
            'test-fixer', 1.5)

    # ── MEDIUM ────────────────────────────────────────────────────
    if m['except_pass'] > 5:
        finding('except_pass', 'medium', 'stability',
            f"except+pass: {m['except_pass']} Stück",
            "Fehler werden absorbiert ohne Konsequenz",
            'quality-fixer', 1.0)

    if m['fn_over_200'] > 3:
        top = m['long_functions'][:3]
        finding('long_functions', 'medium', 'quality',
            f"{m['fn_over_200']} Funktionen über 200 Zeilen",
            f"Größte: " + ', '.join(f"{f['name']}({f['length']}Z)" for f in top),
            'refactor-agent', 2.0)

    if m['fn_over_500'] > 0:
        worst = [f for f in m['long_functions'] if f['length'] > 500]
        finding('fn_500', 'medium', 'quality',
            f"Funktion(en) über 500 Zeilen: {len(worst)}",
            f"Muss aufgeteilt werden: " + ', '.join(f"{f['name']}({f['length']}Z)" for f in worst),
            'refactor-agent', 3.0)

    if not m['has_dark_mode']:
        finding('no_dark_mode', 'medium', 'ui',
            "Kein Dark Mode",
            "UI-Konsistenz mit anderen Tools der Suite fehlt",
            'ui-fixer', 2.0)

    if not m['has_dnd']:
        finding('no_dnd', 'medium', 'ui',
            "Kein Drag & Drop",
            "DND_FILES / tkinterdnd2 nicht implementiert",
            'ui-fixer', 1.0)

    if not m['has_backup'] and m['lines'] > 1000:
        finding('no_backup', 'medium', 'safety',
            "Keine Backup-Rotation",
            "_rotate_backup fehlt — Überschreiben ohne .bak",
            'safety-fixer', 0.5)

    if not m['has_audit']:
        finding('no_audit', 'medium', 'safety',
            "Kein Audit Log",
            "Änderungen werden nicht protokolliert",
            'safety-fixer', 1.0)

    # ── LOW ───────────────────────────────────────────────────────
    if m['type_hint_fns'] < 20 and m['functions'] > 50:
        finding('few_type_hints', 'low', 'quality',
            f"Wenig Type Hints ({m['type_hint_fns']}/{m['functions']} Fkt)",
            "IDE-Unterstützung und Lesbarkeit leiden",
            'quality-fixer', 2.0)

    if m['dataclasses'] == 0 and m['classes'] > 5:
        finding('no_dataclasses', 'low', 'quality',
            "Keine @dataclass genutzt",
            "Daten-Klassen als plain dict oder manuelle __init__",
            'refactor-agent', 1.0)

    if not m['has_settings']:
        finding('no_settings', 'low', 'ui',
            "Kein Settings Dialog",
            "Konfiguration nur über Datei oder Konstanten",
            'ui-fixer', 2.0)

    if m['todos'] > 0:
        finding('has_todos', 'low', 'quality',
            f"{m['todos']} TODOs/FIXMEs im Code",
            "Unfertige Stellen dokumentiert",
            'quality-fixer', 1.0)

    # Sortierung: critical → high → medium → low
    order = {'critical': 0, 'high': 1, 'medium': 2, 'low': 3}
    findings.sort(key=lambda x: order[x['severity']])
    return findings
```

---

## Phase 3 — Score berechnen

```python
def compute_score(metrics: dict, findings: list) -> dict:
    """
    Gewichteter Score 0–100.
    Abzüge nach Severity: critical=-15, high=-8, medium=-3, low=-1
    Boni für positive Features.
    """
    score = 100
    deductions = {'critical': 15, 'high': 8, 'medium': 3, 'low': 1}
    for f in findings:
        score -= deductions.get(f['severity'], 0)

    # Boni
    m = metrics
    if m['has_selftest']:    score += 3
    if m['has_roundtrip']:   score += 2
    if m['has_audit']:       score += 2
    if m['has_undo']:        score += 2
    if m['has_error_hier']:  score += 2
    if m['log_calls'] > 100: score += 2

    score = max(0, min(100, score))

    grade = 'A' if score >= 90 else 'B' if score >= 80 else 'C' if score >= 70 else 'D' if score >= 60 else 'F'

    return {
        'score': score,
        'grade': grade,
        'critical': sum(1 for f in findings if f['severity']=='critical'),
        'high':     sum(1 for f in findings if f['severity']=='high'),
        'medium':   sum(1 for f in findings if f['severity']=='medium'),
        'low':      sum(1 for f in findings if f['severity']=='low'),
        'total':    len(findings),
    }
```

---

## Phase 4 — findings.json schreiben

```python
import json, datetime
from pathlib import Path

def run_audit(file_paths: list, output_dir: str = '.') -> dict:
    """
    Hauptfunktion. Auditiert alle Dateien, schreibt findings.json.
    """
    timestamp = datetime.datetime.now(datetime.timezone.utc).isoformat()
    report = {
        'audit_timestamp': timestamp,
        'auditor_version': '1.0',
        'files': [],
        'summary': {},
        'all_findings': [],
        'routing': {},      # finding_id → fix_agent
    }

    for path in file_paths:
        p = Path(path)
        if not p.exists():
            print(f"SKIP: {path} nicht gefunden")
            continue

        print(f"Auditiere: {p.name}...")
        audit  = audit_file(p)
        findings = generate_findings(audit)
        score_info = compute_score(audit['metrics'], findings)

        audit['findings'] = findings
        audit['score_info'] = score_info
        report['files'].append(audit)
        report['all_findings'].extend(findings)

        print(f"  Score: {score_info['score']}/100 ({score_info['grade']})  "
              f"Findings: {score_info['critical']}C / {score_info['high']}H / "
              f"{score_info['medium']}M / {score_info['low']}L")

    # Routing-Map (welcher Agent soll welche Findings bearbeiten)
    routing = {}
    for f in report['all_findings']:
        agent = f['fix_agent']
        routing.setdefault(agent, []).append(f['id'] + ':' + f['file'])
    report['routing'] = routing

    # Summary
    all_scores = [a['score_info']['score'] for a in report['files']]
    report['summary'] = {
        'files_audited': len(report['files']),
        'avg_score': int(sum(all_scores) / len(all_scores)) if all_scores else 0,
        'total_findings': len(report['all_findings']),
        'by_agent': {agent: len(ids) for agent, ids in routing.items()},
        'total_effort_h': round(sum(f['effort_h'] for f in report['all_findings']), 1),
    }

    # Schreiben
    out_path = Path(output_dir) / 'findings.json'
    out_path.write_text(json.dumps(report, indent=2, ensure_ascii=False))
    print(f"\nfindings.json → {out_path}")
    print(f"Gesamtaufwand: ~{report['summary']['total_effort_h']}h")
    print(f"Routing:")
    for agent, ids in routing.items():
        print(f"  {agent}: {len(ids)} Findings")

    return report

# ── Direktaufruf ───────────────────────────────────────────────────
if __name__ == '__main__':
    import sys
    files = sys.argv[1:] if len(sys.argv) > 1 else []
    if not files:
        print("Verwendung: python3 code_auditor.py datei1.py datei2.py ...")
        print("            python3 code_auditor.py ./mein_projekt/*.py")
        sys.exit(1)
    run_audit(files)
```

---

## findings.json Format (für nachgelagerte Agenten)

```json
{
  "audit_timestamp": "2026-04-12T10:30:00Z",
  "auditor_version": "1.0",
  "files": [
    {
      "file": "sor_editor.py",
      "name": "sor_editor",
      "lines": 15684,
      "metrics": {
        "functions": 295,
        "exc_total": 217,
        "exc_logged": 73,
        "exc_coverage_pct": 33,
        "has_undo": false,
        "has_selftest": true,
        "long_functions": [
          {"name": "_s", "start": 1879, "length": 359}
        ],
        "fn_over_200": 12,
        "gui_blockers": ["export_csv", "batch_analyze"]
      },
      "findings": [
        {
          "id": "exc_logging_low",
          "severity": "high",
          "category": "stability",
          "title": "Exception-Logging nur 33%",
          "detail": "73/217 Catches geloggt, 144 stumm",
          "fix_agent": "quality-fixer",
          "effort_h": 1.5,
          "file": "sor_editor"
        }
      ],
      "score_info": {
        "score": 82,
        "grade": "B",
        "critical": 0, "high": 2, "medium": 4, "low": 3
      }
    }
  ],
  "routing": {
    "quality-fixer":    ["exc_logging_low:sor_editor", "exc_logging_zero:fluke_editor"],
    "threading-fixer":  ["gui_blocking:sor_editor", "gui_blocking:fluke_editor"],
    "test-fixer":       ["no_selftest:fluke_editor"],
    "undo-fixer":       ["no_undo:sor_editor"],
    "ui-fixer":         ["no_dark_mode:sor_editor"]
  },
  "summary": {
    "files_audited": 3,
    "avg_score": 73,
    "total_findings": 18,
    "total_effort_h": 24.5,
    "by_agent": {
      "quality-fixer": 6,
      "threading-fixer": 2,
      "test-fixer": 3
    }
  }
}
```

---

## Wie nachgelagerte Agenten die findings.json lesen

```python
import json

def load_findings_for_agent(findings_path: str, my_agent_name: str) -> list:
    """
    Lädt nur die Findings die für diesen Agenten bestimmt sind.
    Aufruf in jedem Fix-Agenten am Anfang.
    """
    report = json.loads(open(findings_path).read())
    my_ids = set(report['routing'].get(my_agent_name, []))

    my_findings = []
    for file_audit in report['files']:
        for f in file_audit['findings']:
            key = f['id'] + ':' + f['file']
            if key in my_ids:
                my_findings.append({
                    **f,
                    'metrics': file_audit['metrics'],
                    'filepath': file_audit['file'],
                })

    return sorted(my_findings, key=lambda x: {'critical':0,'high':1,'medium':2,'low':3}[x['severity']])

# Beispiel im quality-fixer:
findings = load_findings_for_agent('findings.json', 'quality-fixer')
for f in findings:
    print(f"[{f['severity'].upper()}] {f['filepath']}: {f['title']}")
    print(f"  → {f['detail']}")
```

---

## Checkliste

- [ ] Alle Zieldateien gefunden und lesbar
- [ ] Jede Metrik direkt aus dem Code-Text gemessen
- [ ] findings.json vollständig geschrieben
- [ ] Routing-Map vollständig (jedes Finding hat fix_agent)
- [ ] Summary-Aufwand plausibel (<100h für ein einzelnes Tool)
- [ ] Keine Annahmen — nur was messbar ist
