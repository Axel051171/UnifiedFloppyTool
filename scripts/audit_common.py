#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
audit_common.py — gemeinsames Gerüst für die Prüfer.

Herausgezogen aus substring_audit.py, damit code_audit.py dieselbe
Grundlinien-, SARIF- und Anmerkungsmechanik benutzt statt eine zweite zu
bauen. Zwei Prüfer mit zwei Grundlinienformaten wären genau die
Doppelhaltung, gegen die Regel K4 antritt.

Wer eine dritte Prüfregelgruppe baut, importiert von hier.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from dataclasses import dataclass, asdict, field

__all__ = ['Finding', 'walk', 'load_baseline', 'write_baseline',
           'apply_baseline', 'emit_text', 'emit_github', 'emit_sarif',
           'add_common_args', 'run_and_report', 'C_EXT']

C_EXT = ('.c', '.h', '.cpp', '.hpp', '.cc', '.cxx')

#: Verzeichnisse, die beim Durchlauf übersprungen werden.
SKIP_DIRS = {'.git', 'build', 'obj', 'bin', '__pycache__', 'node_modules',
             '.venv', 'venv'}


@dataclass
class Finding:
    rule: str
    path: str
    line: int
    col: int
    severity: str          # 'sicher' | 'pruefen'
    what: str
    why: str
    snippet: str = ''
    #: Weitere Fundstellen desselben Fundes — nur K4 benutzt das.
    also: list = field(default_factory=list)

    def fingerprint(self) -> str:
        """Stabile Kennung OHNE Zeilennummer.

        Eine Grundlinie, die an Zeilennummern hängt, läuft bei der ersten
        eingefügten Leerzeile aus dem Ruder und wird binnen einer Woche
        abgeschaltet.

        Zwei Preise, und sie stehen hier, damit sie nicht überraschen:
        zwei identische Stellen in einer Datei haben denselben
        Fingerabdruck (dafür --strict-baseline), und eine umbenannte
        Datei markiert alles neu.
        """
        norm = re.sub(r'\s+', ' ', self.snippet).strip()
        # A-028, Defekt J: der PFADTRENNER wird normiert, und das ist kein
        # Feinschliff — ohne ihn ist die Grundlinie in CI wirkungslos.
        #
        # Gemessen: dieselbe Fundstelle ergibt
        #   `src\formats\sega\uft_genesis.c` -> 8444c507781de807
        #   `src/formats/sega/uft_genesis.c` -> b8752fa4aeb35c9b
        # Eine auf Windows geschriebene Grundlinie (`os.path.relpath`
        # liefert dort Backslashes) enthaelt nur die erste Fassung. In CI
        # laeuft der Pruefer auf Linux mit Schraegstrichen und haette
        # **alle 51** bekannten Fundstellen als NEU gemeldet — das Tor
        # haette ab dem ersten Tag jeden Pull Request blockiert, also
        # genau der Fehlschlag, vor dem der Workflow-Kopf warnt („Ein
        # Pruefer, der am ersten Tag jede Zusammenfuehrung blockiert, ist
        # in einer Woche abgeschaltet").
        #
        # Und `check()` konnte es nicht sehen: es schreibt und liest mit
        # demselben `os.path.relpath`, also stimmten beide Seiten immer
        # ueberein — ein geschlossener Kreis. Gefunden hat es erst der
        # Handlauf mit dem Pfad, den ein Mensch und CI tippen.
        pfad = self.path.replace('\\', '/')
        return hashlib.sha256(
            f'{self.rule}\0{pfad}\0{norm}'.encode()).hexdigest()[:16]


