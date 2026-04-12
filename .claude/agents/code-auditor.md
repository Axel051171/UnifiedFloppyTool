---
name: code-auditor
description: >
  Universeller C++/Qt6 Audit für UnifiedFloppyTool mit drei Analyse-Ebenen:
  (1) Statische Code-Qualität, (2) Cross-Platform-Kompatibilität Win/Mac/Linux,
  (3) GitHub Release CI-Readiness — prüft ob ein Release auf allen drei
  Plattformen ohne Fehler auf Grün geht. Produziert findings.json für alle
  nachgelagerten Agenten. Use when: vor einem Release, nach größeren Änderungen,
  bei CI-Fehlern, oder wenn cross-platform Kompatibilität geprüft werden soll.
  Läuft ZUERST — seine Findings steuern alle anderen Agenten.
model: claude-sonnet-4-5-20251022
tools:
  - type: bash
  - type: text_editor
  - type: agent
---

# Code Auditor — UnifiedFloppyTool (C++/Qt6)

## Drei Analyse-Ebenen

```
Ebene 1: Statische Code-Qualität    → Immer ausführen
Ebene 2: Cross-Platform-Checks      → Vor jedem Commit
Ebene 3: CI Release-Readiness       → Vor jedem Release-Tag
```

---

## Ebene 1 — Statische Code-Qualität

```python
import re, json, subprocess, datetime
from pathlib import Path

def find_sources(root='.'):
    result = subprocess.run(
        'find . -name "*.cpp" -o -name "*.h" -o -name "*.c" '
        '| grep -v build | grep -v ".moc" | grep -v formats_v2',
        shell=True, cwd=root, capture_output=True, text=True)
    return [Path(p) for p in result.stdout.strip().split('\n') if p]

def audit_static(sources):
    all_src = '\n'.join(p.read_text(errors='replace') for p in sources)
    m = {}
    m['files']            = len(sources)
    m['lines_total']      = sum(len(p.read_text(errors='replace').splitlines()) for p in sources)
    m['todo_count']       = len(re.findall(r'//.*\b(TODO|FIXME|HACK|XXX)\b', all_src, re.I))
    m['bare_catch']       = len(re.findall(r'catch\s*\(\s*\.\.\.\s*\)', all_src))
    m['raw_new']          = len(re.findall(r'\bnew\s+\w', all_src))
    m['raw_malloc']       = len(re.findall(r'\bmalloc\s*\(', all_src))
    m['smart_ptr']        = len(re.findall(r'(?:unique|shared)_ptr\s*<', all_src))
    raw = m['raw_new'] + m['raw_malloc']
    m['raw_ptr_pct']      = int(raw / max(1, raw + m['smart_ptr']) * 100)
    m['unsafe_int_cast']  = len(re.findall(r'\(int\)\s*(?:size|length|count|offset|sz|len)\b', all_src, re.I))
    m['silent_normalize'] = len(re.findall(r'(?:toupper|tolower|normalize|sanitize|fix|repair)\s*\(', all_src, re.I))
    m['old_connect']      = len(re.findall(r'SIGNAL\s*\(|SLOT\s*\(', all_src))
    m['qwidget_in_core']  = sum(
        1 for p in sources
        if not any(x in str(p).lower() for x in ['gui','widget','dialog','view','window'])
        and re.search(r'#include.*<QWidget>|#include.*<QPainter>', p.read_text(errors='replace')))
    m['raii_guard']       = len(re.findall(r'QMutexLocker|std::lock_guard|std::unique_lock', all_src))
    m['mutex_usage']      = len(re.findall(r'\bQMutex\b|\bstd::mutex\b', all_src))
    m['test_files']       = sum(1 for p in sources if 'test' in str(p).lower())
    m['fn_over_200']      = 0  # populated by _long_functions
    m['fn_over_500']      = 0
    m['long_functions']   = []
    return m
```

---

## Ebene 2 — Cross-Platform-Kompatibilität

### Windows-Blocker (POSIX ohne Guards)

