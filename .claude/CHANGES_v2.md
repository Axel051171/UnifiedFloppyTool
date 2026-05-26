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