def _ausgabe_haerten() -> None:
    """Ein Absturz ist kein Urteil (MF-1171).

    BERICHTIGT bei der Uebernahme (A-028). Gemessen an der Vorfassung
    dieses Geruests: der erste Lauf ueber `src include` starb mit
    `UnicodeEncodeError: 'charmap' codec can't encode character
    '\\u2044'` beim Drucken eines Schnipsels aus
    `src/formats/reference/uft_floppy_reference.c:168` — die
    Vorgabekodierung der Windows-Konsole ist cp1252. Er hatte vier von
    14 Dateien mit Funden gedruckt und den Rest des Baums nie gesehen;
    der Rueckgabewert war **1** und sah damit genau wie „hat etwas
    gefunden" aus.

    Woertlich die Klasse aus MF-1171: dort starb
    `enum_macro_conflicts.py` an `int('0170000', 0)`, und
    `check_consistency.py` endete mit rc 1, BEVOR die uebrigen 23
    Kategorien liefen.

    Nur `sys.stdout`/`sys.stderr` werden gehaertet. `--out` oeffnet die
    Datei mit ausdruecklichem `encoding='utf-8'`, und dort ist
    `ensure_ascii=False` deshalb richtig — die DATENausgabe bleibt
    unberuehrt.
    """
    for strom in (sys.stdout, sys.stderr):
        try:
            strom.reconfigure(errors='replace')     # Python >= 3.7
        except (AttributeError, OSError):
            pass


def walk(roots, exts, wurzel=None):
    """Die Dateimenge kommt aus git, nicht aus `SKIP_DIRS`.

    BERICHTIGT bei der Uebernahme (A-028). `SKIP_DIRS` oben ist eine
    hartkodierte Ausschlussliste, und `CLAUDE.md` §MF-636 verbietet
    genau das: „Wer in einem Skript entscheidet, WELCHE Dateien
    geprueft werden, fragt `git ls-files` — nie eine hartkodierte
    Verzeichnisliste." Der Preis ist gemessen, nicht theoretisch: von
    **10** Python-Fundstellen der Einschaetzung „sicher" lagen **5**
    unter `tools/uft-scout/work/` — geklonten FREMD-Repos (fluxfox,
    hardsector_tool, greaseweazle). CI sieht diese Dateien nie; ein
    Befund darin ist richtig gesehen und vollkommen belanglos (MF-633).
    `scripts/repo_scope.py` beschreibt in seiner eigenen Zusage
    DENSELBEN Fall, nach dem nibtools-Klon.

    **Und der Umzug in dieses gemeinsame Modul machte den Defekt
    breiter statt schmaler:** `walk()` lag vorher in
    `substring_audit.py`; hier betrifft er JEDEN Pruefer, der von hier
    importiert.

    `SKIP_DIRS` bleibt als zweite, billige Schranke stehen — sie spart
    das Ablaufen grosser Bauverzeichnisse, entscheidet aber nichts
    mehr.

    @param wurzel  Der Baum, gegen den gefiltert wird. **Ohne diese
                   Angabe wird der Ort DIESER Datei genommen**, und das
                   ist fuer einen gepflanzten Pruefbaum falsch: dort
                   sind die Dateien im echten Baum nicht verzeichnet,
                   und `walk()` filterte sie restlos weg. Gemessen
                   meldete das Tor auf drei gepflanzten Defekten
                   „blind — gepflanzter Defekt nicht gemeldet"
                   (`scripts/audit_selbsttest.py`). Ein Tor, das im
                   gepflanzten Baum nichts sieht, sieht auch sonst
                   nichts, sobald es auf einer anderen Wurzel laeuft —
                   die Klasse MF-1000/Tor 64.
    """
    from pathlib import Path
    hier = os.path.dirname(os.path.abspath(__file__))
    basis = Path(wurzel) if wurzel is not None else Path(hier).parent
    try:
        sys.path.insert(0, hier)
        from repo_scope import make_filter          # noqa: PLC0415
        im_baum, warnung = make_filter(basis)
    except Exception as exc:                        # noqa: BLE001
        im_baum, warnung = (lambda p: True), (
            'repo_scope nicht ladbar (%s) — es wird der GANZE '
            'Verzeichnisbaum geprueft' % exc)
    if warnung:
        print(warnung, file=sys.stderr)

    for root in roots:
        if os.path.isfile(root):
            if root.endswith(exts) and im_baum(Path(root)):
                yield root
            continue
        for dirpath, dirnames, files in os.walk(root):
            # Die Verzeichnisse werden SORTIERT, nicht nur gefiltert.
            #
            # MF-1231, gemessen in CI: hier stand nur der Filter, und
            # `os.walk` gibt Unterverzeichnisse in der Reihenfolge des
            # DATEISYSTEMS. Auf Windows und auf Linux ist das eine
            # andere. Fuer jede Regel, die eine Datei fuer sich
            # beurteilt, ist das belanglos — fuer K4 in
            # `audit_codefallen.py` nicht: sie ist die einzige Regel
            # ueber MEHRERE Dateien und verankert ihren Fund an der
            # ERSTEN Fundstelle. Deren Pfad steckt im Fingerabdruck.
            #
            # Die Folge war gemessen: lokal „20 in der Grundlinie, 0
            # NEU", in CI **12 Funde** bei identischen Zahlen — nur mit
            # einem anderen ersten Treffer (`368640` stand hier bei
            # `uft_bayesian_detect.c:151`, dort bei
            # `uft_file_ops_extended.c:655`). Dieselbe Klasse wie
            # Defekt J aus MF-1229: lokal unsichtbar, weil beide
            # Seiten dieselbe Reihenfolge benutzen.
            dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
            for f in sorted(files):
                if not f.endswith(exts):
                    continue
                p = os.path.join(dirpath, f)
                if im_baum(Path(p)):
                    yield p


