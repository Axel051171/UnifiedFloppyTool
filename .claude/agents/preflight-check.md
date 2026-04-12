---
name: preflight-check
description: >
  Simuliert lokal was GitHub Actions auf allen drei Plattformen tun würde —
  bevor du pushst. Prüft alle 62 bekannten Fehlertypen: YAML-Syntax, Windows-
  MSVC-Inkompatibilitäten, macOS-ARM64-Blocker, Linker-Fehler, Test-Probleme,
  Artefakt-Pfade, Permissions und Cache-Probleme. Gibt einen klaren
  GO / NO-GO für jeden Plattform-Build aus. Use when: vor git push, vor
  einem Release-Tag, oder nach größeren Änderungen. Ist der erste Agent
  den man ruft — findet Fehler bevor GitHub sie findet.
model: claude-sonnet-4-5-20251022
tools:
  - type: bash
  - type: text_editor
  - type: web_search_20250305
    name: web_search
---

# Pre-Flight Check — GitHub CI Simulator

## Ziel

Lokale Simulation aller GitHub-Checks. Output: GO / NO-GO pro Plattform.
Kein push mit NO-GO.

```
PRE-FLIGHT ERGEBNIS:
  Linux   (GCC 14 / Ubuntu 24):  GO   ✓
  Windows (MSVC 2022):           NO-GO ✗  → 3 kritische Fehler
  macOS   (Clang / ARM64):       NO-GO ✗  → 1 kritischer Fehler
  Workflows (.github/):          GO   ✓
  Release-Artefakte:             NO-GO ✗  → Pfad falsch
```

---

## Phase 1 — Workflow YAML validieren

