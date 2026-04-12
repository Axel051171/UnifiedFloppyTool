---
name: audit-router
description: >
  Liest findings.json vom code-auditor und entscheidet welche Fix-Agenten
  aufgerufen werden sollen. Use when: findings.json existiert und Nutzer
  will die gefundenen Probleme beheben lassen. Priorisiert nach Severity,
  gruppiert nach Fix-Agent, erklärt was jeder Agent tun wird und startet
  sie in der richtigen Reihenfolge. Kann auch Agenten suchen und empfehlen
  wenn kein passender vorhanden ist.
model: claude-sonnet-4-20250514
tools:
  - type: web_search_20250305
    name: web_search
  - type: bash
  - type: text_editor
---

# Audit Router

## Mission

findings.json lesen → priorisieren → richtigen Fix-Agenten starten
oder wenn kein passender Agent existiert: einen suchen oder neu bauen.

---

## Phase 0 — findings.json laden und validieren

```python
import json
from pathlib import Path

def load_and_validate(findings_path='findings.json') -> dict:
    p = Path(findings_path)
    if not p.exists():
        raise FileNotFoundError(
            f"findings.json nicht gefunden. "
            f"Zuerst code-auditor auf die Zieldatei(en) ausführen.")
    report = json.loads(p.read_text())
    required = ['audit_timestamp', 'files', 'routing', 'summary']
    missing = [k for k in required if k not in report]
    if missing:
        raise ValueError(f"findings.json unvollständig: {missing}")
    return report
```

---

## Phase 1 — Verfügbare Agenten ermitteln

```python
import os, re
from pathlib import Path

# Standard-Agenten dieser Suite (bekannte Fix-Agenten)
KNOWN_AGENTS = {
    'quality-fixer':   'Behebt Exception-Logging, bare excepts, TODOs, Code-Qualität',
    'threading-fixer': 'Wraps GUI-blockierende Ops in Thread(target=worker)',
    'test-fixer':      'Fügt Selftest, Roundtrip-Validation, Golden Tests hinzu',
    'undo-fixer':      'Implementiert _undo_stack / _redo_stack mit MAX_UNDO',
    'safety-fixer':    'Fügt _rotate_backup, Audit-Log, Dirty-Flag hinzu',
    'ui-fixer':        'Implementiert Dark Mode, Drag&Drop, Settings Dialog',
    'refactor-agent':  'Teilt lange Funktionen auf, extrahiert Helper-Klassen',
    'sim-analyst':     'Analysiert Simulationsdateien (SOR/OLTS/FLW)',
    'unified-debugger':'Debuggt Ladefehler in sor/olts/fluke Tools',
    'code-auditor':    'Dieser Audit-Lauf — erneut ausführen nach Fixes',
}

def find_available_agents(agents_dir='.claude/agents') -> dict:
    """
    Sucht .md Dateien in agents_dir.
    Gibt {agent_name: description} zurück.
    """
    available = {}
    agents_path = Path(agents_dir)
    if agents_path.exists():
        for md in agents_path.glob('*.md'):
            name = md.stem
            src  = md.read_text(encoding='utf-8', errors='replace')
            desc_m = re.search(r'description: >\s*\n((?:  .+\n)+)', src)
            if desc_m:
                desc = ' '.join(desc_m.group(1).split())
            else:
                desc = '(keine Beschreibung)'
            available[name] = desc
    return available

def match_agent(fix_agent_id: str, available: dict) -> str | None:
    """
    Sucht verfügbaren Agenten der zum fix_agent_id passt.
    Exakt-Match zuerst, dann Partial-Match.
    """
    if fix_agent_id in available:
        return fix_agent_id
    for name in available:
        if fix_agent_id in name or name in fix_agent_id:
            return name
    return None
```

---

## Phase 2 — Routing-Plan erstellen

```python
def build_routing_plan(report: dict, available: dict) -> dict:
    """
    Erstellt einen priorisierten Plan:
    {
      'immediate':  [(agent, [findings])],   ← critical+high
      'scheduled':  [(agent, [findings])],   ← medium
      'backlog':    [(agent, [findings])],   ← low
      'missing':    [(fix_agent_id, [findings])],  ← kein Agent gefunden
    }
    """
    order = {'critical': 0, 'high': 1, 'medium': 2, 'low': 3}
    all_findings = report['all_findings']
    all_findings.sort(key=lambda f: order[f['severity']])

    by_agent = {}
    for f in all_findings:
        by_agent.setdefault(f['fix_agent'], []).append(f)

    plan = {'immediate': [], 'scheduled': [], 'backlog': [], 'missing': []}

    for agent_id, findings in by_agent.items():
        matched = match_agent(agent_id, available)
        max_sev = min(order[f['severity']] for f in findings)

        if matched is None:
            plan['missing'].append((agent_id, findings))
        elif max_sev <= 1:   # critical or high
            plan['immediate'].append((matched, findings))
        elif max_sev == 2:   # medium
            plan['scheduled'].append((matched, findings))
        else:                # low
            plan['backlog'].append((matched, findings))

    return plan
```

---

## Phase 3 — Ausgabe + Empfehlung