# ── Grundlinie ───────────────────────────────────────────────────────────

def load_baseline(path):
    if not path or not os.path.exists(path):
        return None
    with open(path, encoding='utf-8') as fh:
        return json.load(fh)


def write_baseline(path, findings, note=''):
    counts = {}
    for f in findings:
        counts[f.fingerprint()] = counts.get(f.fingerprint(), 0) + 1
    data = {
        'version': 1,
        'note': note or ('Altbestand, der nicht blockiert. NEUE Funde '
                         'blockieren. Eintraege hier sind Schulden, keine '
                         'Ausnahmen — wer einen behebt, entfernt ihn.'),
        'count': len(findings),
        'accepted': counts,
    }
    with open(path, 'w', encoding='utf-8') as fh:
        json.dump(data, fh, indent=2, sort_keys=True, ensure_ascii=False)
        fh.write('\n')
    return data


def apply_baseline(findings, baseline, strict=False):
    if not baseline:
        return findings, []
    accepted = dict(baseline.get('accepted', {}))
    new, known = [], []
    for f in findings:
        fp = f.fingerprint()
        left = accepted.get(fp, 0)
        if left > 0:
            known.append(f)
            if strict:
                accepted[fp] = left - 1
        else:
            new.append(f)
    return new, known


# ── Ausgabe ──────────────────────────────────────────────────────────────

def emit_text(findings, files, rule_help=None):
    by_rule = {}
    for f in findings:
        by_rule.setdefault(f.rule, []).append(f)

    for rule in sorted(by_rule):
        group = by_rule[rule]
        sure = sum(1 for f in group if f.severity == 'sicher')
        title = f' — {rule_help[rule]}' if rule_help and rule in rule_help \
                else ''
        print(f'\n=== {rule}: {len(group)} Funde '
              f'({sure} sicher, {len(group) - sure} zu pruefen){title} ===')
        for f in group:
            print(f'\n{f.path}:{f.line}:{f.col}  [{f.severity}]  '
                  f'{f.fingerprint()}')
            print(f'  {f.what}')
            print(f'  {f.why}')
            if f.snippet:
                print(f'  > {f.snippet}')
            for a in f.also:
                print(f'  auch: {a}')

    print(f'\n{files} Dateien geprueft, {len(findings)} Funde.')


def emit_github(findings):
    """Arbeitsablaufbefehle, damit die Funde am Diff kleben.

    Komma, Doppel-Doppelpunkt und Zeilenumbruch müssen kodiert werden,
    sonst bricht der Befehl an der ersten Stelle ab."""
    for f in findings:
        level = 'error' if f.severity == 'sicher' else 'warning'
        msg = (f'{f.what}\n{f.why}').replace('\r', '') \
                                    .replace('\n', '%0A') \
                                    .replace(',', '%2C') \
                                    .replace('::', '%3A%3A')
        print(f'::{level} file={f.path},line={f.line},col={f.col},'
              f'title=Code-Falle {f.rule}::{msg}')