```python
POSIX_HEADERS = ['unistd.h','sys/time.h','sys/resource.h','sys/ioctl.h','termios.h']
MSVC_INCOMPATIBLE = [r'\btypeof\b', r'__attribute__\s*\(\(', r'(?<!\w)\w+\s+\w+\[\w+\]']

def audit_windows_compat(sources):
    issues = []
    for p in sources:
        src = p.read_text(errors='replace')
        for h in POSIX_HEADERS:
            if f'#include <{h}>' in src:
                ctx = src[max(0, src.find(h)-120):src.find(h)+60]
                if '_WIN32' not in ctx:
                    issues.append({
                        'file': p.name, 'severity': 'critical', 'type': 'posix_header',
                        'msg': f"#include <{h}> ohne _WIN32-Guard → MSVC-Build bricht",
                        'fix': f'#ifndef _WIN32\n#include <{h}>\n#else\n#include "compat/win_{h.replace("/","_")}"\n#endif',
                        'platform': 'windows'
                    })
        for pat in MSVC_INCOMPATIBLE:
            m = re.search(pat, src)
            if m:
                ctx = src[max(0, m.start()-120):m.start()+60]
                if '_WIN32' not in ctx and '_MSC_VER' not in ctx:
                    issues.append({
                        'file': p.name, 'severity': 'critical', 'type': 'msvc_syntax',
                        'msg': f"'{m.group(0)[:30]}' → MSVC kennt das nicht → Compat-Makro nötig",
                        'fix': '#ifdef _MSC_VER\n// MSVC Alternative\n#else\n// GCC/Clang Version\n#endif',
                        'platform': 'windows'
                    })
    return issues

### macOS ARM64-Blocker (SIMD ohne Architektur-Guard)

SIMD_PATTERNS = [r'#include\s*<[sx]mmintrin\.h>', r'_mm_\w+\(', r'__m128', r'__m256']

def audit_macos_compat(sources):
    issues = []
    for p in sources:
        src = p.read_text(errors='replace')
        for pat in SIMD_PATTERNS:
            m = re.search(pat, src)
            if m:
                ctx = src[max(0, m.start()-150):m.start()+80]
                if '__x86_64__' not in ctx and '__SSE' not in ctx:
                    issues.append({
                        'file': p.name, 'severity': 'critical', 'type': 'x86_simd_no_guard',
                        'msg': f"x86 SIMD ohne Architektur-Guard → Crash auf Apple Silicon (M1/M2/M3/M4)",
                        'fix': '#ifdef __x86_64__\n    // SSE/AVX Code hier\n#elif defined(__aarch64__)\n    // NEON Alternative\n#endif',
                        'platform': 'macos_arm64'
                    })
                    break

    # .pro-Datei: Universal Binary gesetzt?
    for pro in Path('.').glob('**/*.pro'):
        src = pro.read_text(errors='replace')
        if 'macx' in src and 'QMAKE_APPLE_DEVICE_ARCHS' not in src:
            issues.append({
                'file': pro.name, 'severity': 'high', 'type': 'no_universal_binary',
                'msg': "QMAKE_APPLE_DEVICE_ARCHS fehlt → kein Universal Binary (x86_64 + arm64)",
                'fix': 'macx {\n    QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64\n}',
                'platform': 'macos'
            })
    return issues

### .pro Datei (alle Plattformen)

def audit_pro_file():
    issues = []
    pro_files = list(Path('.').glob('**/*.pro'))
    if not pro_files:
        return [{'severity':'critical','type':'no_pro_file',
                 'msg':'Keine .pro-Datei gefunden!','platform':'all',
                 'fix':''}]
    for pro in pro_files:
        src = pro.read_text(errors='replace')
        if 'object_parallel_to_source' not in src:
            issues.append({
                'file': pro.name, 'severity': 'critical', 'type': 'missing_parallel_source',
                'msg': "CONFIG += object_parallel_to_source fehlt → 35+ Basename-Kollisionen → stille Link-Fehler",
                'fix': 'CONFIG += object_parallel_to_source',
                'platform': 'all'
            })
        if 'c++17' not in src and 'c++20' not in src:
            issues.append({
                'file': pro.name, 'severity': 'high', 'type': 'no_cxx_standard',
                'msg': "C++-Standard nicht explizit gesetzt",
                'fix': 'CONFIG += c++17',
                'platform': 'all'
            })
        if 'win32' not in src or 'windeployqt' not in src:
            issues.append({
                'file': pro.name, 'severity': 'medium', 'type': 'missing_windeployqt',
                'msg': "windeployqt nicht konfiguriert → DLLs fehlen im Windows-Release",
                'fix': 'win32 {\n    QMAKE_POST_LINK += windeployqt $$OUT_PWD/$$TARGET.exe\n}',
                'platform': 'windows'
            })
    return issues
```

---

## Ebene 3 — CI Release-Readiness

