# DTC-derived analysis components (independent C reconstruction)

Portable C11 building blocks reconstructed from observable ARM64 behavior and
public disk-format conventions. This is not the original DTC source code and
is not a drop-in replacement for KryoFlux/SPS DTC.

Implemented: bit buffers, FM/MFM, configurable 4-to-N GCR tables, Commodore,
Apple 6-and-2, Vorpal and V-Max tables, CRC-32/CCITT/IBM, flux statistics and
cell quantisation, basic disk-image detection, IBM MFM and Amiga sync scanning,
weak-bit comparison, shifted track matching, basic protection heuristics, an
independent documented CT-raw-like container, and IPF chunk inspection.

Not claimed: SPS-compatible CT Raw compression, complete IPF encode/decode,
or DTC's proprietary format/protection database. Those require public format
specifications, licensed source, or authoritative test vectors. The APIs here
fail or stay deliberately narrow instead of inventing compatibility.

Build:

    make test

or:

    cmake -S . -B build
    cmake --build build
    ctest --test-dir build --output-on-failure

Important: `DTCCTRW` is this package's independent interchange format. It is
not asserted to be compatible with SPS/KryoFlux CT Raw.
