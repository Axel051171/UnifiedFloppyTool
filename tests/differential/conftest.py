"""
conftest.py — fixtures for the P3.2 gw-vs-UFT differential suite (#109).

Provides three session fixtures, each of which SKIPS the suite cleanly
(never fails) when its external is unavailable — a missing tool is an
environment gap, not a test failure:

  gw_path       resolves the Greaseweazle `gw` binary ($GW env var, then
                PATH). gw is not on PyPI — install from
                github.com/keirf/greaseweazle (Windows: gw.exe).
  uft_decoder   builds tests/differential/uft_flux_decode.exe from its
                committed .c source via gcc/cc if absent or stale. That
                helper is a TEST FIXTURE that links the UFT flux decode
                engine — UFT itself is GUI-only (memory feedback_no_cli).
  corpus_ready  ensures corpus/flux/*.scp exist, running gen_corpus.py
                (which needs gw) if they do not. The .scp streams are a
                build artifact; corpus/sources/* is the committed,
                hash-manifested deterministic input.
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parents[1]
CORPUS_FLUX = HERE / "corpus" / "flux"
HELPER_SRC = HERE / "uft_flux_decode.c"
HELPER_EXE = HERE / ("uft_flux_decode.exe" if os.name == "nt"
                     else "uft_flux_decode")
GEN_CORPUS = HERE / "gen_corpus.py"

# UFT engine sources the helper links (kept in sync with the helper's
# #include list — both are self-contained C TUs).
# Quellen des Helfers.
#
# MF-777: hier standen ZWEI Dateien, gebraucht werden NEUN. Der Bau war
# damit kaputt — und niemand hat es gemerkt, weil die sechs Tests
# davor am Skip „no C compiler" hängenblieben. Ein Skip, der einen
# Bruch verdeckt, ist schlimmer als ein roter Test: er sieht aus wie
# eine bewusste Auslassung.
#
# Das ist dasselbe Muster wie die 43 CMake-Blöcke aus MF-772/773 — eine
# von Hand gepflegte Quellenliste, die bei jeder neuen Abhängigkeit des
# Dekoders nachgezogen werden muss und es nicht wird. Die strukturelle
# Antwort ist **P3-10** (eine Objektbibliothek, gegen die alles linkt);
# bis dahin ist diese Liste die dritte Stelle, an der derselbe Satz
# gepflegt wird.
HELPER_DEPS = [
    REPO_ROOT / "src" / "flux" / "uft_scp_parser.c",
    REPO_ROOT / "src" / "flux" / "uft_flux_decoder.c",
    REPO_ROOT / "src" / "flux" / "uft_track_verdikt.c",      # MF-765
    REPO_ROOT / "src" / "flux" / "uft_flux_histogram.c",
    REPO_ROOT / "src" / "flux" / "uft_flux_sync_search.c",
    REPO_ROOT / "src" / "flux" / "uft_dewarp.c",
    REPO_ROOT / "src" / "flux" / "uft_decode_timeline.c",
    REPO_ROOT / "src" / "flux" / "uft_media_profile.c",
    REPO_ROOT / "src" / "flux" / "uft_mfm_sector_parser.c",
    REPO_ROOT / "src" / "formats" / "amiga" / "uft_amiga_syncs.c",  # MF-768
    REPO_ROOT / "src" / "core" / "uft_log.c",                # MF-766
]


def _resolve_gw() -> str | None:
    env = os.environ.get("GW")
    if env:
        return env if Path(env).exists() else None
    return shutil.which("gw")


@pytest.fixture(scope="session")
def gw_path() -> str:
    gw = _resolve_gw()
    if gw is None:
        pytest.skip("gw not found — set $GW or put it on PATH "
                    "(github.com/keirf/greaseweazle, not on PyPI)")
    return gw


@pytest.fixture(scope="session")
def uft_decoder() -> Path:
    """Build (if needed) and return the uft_flux_decode helper path."""
    need_build = (not HELPER_EXE.exists() or
                  HELPER_EXE.stat().st_mtime <
                  max(p.stat().st_mtime
                      for p in [HELPER_SRC, *HELPER_DEPS] if p.exists()))
    if need_build:
        cc = shutil.which("gcc") or shutil.which("cc") or shutil.which("clang")
        if cc is None:
            pytest.skip("no C compiler (gcc/cc/clang) — cannot build the "
                        "uft_flux_decode helper")
        # MF-817: `-std=gnu11`, NICHT `-std=c11`.
        #
        # `-std=c11` setzt `__STRICT_ANSI__`, und glibc blendet damit
        # `strdup()` aus — es ist POSIX, nicht ISO C11. GCC nahm die
        # implizite Deklaration `int strdup()` an, stutzte den Zeiger auf
        # 32 Bit, legte ihn in `ctx->creator_string`
        # (`uft_scp_parser.c:156`) und reichte ihn in `uft_scp_close()`
        # (`:325`) an `free()`. Ergebnis: SIGSEGV (-11) in ALLEN SECHS
        # Paritaetsfaellen — und zwar NACH dem Dekodieren, die
        # Sektorabbilder waren laengst richtig geschrieben. Nur der
        # Rueckgabewert war ungleich 0.
        #
        # Der Produktivbau setzt nirgends einen C-Standard (weder .pro
        # noch CMake), also gilt GCCs Vorgabe gnu11/gnu17 und `strdup`
        # ist deklariert. Der EINZIGE Pfad mit striktem `-std=c11` war
        # diese Zeile — und `uft_scp_parser.c` steht in HELPER_DEPS erst
        # seit MF-777, das die Liste von zwei auf elf Dateien erweitert
        # hat. Davor hing die Reihe an „no C compiler" bzw. „gw not
        # found": genau das Skip-Muster aus MF-598.
        #
        # `-Werror=implicit-function-declaration` gehoert dazu, sonst
        # rutscht derselbe Fehler beim naechsten POSIX-Aufruf wieder als
        # still gestutzter Zeiger durch statt als Uebersetzungsfehler.
        # Dasselbe Muster wartet bereits bei `localtime_r` in
        # `src/core/uft_log.c:137` — ab GCC 14 ist die Option ohnehin
        # Vorgabe, dann braeche dort schon der Bau.
        cmd = [cc, "-std=gnu11", "-O2",
               "-Werror=implicit-function-declaration",
               "-I", str(REPO_ROOT / "include"),
               str(HELPER_SRC), *[str(p) for p in HELPER_DEPS],
               "-lm", "-o", str(HELPER_EXE)]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            pytest.fail("uft_flux_decode helper failed to build:\n"
                        + r.stderr)
    return HELPER_EXE


@pytest.fixture(scope="session")
def corpus_ready(gw_path) -> Path:
    """Ensure corpus/flux/*.scp exist; regenerate via gen_corpus.py if not."""
    scps = list(CORPUS_FLUX.glob("*.scp"))
    if len(scps) < 6:
        env = dict(os.environ, GW=gw_path)
        r = subprocess.run([sys.executable, str(GEN_CORPUS)],
                           capture_output=True, text=True, env=env)
        if r.returncode != 0:
            pytest.fail("gen_corpus.py failed:\n" + r.stdout + r.stderr)
        scps = list(CORPUS_FLUX.glob("*.scp"))
    if not scps:
        pytest.skip("corpus flux streams unavailable")
    return CORPUS_FLUX