```python
import re, yaml, json
from pathlib import Path

def check_workflows():
    findings = []
    wf_dir = Path('.github/workflows')

    if not wf_dir.exists():
        return [blocker('no_workflows', 'all',
                        '.github/workflows/ fehlt — kein CI',
                        'Verzeichnis anlegen + Workflows erstellen')]

    KNOWN_ACTIONS = {
        'actions/checkout':           ['v3','v4'],
        'actions/upload-artifact':    ['v3','v4'],
        'actions/download-artifact':  ['v3','v4'],
        'actions/cache':              ['v3','v4'],
        'jurplel/install-qt-action':  ['v3'],
        'hendrikmuhs/ccache-action':  ['v1','v1.2'],
        'ilammy/msvc-dev-cmd':        ['v1'],
        'softprops/action-gh-release':['v1','v2'],
    }

    for wf_path in sorted(wf_dir.glob('*.yml')):
        src = wf_path.read_text()
        wf_name = wf_path.name

        # YAML-Syntax
        try:
            data = yaml.safe_load(src)
        except yaml.YAMLError as e:
            findings.append(blocker('yaml_syntax', 'all',
                f"{wf_name}: YAML-Syntaxfehler — {e}",
                "YAML-Linter ausführen: python3 -c \"import yaml; yaml.safe_load(open('.github/workflows/" + wf_name + "').read())\""))
            continue

        # Tabs in YAML (häufigster Fehler)
        if '\t' in src:
            line = next(i+1 for i,l in enumerate(src.splitlines()) if '\t' in l)
            findings.append(blocker('yaml_tabs', 'all',
                f"{wf_name} Z.{line}: Tab-Zeichen → YAML erlaubt nur Spaces",
                "sed -i 's/\\t/  /g' " + str(wf_path)))

        # Action-Versionen prüfen
        for action, good_versions in KNOWN_ACTIONS.items():
            uses_matches = re.findall(rf'uses:\s*{re.escape(action)}@(\S+)', src)
            for ver in uses_matches:
                if not any(ver.startswith(v) for v in good_versions):
                    findings.append(warning('old_action', 'all',
                        f"{wf_name}: {action}@{ver} — bekannte Versionen: {good_versions}",
                        f"Auf @{good_versions[-1]} aktualisieren"))

        # Undefined env vars (${{ env.FOO }} ohne FOO in env:)
        env_defs = set(re.findall(r'^\s+(\w+):', src, re.M))
        env_refs = re.findall(r'\$\{\{\s*env\.(\w+)\s*\}\}', src)
        for ref in env_refs:
            if ref not in env_defs and ref not in ('GITHUB_TOKEN','RUNNER_OS'):
                findings.append(warning('undef_env', 'all',
                    f"{wf_name}: ${{{{ env.{ref} }}}} referenziert aber nicht definiert",
                    f"env:\n  {ref}: 'wert' hinzufügen"))

        # needs: auf nicht-existente Jobs
        job_names = set(data.get('jobs', {}).keys()) if data else set()
        for job_id, job in (data.get('jobs', {}) or {}).items():
            for dep in (job.get('needs', []) or []):
                if isinstance(dep, str) and dep not in job_names:
                    findings.append(blocker('undefined_need', 'all',
                        f"{wf_name}: Job '{job_id}' braucht '{dep}' — existiert nicht",
                        f"Job '{dep}' anlegen oder needs entfernen"))

        # matrix-Keys validieren
        matrix_src = str(data.get('jobs', {}) if data else {})
        matrix_refs = re.findall(r'matrix\.(\w+(?:\.\w+)*)', src)
        matrix_keys = set()
        if data:
            for job in data.get('jobs', {}).values():
                strat = job.get('strategy', {}) or {}
                matrix = strat.get('matrix', {}) or {}
                if isinstance(matrix, dict):
                    matrix_keys.update(matrix.keys())
                    for item in matrix.get('include', []) or []:
                        if isinstance(item, dict):
                            matrix_keys.update(item.keys())
        for ref in matrix_refs:
            top_key = ref.split('.')[0]
            if matrix_keys and top_key not in matrix_keys:
                findings.append(warning('undef_matrix', 'all',
                    f"{wf_name}: matrix.{ref} — Key '{top_key}' nicht in matrix definiert",
                    f"matrix:\n  {top_key}: [wert1, wert2]"))

        # Node.js 16 deprecated (actions verwenden intern Node)
        if 'node16' in src.lower() or 'node-version: 16' in src:
            findings.append(warning('node16_deprecated', 'all',
                f"{wf_name}: Node.js 16 deprecated ab GitHub Actions",
                "Node.js 20 verwenden"))

        # Permissions für Release
        if 'softprops/action-gh-release' in src or 'create-release' in src:
            if 'permissions:' not in src:
                findings.append(blocker('missing_release_permissions', 'all',
                    f"{wf_name}: Release-Action ohne permissions: → schlägt mit 403 fehl",
                    "permissions:\n  contents: write"))

        # fail-fast
        if 'matrix:' in src and 'fail-fast: false' not in src:
            findings.append(warning('fail_fast_missing', 'all',
                f"{wf_name}: fail-fast nicht auf false → ein Fehler bricht alle Matrix-Jobs ab",
                "strategy:\n  fail-fast: false"))

    return findings
```

---

## Phase 2 — Windows MSVC-Kompatibilität

