# .claude/ — Hygiene-Pass v2

**Datum:** 2026-05-26
**Scope:** Konsistenz-, Drift- und Cruft-Bereinigung. Keine semantischen
Änderungen an Agenten- oder Skill-Verhalten.

## Was sich geändert hat

### Entfernt (Müll / nicht committable)

- `worktrees/agent-a2146375/` — 7.194 Dateien / 77 MB Sub-Agent-Worktree-
  Leftover. Per `.gitignore` jetzt für die Zukunft blockiert.
- `settings.local.json`: 6 kaputt-geparste Permission-Patterns:
  - `Bash(1 echo:*)`
  - `Bash(/dev/null echo:*)` / `head:*` / `grep:*`
  - `Bash(head -5 wc -l src/formats/ti99/*.c 2)`
  - `Read(//c/Users/Axel/.../UnifiedFloppyTool-4.1.0 echo === USB Floppy in HAL === find src/**)`

### Reorganisiert

- `skills/htb-ai-mentor-main/claude-skill/htb-mentor/` → `skills/htb-mentor/`
  (3 Wrapper-Verzeichnisse entfernt; Upstream-README als
  `htb-mentor/UPSTREAM_README.md` erhalten).
- `goals/v4.1.4-final.md` → `goals/archive/v4.1.4-final.md` (Release durch).
- Neu: `goals/README.md` mit Konvention (was archivieren, wann).

### Synchronisiert (Doku ↔ Realität)

| Datei | Vorher | Nachher |
|---|---|---|
| `CLAUDE.md` Agenten-Übersicht | 13 in Tabelle, 21 auf Disk | 21 in Tabelle (13 Kern + 8 Refactor) |
| `CLAUDE.md` Z.115 | "4 von 13 Agenten" | "4 von 21 Agenten" |
| `skills/README.md` Z.3 | "19 Skills" | "20 UFT-Skills + 1 HTB-Skill" |
| `skills/README.md` Verifikation | `# erwartet: 19` | `# erwartet: 20` |
| `commands/team-onboarding.md` Z.123 | "SKILLS (13 …)", listet 14 | "SKILLS (21 = 20 UFT + 1 HTB …)", listet alle 21 |
| `CONSULT_PROTOCOL.md` Z.79 | "Zwei Agenten" + listet 4 | "Vier Agenten" + listet 4 |

### Normalisiert

- **Line-Endings:** Alle 50+ `.md`-Dateien in `.claude/` auf LF gebracht
  (vorher: 7 Agent-Files + alle `skills/**`-Files in CRLF).
- **Modell-Strings in `agents/*.md`:**
  - `proof-of-concept-builder.md`: `model: opus` → `model: claude-opus-4-7`
  - `dto-migrator.md`: `model: claude-haiku-4-5-20251001` → `model: claude-haiku-4-5`
  - Alle 21 Agenten nutzen jetzt das gleiche Alias-Schema:
    `claude-opus-4-7` (10×), `claude-sonnet-4-6` (10×), `claude-haiku-4-5` (1×).

### Reparaturen

- `CLAUDE.md`: ungeschlossene ```` ```markdown ``` ````-Fence (Z.176/Z.219
  in v1) entfernt. Der ganze "AI-Zusammenarbeit"-Abschnitt rendert jetzt
  korrekt als Headings + Listen statt als Code-Block.

### Neu

- `.gitignore` — `worktrees/`, `settings.local.json`, OS-Müll.
- `.gitattributes` — `*.md` etc. mit `text eol=lf`, verhindert CRLF-Drift.
- `goals/README.md` — Konvention für aktive vs. archivierte Goals.

## Verifikation

Ein paar Einzeiler die nach jeder Änderung grün bleiben sollten:

