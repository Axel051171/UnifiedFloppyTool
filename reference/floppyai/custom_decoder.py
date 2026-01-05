import struct
import numpy as np
from pathlib import Path
import os
from flux_analyzer import FluxAnalyzer
from sklearn.cluster import KMeans

class CustomDecoder:
    """
    Prototype decoder for custom flux streams.
    Reverses Manchester or RLL-like encoding to binary data, with noise tolerance.
    Assumes same parameters as used in encoding.
    """
    def __init__(self, nominal_cell_ns=4000, density=1.0, variable_mode=False, tolerance=0.1, rpm=360):
        self.nominal_cell_ns = nominal_cell_ns
        self.density = density
        self.variable_mode = variable_mode
        self.tolerance = tolerance  # ±10% for noise
        self.rpm = rpm
        self.expected_rev_ns = 60000000000 / rpm if rpm else None
        self.base_cell_ns = nominal_cell_ns / density
        self.half_cell = self.base_cell_ns / 2
        if variable_mode:
            self.short_cell = self.base_cell_ns * 0.8
            self.long_cell = self.base_cell_ns * 1.2
            self.half_short = self.short_cell / 2
        else:
            self.cell_ns = self.base_cell_ns
            self.full_cell = self.cell_ns

    def set_dynamic_thresholds(self, analyzer):
        """
        Use analyzer stats for adaptive thresholds (mean ± std for robustness).
        Args:
            analyzer: FluxAnalyzer instance with parsed data
        """
        if analyzer.flux_data is None:
            raise ValueError("Analyzer must be parsed first")
        mean = analyzer.stats.get('mean_interval_ns', self.base_cell_ns)
        std = analyzer.stats.get('std_interval_ns', mean * 0.1)
        tol = std * 2  # Wider for real noise
        self.base_cell_ns = mean
        self.half_cell = mean / 2
        self.full_cell = mean
        if self.variable_mode:
            self.short_cell = mean * 0.8
            self.long_cell = mean * 1.2
            self.half_short = self.short_cell / 2
        self.tolerance = tol / mean  # Relative tol

    def _classify_interval(self, interval):
        """
        Classify flux interval to type: half, full, short, long.
        Returns: ('half', value), ('full', value), etc.
        """
        half_min = self.half_cell * (1 - self.tolerance)
        half_max = self.half_cell * (1 + self.tolerance)
        full_min = self.full_cell * (1 - self.tolerance)
        full_max = self.full_cell * (1 + self.tolerance)
        if self.variable_mode:
            short_min = self.short_cell * (1 - self.tolerance)
            short_max = self.short_cell * (1 + self.tolerance)
            long_min = self.long_cell * (1 - self.tolerance)
            long_max = self.long_cell * (1 + self.tolerance)
            half_short_min = self.half_short * (1 - self.tolerance)
            half_short_max = self.half_short * (1 + self.tolerance)
            if half_short_min <= interval <= half_short_max:
                return ('half_short', interval)
            if short_min <= interval <= short_max:
                return ('short', interval)  # Fallback, but expect pairs
            if long_min <= interval <= long_max:
                return ('long', interval)
        else:
            if half_min <= interval <= half_max:
                return ('half', interval)
            if full_min <= interval <= full_max:
                return ('full', interval)
        # Fallback: closest to half or full
        if abs(interval - self.half_cell) < abs(interval - self.full_cell):
            return ('half', interval)
        return ('full', interval)

    def decode_flux(self, flux_intervals, num_revs=1):
        """
        Decode flux intervals to bits/bytes.
        Args:
            flux_intervals (list): Flux ns from analyzer
            num_revs (int): Expected revolutions
        
        Returns:
            bytes: Decoded data
        """
        if len(flux_intervals) == 0:
            return b''
        
        bits = []
        rev_len = len(flux_intervals) // num_revs
        for rev in range(num_revs):
            start = rev * rev_len
            end = start + rev_len
            rev_flux = flux_intervals[start:end]
            i = 0
            while i < len(rev_flux):
                interval_type, value = self._classify_interval(rev_flux[i])
                if self.variable_mode:
                    if interval_type == 'long':
                        bits.append(1)
                        i += 1
                    elif interval_type == 'half_short' and i + 1 < len(rev_flux):
                        next_type, _ = self._classify_interval(rev_flux[i+1])
                        if next_type == 'half_short':
                            bits.append(0)
                            i += 2
                        else:
                            # Mismatch: assume long (1) or vote, but simple: 0
                            bits.append(0)
                            i += 1
                    else:
                        # Unknown: skip or assume 0
                        bits.append(0)
                        i += 1
                else:
                    # Manchester
                    if interval_type == 'full':
                        bits.append(1)
                        i += 1
                    elif interval_type == 'half' and i + 1 < len(rev_flux):
                        next_type, _ = self._classify_interval(rev_flux[i+1])
                        if next_type == 'half':
                            bits.append(0)
                            i += 2
                        else:
                            # Mismatch: assume full (1)
                            bits.append(1)
                            i += 1
                    else:
                        # Unknown: assume 1
                        bits.append(1)
                        i += 1
        
        # Pack bits to bytes (take first 8*num_bytes, but since repeated, extract unique if known)
        num_bytes = len(bits) // 8
        bit_array = np.array(bits[:num_bytes * 8])
        decoded_bytes = np.packbits(bit_array).tobytes()
        return decoded_bytes

    def decode_file(self, raw_path, output_path=None, num_revs=1):
        """
        Decode .raw file to .bin.
        Args:
            raw_path (str): Input .raw
            output_path (str): Output .bin (default: stem_decoded.bin)
            num_revs (int): Revolutions
        
        Returns:
            bytes: Decoded data
        """
        if output_path is None:
            base = Path(raw_path).stem
            output_path = f"{base}_decoded.bin"
        
        analyzer = FluxAnalyzer()
        parsed = analyzer.parse(raw_path)
        if not parsed['fluxes'].size:
            raise ValueError("No flux data in file")
        
        # Dynamic clustering for thresholds
        flux_data = parsed['fluxes'].copy()
        if self.expected_rev_ns:
            num_revs = len(analyzer.revolutions) if analyzer.revolutions else num_revs
            if num_revs > 0:
                total_time_ns = np.sum(flux_data)
                actual_rev_ns = total_time_ns / num_revs
                if actual_rev_ns > 0:
                    scale_factor = self.expected_rev_ns / actual_rev_ns
                    flux_data *= scale_factor
                    print(f"Normalized flux intervals for {self.rpm} RPM (scale factor: {scale_factor:.4f})")
        if len(flux_data) > 10:  # Min for clustering
            n_clusters = 3 if self.variable_mode else 2
            kmeans = KMeans(n_clusters=n_clusters, random_state=42, n_init=10)
            labels = kmeans.fit_predict(flux_data.reshape(-1, 1))
            centers = sorted(kmeans.cluster_centers_.flatten())
            if self.variable_mode:
                self.half_short = centers[0]
                self.long_cell = centers[1] if len(centers) > 1 else centers[0] * 1.5
                self.base_cell_ns = (self.half_short * 2 + self.long_cell) / 3
            else:
                self.half_cell = centers[0]
                self.full_cell = centers[1]
                self.base_cell_ns = self.full_cell
            print(f"Clustered centers: {centers} ns")
        else:
            # Fallback to init
            print("Insufficient data for clustering; using fixed thresholds")
        
        # Update tolerance based on normalized data
        std = np.std(flux_data)
        mean_interval = np.mean(flux_data)
        self.tolerance = (std / mean_interval) if mean_interval > 0 else 0.1
        print(f"Applied tolerance: {self.tolerance:.3f} (std/mean from {'normalized' if self.expected_rev_ns else 'raw'} data)")
        
        decoded = self.decode_flux(flux_data, num_revs)
        
        with open(output_path, 'wb') as f:
            f.write(decoded)
        print(f"Decoded to {output_path}: {len(decoded)} bytes ({len(decoded)*8} bits)")
        return decoded

# Example
if __name__ == "__main__":
    decoder = CustomDecoder(density=2.0, variable_mode=True)
    # Assume test_encoded.raw from encoder
    decoded = decoder.decode_file('test_encoded.raw')
    print("Decoded successfully")