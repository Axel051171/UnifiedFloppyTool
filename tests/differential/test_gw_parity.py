"""
test_gw_parity.py — P3.2 gw-vs-UFT differential conformance (#109).

For each synthetic corpus disk: decode the SAME gw-produced .scp flux
stream with BOTH Greaseweazle (`gw convert`) and the UFT flux engine
(the uft_flux_decode helper), and assert the two decoded sector images
are BYTE-IDENTICAL.

This is real parity, not theatre: a format passes only when UFT and gw
produce the exact same bytes from the exact same flux. All six corpus
disk classes — IBM MFM (DD/HD), Atari ST MFM, Commodore 1541 GCR,
Apple II GCR and AmigaDOS MFM — are wired end-to-end and reach
byte-exact parity.

Corpus: tests/differential/corpus/ (see gen_corpus.py). gw + corpus +
helper are resolved by conftest.py; the suite skips cleanly when gw is
absent (it is not on PyPI).
"""
from __future__ import annotations

import subprocess
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent

# name, gw --format, helper encoding, heads, spt, secsize, first_sec, bitcell_ns
#
# The MFM family is geometry-parameterised (heads/spt/secsize/first_sec/
# bitcell). The three fixed-geometry encodings — gcr_c64, gcr_apple,
# amiga — hardcode their disk class in the helper, so their numeric
# columns are 0 and ignored.
CORPUS = [
    ("ibm_dd",     "ibm.720",             "mfm",       2,  9, 512, 1, 2000),
    ("ibm_hd",     "ibm.1440",            "mfm",       2, 18, 512, 1, 1000),
    ("atarist_dd", "atarist.720",         "mfm",       2,  9, 512, 1, 2000),
    ("c64_gcr",    "commodore.1541",      "gcr_c64",   0,  0,   0, 0,    0),
    ("apple2_525", "apple2.appledos.140", "gcr_apple", 0,  0,   0, 0,    0),
    ("amiga_dd",   "amiga.amigados",      "amiga",     0,  0,   0, 0,    0),
]


@pytest.mark.parametrize("spec", CORPUS, ids=[c[0] for c in CORPUS])
def test_gw_parity(spec, gw_path, uft_decoder, corpus_ready, tmp_path):
    name, gw_fmt, enc, heads, spt, secsize, first_sec, bitcell = spec
    scp = corpus_ready / f"{name}.scp"
    assert scp.is_file(), f"corpus flux missing: {scp}"

    # --- UFT side -------------------------------------------------------
    uft_img = tmp_path / f"{name}.uft.img"
    args = [str(uft_decoder), str(scp), enc, str(uft_img),
            str(heads), str(spt), str(secsize), str(first_sec)]
    if bitcell:
        args.append(str(bitcell))
    uft = subprocess.run(args, capture_output=True, text=True)
    assert uft.returncode == 0, (
        f"{name}: uft_flux_decode exit {uft.returncode}\n{uft.stderr}")

    # --- gw side --------------------------------------------------------
    gw_img = tmp_path / f"{name}.gw.img"
    gw = subprocess.run(
        [gw_path, "convert", str(scp), str(gw_img), f"--format={gw_fmt}"],
        capture_output=True, text=True, timeout=180)
    assert gw.returncode == 0 and gw_img.is_file(), (
        f"{name}: gw convert exit {gw.returncode}\n{gw.stderr}")

    # --- byte-exact differential ---------------------------------------
    gw_bytes = gw_img.read_bytes()
    uft_bytes = uft_img.read_bytes()
    if gw_bytes != uft_bytes:
        # find + show the first divergence as hex context
        n = min(len(gw_bytes), len(uft_bytes))
        first = next((i for i in range(n) if gw_bytes[i] != uft_bytes[i]), n)
        lo = max(0, first - 8)
        ctx_gw = gw_bytes[lo:lo + 32].hex()
        ctx_uft = uft_bytes[lo:lo + 32].hex()
        pytest.fail(
            f"{name}: gw decode != UFT decode "
            f"(gw {len(gw_bytes)} B, UFT {len(uft_bytes)} B; "
            f"first diff @ offset {first})\n"
            f"  gw  [@{lo}]: {ctx_gw}\n"
            f"  uft [@{lo}]: {ctx_uft}")
