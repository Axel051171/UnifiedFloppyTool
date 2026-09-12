#!/usr/bin/env python3
"""
extract_ref.py — the official FluxEngine CLI reference.

Emits the same JSON shape as extract_uft.py so diff.py can compare them
key-by-key.

PROVENANCE: grade = "recalled" for the FluxEngine CLI contract,
            "recalled" for the 8 MHz / 125 ns sample clock.

FluxEngine (davidgiven/fluxengine) is OPEN SOURCE — the CLI is defined
in `src/fluxengine.cc` (subcommand registry) and `src/fe-*.cc` +
`lib/config/flags.cc` (flag definitions). UFT additionally carries an
IN-REPO external audit of exactly this contract:
    tests/external_audits/fluxengine/REPORT.md
    tests/external_audits/fluxengine/mock_fluxengine.py
    tests/external_audits/fluxengine/extract_fe_contract.py
That audit (dated 2026-05-14) READ FluxEngine's own flag-definition
source and produced the corrected CLI form. The V2 provider's
build_read_argv() / build_write_argv() were then rewritten under MF-178
to match it (see fluxengine_provider_v2.cpp:134-163).

This makes the FE CLI reference STRONGER than SCP's or KryoFlux's: it is
cross-checked against the project's own source via the in-repo audit.
BUT — per that audit's own Stufe 5 note and the MF-178 comment in
fluxengine_provider_v2.cpp:155-163 — the corrected syntax has NOT been
end-to-end-tested against a real `fluxengine` binary. So the grade is
"recalled" (well-known + cross-checked from FE source), not "vendored"
(no checked-in copy of FE source) and not "needs-source" (the contract
IS established, just not HIL-confirmed).

CORRECT FluxEngine CLI form (per tests/external_audits/fluxengine/):
  read:   fluxengine read  -c <profile> -s drive:0 --tracks=cNhM \
                           --drive.revolutions=R -o <out>
  write:  fluxengine write -c <profile> -d drive:0 --tracks=cNhM -i <in>
  rpm:    fluxengine rpm
  detect: fluxengine rpm   (same command — rpm both measures + detects)

Key flag semantics (from mock_fluxengine.py / FE source):
  -c / --config  : loads a config/PROFILE BY NAME (NOT a cylinder!)
  -s / --source  : source drive spec, e.g. drive:0
  -d / --dest    : destination drive spec
  -i / --input   : input flux file
  -o / --output  : output flux file
  --tracks       : track selector, e.g. --tracks=c40h0
  --drive.revolutions : revolution count (was: --revs, pre-2022)

Upgrading to "vendored": check a copy of FluxEngine's
src/fluxengine.cc + src/fe-*.cc into the repo and parse them here
(extract_fe_contract.py already does this against a /home/claude/fe
checkout — vendor that checkout).
"""

import json
import sys

PROVENANCE = {
    "grade": "recalled (cross-checked vs FE source via in-repo external audit)",
    "project": "davidgiven/fluxengine (open source)",
    "baseline": "post-2022 config-layer CLI (--tracks / --drive.revolutions / "
                "-c <profile>); confirmed in tests/external_audits/fluxengine/",
    "files_recalled_from": [
        "tests/external_audits/fluxengine/REPORT.md (in-repo audit, 2026-05-14)",
        "tests/external_audits/fluxengine/mock_fluxengine.py (FE flag semantics)",
        "FluxEngine src/fluxengine.cc + src/fe-*.cc (read BY the in-repo audit)",
    ],
    "note": "FE CLI contract is established and cross-checked against FE's "
            "own source — but per the in-repo audit's Stufe 5 and the MF-178 "
            "comment, NOT yet end-to-end-tested against a real fluxengine "
            "binary. recalled-grade, not vendored, not HIL-confirmed.",
}

# The flag SET the corrected FluxEngine CLI expects for `read`.
# Plain int 1 = "this exact flag is expected"; comparison is presence-based
# (the diff checks UFT emits exactly this set, no more, no less).
READ_ARGV = {
    "read":                  "expected — subcommand",
    "-c":                    "expected — loads profile BY NAME",
    "{profile}":             "expected — profile name (value of -c); FE-F2 made "
                             "it the m_profile ctor parameter (was hard-coded "
                             "\"ibm\"), so a {profile} placeholder is emitted",
    "-s":                    "expected — source drive spec flag",
    "drive:0":               "expected — drive spec (value of -s)",
    "--tracks=cNhM":         "expected — track selector (was: -c N -h H); "
                             "doc/using.md: --tracks='c0h0 c1h0 c3-5h1'",
    "--drive.revolutions={N}": "expected — revolution count (was: --revs)",
    # BERICHTIGT MF-1047/MF-1049. Hier stand fuer `-o` woertlich
    # „expected — output FLUX file flag". Das ist die Stelle, an der der
    # Irrtum schriftlich vorlag: doc/using.md des Urhebers sagt
    #   `fluxengine read -c <profile> <options> -s <flux source>
    #    -o <image output>`
    #   „Reads flux (possibly from a disk) and DECODES IT INTO A FILE
    #    SYSTEM IMAGE."
    # `-o` nimmt also das DEKODIERTE ABBILD; der Fluss geht ueber
    # `--copy-flux-to=`, und im Beispiel des Urhebers stehen beide
    # nebeneinander:
    #   $ fluxengine read -c brother240 -s drive:0 -o brother.img
    #     --copy-flux-to=brother.flux
    "-o":                    "expected — DECODED IMAGE output flag "
                             "(doc/using.md: '-o <image output>'), NICHT der "
                             "Flussausgang",
    "--copy-flux-to={N}":    "expected — der Flussausgang (doc/using.md). "
                             "Seit MF-1047 setzt der Provider ihn; vorher "
                             "erwartete er den Fluss an `-o` und las ein "
                             "dekodiertes Abbild als SCP-Behaelter",
}