def emit_sarif(findings, roots, rule_help, tool_name,
               error_rules=frozenset()):
    rules = [{
        'id': rid,
        'name': f'{tool_name}{rid}',
        'shortDescription': {'text': txt},
        'fullDescription': {'text': txt},
        'help': {'text': 'Siehe die Berichte UFT-NN im Projekt.'},
        'defaultConfiguration': {
            'level': 'error' if rid in error_rules else 'warning'},
    } for rid, txt in sorted(rule_help.items())]

    results = []
    for f in findings:
        region = {'startLine': max(1, f.line), 'startColumn': max(1, f.col)}
        if f.snippet:
            region['snippet'] = {'text': f.snippet}
        results.append({
            'ruleId': f.rule,
            'level': 'error' if f.severity == 'sicher' else 'warning',
            'message': {'text': f'{f.what} — {f.why}'},
            'partialFingerprints': {f'{tool_name}/v1': f.fingerprint()},
            'locations': [{'physicalLocation': {
                'artifactLocation': {'uri': f.path.replace(os.sep, '/')},
                'region': region,
            }}],
        })

    return {
        'version': '2.1.0',
        '$schema': 'https://json.schemastore.org/sarif-2.1.0.json',
        'runs': [{
            'tool': {'driver': {
                'name': tool_name,
                'semanticVersion': '1.0.0',
                'rules': rules,
            }},
            'invocations': [{
                'executionSuccessful': True,
                'commandLine': ' '.join([tool_name] + list(roots)),
            }],
            'results': results,
        }],
    }


# ── Kommandozeile ────────────────────────────────────────────────────────

def add_common_args(ap):
    # MF-1236: die Ausgabehaertung gehoert HIERHER, nicht nur in
    # `run_and_report()`.
    #
    # Der Kommentar dort sagte „an EINER Stelle, weil beide Pruefer hier
    # durchkommen" — und das gilt fuer den BERICHTSPFAD. Der
    # `--selbsttest`-Pfad laeuft daran vorbei: beide Pruefer verzweigen
    # gleich nach `parse_args()` (`audit_teilstring.py:715`,
    # `audit_codefallen.py:925`) und erreichen `run_and_report()` nie.
    #
    # Gemessen: `audit_teilstring.py` hat das mit einem eigenen Aufruf in
    # seinem `selbsttest()` (`:659`) geflickt — `audit_codefallen.py`
    # enthaelt den Namen `haerten` **0 Mal**. Folge unter cp1252, der
    # Windows-Vorgabe: `audit_codefallen.py --selbsttest` meldet 41/42,
    # der fallende Fall heisst „Ausgabe haelt ein Zeichen ausserhalb von
    # cp1252 aus", und er druckt genau das ⁄ aus dem
    # MF-1171-Absturz. Mit `PYTHONIOENCODING=utf-8` sind es 42/42.
    #
    # **Und CI kann das nicht sehen.** `.github/workflows/teilstring.yml`
    # faehrt diesen Selbsttest (`:131`, zwei Jobs haengen daran), aber auf
    # `ubuntu-latest` und ohne `PYTHONIOENCODING`/`LANG` — dort ist die
    # Kodierung UTF-8, der Fall kann nie feuern. Die einzige Umgebung mit
    # der Bedingung ist der Entwicklerrechner, und den bewacht nichts:
    # die Umkehrung der ueblichen Falle, CI gruen und lokal rot.
    #
    # `add_common_args()` ist der Engpass, den BEIDE Eingaenge passieren,
    # vor jeder Verzweigung. Ein Aufruf beim IMPORT waere noch staerker —
    # er koennte von keinem Eingang umgangen werden —, ist aber eine
    # Nebenwirkung beim Einbinden und wuerde einen kuenftigen Importeur
    # ueberraschen, der rohe Ausgabe will. Deshalb hier und ausdruecklich.
    # Der Aufruf in `run_and_report()` bleibt stehen: er deckt einen
    # Aufrufer, der `run_and_report()` ohne diese Argumentschicht nutzt,
    # und `reconfigure()` ist mehrfach anwendbar.
    _ausgabe_haerten()

    ap.add_argument('roots', nargs='+', help='Dateien oder Verzeichnisse')
    ap.add_argument('--format', default='text',
                    choices=('text', 'json', 'sarif', 'github'))
    ap.add_argument('--json', action='store_true',
                    help='Kurzform fuer --format json')
    ap.add_argument('--out', metavar='DATEI')
    ap.add_argument('--only', metavar='REGEL')
    ap.add_argument('--baseline', metavar='DATEI')
    ap.add_argument('--write-baseline', metavar='DATEI')
    ap.add_argument('--strict-baseline', action='store_true')
    ap.add_argument('--fail-on', default='sicher',
                    choices=('nichts', 'sicher', 'alles'))
    return ap


