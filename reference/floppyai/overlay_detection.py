import numpy as np
from typing import List, Tuple, Optional
from pathlib import Path
from flux_analyzer import FluxAnalyzer


def detect_side_overlay_mfm(files: List[str], bins: int) -> Tuple[Optional[int], list, float]:
    """Detect sector overlay for MFM-like media using FFT+ACF with local refinement.

    Returns (sector_count, boundaries_deg, confidence)
    - sector_count: int or None
    - boundaries_deg: list of floats in degrees
    - confidence: 0..1 heuristic based on normalized peak strengths
    """
    import math
    hist = np.zeros(bins, dtype=float)
    used = 0
    for fp in files[:8]:  # limit cost
        try:
            fa = FluxAnalyzer(); fa.parse(fp)
            for rev in fa.revolutions:
                if len(rev) < 8:
                    continue
                t = np.cumsum(rev.astype(np.float64))
                total = t[-1]
                if total <= 0:
                    continue
                idx = np.floor((t / total) * bins).astype(int)
                idx[idx >= bins] = bins - 1
                for k in idx:
                    hist[k] += 1.0
            used += 1
        except Exception:
            continue
    if used == 0 or np.all(hist == 0):
        return (None, [], 0.0)

    # Compute FFT and ACF
    H = np.fft.rfft(hist)
    P = np.abs(H) ** 2
    if len(P) < 3:
        return (None, [], 0.0)
    P[0] = 0.0
    k0 = int(np.argmax(P))
    # Autocorrelation for confirmation
    acf = np.fft.irfft(P)
    acf[0] = 0.0
    k1 = int(np.argmax(acf[: max(2, bins // 2)]))
    # Choose a k consistent between FFT & ACF
    cand = []
    for k in [k0, k1]:
        if k >= 2 and k <= bins // 2:
            cand.append(k)
    if not cand:
        return (None, [], 0.0)
    k_sel = int(np.median(cand))
    pk = float(P[k_sel]) if k_sel < len(P) else 0.0
    total = float(np.sum(P) + 1e-9)
    confidence = min(1.0, pk / total)

    # Initial phase via dominant frequency component
    phase = np.angle(H[k_sel])
    # Convert to starting bin index
    step = (2 * np.pi) / k_sel
    phi = (phase % (2 * np.pi)) / step

    # Place boundaries and locally refine to nearest maxima in a small window
    step_bins = bins / float(k_sel)
    boundaries = []
    refine_win = max(1, int(0.01 * bins))
    for m in range(k_sel):
        i0 = int(round((phi + m) * step_bins)) % bins
        lo_i = max(0, i0 - refine_win)
        hi_i = min(bins - 1, i0 + refine_win)
        if hi_i > lo_i:
            loc = lo_i + int(np.argmax(hist[lo_i : hi_i + 1]))
        else:
            loc = i0
        boundaries.append(loc)
    boundaries = sorted(boundaries)
    boundaries_deg = [(b / bins) * 360.0 for b in boundaries]
    return (k_sel, boundaries_deg, confidence)


def detect_side_overlay_gcr(
    files: List[str], bins: int, candidates: List[int], win_frac: float = 0.01
) -> Tuple[Optional[int], list, float]:
    """Detect sector overlay for Apple GCR-like media using boundary contrast with refinement.

    Returns (sector_count, boundaries_deg, confidence)
    - Examines candidate sector counts k and phases to maximize contrast between boundary windows
      and within-sector windows. Refines boundary bins to local maxima like the MFM path.
    """
    hist = np.zeros(bins, dtype=float)
    used = 0
    for fp in files[:8]:
        try:
            fa = FluxAnalyzer(); fa.parse(fp)
            for rev in fa.revolutions:
                if len(rev) < 8:
                    continue
                t = np.cumsum(rev.astype(np.float64))
                total = t[-1]
                if total <= 0:
                    continue
                idx = np.floor((t / total) * bins).astype(int)
                idx[idx >= bins] = bins - 1
                for k in idx:
                    hist[k] += 1.0
            used += 1
        except Exception:
            continue
    if used == 0 or np.all(hist == 0):
        return (None, [], 0.0)

    # Light smoothing to stabilize maxima
    if bins >= 5:
        kernel = np.ones(5, dtype=float) / 5.0
        hist = np.convolve(hist, kernel, mode="same")
    total_sum = float(np.sum(hist) + 1e-9)
    win = max(1, int(win_frac * bins))

    best = None
    best_params = (None, 0.0, 0.0)  # (k, phi_bins, contrast)
    for k in candidates:
        if not isinstance(k, int) or k < 2 or k > bins // 2:
            continue
        step = bins / float(k)
        phase_steps = max(8, int(step))
        for ps in range(phase_steps):
            phi = (ps / float(phase_steps)) * step
            # boundary indices
            bidx = [int(round((phi + m * step))) % bins for m in range(k)]
            # boundary window mass
            bsum = 0.0
            for bi in bidx:
                lo = max(0, bi - win)
                hi = min(bins - 1, bi + win)
                bsum += float(np.sum(hist[lo : hi + 1]))
            within = max(0.0, total_sum - bsum)
            b_len = k * (2 * win + 1)
            w_len = max(1, bins - b_len)
            b_mean = bsum / b_len
            w_mean = within / w_len
            if w_mean <= 1e-9:
                contrast = 0.0
            else:
                contrast = max(0.0, (w_mean - b_mean) / w_mean)
            if (best is None) or (contrast > best):
                best = contrast
                best_params = (k, phi, contrast)

    if best is None or best_params[0] is None:
        return (None, [], 0.0)

    # Build/refine boundaries from best params
    k, phi, contrast = best_params
    step = bins / float(k)
    win_ref = max(1, int(0.01 * bins))
    refined_bins = []
    for m in range(k):
        b0 = int(round((phi + m * step))) % bins
        lo = max(0, b0 - win_ref)
        hi = min(bins - 1, b0 + win_ref)
        if hi > lo:
            loc = lo + int(np.argmax(hist[lo : hi + 1]))
        else:
            loc = b0
        refined_bins.append(loc)
    refined_bins = sorted(refined_bins)
    boundaries_deg = [(b / bins) * 360.0 for b in refined_bins]
    return (k, boundaries_deg, float(min(1.0, contrast)))