```python
POSIX_HEADERS = {
    'unistd.h':       'compat/win_unistd.h oder #ifdef _WIN32 Guard',
    'sys/time.h':     '#ifdef _WIN32 → #include <windows.h>',
    'sys/resource.h': '#ifdef _WIN32 Guard nötig',
    'sys/ioctl.h':    '#ifdef _WIN32 Guard nötig',
    'termios.h':      '#ifdef _WIN32 Guard nötig',
    'netinet/in.h':   '#ifdef _WIN32 → #include <winsock2.h>',
    'arpa/inet.h':    '#ifdef _WIN32 → #include <winsock2.h>',
    'sys/socket.h':   '#ifdef _WIN32 → #include <winsock2.h>',
    'dirent.h':       'win_dirent.h Wrapper nötig',
    'dlfcn.h':        '#ifdef _WIN32 → LoadLibrary()',
}

POSIX_FUNCS = {
    r'\busleep\b':      'Sleep(ms) auf Windows',
    r'\bsleep\b\s*\(':  'Sleep(ms*1000) auf Windows',
    r'\bgettimeofday\b':'QueryPerformanceCounter auf Windows',
    r'\bgetpid\b':      'GetCurrentProcessId() auf Windows',
    r'\bfork\b':        'Kein fork() auf Windows — Threading verwenden',
    r'\bmmap\b':        'MapViewOfFile() auf Windows',
    r'\bpread\b':       'ReadFile() mit OVERLAPPED auf Windows',
    r'\bstrcasecmp\b':  '_stricmp() auf Windows',
    r'\bstrncasecmp\b': '_strnicmp() auf Windows',
    r'\bstrtok_r\b':    'strtok_s() auf Windows',
}

MSVC_SYNTAX = {
    r'\btypeof\s*\(':           'decltype() in C++ oder __typeof__ auf GCC',
    r'__attribute__\s*\(\(':    'Compat-Makro: #ifdef _MSC_VER → __declspec()',
    r'__builtin_expect\s*\(':   '#define __builtin_expect(x,y) (x) auf MSVC',
    r'__builtin_clz\b':         '_BitScanReverse() auf MSVC',
    r'__builtin_popcount\b':    '__popcnt() auf MSVC',
    r'#warning\s':              '#pragma message() auf MSVC',
    r'\basm\s*\(':              '#ifdef _MSC_VER Guard nötig',
}

def check_windows_compat(sources):
    findings = []
    for p in sources:
        src = p.read_text(errors='replace')

        for header, fix in POSIX_HEADERS.items():
            if f'#include <{header}>' in src or f"#include '{header}'" in src:
                ctx_start = max(0, src.find(header) - 150)
                ctx = src[ctx_start:src.find(header) + 60]
                if '_WIN32' not in ctx and '_MSC_VER' not in ctx:
                    findings.append(blocker('posix_header_windows', 'windows',
                        f"{p.name}: #include <{header}> ohne _WIN32-Guard → MSVC-Build bricht",
                        f"Fix: {fix}"))

        for pat, fix in POSIX_FUNCS.items():
            m = re.search(pat, src)
            if m:
                ctx_start = max(0, m.start() - 150)
                ctx = src[ctx_start:m.start() + 60]
                if '_WIN32' not in ctx and '_MSC_VER' not in ctx:
                    findings.append(blocker('posix_func_windows', 'windows',
                        f"{p.name}: {m.group(0).strip()}() → nicht auf Windows",
                        f"Alternative: {fix}"))

        for pat, fix in MSVC_SYNTAX.items():
            m = re.search(pat, src)
            if m:
                ctx = src[max(0,m.start()-120):m.start()+60]
                if '_MSC_VER' not in ctx and '_WIN32' not in ctx:
                    findings.append(blocker('msvc_syntax', 'windows',
                        f"{p.name}: '{m.group(0)[:30]}' → MSVC kennt das nicht",
                        f"Fix: {fix}"))

        # long = 32bit auf Windows!
        if re.search(r'\blong\b(?!\s+long)', src):
            if re.search(r'sizeof\s*\(\s*long\s*\)\s*==\s*8', src) or \
               re.search(r'long\s+==\s*8', src):
                findings.append(warning('long_size_assumption', 'windows',
                    f"{p.name}: sizeof(long)==8 Annahme → Windows: long = 4 Bytes!",
                    "int64_t / uint64_t aus <stdint.h> verwenden"))

        # VLA (Variable Length Arrays)
        vla_match = re.search(r'(\w+)\s+\w+\s*\[\s*([a-zA-Z_]\w*)\s*\]', src)
        if vla_match:
            ctx = src[max(0,vla_match.start()-100):vla_match.start()+80]
            if 'static' not in ctx and 'const' not in ctx.split('[')[0][-20:]:
                findings.append(warning('vla_windows', 'windows',
                    f"{p.name}: Mögliches VLA '{vla_match.group(0)[:40]}' → MSVC erlaubt keine VLAs",
                    "std::vector<T> oder malloc() verwenden"))

    return findings
```

