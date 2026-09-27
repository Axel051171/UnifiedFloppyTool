---
name: uft-protection-db
description: |
  Use when adding, removing, or auditing entries in UFT's copy-protection
  title databases (`src/protection/c64/c64_protection_db.c`,
  `src/protection/uft_protection_classify.c`, `uft_protection_extended.c`).
  Trigger phrases: "protection db", "known title", "neuer kopierschutz
  eintrag", "c64 protection", "amiga protection", "duplicate title",
  "title lookup", "substring shadow bug". Captures the title-lookup
  contract (substring + bidirectional `strstr` → first-match-wins) and
  the de-dup invariant that was violated in v4.1.5 (9 stale duplicates
  shadowed canonical entries). DO NOT use for: protection-detection
  *algorithm* (→ `uft-protection-scheme`), generic database refactor
  (this skill is data-curation only).
---

# UFT Protection Database

Use this skill when curating the static protection-title databases. These
DBs map disk-name strings to known copy-protection schemes, version, year,
and notes. A wrong entry produces a wrong forensic verdict on a real
disk — and forensic verdicts are what the tool is *for*.

This skill exists because of a class of bugs that took **9 commits to
fully unmask** in v4.1.5 (see `git log --grep="duplicate-title"`): the
lookup function is permissive, so a stale duplicate near the top of the
table silently shadows the correct, canonical entry far below.

## When to use this skill

- Adding a new entry to a protection DB
- Removing or merging duplicate entries (9 known cases as of v4.1.5,
  ~60 latent — see follow-up below)
- Auditing a DB for consistency after a large data ingestion
- Adding a *new* platform DB (e.g. spectrum, amstrad protections)
- Re-checking lookup behaviour after a refactor

**Do NOT use this skill for:**
- Implementing a protection-detection *algorithm* (signature matching,
  flux-pattern analysis) — use `uft-protection-scheme`
- Replacing the lookup function — that's a public-ABI change, requires
  `abi-bomb-detector` first
- Filesystem-level "is this a copy-protected file?" — different layer

## The lookup contract (read before editing)

```c
/* src/protection/c64/c64_protection_db.c:1292 — canonical lookup */
bool c64_lookup_title(const char *title, c64_known_title_t *entry) {
    /* lowercase both sides */
    /* iterate g_known_titles in declaration order */
    if (strstr(lower_known, lower_title) ||      /* DB ⊇ query */
        strstr(lower_title,  lower_known)) {      /* query ⊇ DB */
        return *entry = g_known_titles[i], true;  /* FIRST match wins */
    }
}
```

Three properties matter — internalize them:

1. **Bidirectional substring.** `"Bruce Lee"` matches both
   `"Bruce Lee II"` (DB ⊇ query) **and** `"Bruce"` (query ⊇ DB).
2. **First-match-wins.** Iteration order = declaration order in
   `g_known_titles[]`.
3. **Lowercase, no Unicode normalization.** Diacritics, full-width
   characters, non-ASCII punctuation aren't normalized.

Consequence: if entry `"Bruce Lee"` (line 38) and entry
`"Bruce Lee (Datasoft, US)"` (line 700) both exist, the line-38 entry
**always** wins for any query containing "bruce lee", even when the
caller wanted the specific Datasoft variant. **That is the shadow bug.**

## The 5 de-dup invariants (hard)

| # | Invariant | Violation example |
|---|---|---|
| **D1** | **Exactly one canonical entry per title.** | "Bruce Lee" appears twice. |
| **D2** | **No substring-superset shadows the more-specific entry.** | "Gauntlet" at line 100, "Gauntlet II" at line 600 → query for "Gauntlet II" returns the wrong row. |
| **D3** | **No title-prefix that swallows unrelated titles.** | DB entry `"Pool"` would match `"Pool of Radiance"`, `"Pool Shark"`, `"Pool Champion"` — too greedy. Minimum 6 chars or distinctive token. |
| **D4** | **Canonical entry is the latest curated revision.** | Old entry at line 38 with `year=0`, new curated entry at line 700 with full metadata → keep the line-700 entry, delete the line-38 stub. |
| **D5** | **Sorted alphabetically within platform section.** | New entry appended at file-end produces incorrect first-match behaviour for ambiguous queries. |

`D1+D2` together are what got violated in v4.1.5: 9 stale stubs at the
top shadowed canonical entries below, each fix unmasked the next bug
(whack-a-mole). See `git show c0ea296`.

## Workflow

### Step 1: Search before adding

```bash
# Case-insensitive search for the title core (NOT the full string —
# substring matching means even a partial collision matters)
grep -in "bruce lee" src/protection/c64/c64_protection_db.c

# Also search reverse: shorter titles that would shadow yours
grep -in '"[A-Z][a-z]\{2,8\}"' src/protection/c64/c64_protection_db.c | \
    awk -F'"' '{print $2}' | sort -u
```

If you get a hit: **stop adding, decide which is canonical**, delete the
other. Never have two.