def run_and_report(args, findings, files, rule_help, tool_name,
                   error_rules=frozenset(), empty_note=''):
    """Grundlinie anwenden, ausgeben, Rückgabewert bestimmen.

    Der gemeinsame Schwanz beider Prüfer."""
    # A-028: an EINER Stelle, weil beide Prüfer hier durchkommen. Vorher
    # starb der Lauf mitten im Baum an einem Zeichen, das die Konsole
    # nicht darstellen kann — siehe `_ausgabe_haerten()`.
    #
    # BERICHTIGT MF-1236. „Beide Prüfer kommen hier durch" gilt für den
    # BERICHTSPFAD und nicht für den `--selbsttest`-Pfad: beide
    # verzweigen gleich nach `parse_args()` und erreichen diese Funktion
    # nie. Gemessen kostete das `audit_codefallen.py --selbsttest` unter
    # cp1252 eine rote Zusage (41/42) — in der Datei kam `haerten`
    # **0 Mal** vor. Seit MF-1236 steht der Aufruf zusätzlich in
    # `add_common_args()`, dem Engpass VOR jeder Verzweigung; dieser hier
    # bleibt für einen Aufrufer, der `run_and_report()` ohne die
    # Argumentschicht nutzt. Der Satz stand seit A-028 und war nie ganz
    # falsch — nur nicht so allgemein, wie er klang.
    _ausgabe_haerten()
    if args.json:
        args.format = 'json'

    if args.only:
        findings = [f for f in findings if f.rule == args.only]

    if args.write_baseline:
        data = write_baseline(args.write_baseline, findings)
        print(f'{args.write_baseline}: {data["count"]} Funde als '
              f'Grundlinie festgehalten '
              f'({len(data["accepted"])} Fingerabdruecke).')
        print('Das sind SCHULDEN, keine Ausnahmen. Wer einen Fund behebt, '
              'entfernt\nseinen Eintrag — sonst waechst die Datei nur.')
        return 0

    baseline = load_baseline(args.baseline)
    new, known = apply_baseline(findings, baseline, args.strict_baseline)
    report = new if baseline else findings

    out = open(args.out, 'w', encoding='utf-8') if args.out else sys.stdout
    try:
        if args.format == 'json':
            json.dump([asdict(f) | {'fingerprint': f.fingerprint()}
                       for f in report], out, indent=2, ensure_ascii=False)
            out.write('\n')
        elif args.format == 'sarif':
            json.dump(emit_sarif(report, args.roots, rule_help, tool_name,
                                 error_rules),
                      out, indent=2, ensure_ascii=False)
            out.write('\n')
        else:
            old, sys.stdout = sys.stdout, out
            try:
                if args.format == 'github':
                    emit_github(report)
                else:
                    emit_text(report, files, rule_help)
                    if not report and empty_note:
                        print(empty_note)
            finally:
                sys.stdout = old
    finally:
        if args.out:
            out.close()

    if baseline and args.format in ('text', 'github'):
        print(f'\nGrundlinie: {len(known)} bekannte Funde uebergangen, '
              f'{len(new)} neu.', file=sys.stderr)

    if args.fail_on == 'nichts':
        return 0
    if args.fail_on == 'alles':
        return 1 if report else 0
    return 1 if any(f.severity == 'sicher' for f in report) else 0