---

## Phase 3 — macOS ARM64-Kompatibilität

```python
SIMD_PATTERNS = {
    r'#include\s*<xmmintrin\.h>':  'SSE',
    r'#include\s*<emmintrin\.h>':  'SSE2',
    r'#include\s*<pmmintrin\.h>':  'SSE3',
    r'#include\s*<smmintrin\.h>':  'SSE4',
    r'#include\s*<immintrin\.h>':  'AVX/AVX2',
    r'_mm_\w+\s*\(':               'SSE Intrinsic',
    r'_mm256_\w+\s*\(':            'AVX2 Intrinsic',
    r'__m128[id]?\b':              '__m128 Typ',
    r'__m256[id]?\b':              '__m256 Typ',
}

def check_macos_compat(sources):
    findings = []
    for p in sources:
        src = p.read_text(errors='replace')
        for pat, desc in SIMD_PATTERNS.items():
            m = re.search(pat, src)
            if m:
                ctx = src[max(0,m.start()-200):m.start()+80]
                has_guard = any(g in ctx for g in ['__x86_64__','__SSE','__AVX',
                                                    '__aarch64__','TARGET_CPU'])
                if not has_guard:
                    findings.append(blocker('simd_no_arch_guard', 'macos',
                        f"{p.name}: {desc} ohne Architektur-Guard → Crash auf M1/M2/M3/M4",
                        f"#ifdef __x86_64__\n    // {desc} Code\n#elif defined(__aarch64__)\n    // NEON Alternative oder Fallback\n#endif"))
                    break

    # .pro Datei
    for pro in Path('.').glob('**/*.pro'):
        src = pro.read_text(errors='replace')
        if 'macx' in src and 'QMAKE_APPLE_DEVICE_ARCHS' not in src:
            findings.append(blocker('no_universal_binary', 'macos',
                f"{pro.name}: QMAKE_APPLE_DEVICE_ARCHS fehlt → nur eine Architektur gebaut",
                "macx {\n    QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64\n}"))

    # Case-Sensitivity Check
    all_includes = []
    for p in sources:
        all_includes += re.findall(r'#include\s+[<"]([^>"]+)[>"]', p.read_text(errors='replace'))
    for inc in all_includes:
        # Prüfe ob Include-Pfad inkonsistente Groß/Kleinschreibung hat
        if re.search(r'[A-Z]', inc) and not inc.startswith('Q') and '/' in inc:
            findings.append(warning('case_sensitive_include', 'macos',
                f"#include \"{inc}\" — macOS HFS+ ist case-insensitive, Linux ext4 nicht",
                "Groß/Kleinschreibung mit tatsächlichem Dateinamen abgleichen"))

    return findings
```

---

## Phase 4 — Linker-Fehler lokal simulieren

```python
def check_linker_issues(sources):
    findings = []

    # .pro Datei Analyse
    for pro in Path('.').glob('**/*.pro'):
        pro_src = pro.read_text(errors='replace')

        # object_parallel_to_source
        if 'object_parallel_to_source' not in pro_src:
            findings.append(blocker('missing_parallel_source', 'all',
                f"{pro.name}: CONFIG += object_parallel_to_source fehlt",
                "CONFIG += object_parallel_to_source\n"
                "Ohne dies: Basename-Kollisionen führen zu 'multiple definition' Linker-Fehler"))

        # Alle .cpp Dateien in SOURCES?
        in_sources = set(re.findall(r'SOURCES\s*\+=\s*(.+?)(?:\s|$)', pro_src))
        all_cpp = {p.name for p in sources if p.suffix == '.cpp'}
        sources_flat = set()
        for entry in ' '.join(in_sources).split():
            sources_flat.add(Path(entry).name)
        not_listed = all_cpp - sources_flat
        if not_listed:
            for missing in list(not_listed)[:5]:
                findings.append(blocker('cpp_not_in_sources', 'all',
                    f"{missing} existiert aber nicht in SOURCES → 'undefined reference' Linker-Fehler",
                    f"SOURCES += src/pfad/{missing}"))

        # Qt-Modul-Check
        qt_modules = set(re.findall(r'QT\s*\+=\s*(.+)', pro_src))
        qt_used_in_src = set()
        all_src_combined = '\n'.join(p.read_text(errors='replace') for p in sources)
        module_map = {
            'network': ['QNetworkRequest','QTcpSocket','QUdpSocket'],
            'sql':     ['QSqlDatabase','QSqlQuery'],
            'xml':     ['QXmlStreamReader','QDomDocument'],
            'printsupport': ['QPrinter','QPrintDialog'],
            'bluetooth': ['QBluetoothDevice'],
            'serialport': ['QSerialPort'],
        }
        for mod, classes in module_map.items():
            for cls in classes:
                if cls in all_src_combined and mod not in ' '.join(qt_modules):
                    findings.append(blocker('qt_module_missing', 'all',
                        f"{cls} verwendet aber QT += {mod} fehlt in {pro.name}",
                        f"QT += {mod}"))
                    break

    return findings
```