```bash
# 21 Agenten, alle LF
[ "$(ls .claude/agents/*.md | wc -l)" = "21" ] || echo FAIL: agent count
[ "$(file .claude/agents/*.md | grep -c CRLF)" = "0" ] || echo FAIL: CRLF in agents

# 20 UFT-Skills + htb-mentor
[ "$(ls -d .claude/skills/uft-*/ | wc -l)" = "20" ] || echo FAIL: uft skill count
[ -d .claude/skills/htb-mentor ] || echo FAIL: htb-mentor missing
[ ! -d .claude/skills/htb-ai-mentor-main ] || echo FAIL: htb wrapper still there

# Keine alten Modell-Aliase
! grep -q "^model: opus$" .claude/agents/*.md || echo FAIL: bare opus alias
! grep -q "^model: claude-haiku-4-5-20251001" .claude/agents/*.md || echo FAIL: pinned haiku

# Keine worktrees mehr
[ ! -d .claude/worktrees ] || echo FAIL: worktrees still present

# .gitignore + .gitattributes vorhanden
[ -f .claude/.gitignore ] || echo FAIL: no .gitignore
[ -f .claude/.gitattributes ] || echo FAIL: no .gitattributes

# CLAUDE.md hat keine offenen Code-Fences mehr
[ "$(grep -c '^```' .claude/CLAUDE.md)" = "$(awk 'BEGIN{c=0} /^```/{c++} END{print (c%2==0)?c:"odd"}' .claude/CLAUDE.md)" ] \
    || echo FAIL: unbalanced fences in CLAUDE.md
```

## Was nicht angefasst wurde

- **Skill-Inhalte** (`SKILL.md`-Body) — alle Skills behalten ihre
  semantische Funktion. Nur Frontmatter und Cross-References sind
  ggf. konsistenter.
- **Agent-Prompts** (`agents/*.md`-Body) — alle Agenten behalten ihre
  Tools, Mission und Working-Rules. Nur Modell-Strings normalisiert
  und CRLF entfernt.
- **`scripts/uft_p24_verify.sh`** — projekt-spezifisches Verify-Skript,
  unverändert.
- **`docs/SKILL_AUTHORING_GUIDE.md`** — unverändert (gut wie es ist).

---

# Hygiene-Pass v3 — Eigenständigkeit, Eigenverantwortung, Vollständigkeit

**Datum:** 2026-05-26 (gleiche Session, neue Iteration)

## Was sich geändert hat (v2 → v3)

### Neue Verfassungs-Sektion in `CLAUDE.md`

Direkt nach `## Projekt` eingefügt: **„Eigenständigkeit, Eigenverantwortung
& Vollständigkeit"** — drei nicht verhandelbare Prinzipien:

1. **Eigenständigkeit** — explizite Pflicht-Aktionen vor jeder Frage
   (Stack-Trace, git log, 2-3 Hypothesen, Test-File-Lesen, Lösungsversuch).
   Klare Listen für „wann fragen" und „wann NICHT fragen".
2. **Eigenverantwortung** — „fix completes the work". Sechs Checks bevor
   „fertig" gemeldet werden darf.
3. **Vollständigkeit** — Tabelle mit 7 verbotenen Anti-Patterns (Stubs,
   TODOs, Workarounds) + Mapping auf erlaubte Alternativen. Plus eine
   **Definition-of-Done-Tabelle** pro Code-Typ.

### Hard-Rule #6 geschärft

Vorher: „Escalation statt Alleingang. Bei Korrektheits-Bugs, UB oder
nötigem API-Break: STOP und flaggen."

Nachher: STOP-Gründe sind enumeriert (Forensik-Verlust, Hard-Rule-
Verletzung, geschützte Datei, ABI-Break, Mensch-Information). Explizit
KEINE STOP-Gründe: „es ist schwer", „Test failt komisch", „ich bin
unsicher" — „das ist Arbeit. Mach sie."

### Neue Anti-Goals in `CLAUDE.md`

Zusätzlich zu den bestehenden:

- Keine Stubs in neu geschriebenem Code (mit `honest-stub` als einzige Ausnahme)
- Keine `TODO`/`FIXME`/`XXX` ohne Issue-Link
- Keine „ich habe angefangen, der nächste macht weiter"-Übergaben
- Keine Workaround-Stubs (`if (!feature) return DEFAULT`)
- Keine Rückfragen zur Bestätigung wenn das Briefing klar war

### Neue Sektion in `CONSULT_PROTOCOL.md`