### Step 2: Choose canonical fields

Required: `title`, `protection_type`, `year`, `publisher`. Optional:
`notes`. If a field is unknown, leave the existing default — **don't
fabricate** ("circa 1985" is fabrication; year=0 is honest unknown).

### Step 3: Insert at correct alphabetical position

```c
/* WRONG — append to end, breaks D5 + risks shadowing */
{ "Zynaps",       PROT_RAPIDLOK,    1987, "Hewson",   "..." },
{ "Bruce Lee",    PROT_VMAX,        1984, "Datasoft", "..." },  /* NEW, end */

/* RIGHT — insert alphabetically, run de-dup check */
{ "Boulder Dash", PROT_NONE,        1984, "First Star", "" },
{ "Bruce Lee",    PROT_VMAX,        1984, "Datasoft", "..." },  /* NEW */
{ "Buggy Boy",    PROT_NONE,        1987, "Elite",    "" },
```

### Step 4: Run the lookup probe

```bash
# Build a tiny C probe (5 LOC) that calls c64_lookup_title for every
# title in the DB and asserts entry == the inserted record. Catches
# shadow bugs immediately.
gcc -I include/ tests/protection_db_selfcheck.c \
    src/protection/c64/c64_protection_db.c -o /tmp/dbcheck && /tmp/dbcheck

# Expected: "1032/1032 self-lookups returned canonical row"
# Failure mode: "Entry N at line L returns row M (shadowed)"
```

If a self-lookup returns the wrong row, D1 or D2 is violated.

### Step 5: Run downstream tests

```bash
ctest -R "c64_protection|protection_db" --output-on-failure
```

`test_c64_protection` is the canary. If it goes red after your edit, you
either inserted a shadow or perturbed an existing one. **Don't relax the
test** — fix the data.

## Pre-Done Checklist (vor dem „fertig"-Report verpflichtend)

Bevor du eine DB-Änderung als done meldest, gehst du diese Checkliste
durch. Jeder Punkt = explizite Antwort. D1-D5 sind die Invarianten; die
Checkliste ist der aktive Pass der sie durchsetzt.

| # | Frage | Wie geprüft |
|---|---|---|
| **P1** | **Substring-Suche vor Insert gemacht?** Habe ich case-insensitive nach dem Title-Kern UND nach möglichen Shadow-Kandidaten gegrept? | `grep -in "<core>"` Output im Report zeigen, nicht „habe ich gemacht". |
| **P2** | **Genau EINE Zeile pro Title?** Bestätigt die Title-Uniqueness-Audit (§Verification 1) dass kein Duplikat besteht? | `python3 -c "..."` Ausgabe `NONE` im Report. D1. |
| **P3** | **Kein Substring-Shadow?** Audit (§Verification 2) meldet 0 D2-Violations? Spezifischer Title („Gauntlet II") wird nicht von kürzerem („Gauntlet") überschattet? | `scripts/audit_protection_db.py` Output mit `0 D2 violations`. D2. |
| **P4** | **Kein Prefix-Swallow?** Mein neuer Title ist >= 6 Zeichen ODER ein distinktives Token, der keinen unrelated Title schluckt? | Audit-Output `0 D3 violations` + manueller Sanity-Check der nachbarten Titel. D3. |
| **P5** | **Canonical statt Stub?** Wenn ich einen alten `PROT_UNKNOWN, 0, "", ""`-Stub finde der durch meinen Insert canonical wird: ist der Stub GELÖSCHT, nicht behalten „just in case"? | `git diff` zeigt eine ALTE Zeile gelöscht UND eine NEUE Zeile eingefügt. D4. |
| **P6** | **Alphabetisch korrekt eingefügt?** Position liegt zwischen den richtigen Nachbarn in der Plattform-Sektion, nicht ans Datei-Ende drangehängt? | `git diff` Kontext zeigt direkt vorausgehende + nachfolgende Zeile alphabetisch korrekt. D5. |
| **P7** | **Keine fabrizierten Jahre?** Wenn ich `year` nicht aus einer verifizierten Quelle weiß: steht 0, nicht „felt about 1985"? | Quelle für jedes != 0 year im Commit-Body ODER `notes`-Feld. DESIGN_PRINCIPLE 4. |
| **P8** | **Self-Lookup-Probe gelaufen?** Hat `protection_db_selfcheck` für jeden Eintrag (auch die nicht von mir geänderten) die canonical Row zurückgegeben? | `1032/1032 self-lookups returned canonical row` Output. Sonst → D1 oder D2 verletzt. |
| **P9** | **Downstream-Tests grün?** `ctest -R "c64_protection\|protection_db"` ist grün NACH meiner Änderung? | `--output-on-failure` Output zeigen, nicht „läuft hoffentlich". |
| **P10** | **Article-Variant entschieden?** Bei „The X" / „X, The": habe ich für die Plattform-Sektion EINE Form gewählt und im Kommentar oberhalb der Sektion dokumentiert? | Code-Kommentar im Diff sichtbar. |

