#!/usr/bin/env bash
# Pruefung P1-P4 fuer einen Entfernungskandidaten (MF-1091).
#
# Aufruf:  scripts/check_removal.sh <pfad>
# Ausgabe gehoert nach V1 der Aufraeum-Anweisung in die Commit-Nachricht.
#
# ── Warum P1 zweigeteilt ist ────────────────────────────────────────────
#
# Die erste Fassung greppte den BASISNAMEN. Das ist an `tests/test_smoke`
# gescheitert, und zwar still: `test_smoke` ist ein PRAEFIX von
# `tests/conformance/test_smoke.py`, also meldete die Pruefung acht
# Treffer und lehnte die Entfernung ab — obwohl keiner davon die Datei
# benutzte. Gemessen MF-1090/MF-1091:
#
#     roher Basisname   12 Treffer   (davon 0 Verwendungen)
#     P1a exakter Pfad   2 Treffer   (beide EINTRAEGE ueber die Datei)
#     P1b Wortgrenzen    3 Treffer   (dito)
#
# Daher zwei Zahlen statt einer, und beide werden AUSGEGEBEN statt
# verrechnet: P1a findet Verwendungen des Pfades, P1b Nennungen des
# Namens an Wortgrenzen. **Keine der beiden darf blind als Freibrief
# gelesen werden** — ein Treffer kann eine Verwendung sein oder ein
# Grabstein (`docs/MASTER_PLAN.md`, `docs/OPEN_ITEMS.md`). Das Skript
# zeigt die Zeilen; die Einordnung macht ein Mensch.
#
# Die Endung wird in P1b ausdruecklich NICHT mitgezaehlt: `foo` trifft
# nicht `foo.py`, wohl aber `foo` und `foo.c` — letzteres, weil eine
# Quelldatei zum Kandidaten gehoeren kann.
set -uo pipefail

p="${1:?Pfad angeben}"
b="$(basename "$p")"
fail=0

echo "== P1a Verweise auf den PFAD '$p' =="
n1=$(git grep -nI --fixed-strings "$p" -- . ":(exclude)$p" | wc -l | tr -d ' ')
echo "   Treffer: $n1"
git grep -nI --fixed-strings "$p" -- . ":(exclude)$p" | head -20
[ "$n1" -eq 0 ] || echo "   -> ansehen: Verwendung oder Eintrag UEBER die Datei?"

echo "== P1b Basisname '$b' an Wortgrenzen =="
n2=$(git grep -nIE "(^|[^A-Za-z0-9_./-])${b}([^A-Za-z0-9_.-]|\$)" \
     -- . ":(exclude)$p" | wc -l | tr -d ' ')
echo "   Treffer: $n2"
git grep -nIE "(^|[^A-Za-z0-9_./-])${b}([^A-Za-z0-9_.-]|\$)" \
    -- . ":(exclude)$p" | head -20
[ "$n2" -eq 0 ] || echo "   -> ansehen: Verwendung oder Eintrag UEBER die Datei?"

echo "== P2 Manifest =="
if grep -qF "$b" tests/corpus_manifest/manifest.json 2>/dev/null; then
  echo "   -> P2 FEHLGESCHLAGEN: Manifest-Eintrag vorhanden"; fail=1
else echo "   ok"; fi

echo "== P3 Stufentabelle =="
if grep -qF "$b" docs/VERIFICATION_TIERS.md 2>/dev/null; then
  echo "   -> P3 FEHLGESCHLAGEN: als Beleg genannt"; fail=1
else echo "   ok"; fi

echo "== P4 Offene Punkte / Quarantaene =="
p4=0
for f in docs/OPEN_ITEMS.md docs/KNOWN_ISSUES.md docs/QUARANTINE.md; do
  [ -f "$f" ] || continue
  if grep -qF "$b" "$f"; then
    echo "   Treffer in $f — GRABSTEIN oder Verwendung? ansehen"; p4=1
  fi
done
[ "$p4" -eq 0 ] && echo "   ok"

echo
if [ "$fail" -eq 0 ]; then
  echo "P2-P4 BESTANDEN. P1a=$n1 P1b=$n2 — die Treffer EINZELN einordnen,"
  echo "dann P5 (Bau+Test) und P6 (Tore) fahren und beide Zahlen in die"
  echo "Commit-Nachricht schreiben."
else
  echo "PRUEFUNG FEHLGESCHLAGEN — Datei bleibt."
  exit 1
fi