**„Wann NICHT konsultieren (Autonomie-Grenze)"** — Tabelle mit 6
verbotenen CONSULT-Patterns („Ist das OK?", „Was würdest du tun?",
„Kannst du mal schauen?", etc.) und konkreten Pflicht-Feldern, die ein
legitimes CONSULT enthalten muss (was selbst versucht, welche Hypothesen
verworfen, welche konkrete Information fehlt, warum genau dieser
Empfänger). Empfänger lehnt nicht-konforme CONSULTs ab.

### `stub-eliminator` Agent — Doppelrolle

Vorher: nur reaktiv (existierende Stubs eliminieren).

Nachher: zusätzlich **proaktiv** — Diff-Review vor Commit/Push, der
neue Lazy-Stubs blockiert. Neue Sektionen:

- **„Honest-Stub vs. Lazy-Stub"** — 6-spaltige Tabelle die das einzige
  erlaubte Stub-Pattern (Hardware-Provider-Wiring) vom verbotenen Pattern
  abgrenzt.
- **„Diff-Review-Modus"** — konkrete Regex-Patterns die einen Block
  auslösen, mit Reaktions-Tabelle und Output-Format.
- **„Selbstanwendung (Eigenständigkeit)"** — Agent ist nicht der
  Eskalations-Endpunkt für „ist das ein Stub?"; nur echte Scope-Trade-offs
  qualifizieren als CONSULT.

### `quick-fix` Agent — Stub-als-Fix verboten

Drei neue Nicht-Ziele:

- Kein Fix der den Bug durch Stub maskiert
- Kein `TODO` als „Fix"
- Kein `return UFT_OK;` als Quick-Fix

Plus neue Sektion „Eigenständigkeit": Pflicht-Vorlauf (Fehler-Meldung
vollständig lesen, `git log -p`, `git blame`, 2 Hypothesen, Fix tatsächlich
versuchen) bevor CONSULT erlaubt ist.

### `team-onboarding.md` — Autonomie-Knowledge-Check

5 neue Knowledge-Check-Fragen nach Phase 3, die ein neuer Session-Start
beantworten muss bevor die erste Code-Änderung passiert:

1. STOP-Gründe vollständig nennen (5)
2. Pseudo-STOP-Gründe erkennen (3)
3. Honest- vs Lazy-Stub-Unterschied (6 Spalten)
4. Definition-of-Done auflisten
5. Wann CONSULT erlaubt ist

## Was nicht geändert wurde

- **`honest-stub`-Konvention** für unverdrahtete Hardware-Provider
  (`src/hardwaretab.h:45-62`) bleibt explizit erhalten — das ist das
  Gegenmodell zum Lazy-Stub, nicht ein Stub-Pattern das wir abschaffen.
- **Bestehende 4-Aktionen-Logik** im stub-eliminator (IMPLEMENT / DELEGATE
  / DOCUMENT / DELETE) bleibt unverändert — funktioniert.
- **CONSULT-Block-Format** und Richtungsregel — unverändert; nur ergänzt
  um „wann gar nicht".
- **Agent-Bodies / Skill-Bodies** außerhalb von quick-fix und stub-eliminator
  — unverändert. Die neuen Prinzipien gelten via CLAUDE.md global.

## Verifikation

```bash
# Prinzipien-Sektion in CLAUDE.md vorhanden
grep -q "Eigenständigkeit, Eigenverantwortung & Vollständigkeit" .claude/CLAUDE.md
grep -q "Definition of Done" .claude/CLAUDE.md
grep -q "honest-stub" .claude/CLAUDE.md

# Hard-Rule #6 geschärft
grep -q "STOP-Gründe sind eng definiert" .claude/CLAUDE.md
grep -q "das ist Arbeit. Mach sie" .claude/CLAUDE.md

# CONSULT-Autonomie-Grenze vorhanden
grep -q "Wann NICHT konsultieren" .claude/CONSULT_PROTOCOL.md
grep -q "Arbeit zurückgeben, nicht Wissen holen" .claude/CONSULT_PROTOCOL.md

# stub-eliminator Doppelrolle
grep -q "Doppelrolle" .claude/agents/stub-eliminator.md
grep -q "Honest-Stub vs. Lazy-Stub" .claude/agents/stub-eliminator.md
grep -q "Diff-Review-Modus" .claude/agents/stub-eliminator.md

# quick-fix Anti-Stub
grep -q "den Bug durch einen Stub maskiert" .claude/agents/quick-fix.md

# Onboarding-Check
grep -q "Autonomie- & Vollständigkeits-Check" .claude/commands/team-onboarding.md
```

Alle Greps müssen Treffer liefern.
