import struct
import numpy as np
from pathlib import Path

class CustomEncoder:
    """
    Prototype for custom flux encoding to achieve higher data density.
    Encodes binary data to flux intervals using variable-length Manchester-like coding.
    For density >1.0, shortens cell lengths (riskier readability).
    """
    def __init__(self, nominal_cell_ns=4000, density=1.0, variable_mode=False):
        self.nominal_cell_ns = nominal_cell_ns  # Standard MFM cell ~4µs
        self.density = density  # >1.0 for higher density (shorter cells)
        self.variable_mode = variable_mode  # True for RLL-like variable lengths
        self.base_cell_ns = nominal_cell_ns / density
        if variable_mode:
            self.short_cell = self.base_cell_ns * 0.8  # Short for 0s
            self.long_cell = self.base_cell_ns * 1.2   # Long for 1s
            self.half_short = self.short_cell / 2
            self.half_long = self.long_cell / 2
        else:
            self.cell_ns = self.base_cell_ns
            self.half_cell = self.cell_ns / 2

    def encode_data(self, data_bytes, num_revs=1):
        """
        Encode bytes to flux intervals using Manchester or variable RLL-like.
        Fills full revolutions by repeating data if necessary.
        
        Args:
            data_bytes (bytes): Input data
            num_revs (int): Number of revolutions
        
        Returns:
            list: Flux intervals (ns) for all revs
        """
        if len(data_bytes) == 0:
            raise ValueError("No data to encode")
        
        bits = np.unpackbits(np.frombuffer(data_bytes, dtype=np.uint8))
        total_bits = len(bits)
        
        # Calculate bits per revolution
        rev_time_ns = 166666667  # ~167ms at 360 RPM
        bits_per_rev = int(rev_time_ns / self.base_cell_ns)
        total_target_bits = bits_per_rev * num_revs
        
        # Repeat data bits to fill (or truncate if too long)
        if total_bits < total_target_bits:
            repeat_factor = (total_target_bits + total_bits - 1) // total_bits
            bits = np.tile(bits, repeat_factor)[:total_target_bits]
        else:
            bits = bits[:total_target_bits]
        
        encoded_bits = len(bits)
        
        flux_per_rev = []
        for _ in range(num_revs):
            rev_bits = bits[(_ * bits_per_rev):((_+1) * bits_per_rev)]
            rev_flux = []
            for bit in rev_bits:
                if self.variable_mode:
                    # RLL-like: 0 -> two short halves (transition mid-short), 1 -> long full
                    if bit == 0:
                        rev_flux.extend([int(self.half_short), int(self.half_short)])
                    else:
                        rev_flux.append(int(self.long_cell))
                else:
                    # Standard Manchester
                    if bit == 0:
                        rev_flux.extend([int(self.half_cell), int(self.half_cell)])
                    else:
                        rev_flux.append(int(self.cell_ns))
            
            # Ensure sum ≈ rev_time_ns (minor adjustment if needed)
            current_sum = sum(rev_flux)
            if current_sum < rev_time_ns:
                # Fill with clocking 0s (half, half) to avoid large gap
                half = int(self.base_cell_ns / 2)
                padding_intervals = int((rev_time_ns - current_sum) / half)
                rev_flux.extend([half] * (padding_intervals % 2))  # Even number for pairs
                rev_flux.extend([half, half] * (padding_intervals // 2))
            
            flux_per_rev.extend(rev_flux)
        
        # Add 5% noise simulation
        for i in range(len(flux_per_rev)):
            flux_per_rev[i] += np.random.normal(0, self.base_cell_ns * 0.05)
            flux_per_rev[i] = max(500, int(flux_per_rev[i]))
        
        # Store encoded bits for density calc
        self._encoded_bits = encoded_bits
        self._num_revs = num_revs
        
        return flux_per_rev

    def generate_raw(self, flux_intervals, track, side, output_path, version='3.00s', num_revs=1):
        """
        Generate .raw file from flux intervals.
        
        Args:
            flux_intervals (list): Flux ns for all revs
            track (int): Track
            side (int): Side
            output_path (str): Output .raw
            version (str): DTC version
            num_revs (int): Number of revolutions
        """
        # Header: ASCII metadata (null-terminated)
        metadata = f"host_date=2025.09.19, host_time=08:00:00, hc=0, name=KryoFlux DiskSystem, version={version}, date=Sep 19 2025, time=08:00:00, hwvid=1, hwrv=1, hs=0, sck=24027428.5714285, ick=3003428.57142857, track={track}, side={side}\x00"
        header = metadata.encode('ascii')
        
        # Flux data with index pulses after each rev
        full_data = header
        intervals_per_rev = len(flux_intervals) // num_revs
        pos = 0
        for rev in range(num_revs):
            rev_intervals = flux_intervals[pos:pos + intervals_per_rev]
            for interval in rev_intervals:
                full_data += struct.pack('<I', int(interval))
            full_data += struct.pack('<I', 0xFFFFFFFF)  # Index pulse
            pos += intervals_per_rev
        
        with open(output_path, 'wb') as f:
            f.write(full_data)
        print(f"Generated .raw with {len(flux_intervals)} intervals ({num_revs} revs) to {output_path}")

    def calculate_density(self, original_data_bytes):
        """
        Calculate achieved density in bits per revolution.
        
        Args:
            original_data_bytes (bytes): Original input data size
            
        Returns:
            float: Bits per revolution
        """
        if not hasattr(self, '_encoded_bits') or self._num_revs == 0:
            return 0.0
        total_bits = len(original_data_bytes) * 8
        return total_bits / self._num_revs  # Effective bits/rev (repeated to fill)

# Example
if __name__ == "__main__":
    encoder = CustomEncoder(density=2.0, variable_mode=True)
    data = b'Test data for dense flux encoding'
    flux = encoder.encode_data(data, num_revs=1)
    encoder.generate_raw(flux, 0, 0, 'test_dense.raw', num_revs=1)
    density = encoder.calculate_density(data)
    print(f"Achieved density: {density:.1f} bits/rev")