```python
def print_routing_plan(plan: dict, report: dict):
    summary = report['summary']

    print(f"\n{'='*65}")
    print(f"  AUDIT ROUTING PLAN")
    print(f"{'='*65}")
    print(f"  Dateien: {summary['files_audited']}  "
          f"Ø Score: {summary['avg_score']}/100  "
          f"Findings: {summary['total_findings']}  "
          f"Aufwand: ~{summary['total_effort_h']}h")

    # Per-Datei Score
    print(f"\n  Scores:")
    for fa in report['files']:
        si = fa['score_info']
        bar = '█' * (si['score']//10) + '░' * (10-si['score']//10)
        print(f"    {fa['name']:<25} {bar} {si['score']}/100 ({si['grade']})  "
              f"{si['critical']}C/{si['high']}H/{si['medium']}M/{si['low']}L")

    for tier, label, icon in [
        ('immediate', 'SOFORT — Critical + High', '🔴'),
        ('scheduled', 'GEPLANT — Medium',          '🟡'),
        ('backlog',   'BACKLOG — Low',              '🟢'),
    ]:
        items = plan[tier]
        if not items: continue
        print(f"\n  [{icon} {label}]")
        for agent, findings in items:
            total_h = sum(f['effort_h'] for f in findings)
            print(f"    → {agent}  ({len(findings)} Findings, ~{total_h:.1f}h)")
            for f in findings[:4]:
                sev = f['severity'].upper()[:4]
                print(f"       [{sev}] {f['file']}: {f['title']}")
            if len(findings) > 4:
                print(f"       ... und {len(findings)-4} weitere")

    if plan['missing']:
        print(f"\n  [⚪ KEIN AGENT VERFÜGBAR]")
        for agent_id, findings in plan['missing']:
            print(f"    → Braucht: {agent_id}  ({len(findings)} Findings)")
            for f in findings:
                print(f"       [{f['severity'].upper()[:4]}] {f['title']}")
        print(f"\n  Optionen für fehlende Agenten:")
        print(f"    A) Web-Suche nach passendem Agenten")
        print(f"    B) Neuen Agenten auf Basis der Findings erstellen lassen")
        print(f"    C) code-auditor Findings manuell abarbeiten")

def print_agent_call_sequence(plan: dict):
    """Gibt die exakte Aufruf-Reihenfolge aus."""
    print(f"\n  Empfohlene Aufruf-Reihenfolge:")
    seq = 1
    for tier in ['immediate', 'scheduled', 'backlog']:
        for agent, findings in plan[tier]:
            print(f"    {seq}. {agent}  "
                  f"(findings.json → filter: {agent})")
            seq += 1
    print(f"    {seq}. code-auditor  (erneut — Verbesserung messen)")
```

---

## Phase 4 — Fehlende Agenten suchen/bauen

```python
def handle_missing_agent(agent_id: str, findings: list):
    """
    Wird aufgerufen wenn kein Agent für fix_agent_id gefunden.
    Sucht im Web oder erstellt Agent-Beschreibung.
    """
    print(f"\nKein Agent für '{agent_id}' gefunden.")
    print(f"Betroffene Findings:")
    for f in findings:
        print(f"  [{f['severity']}] {f['title']} in {f['file']}")

    # Web-Suche nach ähnlichen Agenten/Tools
    # web_search(f"Claude Code agent {agent_id} Python quality fix")

    # Oder: Minimal-Agent-Template generieren
    template = f'''---
name: {agent_id}
description: >
  Behebt folgende Findings aus findings.json: 
  {", ".join(f["id"] for f in findings[:3])}.
  Generiert von audit-router weil kein passender Agent gefunden.
model: claude-sonnet-4-20250514
tools:
  - type: bash
  - type: text_editor
---

# {agent_id.replace("-", " ").title()}

## Aufgabe

Lese findings.json und behebe folgende Finding-Typen:
{chr(10).join(f"- {f['id']}: {f['title']}" for f in findings)}

## Einstieg

```python
from pathlib import Path
import json

findings = json.loads(Path("findings.json").read_text())
my_findings = [
    f for fa in findings["files"]
    for f in fa["findings"]
    if f["fix_agent"] == "{agent_id}"
]
for f in my_findings:
    print(f"[{{f['severity']}}] {{f['file']}}: {{f['title']}}")
    print(f"  Detail: {{f['detail']}}")
    # TODO: Fix implementieren
```
'''
    agent_path = Path(f'.claude/agents/{agent_id}.md')
    agent_path.write_text(template)
    print(f"  → Template erstellt: {agent_path}")
    print(f"  → Öffne den Agenten und implementiere die Fix-Logik")
```

---

## Hauptprogramm

```python
def main(findings_path='findings.json', agents_dir='.claude/agents'):
    # 1. Laden
    report    = load_and_validate(findings_path)
    available = find_available_agents(agents_dir)

    # 2. Plan
    plan = build_routing_plan(report, available)

    # 3. Ausgabe
    print_routing_plan(plan, report)
    print_agent_call_sequence(plan)

    # 4. Fehlende Agenten behandeln
    for agent_id, findings in plan['missing']:
        handle_missing_agent(agent_id, findings)

    return plan

if __name__ == '__main__':
    import sys
    path = sys.argv[1] if len(sys.argv) > 1 else 'findings.json'
    main(findings_path=path)
```

---

## Workflow für den Nutzer

```
1. code-auditor    datei.py          → erzeugt findings.json
2. audit-router    findings.json     → zeigt Plan, startet Agenten
3. quality-fixer   findings.json     → behebt Exceptions, Logging
4. threading-fixer findings.json     → wraps GUI-Blocker
5. test-fixer      findings.json     → fügt Tests hinzu
6. code-auditor    datei.py          → misst Verbesserung
```

Der Router koordiniert — er startet selbst nichts, sondern zeigt
welcher Agent als nächstes aufgerufen werden soll und warum.