# BERICHTIGT MF-1047/MF-1049 — alle Zeilen von PASS auf UNVERIFIED.
#
# `build_write_argv()` erzeugt diese Token weiterhin, aber seit MF-1047
# RUFT SIE NIEMAND: `do_write_raw_flux()` sagt ab, bevor ein Prozess
# laeuft. Der Bauer bleibt stehen (MF-699: erst der Ersatz, dann die
# Loeschung) — als Beleg dafuer, WAS behauptet wurde.
#
# Und behauptet wurde das Falsche. doc/using.md des Urhebers:
#
#   `fluxengine rawwrite -s <flux source> -d <flux destination>`
#     „Reads flux from a file and writes it (possibly to a disk)
#      WITHOUT DOING ANY ENCODING."
#
# `write -i <datei>` KODIERT dagegen ein Dateisystem-Abbild. Dazu
# uebergab der Provider die `transitions_ns` als rohe 32-Bit-Worte —
# kein Behaelter, den fluxengine liest (dokumentiert: eigenes `.flux`,
# `.scp`, KryoFlux-Strom).
#
# Diese Zeilen als PASS zu fuehren hiesse, eine Form zu bestaetigen,
# deren Bedeutung falsch ist — genau der Fehler, den MF-1047 an
# „PASS (recalled)" gemessen hat. Sie stehen deshalb als
# needs-source/UNVERIFIED, bis P3-342 erledigt ist: SCP-Behaelter
# erzeugen (`src/formats/scp/uft_scp_writer.c` liegt im Baum) und
# `rawwrite` statt `write` rufen. Danach gehoert dieser Block auf die
# dokumentierte Form umgeschrieben, nicht auf die heutige.
_WRITE_UNVERIFIED = {
    "kind": "needs-source — der Schreibpfad ist seit MF-1047 unerreichbar; "
            "die dokumentierte Form ist `rawwrite -s <flux> -d <ziel>`, "
            "nicht `write -i <datei>` (P3-342)",
}

WRITE_ARGV = {
    "write":         dict(_WRITE_UNVERIFIED,
                          ref="doc/using.md: fuer Fluss ist es `rawwrite`, "
                              "nicht `write` (das kodiert ein Abbild)"),
    "-c":            dict(_WRITE_UNVERIFIED,
                          ref="Profil; bei `rawwrite` laut Doku nur noetig, "
                              "um eine Teilmenge der Diskette zu schreiben"),
    "{profile}":     dict(_WRITE_UNVERIFIED,
                          ref="Profilname (Wert von -c)"),
    "-d":            dict(_WRITE_UNVERIFIED,
                          ref="doc/using.md: `-d <flux destination>` — bei "
                              "`rawwrite` das Ziel, also drive:N"),
    "drive:0":       dict(_WRITE_UNVERIFIED,
                          ref="Laufwerksangabe (Wert von -d)"),
    "--tracks=cNhM": dict(_WRITE_UNVERIFIED,
                          ref="Spurwahl"),
    "-i":            dict(_WRITE_UNVERIFIED,
                          ref="doc/using.md: `-i` ist der Eingang fuer das "
                              "DATEISYSTEM-ABBILD. Die Flussquelle von "
                              "`rawwrite` ist `-s`"),
}

# `fluxengine rpm` — exists in FE; used for both measure-rpm and detect.
RPM_ARGV = {
    "rpm": "expected — subcommand (exists in current FE)",
}
# `fluxengine version` — FE-F6: query_version() emits this dedicated
# invocation so do_detect_drive reports a real version instead of a
# placeholder. recalled-grade: the `version` subcommand was chosen from
# the modern FE CLI surface; not HIL-confirmed (see the MF-178/FE-F6
# VERIFICATION STATUS notes in fluxengine_provider_v2.{h,cpp}).
VERSION_ARGV = {
    "version": "expected — `fluxengine version` subcommand (FE-F6)",
}
DETECT_ARGV = {
    "rpm": "expected — detect reuses the rpm subcommand (V1 parity)",
}

# Sample clock: NEEDS-SOURCE. UFT's provider hard-codes 8 MHz / 125 ns
# (fluxengine_provider_v2.cpp:470 and the .h comment). FluxEngine's
# actual .flux tick rate could NOT be confirmed from a vendored source
# in this audit environment — FluxEngine has historically used a 12 MHz
# device sample clock, which would make UFT's 125 ns assumption wrong by
# ~1.5x. But because (a) UFT does not currently DECODE the .flux bytes
# (it stores them raw — see D2) and (b) the in-repo external audit did
# not record the clock, this is graded needs-source rather than asserted
# as a FAIL on an unconfirmed value. See FE-D1-2.
TIMING = {
    "fe_sample_clock_mhz": {"ref": "needs-source — FluxEngine .flux tick rate "
                                   "not vendored; UFT assumes 8 MHz, FE has "
                                   "historically used 12 MHz (see FE-D1-2)",
                            "kind": "needs-source"},
    "fe_sample_ns":        {"ref": "needs-source — depends on the unverified "
                                   "tick rate above",
                            "kind": "needs-source"},
}


def main():
    json.dump({
        "_provenance": PROVENANCE,
        "read_argv": READ_ARGV,
        "write_argv": WRITE_ARGV,
        "rpm_argv": RPM_ARGV,
        "version_argv": VERSION_ARGV,
        "detect_argv": DETECT_ARGV,
        "timing": TIMING,
    }, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
