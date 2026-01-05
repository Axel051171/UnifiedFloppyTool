# kristomu/flux-analyze - Advanced MFM Decoder

## Source
- **Repository**: https://github.com/kristomu/flux-analyze
- **License**: GPL-3.0
- **Language**: C++ (primary), Python (legacy)
- **Purpose**: Recovery of IBM MFM floppy data from damaged/warped disks

## Key Insight

This decoder was created because **FluxEngine fails on certain warped disks** that 
KryoFlux can read. The MS Plus disk example has "jitter/warping of the surface that 
causes clocks to vary in a consistent pattern" - marginal enough to make FluxEngine 
lose its bearings.

### Why It Matters for UFT
- Demonstrates limitations of standard PLL approaches
- Provides alternative recovery algorithms
- Excellent test cases for PLL tuning

## Technical Approach

### Multi-Revolution Fusion
```cpp
// If more than one flux file is specified, they'll be treated 
// as different images of the same floppy
// This enables confidence-based sector selection
```

### Adaptive Clock Recovery
The decoder handles:
- Consistent clock variation patterns (warped disks)
- Non-uniform bit cell timing
- Edge case synchronization issues

## File Structure
```
csrc/           # C++ version (current development)
  ├── CMakeLists.txt
  ├── flux_analyze.cpp
  ├── mfm_decoder.cpp
  └── ...
python/         # Python version (legacy)
  ├── mfm_decoder.py
  └── README.md
tracks/         # Sample flux files
  ├── low_level_format_with_noise/
  ├── MS_Plus_OK_track/
  └── MS_Plus_warped_track/
```

## Input Formats
- **C++ version**: FluxEngine .flux files
- **Python version**: FluxEngine .au dumps

## Sample Tracks

### MS_Plus_warped_track.flux
- FluxEngine: FAILS (loses sync due to consistent clock variation)
- KryoFlux DTC: SUCCESS
- flux-analyze: SUCCESS (all sectors recovered)

### low_level_format_with_noise
- Known data pattern (0x00 or 0xF6)
- Weird flux transition effects
- Useful for testing recovery algorithms

## Build Instructions
```bash
cd ~/Sources/
git clone https://github.com/kristomu/flux-analyze.git
cd flux-analyze
mkdir build && cd build
cmake -S ../csrc -B .
make
```

## Usage
```bash
# Single flux file
./flux-analyze disk.flux

# Multiple revolutions (for confidence fusion)
./flux-analyze rev1.flux rev2.flux rev3.flux
```

## Integration Priority for UFT

### Critical Algorithms
1. **Adaptive bit-cell timing** - handles consistent clock variation
2. **Multi-revolution confidence scoring** - selects best sector reads
3. **Robust synchronization** - doesn't lose lock on marginal timing

### Key Differences from Standard PLL
| Standard PLL | flux-analyze |
|-------------|--------------|
| Fixed timing tolerance | Adaptive tolerance |
| Single-pass decode | Multi-pass with comparison |
| Immediate error on sync loss | Recovery attempts |

## Code Patterns to Extract

### Bit Cell Detection
```cpp
// The decoder doesn't rely on fixed timing thresholds
// Instead it tracks the pattern of clock variations
// This is crucial for warped disks where the variation
// is consistent across the track
```

### Sector Validation
```cpp
// Multiple reads enable confidence scoring:
// - CRC match = high confidence
// - Multiple matching reads = higher confidence
// - Single failed read with good alternates = recoverable
```

## References
- FluxEngine Issue #170: IBM MFM sector can't be decoded with FluxEngine
- Author: Kristofer Munsterhjelm (Norway)
- Last active: Seeking marginal disk images for algorithm tuning