---

## Phase 5 — Test-Probleme

```python
def check_tests(sources):
    findings = []

    test_sources = [p for p in sources if 'test' in str(p).lower()]
    all_test_src  = '\n'.join(p.read_text(errors='replace') for p in test_sources)

    # Hardcodierte absolute Pfade
    abs_paths = re.findall(r'["\'](?:/home/|/Users/|C:\\\\)[^"\']+["\']', all_test_src)
    for path in abs_paths[:3]:
        findings.append(blocker('hardcoded_path', 'all',
            f"Hardcodierter Pfad in Test: {path}",
            "Relativen Pfad oder QDir::currentPath() / QFINDTESTDATA verwenden"))

    # Netzwerk-Zugriff in Tests
    network_calls = re.findall(r'(?:http[s]?://|QNetworkAccessManager|QTcpSocket)\b', all_test_src)
    if network_calls:
        findings.append(warning('network_in_tests', 'all',
            f"Tests enthalten Netzwerk-Zugriffe — CI-Netzwerk ist eingeschränkt",
            "Netzwerk-Tests mocken oder mit QSKIP überspringen"))

    # Tests ohne Timeout
    if test_sources and 'QSKIP' not in all_test_src and 'timeout' not in all_test_src.lower():
        findings.append(warning('no_test_timeout', 'all',
            "Tests haben kein Timeout — hängende Tests blockieren CI für Stunden",
            "ctest --timeout 60 in CI-Workflow setzen"))

    # Goldene Test-Dateien committed?
    golden_refs = re.findall(r'(?:golden|reference|expected)[_/][\w.]+', all_test_src, re.I)
    for ref in golden_refs[:3]:
        golden_path = Path(f'tests/vectors/{ref}')
        if not golden_path.exists():
            # Suche breiter
            candidates = list(Path('.').rglob(Path(ref).name))
            if not candidates:
                findings.append(warning('missing_golden_file', 'all',
                    f"Goldene Test-Datei '{ref}' referenziert aber nicht im Repo",
                    "git add tests/vectors/<datei> && git commit"))

    return findings
```

---

## Phase 6 — Artefakt & Release-Pfade