```python
def audit_ci_workflows():
    issues = []
    wf_dir = Path('.github/workflows')

    if not wf_dir.exists():
        return [{'severity':'critical','type':'no_workflows',
                 'msg':'.github/workflows/ fehlt — kein CI!','platform':'all'}]

    workflows = list(wf_dir.glob('*.yml')) + list(wf_dir.glob('*.yaml'))
    has_ci = has_san = has_release = False

    for w in workflows:
        src = w.read_text()

        if any(x in w.stem for x in ['ci','build']):
            has_ci = True
            for plat, keys in [('linux',['ubuntu','linux']),
                                ('windows',['windows','win']),
                                ('macos',['macos','mac'])]:
                if not any(k in src for k in keys):
                    issues.append({
                        'severity': 'critical', 'type': f'missing_{plat}_matrix',
                        'msg': f"Kein {plat.capitalize()}-Job in {w.name} → Release NICHT auf Grün möglich",
                        'platform': plat
                    })
            if 'fail-fast: false' not in src:
                issues.append({'severity':'medium','type':'fail_fast',
                               'msg':f"{w.name}: fail-fast fehlt → ein Fehler bricht alle Plattformen ab",
                               'fix':'strategy:\n  fail-fast: false'})
            if 'cache' not in src and 'install-qt-action' not in src:
                issues.append({'severity':'high','type':'no_qt_cache',
                               'msg':f"{w.name}: Qt6 nicht gecacht → +30-60min je Build-Run",
                               'fix':"jurplel/install-qt-action@v3 mit cache: true"})
            if 'ccache' not in src and 'sccache' not in src:
                issues.append({'severity':'medium','type':'no_compiler_cache',
                               'msg':f"{w.name}: Kein ccache/sccache"})
            # Mindestens Qt 6.5 LTS?
            qt_vers = re.findall(r"'(6\.\d+\.\d+)'", src)
            if qt_vers:
                if all(float(v.rsplit('.',1)[0]) < 6.5 for v in qt_vers):
                    issues.append({'severity':'high','type':'old_qt_version',
                                   'msg':f"Qt {qt_vers} — empfohlen: >= 6.5 (LTS)"})

        if 'sanitizer' in w.stem:
            has_san = True
            if 'address' not in src.lower():
                issues.append({'severity':'high','type':'no_asan',
                               'msg':f"{w.name}: ASan fehlt"})
            if 'undefined' not in src.lower():
                issues.append({'severity':'high','type':'no_ubsan',
                               'msg':f"{w.name}: UBSan fehlt"})

        if 'release' in w.stem:
            has_release = True
            for art in ['appimage','dmg','.exe','.zip','.msi']:
                if art in src.lower(): break
            else:
                issues.append({'severity':'high','type':'no_artifacts',
                               'msg':f"{w.name}: Keine Release-Artefakte definiert"})
            if 'on:\n  push:\n    tags:' not in src and "push:\n    tags:" not in src:
                issues.append({'severity':'high','type':'no_tag_trigger',
                               'msg':f"{w.name}: Kein Tag-Trigger — Release wird nicht ausgelöst",
                               'fix':"on:\n  push:\n    tags:\n      - 'v*.*.*'"})

    if not has_ci:
        issues.append({'severity':'critical','type':'no_ci','msg':'Kein ci.yml!'})
    if not has_san:
        issues.append({'severity':'high','type':'no_sanitizers','msg':'Kein sanitizers.yml!'})
    if not has_release:
        issues.append({'severity':'high','type':'no_release_wf','msg':'Kein release.yml!'})

    return issues


def audit_release_readiness():
    issues = []
    # Versions-Konsistenz
    versions = {}
    for pro in Path('.').glob('**/*.pro'):
        m = re.search(r'^VERSION\s*=\s*([\d.]+)', pro.read_text(), re.M)
        if m: versions[str(pro)] = m.group(1)
    for cmake in Path('.').glob('**/CMakeLists.txt'):
        m = re.search(r'project\s*\([^)]*VERSION\s+([\d.]+)', cmake.read_text(), re.I)
        if m: versions[str(cmake)] = m.group(1)
    for f in ['README.md','CHANGELOG.md']:
        p = Path(f)
        if p.exists():
            m = re.search(r'v?([\d]+\.[\d]+\.[\d]+)', p.read_text())
            if m: versions[f] = m.group(1)
    if len(set(versions.values())) > 1:
        issues.append({'severity':'critical','type':'version_mismatch',
                       'msg':f"Versions-Mismatch: {versions}",
                       'fix':"Alle Stellen auf dieselbe VERSION bringen"})
    if not Path('CHANGELOG.md').exists():
        issues.append({'severity':'medium','type':'no_changelog','msg':"CHANGELOG.md fehlt"})
    # formats_v2 nicht im Build
    for pro in Path('.').glob('**/*.pro'):
        if 'formats_v2' in pro.read_text():
            issues.append({'severity':'high','type':'dead_code_in_build',
                           'msg':f"formats_v2/ ist in {pro.name} — toter Code im Build!"})
    return issues
```

