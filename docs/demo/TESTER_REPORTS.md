# UFT v4.1.5 — Tester Reports

**Zweck:** Belege für Definition-of-Done Punkt 5 in
`V415_DEMO_READY_GOAL.md` — "3 unabhängige Tester (oder du selbst auf
3 verschiedenen Systemen) können das v4.1.5 Release herunterladen,
installieren, das Demo-Szenario durchspielen."

**Phase:** D (Buffer & Bugfix-Window, 2026-06-09 → 2026-06-15)
**Gate für Goal-Completion:** mindestens 3 Einträge mit Status `PASS`.

---

## Wie man einen Tester-Report einreicht

1. v4.1.5 herunterladen:
   - Linux: `.deb` oder `.tar.gz` von https://github.com/Axel051171/UnifiedFloppyTool/releases/tag/v4.1.5
   - macOS: `.dmg`
   - Windows: `.zip` portable
2. `docs/demo/QUICK_DEMO.md` durchspielen.
3. Diesen Report-Eintrag ergänzen (PR oder Issue mit Label `demo-tester`).
4. Zeit messen (Setup ausgenommen). Ziel: ≤12 Minuten.

---

## Report-Eintrag-Format

```markdown
### Tester: <Name oder Pseudonym>
- **System:** <OS-Version, Hardware-Modell, Qt-Version falls bekannt>
- **Datum:** YYYY-MM-DD
- **Demo-Zeit:** <MM:SS> (Setup nicht mitgezählt)
- **Status:** PASS / PASS-WITH-WORKAROUND / FAIL
- **Probleme:** <Liste oder "keine">
- **Notizen:** <freier Text>
```

---

## Reports

(Dieser Abschnitt wird ab 2026-06-09 befüllt — Phase D Start.)

### Tester: (Beispiel — wird beim ersten echten Report ersetzt)
- **System:** Ubuntu 24.04 LTS, ThinkPad E15 Gen 4, Qt 6.7.3
- **Datum:** 2026-06-15
- **Demo-Zeit:** 11:30
- **Status:** PASS
- **Probleme:** keine
- **Notizen:** Build dauerte 4:20 auf 4-Core i5; Sidecar-JSON wurde korrekt
  emittiert, Wizard-Warnung war eindeutig.

---

## Bekannte Probleme-Klassen (vorab dokumentiert für schnelle Triage)

Diese Klassen sind in `V415_DEMO_READY_GOAL.md` Phase D als erwartbar genannt:

| Klasse | Mitigation | Workaround |
|---|---|---|
| Build-Issues seltener Distros (Fedora 41, Arch, Debian Trixie) | hotfix `fix(v4.1.5-post):` auf main | bis hotfix: README mit Distro-spezifischen `apt`/`dnf`/`pacman` Befehlen ergänzen |
| macOS-Pfad-Konflikte (Homebrew Qt6 Location) | hotfix mit `CMAKE_PREFIX_PATH` Hint | Tester setzt `export CMAKE_PREFIX_PATH=$(brew --prefix qt6)` |
| Windows long-path issues (MAX_PATH) | `git config --global core.longpaths true` als Doku | Repo flach klonen oder Powershell Long-Path-Reg-Key |
| AppImage / .deb Packaging quirks | hotfix release.yml | Tester nutzt `.tar.gz` als Fallback |

Wenn ein Problem in einer dieser Klassen liegt: PASS-WITH-WORKAROUND mit
expliziter Workaround-Notiz im Report.

---

## Zählweise für die "3 Tester" Anforderung

Die Goal-Spec sagt: "3 unabhängige Tester (oder du selbst auf 3
verschiedenen Systemen)". Das heißt:

- **3 unabhängige Personen** auf beliebigen Systemen, ODER
- **1 Person (z.B. Axel) auf 3 verschiedenen Systemen** (z.B. Ubuntu-Laptop +
  macOS-Desktop + Windows-VM)
- **Mischformen erlaubt:** Axel auf 2 Systemen + 1 externer Tester = OK

PASS-WITH-WORKAROUND zählt als PASS, wenn der Workaround einfach genug ist
(<2 min, dokumentiert). FAIL zählt nicht — und löst hotfix-PR aus.

---

## Goal-Verbindung

Wenn dieser Abschnitt mindestens 3 Einträge mit Status PASS oder
PASS-WITH-WORKAROUND zeigt, ist
**`V415_DEMO_READY_GOAL.md` Erfolgs-Kriterium #5 erfüllt.**

Die anderen 6 Kriterien:
1. ✓/⬜ `git describe` zeigt `v4.1.5` auf main (Phase B.3)
2. ✓/⬜ GitHub Release v4.1.5 mit 3 Plattform-Binaries (Phase B.5)
3. ✓ `docs/SHOWCASE.md` + `docs/CAPABILITIES.md` aktuell (Phase A.1/A.2 — done)
4. ✓/⬜ Frischer Ubuntu Laptop <12 Min Demo (Phase C.1 docs done, real run Phase C/D)
5. ⬜ Mindestens 3 erfolgreiche Test-Durchläufe (DIESE Datei)
6. ✓/⬜ Keine offenen CI-Failures auf main (Phase B.2 result)
7. ✓ Bekannte Limits klar dokumentiert (Phase A.3 KNOWN_ISSUES Demo-Impact + CAPABILITIES.md)