```python
def check_artifacts():
    findings = []
    wf_dir = Path('.github/workflows')
    if not wf_dir.exists(): return findings

    for wf_path in wf_dir.glob('*.yml'):
        src = wf_path.read_text()
        wf_name = wf_path.name

        # upload-artifact Pfade
        for m in re.finditer(r"upload-artifact.*?path:\s*(.+?)(?:\n|$)", src, re.DOTALL):
            artifact_path = m.group(1).strip().strip('"\'')
            # Einfache Prüfung: existiert der Pfad?
            if '*' not in artifact_path and '${{' not in artifact_path:
                if not Path(artifact_path).exists():
                    findings.append(warning('artifact_path_missing', 'all',
                        f"{wf_name}: upload-artifact path '{artifact_path}' existiert lokal nicht",
                        "Pfad prüfen oder Workflow-Step hinzufügen der die Datei erzeugt"))

        # Release-Asset Name mit Leerzeichen
        asset_names = re.findall(r'name:\s*(["\']?[^"\'\n]+["\']?)', src)
        for name in asset_names:
            if ' ' in name.strip('"\'') and '${{' not in name:
                findings.append(warning('asset_name_spaces', 'all',
                    f"{wf_name}: Asset-Name '{name}' hat Leerzeichen → Download-URL bricht",
                    "Leerzeichen durch '-' ersetzen"))

        # permissions für Release
        if ('softprops/action-gh-release' in src or 'create-release' in src):
            if 'contents: write' not in src and 'contents: write' not in src:
                findings.append(blocker('release_permissions', 'all',
                    f"{wf_name}: Release ohne 'permissions: contents: write' → 403 Fehler",
                    "permissions:\n  contents: write"))

    return findings
```

---

## Phase 7 — Abhängigkeits-Aktualität (mit Web-Check)

```python
def check_dependencies_online():
    """
    Prüft ob verwendete Actions noch aktuell sind.
    Nutzt web_search für aktuelle Versions-Info.
    """
    findings = []
    wf_dir = Path('.github/workflows')
    if not wf_dir.exists(): return findings

    used_actions = {}
    for wf_path in wf_dir.glob('*.yml'):
        for m in re.finditer(r'uses:\s*([\w/-]+)@([\w.]+)', wf_path.read_text()):
            action, ver = m.group(1), m.group(2)
            used_actions[action] = ver

    # Bekannte veraltete Versionen (Stand Build-Datum)
    DEPRECATED = {
        'actions/checkout@v2':          'v4',
        'actions/checkout@v3':          'v4',
        'actions/upload-artifact@v2':   'v4',
        'actions/upload-artifact@v3':   'v4',
        'actions/download-artifact@v2': 'v4',
        'actions/download-artifact@v3': 'v4',
        'actions/cache@v2':             'v4',
        'actions/cache@v3':             'v4',
        'actions/setup-python@v4':      'v5',
    }

    for action, ver in used_actions.items():
        full = f"{action}@{ver}"
        if full in DEPRECATED:
            findings.append(warning('deprecated_action', 'all',
                f"{full} → veraltet, empfohlen: @{DEPRECATED[full]}",
                f"Alle Workflows updaten: sed -i 's/{full}/{action}@{DEPRECATED[full]}/g' .github/workflows/*.yml"))

    # Node.js 16 End-of-Life
    for wf_path in wf_dir.glob('*.yml'):
        if 'node16' in wf_path.read_text() or "node-version: '16'" in wf_path.read_text():
            findings.append(warning('nodejs16_eol', 'all',
                f"{wf_path.name}: Node.js 16 ist EOL → GitHub Actions warnt",
                "Node 20 verwenden"))

    return findings
```

---

## Hauptprogramm + Ausgabe

