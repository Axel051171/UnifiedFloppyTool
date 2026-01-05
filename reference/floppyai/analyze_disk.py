#!/usr/bin/env python3
"""
FloppyAI Disk Analysis Module
Analyzes multiple .raw streams for a disk surface map.
"""
import datetime
import json
import re
from pathlib import Path
from typing import Dict, Any

import numpy as np

from flux_analyzer import FluxAnalyzer
from overlay_detection import (
    detect_side_overlay_mfm,
    detect_side_overlay_gcr,
)
from rendering import (
    render_disk_surface,
    render_instability_map,
    render_single_track_detail,
    render_side_report,
    render_disk_dashboard,
)
from utils.json_io import dump_json


def run(args):
    """Analyze multiple .raw streams for a disk surface map.

    This function analyzes all .raw files in the input directory or processes a single .raw file,
    generating per-track/side analysis with surface visualizations and metrics.
    """
    run_dir = get_output_dir(args.output_dir)
    # Prepare run logging (collect and write at end)
    log_lines = []
    def log(msg: str):
        try:
            print(msg)
        except Exception:
            pass
        try:
            log_lines.append(str(msg))
        except Exception:
            pass
    input_path = Path(args.input)

    # Find all .raw files
    raw_files = []
    if input_path.is_dir():
        # Case-insensitive discovery: include *.raw and *.RAW
        raw_files = list(input_path.rglob("*.raw")) + list(input_path.rglob("*.RAW"))
        # Deduplicate and sort
        raw_files = sorted({p.resolve() for p in raw_files})
        if not raw_files:
            print(f"No .raw files found in {input_path}")
            return 1
    elif input_path.is_file() and input_path.suffix == ".raw":
        raw_files = [input_path]
    else:
        print(f"Input must be a directory containing .raw files or a single .raw file: {input_path}")
        return 1

    log(f"Found {len(raw_files)} .raw files to analyze")
    try:
        preview = [str(p) for p in raw_files[:5]]
        if len(raw_files) > 5:
            preview.append("...")
            preview.extend([str(p) for p in raw_files[-3:]])
        log("  Files preview:")
        for line in preview:
            log(f"    {line}")
    except Exception:
        pass

    # Resolve effective RPM (infer from profile or input path when possible)
    rpm_profile_map = {
        '35HD': 300.0,   # 3.5" 1.44MB
        '35DD': 300.0,   # 3.5" 720KB
        '525HD': 360.0,  # 5.25" 1.2MB
        '525DD': 300.0,  # 5.25" 360KB
    }
    try:
        profile = getattr(args, 'profile', None)
    except Exception:
        profile = None
    # Heuristic: infer 3.5" HD (300 RPM) from input path if profile not given
    inferred_rpm = None
    try:
        ip_str = str(Path(getattr(args, 'input', '')).as_posix()).lower()
        if any(k in ip_str for k in ['1.44', '35hd', '3.5']):
            inferred_rpm = 300.0
    except Exception:
        pass
    effective_rpm = (
        float(getattr(args, 'rpm', None)) if getattr(args, 'rpm', None) is not None
        else rpm_profile_map.get(profile, inferred_rpm if inferred_rpm is not None else 360.0)
    )

    # Global surface map structure
    surface_map = {
        'global': {
            'input_path': str(input_path),
            'effective_rpm': effective_rpm,
            'media_type': getattr(args, 'media_type', None),
            'analysis_timestamp': datetime.datetime.now().isoformat(),
            'insights': {
                'overlay': {}
            },
            # In directory mode, include manifest of inputs for transparency
            'inputs': [str(p) for p in raw_files] if input_path.is_dir() else [str(input_path)]
        }
    }

    # Populate overlay config with sensible defaults
    overlay_cfg = surface_map['global']['insights']['overlay']
    fmt_enabled = getattr(args, 'format_overlay', False)
    overlay_cfg['format_overlay_enabled'] = fmt_enabled
    # Determine effective angular bins (auto from quality if not specified)
    try:
        q = str(getattr(args, 'quality', 'ultra') or 'ultra').lower()
    except Exception:
        q = 'ultra'
    user_bins = int(getattr(args, 'angular_bins', 0) or 0)
    if user_bins > 0:
        effective_ang_bins = user_bins
    else:
        # Use higher angular resolution for high/ultra
        effective_ang_bins = 1440 if q in ('high', 'ultra') else 360
    overlay_cfg['angular_bins'] = effective_ang_bins
    # Resolve overlay mode and candidates from profile to minimize CLI
    prof = (profile or '').upper()
    is_gcr_profile = prof in ('35DDGCR', '35HDGCR', '525DDGCR')
    user_overlay = str(getattr(args, 'overlay_mode', 'auto')).lower()
    # Auto picks from profile; otherwise honor explicit
    if user_overlay == 'auto':
        overlay_mode = 'gcr' if is_gcr_profile else 'mfm'
    else:
        overlay_mode = user_overlay
    # Warn on obvious mismatches (non-fatal)
    try:
        if overlay_mode == 'gcr' and prof in ('35HD','35DD','525HD','525DD'):
            log("Note: MFM profile with GCR overlay mode; results may be misleading. Consider --overlay-mode mfm or a GCR profile like 35DDGCR.")
        if overlay_mode == 'mfm' and is_gcr_profile:
            log("Note: GCR profile with MFM overlay mode; results may be misleading. Consider --overlay-mode auto/gcr.")
    except Exception:
        pass
    overlay_cfg['overlay_mode'] = overlay_mode
    # Determine GCR candidates by profile unless user provided
    if overlay_mode == 'gcr':
        user_gc = getattr(args, 'gcr_candidates', None)
        if user_gc:
            gc_candidates = str(user_gc)
        else:
            if prof in ('35DDGCR','35HDGCR'):
                # Apple 400K/800K zone counts outer->inner ~12..8
                gc_candidates = '12,11,10,9,8'
            elif prof == '525DDGCR':
                # Apple II 5.25"
                gc_candidates = '16'
            else:
                # Generic fallback
                gc_candidates = '10,12,8,9,11,13'
        overlay_cfg['gcr_candidates'] = gc_candidates

    # Process each raw file
    processed_count = 0
    for raw_file in sorted(raw_files):
        try:
            log(f"Processing {raw_file}...")

            # Extract track/side from filename if possible
            track_num = getattr(args, 'track', None)
            side_num = getattr(args, 'side', None)

            if track_num is None or side_num is None:
                # Try to parse from filename
                filename = raw_file.name
                # Supported patterns:
                #  - ...t<digits>...s<digit>... (existing)
                #  - NN.S.raw (e.g., 00.0.raw) from generate_disk
                track_match = re.search(r't(\d+)', filename, re.IGNORECASE)
                side_match = re.search(r's(\d)', filename, re.IGNORECASE)

                if track_match:
                    track_num = int(track_match.group(1))
                if side_match:
                    side_num = int(side_match.group(1))

                if track_num is None or side_num is None:
                    nn_s_match = re.match(r'^(\d{2})\.(\d)\.raw$', filename, re.IGNORECASE)
                    if nn_s_match:
                        track_num = int(nn_s_match.group(1))
                        side_num = int(nn_s_match.group(2))

                # Fallback: extract trailing NN.S before .raw anywhere in the name
                if track_num is None or side_num is None:
                    tail_match = re.search(r'(\d{1,2})\.(\d)\.raw$', filename, re.IGNORECASE)
                    if tail_match:
                        track_num = int(tail_match.group(1))
                        side_num = int(tail_match.group(2))

            if track_num is None or side_num is None:
                print(f"Warning: Could not determine track/side for {raw_file}, skipping")
                continue

            # Analyze the stream
            analyzer = FluxAnalyzer()
            parsed = analyzer.parse(str(raw_file))
            stats = parsed.get('stats', {}) if isinstance(parsed, dict) else {}
            if stats:
                log(f"  Parsed ok: total_fluxes={stats.get('total_fluxes')}, num_revolutions={stats.get('num_revolutions')}, mean_interval_ns={stats.get('mean_interval_ns'):.2f}")
                if 'decoder_sck_hz' in stats:
                    log(f"  decoder_sck_hz={stats.get('decoder_sck_hz'):.3f} Hz, decoder_oob_index_count={stats.get('decoder_oob_index_count', 0)}, decoder_total_samples={stats.get('decoder_total_samples', 0)}")

            if not parsed.get('stats', {}):
                log(f"Warning: No flux data in {raw_file}, skipping")
                continue

            # Perform analysis using effective angular bins
            analysis = analyzer.analyze(effective_ang_bins if effective_ang_bins > 0 else None)

            # Per-file plots disabled by default to reduce output volume and speed up runs.
            # If needed, we can gate this behind a CLI flag in the future.

            # Create track entry
            track_key = str(track_num)
            if track_key not in surface_map:
                surface_map[track_key] = {}

            side_key = str(side_num)
            side_data = {
                'file': str(raw_file),
                'track': track_num,
                'side': side_num,
                'analysis': analysis,
                'stats': parsed.get('stats', {})
            }

            # Add overlay information if enabled (populate global.insights.overlay.by_side)
            if getattr(args, 'format_overlay', False):
                try:
                    bins = int(effective_ang_bins or 720)
                    mode = str(getattr(args, 'overlay_mode', 'mfm')).lower()
                    files = [str(raw_file)]
                    k = None; bdeg = []; conf = 0.0
                    hint_k = getattr(args, 'overlay_sectors_hint', None)
                    if isinstance(hint_k, int) and hint_k and hint_k > 1:
                        # Use explicit hint: equally spaced boundaries
                        k = int(hint_k)
                        step = 360.0 / float(k)
                        bdeg = [step * i for i in range(k)]
                        conf = 1.0
                    elif mode == 'mfm':
                        k, bdeg, conf = detect_side_overlay_mfm(files, bins)
                    elif mode == 'gcr':
                        # Prefer profile-derived candidates from overlay_cfg
                        gc_raw = surface_map['global']['insights'].get('overlay', {}).get('gcr_candidates', None)
                        if not gc_raw:
                            gc_raw = getattr(args, 'gcr_candidates', '10,12,8,9,11,13')
                        cand = []
                        for tok in str(gc_raw).replace(' ', '').split(','):
                            try:
                                cand.append(int(tok))
                            except Exception:
                                continue
                        k, bdeg, conf = detect_side_overlay_gcr(files, bins, cand)
                    elif mode == 'auto':
                        # Try MFM then GCR, keep higher confidence
                        k1, b1, c1 = detect_side_overlay_mfm(files, bins)
                        gc_raw = surface_map['global']['insights'].get('overlay', {}).get('gcr_candidates', None)
                        if not gc_raw:
                            gc_raw = getattr(args, 'gcr_candidates', '10,12,8,9,11,13')
                        cand = []
                        for tok in str(gc_raw).replace(' ', '').split(','):
                            try:
                                cand.append(int(tok))
                            except Exception:
                                continue
                        k2, b2, c2 = detect_side_overlay_gcr(files, bins, cand)
                        if (c2 or 0) > (c1 or 0):
                            k, bdeg, conf = k2, b2, c2
                        else:
                            k, bdeg, conf = k1, b1, c1

                    if k is not None and bdeg:
                        overlay_cfg = surface_map['global']['insights']['overlay']
                        by_side = overlay_cfg.get('by_side', {})
                        entry = {
                            'sector_count': int(k),
                            'boundaries_deg': [float(x) for x in bdeg],
                            'confidence': float(conf),
                        }
                        if isinstance(hint_k, int) and hint_k and hint_k > 1:
                            entry['hint_used'] = True
                        by_side[str(side_num)] = entry
                        overlay_cfg['by_side'] = by_side
                except Exception as e:
                    print(f"Warning: Overlay detection failed for {raw_file}: {e}")

            surface_map[track_key][side_key] = side_data
            processed_count += 1

        except Exception as e:
            log(f"Error processing {raw_file}: {e}")
            continue

    if processed_count == 0:
        print("No files were successfully processed")
        return 1

    # Generate visualizations
    try:
        log("Generating visualizations...")

        # Output base prefix
        base_name = str(run_dir / Path(input_path.name).stem)

        # Formatted vs Unformatted heuristic per side (do this first so badges/offsets are ready)
        try:
            fmt_block = surface_map['global']['insights'].get('formatted', {}) if isinstance(surface_map['global'].get('insights'), dict) else {}
            by_side_fmt = fmt_block.get('by_side', {}) if isinstance(fmt_block, dict) else {}
            # Prefer overlay result when present
            ov = surface_map['global']['insights'].get('overlay', {}) if isinstance(surface_map['global'].get('insights'), dict) else {}
            ov_by_side = ov.get('by_side', {}) if isinstance(ov, dict) else {}

            # Aggregate side-level angular histograms across tracks (if available)
            def aggregate_side_hist(side: int):
                bins = 0
                # find max bins
                for tk in surface_map:
                    if tk == 'global':
                        continue
                    ent_list = surface_map[tk].get(str(side), []) if isinstance(surface_map[tk], dict) else []
                    if isinstance(ent_list, dict):
                        ent_list = [ent_list]
                    if not ent_list:
                        continue
                    ent = ent_list[0]
                    b = ent.get('analysis', {}).get('angular_bins') if isinstance(ent, dict) else None
                    if isinstance(b, int) and b > bins:
                        bins = b
                if not bins:
                    return None, 0
                agg = np.zeros(bins, dtype=float)
                used = 0
                for tk in surface_map:
                    if tk == 'global':
                        continue
                    ent_list = surface_map[tk].get(str(side), []) if isinstance(surface_map[tk], dict) else []
                    if isinstance(ent_list, dict):
                        ent_list = [ent_list]
                    if not ent_list:
                        continue
                    ent = ent_list[0]
                    ah = ent.get('analysis', {}).get('angular_hist') if isinstance(ent, dict) else None
                    b = ent.get('analysis', {}).get('angular_bins') if isinstance(ent, dict) else None
                    if isinstance(ah, list) and isinstance(b, int) and b == bins and b > 0:
                        agg[:b] += np.array(ah[:b], dtype=float)
                        used += 1
                if used > 0 and np.max(agg) > 0:
                    agg = agg / float(np.max(agg))
                return (agg if used > 0 else None), bins

            def heuristic_k_from_hist(hist: np.ndarray, bins: int, allowed: list[int] | None = None):
                # FFT-based periodicity strength
                if hist is None or bins <= 0:
                    return None, 0.0
                h = np.array(hist, dtype=float)
                H = np.abs(np.fft.rfft(h))
                # Ignore DC
                H[0] = 0.0
                # Candidate k range
                if isinstance(allowed, list) and allowed:
                    candidates = [k for k in allowed if isinstance(k, int) and 2 <= k <= max(2, bins // 2)]
                else:
                    candidates = list(range(6, min(36, bins // 2)))
                if not candidates:
                    return None, 0.0
                peak_vals = [(k, H[k] if k < len(H) else 0.0) for k in candidates]
                k_best, v_best = max(peak_vals, key=lambda kv: kv[1])
                median_spec = float(np.median(H[1:])) if H.size > 1 else 1.0
                ratio = (v_best / max(1e-9, median_spec)) if median_spec > 0 else 0.0
                # Confidence: squash into 0..1
                conf = float(np.tanh(0.3 * ratio))
                return int(k_best), conf

            formatted_block = {'by_side': {}}
            for s in [0, 1]:
                entry = {}
                # Overlay first
                ov_side = ov_by_side.get(str(s), {}) if isinstance(ov_by_side, dict) else {}
                if isinstance(ov_side, dict) and ov_side.get('boundaries_deg'):
                    entry = {
                        'formatted': True,
                        'confidence': float(ov_side.get('confidence', 0.8)),
                        'mode': surface_map['global']['insights'].get('overlay', {}).get('overlay_mode', 'mfm'),
                        'sector_count': int(ov_side.get('sector_count')) if ov_side.get('sector_count') is not None else None,
                        'method': 'overlay',
                    }
                else:
                    # Heuristic
                    hist, bins = aggregate_side_hist(s)
                    # Restrict heuristic k based on overlay_mode
                    allowed = None
                    try:
                        if str(overlay_mode).lower() == 'gcr':
                            gc_raw = getattr(args, 'gcr_candidates', '10,12,8,9,11,13')
                            allowed = []
                            for tok in str(gc_raw).replace(' ', '').split(','):
                                try:
                                    allowed.append(int(tok))
                                except Exception:
                                    continue
                        elif str(overlay_mode).lower() == 'mfm':
                            # Common MFM sector counts (not exhaustive)
                            allowed = [8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 19]
                    except Exception:
                        allowed = None
                    k, conf = heuristic_k_from_hist(hist, bins, allowed)
                    if k is not None and conf > 0.4:
                        entry = {
                            'formatted': True,
                            'confidence': float(conf),
                            'mode': overlay_mode,
                            'sector_count': int(k),
                            'method': 'heuristic_fft',
                        }
                    else:
                        entry = {
                            'formatted': False,
                            'confidence': float(1.0 - (conf or 0.0)),
                            'mode': overlay_mode,
                            'sector_count': None,
                            'method': 'heuristic_fft',
                        }
                formatted_block['by_side'][str(s)] = entry

            surface_map['global']['insights']['formatted'] = formatted_block
        except Exception as e_fmt:
            log(f"Warning: formatted/unformatted detection failed: {e_fmt}")

        # Create polar surface maps (pass args so overlay flags propagate)
        render_disk_surface(surface_map, base_name, args)
        # If only one track is present, render a single-track angular detail helper
        track_keys = [k for k in surface_map.keys() if k != 'global']
        if len(track_keys) == 1:
            render_single_track_detail(surface_map, base_name)

        # Build and render instability map
        # Prefer new flux-level instability_score (0..1); fallback to noise variance with per-side normalization.
        instab_scores = {0: {}, 1: {}}
        max_track = 0
        side_vars = {0: [], 1: []}
        side_has_abs_scale = {0: False, 1: False}
        for tk in surface_map:
            if tk == 'global':
                continue
            try:
                ti = int(tk); max_track = max(max_track, ti)
            except Exception:
                continue
            for s in [0, 1]:
                entry_list = surface_map[tk].get(str(s), []) if isinstance(surface_map[tk], dict) else []
                if isinstance(entry_list, dict):
                    entry_list = [entry_list]
                # Aggregate across entries for this track/side
                scores = []
                vars_ = []
                for ent in entry_list:
                    try:
                        an = ent.get('analysis', {})
                        sc = an.get('instability_score', None)
                        if isinstance(sc, (int, float)):
                            scores.append(float(sc))
                        var = an.get('noise_profile', {}).get('avg_variance')
                        if isinstance(var, (int, float)):
                            vars_.append(float(var))
                    except Exception:
                        continue
                if scores:
                    instab_scores[s][ti] = float(np.mean(scores))
                    side_has_abs_scale[s] = True
                elif vars_:
                    vmean = float(np.mean(vars_))
                    instab_scores[s][ti] = vmean
                    side_vars[s].append(vmean)
        # Normalize only variance-based sides; leave absolute 0..1 scores as-is
        for s in [0, 1]:
            if side_has_abs_scale[s]:
                # Clamp to [0,1]
                for ti, v in list(instab_scores[s].items()):
                    instab_scores[s][ti] = float(min(1.0, max(0.0, v)))
            else:
                vmax = max(side_vars[s]) if side_vars[s] else 1.0
                if vmax <= 0:
                    vmax = 1.0
                for ti, v in list(instab_scores[s].items()):
                    instab_scores[s][ti] = float(min(1.0, max(0.0, v / vmax)))

        render_instability_map(surface_map, instab_scores, max_track + 1, base_name)

        # Create polar surface maps (pass args so overlay flags propagate)
        render_disk_surface(surface_map, base_name, args)
        # If only one track is present, render a single-track angular detail helper
        track_keys = [k for k in surface_map.keys() if k != 'global']
        if len(track_keys) == 1:
            render_single_track_detail(surface_map, base_name)

        # Per-side composite reports (one PNG per side)
        try:
            for s in [0, 1]:
                render_side_report(surface_map, instab_scores, s, base_name, args)
                log(f"Saved side report for side {s}")
            # Whole-disk dashboard
            try:
                render_disk_dashboard(surface_map, instab_scores, base_name, args)
                log("Saved whole-disk dashboard")
            except Exception as e_dash:
                log(f"Warning: Dashboard rendering failed: {e_dash}")
        except Exception as e:
            log(f"Warning: Side report rendering failed: {e}")

        log(f"Visualizations saved in {run_dir}")

    except Exception as e:
        log(f"Warning: Visualization generation failed: {e}")

    # Save surface map
    surface_map_path = run_dir / 'surface_map.json'
    dump_json(surface_map_path, surface_map)
    log(f"Surface map saved to {surface_map_path}")

    # Optional LLM summary
    if getattr(args, 'summarize', False):
        try:
            log("Generating LLM summary...")
            summarize_disk_analysis(surface_map, run_dir, args)
        except Exception as e:
            log(f"Warning: LLM summary failed: {e}")

    # Write run log
    try:
        rl = run_dir / 'run.log'
        with open(rl, 'w', encoding='utf-8') as f:
            f.write("\n".join(log_lines) + "\n")
        print(f"Log saved to {rl}")
    except Exception:
        pass

    return 0


def get_output_dir(output_dir=None):
    """Resolve output directory.

    - If output_dir is None: create test_outputs/<timestamp>/
    - If output_dir is provided: use it directly (no timestamp subfolder)
    """
    if output_dir is None:
        base_dir = Path("test_outputs")
        base_dir.mkdir(exist_ok=True)
        timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        run_dir = base_dir / timestamp
        run_dir.mkdir(exist_ok=True)
        return run_dir
    else:
        run_dir = Path(output_dir)
        run_dir.mkdir(parents=True, exist_ok=True)
        return run_dir


def summarize_disk_analysis(surface_map, output_dir, args):
    """Generate LLM-powered summary of disk analysis results."""
    try:
        import openai

        host_port = args.lm_host if ':' in args.lm_host else f"{args.lm_host}:1234"
        client = openai.OpenAI(
            base_url=f"http://{host_port}/v1",
            api_key="lm-studio"
        )

        # Extract key metrics for summary
        summary_data = {
            'num_tracks': len([k for k in surface_map.keys() if k != 'global']),
            'effective_rpm': surface_map['global'].get('effective_rpm'),
            'media_type': surface_map['global'].get('media_type'),
        }

        # Calculate overall statistics
        all_densities = []
        all_variances = []
        for track_key, track_data in surface_map.items():
            if track_key == 'global':
                continue
            for side_key, side_data in track_data.items():
                analysis = side_data.get('analysis', {})
                density = analysis.get('density_estimate_bits_per_rev')
                variance = analysis.get('noise_profile', {}).get('avg_variance')
                if density is not None:
                    all_densities.append(density)
                if variance is not None:
                    all_variances.append(variance)

        if all_densities:
            summary_data['density_stats'] = {
                'count': len(all_densities),
                'min': float(np.min(all_densities)),
                'max': float(np.max(all_densities)),
                'avg': float(np.mean(all_densities)),
                'median': float(np.median(all_densities))
            }

        if all_variances:
            summary_data['variance_stats'] = {
                'count': len(all_variances),
                'min': float(np.min(all_variances)),
                'max': float(np.max(all_variances)),
                'avg': float(np.mean(all_variances)),
                'median': float(np.median(all_variances))
            }

        # Create LLM prompt
        schema_description = (
            "Respond ONLY with a JSON object matching this schema: {\n"
            "  summary_stats: { num_tracks: number, effective_rpm: number, media_type: string|null },\n"
            "  density_stats: { count:number, min:number, max:number, avg:number, median:number }|null,\n"
            "  variance_stats: { count:number, min:number, max:number, avg:number, median:number }|null,\n"
            "  analysis_summary: string\n"
            "}. Provide a concise analysis summary in natural language. No extra keys, no text outside JSON."
        )

        system_msg = (
            "You are an expert in floppy disk magnetic flux analysis."
            " Output strictly valid JSON per the given schema."
            " Do NOT include any extra text, code fences, explanations, or hidden reasoning."
        )

        payload = summary_data
        user_prompt = (
            "JSON schema requirements:\n" + schema_description + "\n\n"
            "Data:\n" + json.dumps(payload, indent=2)
        )

        response = client.chat.completions.create(
            model=args.lm_model,
            messages=[
                {"role": "system", "content": system_msg},
                {"role": "user", "content": user_prompt},
            ],
            max_tokens=400,
            temperature=getattr(args, 'lm_temperature', 0.2),
        )

        content = response.choices[0].message.content or ""
        parsed = json.loads(content)

        # Save summary
        summary_json_path = output_dir / 'llm_disk_summary.json'
        dump_json(summary_json_path, parsed)

        summary_txt_path = output_dir / 'llm_disk_summary.txt'
        with open(summary_txt_path, 'w') as f:
            f.write(f"FloppyAI Disk Analysis Summary - Generated on {datetime.datetime.now().isoformat()}\n\n")
            f.write(parsed.get('analysis_summary', ''))

        print(f"LLM summary saved to {summary_txt_path}")

    except Exception as e:
        print(f"LLM summary failed: {e}")
        # Fallback to basic summary
        basic_summary = {
            'summary_stats': summary_data,
            'analysis_summary': f"Analysis completed for disk with {summary_data.get('num_tracks', 0)} tracks."
        }
        summary_json_path = output_dir / 'llm_disk_summary.json'
        dump_json(summary_json_path, basic_summary)


if __name__ == "__main__":
    import sys
    print("This module is meant to be imported, not run directly.")
    sys.exit(1)