---

## Vollständige GitHub Actions Workflow-Vorlagen

Die Agenten `github-expert` und `build-ci-release` können diese Vorlagen direkt in `.github/workflows/` schreiben wenn die Workflows fehlen.

### ci.yml — Matrix-Build (Linux + Windows + macOS)

```yaml
name: CI
on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  build:
    name: ${{ matrix.config.name }}
    runs-on: ${{ matrix.config.os }}
    strategy:
      fail-fast: false
      matrix:
        config:
          - {name: "Linux / GCC 14",       os: ubuntu-24.04, qt_arch: gcc_64}
          - {name: "Windows / MSVC 2022",   os: windows-2022, qt_arch: win64_msvc2022_64}
          - {name: "Windows / MinGW",       os: windows-2022, qt_arch: win64_mingw}
          - {name: "macOS / Clang Univ.",   os: macos-14,     qt_arch: clang_64}
    steps:
      - uses: actions/checkout@v4
      - name: Install Qt
        uses: jurplel/install-qt-action@v3
        with:
          version: '6.7.0'
          arch: ${{ matrix.config.qt_arch }}
          cache: true
      - name: Setup ccache
        uses: hendrikmuhs/ccache-action@v1.2
        with:
          key: ${{ matrix.config.os }}
          max-size: 2G
      - name: Linux deps
        if: runner.os == 'Linux'
        run: sudo apt-get install -y libusb-1.0-0-dev libudev-dev
      - name: macOS deps
        if: runner.os == 'macOS'
        run: brew install libusb
      - name: Build
        run: |
          mkdir build && cd build
          qmake ../UnifiedFloppyTool.pro CONFIG+=release CONFIG+=object_parallel_to_source
          cmake --build . --parallel
      - name: Test
        run: cd build && ctest --output-on-failure -j4
```

### sanitizers.yml — ASan + UBSan

```yaml
name: Sanitizers
on:
  push: {branches: [main]}
  schedule: [{cron: '0 2 * * 1'}]

jobs:
  sanitize:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - uses: jurplel/install-qt-action@v3
        with: {version: '6.7.0', cache: true}
      - name: Build + Test (ASan+UBSan)
        env:
          CXXFLAGS: -fsanitize=address,undefined -fno-omit-frame-pointer -O1
          LDFLAGS:  -fsanitize=address,undefined
          ASAN_OPTIONS:  detect_leaks=1:halt_on_error=1:print_stacktrace=1
          UBSAN_OPTIONS: halt_on_error=1:print_stacktrace=1
        run: |
          sudo apt-get install -y libusb-1.0-0-dev
          mkdir build-san && cd build-san
          qmake ../UnifiedFloppyTool.pro CONFIG+=debug CONFIG+=object_parallel_to_source
          make -j$(nproc)
          ctest --output-on-failure -j1
```

### release.yml — Automatischer Release auf Tag

```yaml
name: Release
on:
  push:
    tags: ['v*.*.*']

jobs:
  build-linux:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - uses: jurplel/install-qt-action@v3
        with: {version: '6.7.0', cache: true}
      - run: |
          sudo apt-get install -y libusb-1.0-0-dev fuse
          mkdir build && cd build
          qmake ../UnifiedFloppyTool.pro CONFIG+=release CONFIG+=object_parallel_to_source
          make -j$(nproc)
          # AppImage
          wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
          chmod +x linuxdeploy-x86_64.AppImage
          VERSION=${{ github.ref_name }} ./linuxdeploy-x86_64.AppImage \
            --appdir AppDir --plugin qt --output appimage --executable build/UnifiedFloppyTool
      - uses: actions/upload-artifact@v4
        with: {name: linux, path: "*.AppImage"}

  build-windows:
    runs-on: windows-2022
    steps:
      - uses: actions/checkout@v4
      - uses: jurplel/install-qt-action@v3
        with: {version: '6.7.0', arch: win64_msvc2022_64, cache: true}
      - uses: ilammy/msvc-dev-cmd@v1
      - run: |
          mkdir build && cd build
          qmake ..\UnifiedFloppyTool.pro CONFIG+=release CONFIG+=object_parallel_to_source
          nmake
          windeployqt UnifiedFloppyTool.exe --release
          7z a ..\uft-${{ github.ref_name }}-windows.zip UnifiedFloppyTool.exe *.dll
      - uses: actions/upload-artifact@v4
        with: {name: windows, path: "uft-*-windows.zip"}

  build-macos:
    runs-on: macos-14
    steps:
      - uses: actions/checkout@v4
      - uses: jurplel/install-qt-action@v3
        with: {version: '6.7.0', cache: true}
      - run: |
          brew install libusb
          mkdir build && cd build
          qmake ../UnifiedFloppyTool.pro CONFIG+=release CONFIG+=object_parallel_to_source \
            QMAKE_APPLE_DEVICE_ARCHS="x86_64 arm64"
          make -j$(nproc)
          macdeployqt UnifiedFloppyTool.app -dmg
          mv *.dmg ../uft-${{ github.ref_name }}-macos.dmg
      - uses: actions/upload-artifact@v4
        with: {name: macos, path: "uft-*-macos.dmg"}

  publish:
    needs: [build-linux, build-windows, build-macos]
    runs-on: ubuntu-latest
    steps:
      - uses: actions/download-artifact@v4
        with: {path: artifacts, merge-multiple: true}
      - uses: softprops/action-gh-release@v2
        with:
          files: artifacts/**
          generate_release_notes: true
```

