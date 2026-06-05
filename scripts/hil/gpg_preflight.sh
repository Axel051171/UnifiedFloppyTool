#!/usr/bin/env bash
# gpg_preflight.sh — verify GPG signing is ready for v4.1.4 tag.
#
# Run before scripts/hil/gen_baseline_phase2.sh succeeds — if GPG is
# not ready, you don't want to discover that AFTER the HIL gate is
# green. Read-only; reports state, never modifies.
#
# Exit codes:
#   0 — GPG ready: at least one secret key + git configured
#   1 — GPG not ready (details below)
#   2 — environment issue (no gpg, no git)

set -u

red()    { printf '\033[31m%s\033[0m\n' "$*"; }
green()  { printf '\033[32m%s\033[0m\n' "$*"; }
yellow() { printf '\033[33m%s\033[0m\n' "$*"; }
bold()   { printf '\033[1m%s\033[0m\n' "$*"; }

bold "=== GPG pre-flight for v4.1.4 signed tag ==="
echo

FAIL=0

# Step 1 — gpg binary present
if ! command -v gpg >/dev/null 2>&1; then
    red "  ✗ gpg not on PATH"
    red "    install: https://gnupg.org/download/"
    exit 2
fi
GPG_VERSION="$(gpg --version 2>/dev/null | head -1)"
green "  ✓ gpg present: $GPG_VERSION"

# Step 2 — secret key exists
# Use --batch + --with-colons so it never prompts for pinentry just to list.
# Run with a short timeout in case gpg-agent is wedged.
echo
bold "Step 2 — secret keys"
KEYS_RAW="$(timeout 10 gpg --batch --with-colons --list-secret-keys 2>/dev/null)"
if [ -z "$KEYS_RAW" ]; then
    red "  ✗ no secret keys found OR gpg-agent is wedged"
    red "    test interactively in your own shell:"
    red "      gpg --list-secret-keys --keyid-format=long"
    red "    if the command hangs:"
    red "      gpgconf --kill gpg-agent && gpgconf --launch gpg-agent"
    red "    if you have no key at all, generate one:"
    red "      gpg --full-generate-key"
    red "      (choose RSA 4096, 0 = never expires or pick a date,"
    red "       UID = same email as your git config user.email)"
    FAIL=1
else
    # Parse the colon-format output for primary key fingerprints.
    KEY_COUNT="$(printf '%s\n' "$KEYS_RAW" | grep -c '^sec:')"
    green "  ✓ $KEY_COUNT secret key(s) in keyring"
    printf '%s\n' "$KEYS_RAW" | grep -E '^(sec|uid):' | while IFS=: read -r kind _ _ _ keyid _ _ _ _ uid _; do
        if [ "$kind" = "sec" ]; then
            echo "    key: $keyid"
        elif [ "$kind" = "uid" ]; then
            echo "      uid: $uid"
        fi
    done
fi

# Step 3 — git is configured to use one of those keys
echo
bold "Step 3 — git configuration"
SIGNING_KEY="$(git config --get user.signingkey 2>/dev/null || echo)"
GPGSIGN="$(git config --get commit.gpgsign 2>/dev/null || echo false)"
TAG_GPGSIGN="$(git config --get tag.gpgsign 2>/dev/null || echo)"

if [ -z "$SIGNING_KEY" ]; then
    yellow "  ⚠ git config user.signingkey is not set"
    yellow "    you can either:"
    yellow "      a) set it: git config --global user.signingkey <key-id>"
    yellow "      b) leave unset — gpg will use the default secret key"
    yellow "    'a' is recommended for explicit-ness"
else
    green "  ✓ git config user.signingkey = $SIGNING_KEY"
fi

if [ "$TAG_GPGSIGN" = "true" ]; then
    green "  ✓ git config tag.gpgsign = true (every tag signed by default)"
else
    yellow "  ⚠ git config tag.gpgsign is not 'true'"
    yellow "    we will use 'git tag -s' explicitly — this is fine."
fi

# Step 4 — actually try to sign a throwaway test object
echo
bold "Step 4 — live signing test (does NOT touch your repo)"
if [ "$FAIL" = "0" ]; then
    TMP="$(mktemp -t gpg_preflight.XXXXXX 2>/dev/null || mktemp)"
    echo "v4.1.4 preflight $(date -u +%s)" > "$TMP"
    if timeout 30 gpg --batch --yes --detach-sign --armor -o "${TMP}.asc" "$TMP" 2>/dev/null; then
        green "  ✓ test signature produced: ${TMP}.asc"
        rm -f "$TMP" "${TMP}.asc"
    else
        red "  ✗ test signature failed"
        red "    common causes:"
        red "      • no default secret key (set user.signingkey)"
        red "      • pinentry cannot prompt (run in a real terminal)"
        red "      • passphrase needed but agent has no cached credential"
        red "    try: echo 'test' | gpg --clearsign  (interactively)"
        rm -f "$TMP" "${TMP}.asc"
        FAIL=1
    fi
else
    yellow "  - skipped (earlier step failed)"
fi

# Step 5 — verdict
echo
bold "Verdict"
if [ "$FAIL" = "0" ]; then
    green "  GPG signing is ready. The Phase-4 tag command will be:"
    echo
    echo "    git tag -s v4.1.4 -m 'UFT v4.1.4 — type-driven HAL refactor'"
    echo
    exit 0
else
    red   "  GPG signing is NOT ready. Either:"
    red   "    a) fix the issues above and re-run this preflight, OR"
    red   "    b) change the GPG-sign decision to 'unsigned' (re-ask),"
    red   "       and use 'git tag -a v4.1.4 -m \"... UNSIGNED — explicit decision ...\"'"
    exit 1
fi