Wenn auch nur EIN Punkt rot oder unbeantwortet: die Aufgabe ist offen.
Ein latenter Shadow-Bug in der DB ist eine forensische Falschmeldung —
„Bruce Lee → V-MAX" wenn es eigentlich die Datasoft-Variante ohne
Protection ist, ist ein Verstoß gegen DESIGN_PRINCIPLE 1.

## Common pitfalls (Beispiel-Anti-Patterns)

Die Pre-Done-Checkliste oben ist die *aktive* Pflicht; die folgenden
Beispiele sind die *Belegquellen* dafür, warum es jeden Punkt gibt.

### Adding a short distinctive token without checking prefix swallow

```c
/* WRONG — "Pool" swallows Pool of Radiance, Pool Shark, ... */
{ "Pool", PROT_NONE, 1986, "Mastertronic", "" },

/* RIGHT — full canonical title */
{ "Pool BCM",   PROT_NONE, 1986, "Mastertronic", "" },
{ "Pool of Radiance", PROT_VMAX, 1988, "SSI", "" },
```

### Keeping the old stub "just in case"

```c
/* OLD stub at line 38 */
{ "Gauntlet", PROT_UNKNOWN, 0, "", "" },                       /* DELETE */

/* NEW canonical at line 540 */
{ "Gauntlet", PROT_RAPIDLOK, 1987, "US Gold", "Atari ST port..." },
```

D4: the canonical entry replaces the stub. Two entries with the same
title = D1 violation = silent shadow. We're not building an audit
history *of the DB* here — that's `git log`'s job.

### Title with leading article variants

```c
/* English "The X" — store both forms */
{ "Last Ninja, The", ... },
{ "The Last Ninja",  ... },    /* duplicate? NO — substring-overlap, but
                                  not identical; depends on disk-name
                                  convention. Document choice in comment. */
```

When the disk-name convention isn't stable across releases (CSDb vs.
GameBase64), pick **one** form, document why in a comment above the
section.

### Fabricating year from "felt about right"

```c
/* WRONG — 1985 is plausible, but you don't actually know */
{ "Some Obscure Demo", PROT_FATBITS, 1985, "Unknown", "" },

/* RIGHT — year=0 = honest unknown, downstream report shows "year unknown" */
{ "Some Obscure Demo", PROT_FATBITS, 0, "Unknown", "" },
```

DESIGN_PRINCIPLE 4: "keine erfundenen Daten" — applies to DB curation
too.

## Verification

```bash
# 1. Title-uniqueness audit
python3 -c "
import re, collections
text = open('src/protection/c64/c64_protection_db.c').read()
titles = re.findall(r'^\s*\{\s*\"([^\"]+)\"', text, re.M)
dupes = [t for t, n in collections.Counter(titles).items() if n > 1]
print('Duplicate titles:', dupes if dupes else 'NONE')
"

# 2. Shadow audit (substring relationships)
python3 scripts/audit_protection_db.py src/protection/c64/c64_protection_db.c
# expected output: "0 D2 violations, 0 D3 violations"

# 3. Self-lookup probe
gcc -I include/ -o /tmp/dbcheck tests/protection_db_selfcheck.c \
    src/protection/c64/c64_protection_db.c
/tmp/dbcheck   # expected: all rows return self

# 4. Full protection test suite
ctest -R protection --output-on-failure
```

## Open follow-up (v4.1.6+)

Per v4.1.5 release notes: ~60 additional duplicate-title entries remain
in `c64_protection_db.c`. They are not currently test-blocking, but
they are latent shadow-bugs. Two paths:

| Option | Effort | Cost |
|---|---|---|
| **A. Curate** — walk the DB, merge each pair, keep richer entry | ~4–6 h | One-time, finite |
| **B. Fix lookup** — replace bidirectional `strstr` with "longest exact match wins" | ~30 LOC | Behaviour change, needs regression tests against existing callers |

**A** preserves the API contract (callers depend on permissive
substring matching for partial disk-name extracts). **B** breaks it but
is structurally cleaner. Decide at v4.1.6 planning; don't mix into
unrelated PRs.

## Related

- `src/protection/c64/c64_protection_db.c` — 1032 entries, C64 platform
- `src/protection/uft_protection_classify.c` — cross-platform DB +
  classifier glue
- `src/protection/uft_protection_extended.c` — extended schemes (Amiga,
  Atari ST)
- `include/uft/protection/uft_c64_protection.h` — `c64_lookup_title()`
  declaration
- `.claude/skills/uft-protection-scheme/` — when implementing a
  *detector* (not a DB entry). This skill is data; that one is code.
- `.claude/agents/stub-eliminator.md` — old `PROT_UNKNOWN, 0, "", ""`
  stub entries qualify as D4 violations and are deletion candidates
- `git show c0ea296` — the v4.1.5 commit that removed 9 shadow duplicates