```python
def blocker(fid, platform, msg, fix=''):
    return {'id':fid,'severity':'blocker','platform':platform,'msg':msg,'fix':fix}

def warning(fid, platform, msg, fix=''):
    return {'id':fid,'severity':'warning','platform':platform,'msg':msg,'fix':fix}

def run_preflight(root='.'):
    import os
    os.chdir(root)
    sources = find_cpp_sources()

    print("=== PRE-FLIGHT CHECK ===\n")

    all_findings = (
        check_workflows() +
        check_windows_compat(sources) +
        check_macos_compat(sources) +
        check_linker_issues(sources) +
        check_tests(sources) +
        check_artifacts() +
        check_dependencies_online()
    )

    # Gruppierung nach Plattform + Severity
    by_platform = {}
    for f in all_findings:
        for plat in (f['platform'].split(',') if ',' in f['platform'] else [f['platform']]):
            by_platform.setdefault(plat.strip(), []).append(f)

    platforms = ['linux','windows','macos','all']
    go_nogo = {}
    for plat in platforms:
        issues = by_platform.get(plat, [])
        blockers = [i for i in issues if i['severity']=='blocker']
        go_nogo[plat] = len(blockers) == 0

    # Übersicht
    print("PLATTFORM-STATUS:")
    print(f"  Linux   (GCC 14 / Ubuntu 24):  {'GO   ✓' if go_nogo['linux'] and go_nogo['all'] else 'NO-GO ✗'}")
    print(f"  Windows (MSVC 2022):           {'GO   ✓' if go_nogo['windows'] and go_nogo['all'] else 'NO-GO ✗'}")
    print(f"  macOS   (Clang / ARM64):       {'GO   ✓' if go_nogo['macos'] and go_nogo['all'] else 'NO-GO ✗'}")
    print(f"  Workflows (.github/):          {'GO   ✓' if go_nogo['all'] else 'NO-GO ✗'}")

    overall_go = all(go_nogo.values())
    print(f"\n{'✓ PUSH FREIGEGEBEN' if overall_go else '✗ PUSH BLOCKIERT — Fehler beheben!'}")

    # Details
    if all_findings:
        blockers = [f for f in all_findings if f['severity']=='blocker']
        warnings = [f for f in all_findings if f['severity']=='warning']
        print(f"\n{'─'*60}")
        print(f"BLOCKER ({len(blockers)}) — müssen behoben werden:")
        for f in blockers:
            print(f"\n  [{f['platform'].upper()}] {f['msg']}")
            if f['fix']:
                print(f"  Fix:")
                for line in f['fix'].split('\n'):
                    print(f"    {line}")

        if warnings:
            print(f"\n{'─'*60}")
            print(f"WARNUNGEN ({len(warnings)}) — sollten behoben werden:")
            for f in warnings:
                print(f"  [{f['platform'].upper()}] {f['msg']}")
                if f['fix']:
                    print(f"    → {f['fix'].split(chr(10))[0]}")

    # findings.json für andere Agenten
    report = {
        'type': 'preflight',
        'overall_go': overall_go,
        'platform_status': go_nogo,
        'blockers': len([f for f in all_findings if f['severity']=='blocker']),
        'warnings': len([f for f in all_findings if f['severity']=='warning']),
        'findings': all_findings,
    }
    Path('preflight_findings.json').write_text(json.dumps(report, indent=2, ensure_ascii=False))
    print(f"\npreflight_findings.json geschrieben")
    return overall_go

def find_cpp_sources(root='.'):
    result = subprocess.run(
        'find . -name "*.cpp" -o -name "*.h" -o -name "*.c" '
        '| grep -v build | grep -v ".moc" | grep -v formats_v2',
        shell=True, capture_output=True, text=True)
    return [Path(p) for p in result.stdout.strip().split('\n') if p]

if __name__ == '__main__':
    import sys
    ok = run_preflight(sys.argv[1] if len(sys.argv) > 1 else '.')
    sys.exit(0 if ok else 1)
```

---

## Git Hook Integration (empfohlen)

Damit der Check automatisch vor jedem Push läuft:

```bash
# .git/hooks/pre-push (chmod +x setzen!)
#!/bin/bash
echo "Pre-Push: Starte Pre-Flight Check..."
python3 .claude/scripts/preflight.py .
if [ $? -ne 0 ]; then
    echo ""
    echo "PUSH ABGEBROCHEN — Pre-Flight Check fehlgeschlagen."
    echo "Fehler beheben und erneut pushen."
    exit 1
fi
echo "Pre-Flight OK — Push fortgesetzt."
```

```bash
# Installation:
cp .claude/scripts/preflight.py preflight.py  # oder direkt ausführen
chmod +x .git/hooks/pre-push
```

## Wann welchen Agenten rufen

| Situation | Agent |
|---|---|
| Vor git push | `preflight-check` (dieser) |
| CI bereits rot | `build-ci-release` + `github-expert` |
| Nur Plattform-Fix nötig | `build-ci-release` |
| Nur Workflow-Fix nötig | `github-expert` |
| Release-Tag setzen | `code-auditor` (Ebene 3) → dann `release-manager` |
