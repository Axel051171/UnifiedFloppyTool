#!/usr/bin/env bash
# MF-925 — Selbsttest fuer den Pre-Push-Haken.
#
#   bash scripts/selftest_prepush_hook.sh
#
# ── WARUM ES DIESEN SELBSTTEST GIBT ──────────────────────────────────
#
# Der Haken hat monatelang seine dritte Pruefung NICHT ausgefuehrt und
# danach freigegeben:
#
#     WARN: CMake configure could not find a C/C++ compiler — skipping
#     ==> checks 1+2 passed (3 skipped) — push allowed
#     exit 0
#
# Niemandem ist es aufgefallen, weil die Zeile sich im Vorbeigehen wie
# "3 passed" liest. Ein Pruefwerkzeug, das sich selbst ueberspringt und
# dann durchwinkt, ist genau die Klasse, die dieser Baum sonst ueberall
# jagt — nur diesmal am Werkzeug selbst.
#
# Geprueft werden die beiden Zweige, die darueber entscheiden:
#
#   Fall 1  keine Werkzeugkette, kein Freibrief  -> Rueckgabe 1
#   Fall 2  dieselbe Lage, UFT_PREPUSH_ALLOW_NO_COMPILER=1 -> 0,
#           aber mit "NICHT GELAUFEN" im Klartext
#
# GRENZE, ausgesprochen: geprueft wird die ENTSCHEIDUNG des Hakens.
# Die Pruefungen 1 und 2 werden mit einem no-op-`python3` ueberbrueckt,
# damit der Lauf Sekunden statt Minuten dauert. Ob `check_consistency.py`
# selbst richtig arbeitet, ist die Sache seiner eigenen Selbsttests.
set -u

REPO="$(git rev-parse --show-toplevel)"
HOOK="$REPO/.git/hooks/pre-push"

if [[ ! -x "$HOOK" ]]; then
    echo "FEHLER: $HOOK fehlt — bash scripts/install-hooks.sh" >&2
    exit 2
fi

# git muss im Pruef-PATH bleiben; der Haken ruft es in Zeile 1.
GITDIR="$(dirname "$(command -v git)")"
SHIM="$(mktemp -d)"
trap 'rm -rf "$SHIM"' EXIT

printf '#!/usr/bin/env bash\nexit 0\n' > "$SHIM/python3"
cat > "$SHIM/cmake" <<'EOF'
#!/usr/bin/env bash
# tut so, als gaebe es keinen Compiler — und schreibt die Meldung, an
# der der Haken diesen Fall erkennt
log=""; prev=""
for a in "$@"; do
  [ "$prev" = "-B" ] && log="$a/configure.log"
  prev="$a"
done
if [ -n "$log" ]; then
  mkdir -p "$(dirname "$log")"
  echo "CMake Error: No CMAKE_C_COMPILER could be found." > "$log"
fi
exit 1
EOF
chmod +x "$SHIM/python3" "$SHIM/cmake"

cd "$REPO"
fehler=0

echo "[1/2] keine Werkzeugkette, kein Freibrief -> muss FEHLSCHLAGEN"
aus1="$(PATH="$SHIM:$GITDIR:/usr/bin:/bin" bash "$HOOK" </dev/null 2>&1)"
rc1=$?
if [[ $rc1 -ne 1 ]]; then
    echo "  ROT: Rueckgabe $rc1 statt 1 — der Haken gibt wieder frei,"
    echo "       ohne die dritte Pruefung ausgefuehrt zu haben."
    fehler=1
elif ! grep -q "NICHT GELAUFEN\|konnte nicht laufen" <<<"$aus1"; then
    echo "  ROT: Rueckgabe stimmt, aber die Meldung sagt nicht, dass die"
    echo "       Pruefung nicht gelaufen ist."
    fehler=1
else
    echo "  gruen (Rueckgabe 1, Grund benannt)"
fi

echo "[2/2] dieselbe Lage, aber bewusst freigegeben -> muss ERLAUBEN"
aus2="$(UFT_PREPUSH_ALLOW_NO_COMPILER=1 PATH="$SHIM:$GITDIR:/usr/bin:/bin" \
        bash "$HOOK" </dev/null 2>&1)"
rc2=$?
if [[ $rc2 -ne 0 ]]; then
    echo "  ROT: Rueckgabe $rc2 statt 0 — der benannte Ausweg traegt nicht."
    fehler=1
elif ! grep -q "NICHT GELAUFEN" <<<"$aus2"; then
    echo "  ROT: erlaubt, sagt aber nicht, dass Pruefung 3 ausfiel — genau"
    echo "       der Zustand, den MF-925 behoben hat."
    fehler=1
else
    echo "  gruen (Rueckgabe 0, Ausfall im Klartext)"
fi

echo
if [[ $fehler -eq 0 ]]; then
    echo "OK — Skip ist ein Fehlschlag, Freigabe nur mit Namen."
    exit 0
fi
echo "FAIL — der Haken darf nicht stillschweigend durchwinken."
exit 1