---

## findings.json Hauptprogramm

```python
def run_full_audit(root='.', output='.'):
    print("=== UFT Code Auditor ===\n")
    sources = find_sources(root)
    print(f"Quellen: {len(sources)} Dateien\n")

    static_m       = audit_static(sources)
    win_issues     = audit_windows_compat(sources)
    mac_issues     = audit_macos_compat(sources)
    pro_issues     = audit_pro_file()
    ci_issues      = audit_ci_workflows()
    release_issues = audit_release_readiness()

    platform_issues = win_issues + mac_issues + pro_issues
    all_findings    = generate_all_findings(static_m, platform_issues, ci_issues, release_issues)
    score_info      = compute_score(all_findings)

    routing = {}
    for f in all_findings:
        routing.setdefault(f.get('fix_agent','manual'),[]).append(f['id'])

    ci_critical   = any(f['category']=='ci'       and f['severity']=='critical' for f in all_findings)
    plat_critical = any(f['category']=='platform' and f['severity']=='critical' for f in all_findings)

    report = {
        'audit_timestamp': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'project': 'UnifiedFloppyTool', 'language': 'C++/Qt6',
        'files': [{'file': root, 'name': 'UnifiedFloppyTool',
                   'metrics': static_m, 'findings': all_findings, 'score_info': score_info}],
        'all_findings': all_findings,
        'routing': routing,
        'summary': {
            'files_audited':  static_m['files'],
            'lines_total':    static_m['lines_total'],
            'avg_score':      score_info['score'],
            'total_findings': len(all_findings),
            'by_agent':       {a: len(ids) for a, ids in routing.items()},
            'total_effort_h': round(sum(f.get('effort_h',0) for f in all_findings), 1),
            'ci_ready':       not ci_critical,
            'platforms_ok':   not plat_critical,
            'release_blocked': ci_critical or plat_critical,
        }
    }

    Path(output, 'findings.json').write_text(json.dumps(report, indent=2, ensure_ascii=False))

    si = score_info
    print(f"Score: {si['score']}/100 ({si['grade']})  "
          f"{si['critical']}C / {si['high']}H / {si['medium']}M / {si['low']}L")
    print(f"CI-Ready:       {'JA' if report['summary']['ci_ready'] else 'NEIN'}")
    print(f"Plattform-OK:   {'JA' if report['summary']['platforms_ok'] else 'NEIN'}")
    print(f"Release blocked: {'JA — kritische Fehler!' if report['summary']['release_blocked'] else 'NEIN'}")
    print(f"\nRouting:")
    for agent, ids in sorted(routing.items(), key=lambda x: -len(x[1])):
        print(f"  {agent:<25}: {len(ids)} Findings")
    print(f"\nfindings.json geschrieben | Aufwand: ~{report['summary']['total_effort_h']}h")
    return report

if __name__ == '__main__':
    import sys
    run_full_audit(sys.argv[1] if len(sys.argv) > 1 else '.')
```

---

## Checkliste vor Release-Tag

- [ ] `run_full_audit()` → keine critical Findings
- [ ] `ci_ready: true`
- [ ] `platforms_ok: true`
- [ ] `release_blocked: false`
- [ ] Linux / Windows(MSVC) / Windows(MinGW) / macOS alle grün
- [ ] Sanitizer-Workflow clean
- [ ] Versions-Konsistenz: .pro = CMakeLists.txt = README = CHANGELOG
- [ ] formats_v2/ aus Build ausgeschlossen
