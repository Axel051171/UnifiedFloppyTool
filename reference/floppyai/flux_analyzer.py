import struct
import numpy as np
import matplotlib.pyplot as plt
import os

class FluxAnalyzer:
    """
    Tool for parsing and analyzing KryoFlux .raw stream files.
    Extracts flux transitions, computes statistics, detects anomalies,
    and visualizes patterns to understand media characteristics.
    """

    def __init__(self):
        self.flux_data = None  # np.array of flux intervals in ns
        self.index_positions = []  # List of index pulse positions
        self.stats = {}
        self.revolutions = []

    def parse(self, raw_path):
        """
        Parse a stream file supporting multiple layouts:
          1) Internal 'FLUX' format (magic + counts + intervals)
          2) Legacy ASCII header (null-terminated), then 32-bit LE ns intervals with 0xFFFFFFFF index
          3) Generic 32-bit LE intervals with 0xFFFFFFFF index (no header)

        Note: True KryoFlux C2/OOB streams are not fully supported yet. If detected,
        this function will raise a helpful error until full support is implemented.
        """
        if not os.path.exists(raw_path):
            raise FileNotFoundError(f"Stream file not found: {raw_path}")

        with open(raw_path, 'rb') as f:
            data = f.read()

        self.revolutions = []
        self.index_positions = []
        header_text = ''

        # Case 1: Internal 'FLUX' format
        if data.startswith(b'FLUX') and len(data) >= 12:
            magic = data[:4]
            (count,) = struct.unpack('<I', data[4:8])
            (revs,) = struct.unpack('<I', data[8:12])
            offset = 12
            need = 12 + 4 * count
            if len(data) < need:
                raise ValueError(f"Corrupt FLUX stream: expected {count} intervals, file too short")
            intervals = list(struct.unpack('<%dI' % count, data[offset:offset + 4 * count]))
            # No explicit index markers in FLUX format; split evenly if possible
            if revs > 0:
                per = len(intervals) // revs
                pos = 0
                for _ in range(revs):
                    rev = intervals[pos:pos + per]
                    if rev:
                        self.revolutions.append(np.array(rev, dtype=np.uint32))
                    pos += per
            flux_list = intervals
            self.flux_data = np.array(flux_list, dtype=np.uint32)
            header_text = 'FLUX internal format'
        else:
            # Peek the first few KB for signatures
            peek = data[:4096]
            is_c2_candidate = False
            if b'KryoFlux' in peek:
                is_c2_candidate = True
            else:
                # Consider as C2 if any plausible OOB header appears in the first 1MB
                n_all = len(data)
                scan_max = min(n_all - 4, 1 << 20)
                i = 0
                while i < scan_max:
                    if data[i] == 13 and i + 3 < n_all:
                        t = data[i + 1]
                        if t in (1, 2, 3, 4):
                            sz = data[i + 2] | (data[i + 3] << 8)
                            if 0 <= sz <= n_all - (i + 4):
                                is_c2_candidate = True
                                break
                    i += 1

            if is_c2_candidate:
                # Decode KryoFlux C2/OOB stream per C2Comm.h
                c2eOOB = 13
                c2eOverflow16 = 11
                c2eValue16 = 12
                # NOPs skip bytes in the buffer (ring buffer alignment), we ignore payload
                c2eNop1 = 8
                c2eNop2 = 9
                c2eNop3 = 10

                # OOB types
                c2otIndex = 2
                c2otStreamEnd = 3
                c2otInfo = 4

                # Default sck (sample clock) in Hz; refined if found in info text
                sck_hz = 24027428.5714285
                # Try to extract sck from peek text (if present like sck=xxxxx)
                try:
                    txt = peek.decode('ascii', errors='ignore')
                    if 'sck=' in txt:
                        part = txt.split('sck=')[1].split(',')[0].strip()
                        sck_hz = float(part)
                except Exception:
                    pass

                flux_list = []
                current_rev = []
                self.index_positions = []
                # Try to synchronize to the first valid OOB header to skip any ASCII preamble
                ptr = 0
                n = len(data)
                sync_found = False
                scan_limit = min(n - 4, 1 << 20)
                for i in range(0, max(0, scan_limit)):
                    b0 = data[i]
                    if b0 != c2eOOB:
                        continue
                    if i + 3 >= n:
                        break
                    typ = data[i + 1]
                    if typ not in (1, 2, 3, 4):
                        continue
                    sz = data[i + 2] | (data[i + 3] << 8)
                    if 0 <= sz <= n - (i + 4):
                        ptr = i
                        sync_found = True
                        break
                # If not found, start at 0 (best effort)
                overflow = 0
                oob_index_count = 0
                total_samples = 0

                def ticks_to_ns(ticks: int) -> int:
                    return max(1, int(round((ticks * 1e9) / sck_hz)))

                while ptr < n:
                    b = data[ptr]
                    ptr += 1
                    if b == c2eOOB:
                        # OOB header: sign(=13) already read, next: type(1), size(2, LE), then data
                        if ptr + 3 > n:
                            break
                        oob_type = data[ptr]
                        oob_size = data[ptr+1] | (data[ptr+2] << 8)
                        ptr += 3
                        oob_payload = data[ptr:ptr+oob_size]
                        ptr += oob_size
                        if oob_type == c2otIndex:
                            # Revolution boundary
                            if current_rev:
                                self.revolutions.append(np.array(current_rev, dtype=np.uint32))
                                flux_list.extend(current_rev)
                                current_rev = []
                            oob_index_count += 1
                        elif oob_type == c2otStreamEnd:
                            # End of stream
                            break
                        elif oob_type == c2otInfo:
                            # Contains ASCII like 'KryoFlux', optional timing info
                            try:
                                info_txt = bytes(oob_payload).decode('ascii', errors='ignore')
                                header_text += info_txt
                                if 'sck=' in info_txt:
                                    part = info_txt.split('sck=')[1].split(',')[0].strip()
                                    sck_hz = float(part)
                            except Exception:
                                pass
                        continue
                    elif b == c2eOverflow16:
                        overflow += 65536
                        continue
                    elif b == c2eValue16:
                        if ptr + 2 > n:
                            break
                        sample = (data[ptr] << 8) | data[ptr+1]
                        ptr += 2
                        sample += overflow
                        overflow = 0
                        current_rev.append(ticks_to_ns(sample))
                        total_samples += 1
                        continue
                    elif b == c2eNop1:
                        # Skip 1 byte
                        ptr = min(n, ptr + 1)
                        continue
                    elif b == c2eNop2:
                        ptr = min(n, ptr + 2)
                        continue
                    elif b == c2eNop3:
                        ptr = min(n, ptr + 3)
                        continue
                    elif b == 0x00:
                        # Small sample encoding: 00, sample(0..0x0d)
                        if ptr < n:
                            sample = data[ptr]
                            ptr += 1
                            sample += overflow
                            overflow = 0
                            current_rev.append(ticks_to_ns(sample))
                            total_samples += 1
                        continue
                    else:
                        # Single-byte sample for 0x0e..0xff
                        if b >= 0x0E:
                            sample = b + overflow
                            overflow = 0
                            current_rev.append(ticks_to_ns(sample))
                            total_samples += 1
                            continue
                        # Unknown control, ignore
                        continue

                if current_rev:
                    self.revolutions.append(np.array(current_rev, dtype=np.uint32))
                    flux_list.extend(current_rev)
                self.flux_data = np.array(flux_list, dtype=np.uint32)
                # Store decoder details for downstream diagnostics
                self._decoder_sck_hz = sck_hz
                self._decoder_oob_index_count = oob_index_count
                self._decoder_total_samples = total_samples

                # Fallback: if C2 decode yielded no samples, try legacy 32-bit LE parse
                if self.flux_data.size == 0:
                    # Case 2: Legacy ASCII header with null terminator
                    pos0 = data.find(b'\x00')
                    if 0 <= pos0 < 2048:
                        header_text = data[:pos0].decode('ascii', errors='ignore')
                        offset = pos0 + 1
                    else:
                        # Case 3: No header; treat entire file as 32-bit LE intervals
                        offset = 0
                    flux_list = []
                    current_rev = []
                    ptr2 = offset
                    while ptr2 + 4 <= len(data):
                        (val,) = struct.unpack('<I', data[ptr2:ptr2 + 4])
                        ptr2 += 4
                        if val == 0xFFFFFFFF:
                            self.index_positions.append(ptr2 - 4)
                            if current_rev:
                                self.revolutions.append(np.array(current_rev, dtype=np.uint32))
                                flux_list.extend(current_rev)
                                current_rev = []
                            continue
                        if 50 <= val <= 20000000:
                            current_rev.append(val)
                    if current_rev:
                        self.revolutions.append(np.array(current_rev, dtype=np.uint32))
                        flux_list.extend(current_rev)
                    self.flux_data = np.array(flux_list, dtype=np.uint32)
            else:
                # Case 2: Legacy ASCII header with null terminator
                pos0 = data.find(b'\x00')
                if 0 <= pos0 < 2048:
                    header_text = data[:pos0].decode('ascii', errors='ignore')
                    offset = pos0 + 1
                else:
                    # Case 3: No header; treat entire file as 32-bit LE intervals
                    offset = 0
                # Parse 32-bit LE intervals with 0xFFFFFFFF index markers
                flux_list = []
                current_rev = []
                ptr = offset
                while ptr + 4 <= len(data):
                    (val,) = struct.unpack('<I', data[ptr:ptr + 4])
                    ptr += 4
                    if val == 0xFFFFFFFF:
                        self.index_positions.append(ptr - 4)
                        if current_rev:
                            self.revolutions.append(np.array(current_rev, dtype=np.uint32))
                            flux_list.extend(current_rev)
                            current_rev = []
                        continue
                    # Treat as ns interval if in plausible bounds
                    if 50 <= val <= 20000000:
                        current_rev.append(val)
                if current_rev:
                    self.revolutions.append(np.array(current_rev, dtype=np.uint32))
                    flux_list.extend(current_rev)
                self.flux_data = np.array(flux_list, dtype=np.uint32)

        # Compute basic stats
        if len(self.flux_data) > 0:
            total_time_ns = np.sum(self.flux_data)
            rev_time_ns = total_time_ns / max(1, len(self.revolutions))
            self.stats = {
                'total_fluxes': len(self.flux_data),
                'mean_interval_ns': float(np.mean(self.flux_data)),
                'std_interval_ns': float(np.std(self.flux_data)),
                'min_interval_ns': int(np.min(self.flux_data)),
                'max_interval_ns': int(np.max(self.flux_data)),
                'total_measured_time_ns': float(total_time_ns),
                'measured_rev_time_ns': float(rev_time_ns),
                'num_revolutions': len(self.revolutions),
                'inferred_rpm': 60000000000 / rev_time_ns if rev_time_ns > 0 else 0,
            }
            # Attach decoder diagnostics if available
            if hasattr(self, '_decoder_sck_hz'):
                self.stats['decoder_sck_hz'] = float(self._decoder_sck_hz)
            if hasattr(self, '_decoder_oob_index_count'):
                self.stats['decoder_oob_index_count'] = int(self._decoder_oob_index_count)
            if hasattr(self, '_decoder_total_samples'):
                self.stats['decoder_total_samples'] = int(self._decoder_total_samples)
        else:
            self.stats = {}

        version = 'unknown'
        if 'version=' in header_text:
            try:
                version = header_text.split('version=')[1].split(',')[0]
            except Exception:
                version = 'unknown'
        flags = 0

        return {
            'fluxes': self.flux_data,
            'index_positions': self.index_positions,
            'revolutions': self.revolutions,
            'stats': self.stats,
            'version': version,
            'flags': flags,
            'header_text': header_text,
        }

    def analyze(
        self,
        angular_bins: int | None = None,
        interval_hist_bins: int | None = 96,
        interval_hist_min_ns: float = 150.0,
        interval_hist_max_ns: float = 60000.0,
    ):
        """
        Analyze flux data for noise, anomalies, weak bits.
        
        Returns:
            dict: Analysis results including noise profile, anomalies
        """
        if self.flux_data is None:
            raise ValueError("Parse a file first with parse()")
        
        if not self.stats:
            return {'anomalies': {}, 'noise_profile': {}, 'density_estimate_bits_per_rev': 0}
        
        mean = self.stats.get('mean_interval_ns', 0)
        std = self.stats.get('std_interval_ns', 0)
        
        # Detect anomalies: intervals >3*std (potential dropouts), <0.5*mean (short cells)
        # For blanks, expect ~2000-4000ns cells; adjust thresholds
        base_cell = 4000  # Typical MFM cell ns
        anomalies = {
            'short_cells': np.where(self.flux_data < 0.5 * mean)[0] if mean > 0 else [],
            'long_intervals': np.where(self.flux_data > mean + 3 * std)[0] if std > 0 else [],
            'weak_bit_candidates': np.where((self.flux_data > base_cell * 1.5) & (self.flux_data < base_cell * 2))[0]  # Potential weak (variable length)
        }
        
        # Simple noise profile: Variance per revolution
        rev_variances = [np.var(rev) for rev in self.revolutions if len(rev) > 0]
        rev_times = [np.sum(rev) for rev in self.revolutions if len(rev) > 0]
        rpm_stability = np.std(rev_times) / np.mean(rev_times) if rev_times else 0  # Relative std of rev times
        noise_profile = {
            'avg_variance': np.mean(rev_variances) if rev_variances else 0,
            'high_noise_revs': [i for i, v in enumerate(rev_variances) if v > np.mean(rev_variances) + std] if rev_variances else [],
            'rpm_stability': rpm_stability
        }
        
        # For weak bits: Compare revolutions (inconsistent timings indicate weak)
        if len(self.revolutions) > 1 and std > 0:
            inconsistencies = []
            for i in range(1, len(self.revolutions)):
                # Simple diff on aligned fluxes (assume same length for demo)
                if len(self.revolutions[0]) == len(self.revolutions[i]):
                    diff = np.abs(self.revolutions[0] - self.revolutions[i])
                    inconsistencies.append(np.where(diff > 2 * std)[0])
            anomalies['weak_bit_candidates'] = inconsistencies
        
        # Density estimate: use average transitions per revolution as a stable proxy
        trans_per_rev = [len(rev) for rev in self.revolutions if isinstance(rev, np.ndarray) and len(rev) > 0]
        dens_est = int(float(np.mean(trans_per_rev))) if trans_per_rev else 0

        # Optional angular histogram over revolutions (with phase alignment)
        angular_hist = None
        per_rev_hists = []        # normalized per-rev histograms (pre-align)
        per_rev_hists_al = []     # aligned per-rev histograms
        per_rev_shifts_bins = []  # circular shift applied to align each rev (bins)
        per_angle_variance = None # variance across aligned revs per angle
        instability_theta = None  # normalized angular-resolved instability profile (0..1)
        if angular_bins is None:
            angular_bins = 0
        try:
            bins = int(angular_bins)
        except Exception:
            bins = 0
        if bins and bins > 0 and self.revolutions:
            # Build per-rev histograms
            for rev in self.revolutions:
                if not isinstance(rev, np.ndarray) or rev.size < 4:
                    continue
                t = np.cumsum(rev.astype(np.float64))
                total = t[-1]
                if total <= 0:
                    continue
                idx = np.floor((t / total) * bins).astype(int)
                idx[idx >= bins] = bins - 1
                h = np.bincount(idx, minlength=bins).astype(np.float64)
                hm = float(np.max(h))
                per_rev_hists.append(h / hm if hm > 0 else h)

            # Phase alignment by circular cross-correlation to reference (mean of first few revs)
            if len(per_rev_hists) > 0:
                H = np.stack(per_rev_hists, axis=0)  # [R, bins]
                ref = np.mean(H[: min(4, H.shape[0])], axis=0)
                # Compute FFT of reference once
                RF = np.fft.rfft(ref)
                for row in H:
                    # circular cross-correlation via FFT
                    F = np.fft.rfft(row)
                    cc = np.fft.irfft(F * np.conj(RF))
                    shift = int(np.argmax(cc))  # bins to roll so peak aligns
                    per_rev_shifts_bins.append(shift)
                    per_rev_hists_al.append(np.roll(row, -shift))
                H_al = np.stack(per_rev_hists_al, axis=0) if per_rev_hists_al else H
                # Angular histogram from aligned stack
                hist_sum_al = np.sum(H_al, axis=0)
                max_sum = float(np.max(hist_sum_al)) if H_al.size else 0.0
                if max_sum > 0:
                    angular_hist = (hist_sum_al / max_sum).tolist()
                else:
                    angular_hist = (np.mean(H, axis=0)).tolist()
                # Per-angle variance across aligned revs
                var_bins = np.var(H_al, axis=0) if H_al.shape[0] >= 2 else np.zeros(bins, dtype=float)
                # Normalize variance to 0..1 for rendering
                vmax = float(np.max(var_bins)) if var_bins.size else 0.0
                if vmax > 0:
                    per_angle_variance = (var_bins / vmax).tolist()
                    instability_theta = per_angle_variance
                else:
                    per_angle_variance = [0.0] * bins
                    instability_theta = per_angle_variance

        # Optional interval histogram (log-spaced over [min_ns, max_ns])
        interval_hist = None
        ih_bins = 0
        ih_range = None
        try:
            if interval_hist_bins and self.flux_data is not None and self.flux_data.size > 0:
                vals = self.flux_data.astype(np.float64)
                mn = max(float(interval_hist_min_ns), 1.0)
                mx = max(float(interval_hist_max_ns), mn + 1.0)
                ih_bins = int(interval_hist_bins)
                # Log-space edges
                edges = np.logspace(np.log10(mn), np.log10(mx), ih_bins + 1)
                h, _ = np.histogram(vals, bins=edges)
                if np.max(h) > 0:
                    h = h.astype(np.float64) / float(np.max(h))
                interval_hist = h.tolist()
                ih_range = [mn, mx]
        except Exception:
            pass

        # Instability v2 (flux-level)
        # - phase variance across revolutions (using per_rev_hists)
        # - cross-rev coherence (mean correlation to mean profile)
        # - outlier rate (short/long intervals)
        # - gap rate (very long intervals)
        phase_var_p95 = None
        phase_incoherence = None
        outlier_rate = 0.0
        gap_rate = 0.0

        # Outlier/gap rates
        vals = self.flux_data.astype(np.float64) if self.flux_data is not None else np.array([], dtype=np.float64)
        n_vals = float(vals.size)
        if n_vals > 0:
            short_thr = 0.5 * mean if mean > 0 else 0
            long_thr = mean + 3 * std if std > 0 else 0
            gap_thr = max(mean + 4 * std, 2.5 * mean) if mean > 0 else 0
            short_rate = float(np.mean(vals < short_thr)) if short_thr > 0 else 0.0
            long_rate = float(np.mean(vals > long_thr)) if long_thr > 0 else 0.0
            outlier_rate = 0.5 * (short_rate + long_rate)
            gap_rate = float(np.mean(vals > gap_thr)) if gap_thr > 0 else 0.0

        # Phase variance and cross-rev coherence (use aligned when available)
        if len(per_rev_hists) >= 2:
            H = np.stack(per_rev_hists_al if per_rev_hists_al else per_rev_hists, axis=0)
            # Variance per angular bin across revolutions
            var_bins = np.var(H, axis=0)
            try:
                phase_var_p95 = float(np.percentile(var_bins, 95))
            except Exception:
                phase_var_p95 = float(np.max(var_bins)) if var_bins.size else 0.0
            # Correlation of each rev to the mean profile
            mu = np.mean(H, axis=0)
            def _corr(a, b):
                sa = np.std(a); sb = np.std(b)
                if sa <= 1e-12 or sb <= 1e-12:
                    return 1.0
                return float(np.corrcoef(a, b)[0, 1])
            corrs = [max(-1.0, min(1.0, _corr(row, mu))) for row in H]
            mean_corr = float(np.mean(corrs)) if len(corrs) > 0 else 1.0
            # Invert to an instability-like value, clipped to 0..1
            phase_incoherence = float(1.0 - np.clip(mean_corr, 0.0, 1.0))
        else:
            phase_var_p95 = 0.0
            phase_incoherence = 0.0

        # Combine into a 0..1 score (heuristic weights)
        w_var = 0.4; w_incoh = 0.3; w_gap = 0.2; w_out = 0.1
        instability_score = float(
            np.clip(
                w_var * (phase_var_p95 or 0.0)
                + w_incoh * (phase_incoherence or 0.0)
                + w_gap * (gap_rate or 0.0)
                + w_out * (outlier_rate or 0.0),
                0.0, 1.0
            )
        )

        return {
            'anomalies': anomalies,
            'noise_profile': noise_profile,
            'density_estimate_bits_per_rev': dens_est,
            'angular_bins': (bins if bins and bins > 0 else None),
            'angular_hist': angular_hist,
            'per_angle_variance': per_angle_variance,
            'per_rev_shifts_bins': per_rev_shifts_bins if per_rev_shifts_bins else None,
            'instability_theta': instability_theta,
            'interval_hist_bins': (ih_bins if ih_bins > 0 else None),
            'interval_hist_range_ns': ih_range,
            'interval_hist': interval_hist,
            'instability_features': {
                'phase_var_p95': phase_var_p95,
                'phase_incoherence': phase_incoherence,
                'outlier_rate': outlier_rate,
                'gap_rate': gap_rate,
            },
            'instability_score': instability_score,
        }

    def visualize(self, base_path, plot_type='intervals'):
        """
        Visualize flux data.
        
        Args:
            base_path (str): Base path for saving plots (appends type.png)
            plot_type (str): 'intervals' for time series, 'histogram' for distribution,
                             'heatmap' for multi-rev density
        """
        if self.flux_data is None:
            raise ValueError("Parse a file first with parse()")
        
        plt.figure(figsize=(12, 8))
        
        if plot_type == 'intervals':
            mean_int = self.stats.get('mean_interval_ns', 0)
            std_int = self.stats.get('std_interval_ns', 0)
            if len(self.flux_data) > 0:
                plt.plot(self.flux_data, label='Flux Intervals (ns)')
            plt.axhline(mean_int, color='r', linestyle='--', label='Mean')
            plt.axhline(mean_int + 3 * std_int, color='orange', linestyle='--', label='Anomaly Threshold')
            plt.xlabel('Flux Position')
            plt.ylabel('Interval Length (ns)')
            plt.title('Flux Transition Intervals')
            plt.legend()
            output_path = base_path + '_intervals.png'
        
        elif plot_type == 'histogram':
            mean_int = self.stats.get('mean_interval_ns', 0)
            if len(self.flux_data) > 0:
                plt.hist(self.flux_data, bins=50, alpha=0.7, label='Intervals')
            plt.axvline(mean_int, color='r', linestyle='--', label='Mean')
            plt.xlabel('Interval Length (ns)')
            plt.ylabel('Frequency')
            plt.title('Distribution of Flux Intervals')
            plt.legend()
            output_path = base_path + '_hist.png'
        
        elif plot_type == 'heatmap' and len(self.revolutions) > 1:
            # Heatmap of variances across revolutions
            rev_means = [np.mean(rev) for rev in self.revolutions]
            plt.imshow([rev_means], cmap='hot', aspect='auto')
            plt.colorbar(label='Mean Interval (ns)')
            plt.xlabel('Revolution')
            plt.ylabel('Track Position')
            plt.title('Revolution Consistency Heatmap')
            output_path = base_path + '_heatmap.png'
        else:
            output_path = base_path + f'_{plot_type}.png'
        
        plt.savefig(output_path)
        plt.close()
        print(f"Visualization saved to {output_path}")

# Example usage (for testing)
if __name__ == "__main__":
    analyzer = FluxAnalyzer()
    parsed = analyzer.parse("../../example_stream_data/unknown-stream00.0.raw")
    print("Parsed Stats:", parsed['stats'])
    analysis = analyzer.analyze()
    print("Analysis:", analysis)
    analyzer.visualize("test_flux_plot.png", "intervals")