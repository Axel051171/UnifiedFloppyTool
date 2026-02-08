/**
 * @file uft_kfstream_air_spec.h
 * @brief KryoFlux Stream Format Specification
 * @version 1.0.0
 *
 * Complete format specification for KryoFlux raw stream files (.raw).
 * KryoFlux is a USB floppy controller by SPS (Software Preservation Society).
 * Stream files capture magnetic flux transitions with nanosecond precision.
 *
 * Reference: AIR by Jean Louis-Guerin (DrCoolzic)
 * Source: KFReader.cs (868 lines)
 *
 * BYTE ORDER: Little-Endian for multi-byte values
 *
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║                 KRYOFLUX STREAM LAYOUT                          ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  Interleaved sequence of:                                       ║
 * ║    Flux Blocks    - Flux transition timing data                 ║
 * ║    OOB Blocks     - Out-of-band metadata (index, status, etc.)  ║
 * ╚══════════════════════════════════════════════════════════════════╝
 *
 * Default Clock Values:
 *   Sample Clock (sck) = 24027428.5714285 Hz  (~41.6 ns resolution)
 *   Index Clock (ick)  = sck / 8 = 3003428.57 Hz
 *
 * These may be overridden by HWInfo OOB blocks in firmware ≥ 2.0.
 */

#ifndef UFT_KFSTREAM_AIR_SPEC_H
#define UFT_KFSTREAM_AIR_SPEC_H

/*============================================================================
 * FLUX BLOCKS - Encode time between magnetic flux transitions
 *
 * Each flux value represents the number of sample clock ticks between
 * two consecutive flux transitions. Multiple overflow blocks can
 * precede a flux block to extend the range beyond 16 bits.
 *
 * Block Type    Header Byte    Size    Description
 * ----------    -----------    ----    ------------------------------------
 * Flux2         0x00-0x07      2       value = (header << 8) + next_byte
 * Nop1          0x08           1       Skip 1 byte (padding)
 * Nop2          0x09           2       Skip 2 bytes
 * Nop3          0x0A           3       Skip 3 bytes
 * Ovl16         0x0B           1       Add 0x10000 to accumulating value
 * Flux3         0x0C           3       value = (byte1 << 8) + byte2
 * OOB           0x0D           var     Out-of-band block (see below)
 * Flux1         0x0E-0xFF      1       value = header byte
 *
 * FLUX VALUE COMPUTATION:
 *   flux_value = overflow_accumulator + raw_value
 *   overflow_accumulator starts at 0, increases by 0x10000 per Ovl16 block
 *   After a Flux1/Flux2/Flux3 block, overflow resets to 0
 *
 * Example: Ovl16 + Ovl16 + Flux1(0x50)
 *   flux = 0x10000 + 0x10000 + 0x50 = 131152 sck ticks
 *
 * TIMING CONVERSION:
 *   time_us = flux_value / sck × 1e6
 *   For standard DD MFM (4µs bit cell): flux ≈ 96 sck ticks
 *   For standard DD MFM (6µs bit cell): flux ≈ 144 sck ticks
 *   For standard DD MFM (8µs bit cell): flux ≈ 192 sck ticks
 *============================================================================*/

/*============================================================================
 * OOB (OUT-OF-BAND) BLOCKS - Metadata embedded in stream
 *
 * All OOB blocks start with header byte 0x0D followed by:
 *   [1 byte]  oob_type    - OOB type identifier
 *   [2 bytes] oob_size    - Payload size (LE)
 *   [N bytes] payload     - Type-specific data
 *
 * OOB Types:
 *
 * ┌─────────────────────────────────────────────────────────────────┐
 * │ Type 0x00: INVALID                                             │
 * │   Should never appear. Indicates stream corruption.            │
 * ├─────────────────────────────────────────────────────────────────┤
 * │ Type 0x01: STREAM_INFO (8 bytes payload)                       │
 * │   Offset  Size  Field           Description                    │
 * │   0x00    4     stream_pos      Encoder stream position        │
 * │   0x04    4     transfer_time   Transfer time (sck ticks)      │
 * │                                                                │
 * │   Used to validate encoder/decoder synchronization.            │
 * │   The decoder's position should match stream_pos.              │
 * │   Transfer rate = stream_pos / transfer_time × sck             │
 * ├─────────────────────────────────────────────────────────────────┤
 * │ Type 0x02: INDEX (12 bytes payload)                            │
 * │   Offset  Size  Field           Description                    │
 * │   0x00    4     stream_pos      Stream position at index       │
 * │   0x04    4     sample_counter  Sample counter (sck ticks)     │
 * │   0x08    4     index_counter   Index counter (ick ticks)      │
 * │                                                                │
 * │   Records the position of an index pulse from the floppy drive.│
 * │   One index pulse per revolution (~200ms for 300 RPM).         │
 * │   The sample_counter and index_counter enable sub-cell timing. │
 * ├─────────────────────────────────────────────────────────────────┤
 * │ Type 0x03: STREAM_END (8 bytes payload)                        │
 * │   Offset  Size  Field           Description                    │
 * │   0x00    4     stream_pos      Final stream position          │
 * │   0x04    4     hw_status       Hardware status code            │
 * │                                                                │
 * │   Hardware Status:                                             │
 * │     0x00 = OK (successful capture)                             │
 * │     0x01 = BUFFER error (data lost due to USB overflow)        │
 * │     0x02 = INDEX error (no index pulse detected)               │
 * ├─────────────────────────────────────────────────────────────────┤
 * │ Type 0x04: HW_INFO (variable payload)                          │
 * │   Null-terminated ASCII string with hardware parameters.       │
 * │   Format: "name1=value1, name2=value2, ..."                    │
 * │                                                                │
 * │   Known keys:                                                  │
 * │     sck    - Sample clock frequency (Hz)                       │
 * │     ick    - Index clock frequency (Hz)                        │
 * │     host_date - Capture date                                   │
 * │     host_time - Capture time                                   │
 * │     hwid   - Hardware identifier string                        │
 * │     hwrv   - Hardware revision                                 │
 * │     fwrv   - Firmware revision                                 │
 * │                                                                │
 * │   Available in firmware ≥ 2.0. If sck/ick values differ from   │
 * │   defaults, all statistics must be recomputed.                 │
 * ├─────────────────────────────────────────────────────────────────┤
 * │ Type 0x0D: EOF                                                 │
 * │   End-of-file marker. Payload size = 0xFFFF (sentinel).        │
 * │   Must be the last block in the stream.                        │
 * └─────────────────────────────────────────────────────────────────┘
 *============================================================================*/

/*============================================================================
 * INDEX ANALYSIS ALGORITHM (Sub-Cell Timing)
 *
 * The index signal arrives between two flux transitions. To determine
 * the exact timing, we must decompose the flux cell containing the
 * index into pre-index and post-index portions.
 *
 * Given:
 *   flux_cell_time  = total sck ticks for the flux transition
 *   sample_counter  = partial sck count within the cell
 *   index_counter   = partial ick count (lower resolution)
 *
 * Computation:
 *   1. Count Ovl16 blocks within the flux cell: ic_overflow_cnt
 *   2. Walk backwards from flux position to find index stream position
 *      Count Ovl16 blocks traversed: pre_overflow_cnt
 *   3. pre_index_time = (ic_overflow_cnt - pre_overflow_cnt) << 16
 *                       + sample_counter
 *   4. post_index_time = flux_cell_time - pre_index_time
 *
 * Revolution Time:
 *   rev_time[n] = accumulated_flux_since_last_index + pre_index_time[n]
 *
 * RPM Calculation:
 *   rpm = 60.0 × sck / rev_time
 *   Standard 300 RPM → rev_time ≈ 4,805,485 sck ticks
 *   Standard 360 RPM → rev_time ≈ 4,004,571 sck ticks
 *
 * IMPORTANT: The first revolution (before first index) is incomplete
 * and must be excluded from RPM statistics.
 *
 *   Time axis:
 *   ─────┬───────────┬─────────────┬───────────────┬────
 *        │   flux n  │  flux n+1   │   flux n+2    │
 *        ├───────────┼──────╫──────┼───────────────┤
 *        │           │ pre  ║ post │               │
 *        │           │◄────►║◄────►│               │
 *                          INDEX
 *                          PULSE
 *============================================================================*/

/*============================================================================
 * STREAM FILE NAMING CONVENTION
 *
 * KryoFlux captures one file per track:
 *   track00.0.raw  - Track 0, Side 0
 *   track00.1.raw  - Track 0, Side 1
 *   track01.0.raw  - Track 1, Side 0
 *   ...
 *   track83.1.raw  - Track 83, Side 1
 *
 * A complete disk dump typically produces 168 files (84 tracks × 2 sides).
 * Each file is independent and self-contained.
 *============================================================================*/

/*============================================================================
 * STATISTICS COMPUTED FROM STREAM
 *
 * RPM:
 *   Average, minimum, maximum RPM from all complete revolutions.
 *   Typical: 299.5-300.5 RPM for well-calibrated drives.
 *
 * Transfer Rate:
 *   Bytes/second from StreamInfo blocks: stream_pos / transfer_time × sck
 *   Typical: ~62,500 bytes/sec for standard DD (250 kbit/s MFM)
 *
 * Flux Statistics:
 *   min_flux:     Shortest flux transition (sck ticks)
 *   max_flux:     Longest flux transition
 *   avg_per_rev:  Average number of transitions per revolution
 *   Typical DD: min≈72, max≈240, avg≈50,000 transitions/rev
 *
 * Anomaly Detection:
 *   - Buffer overflow: USB couldn't keep up, data lost
 *   - Missing index: Drive didn't produce index pulses
 *   - Position mismatch: Encoder/decoder desynchronized
 *   - Clock drift: RPM variation > ±0.5%
 *============================================================================*/

#endif /* UFT_KFSTREAM_AIR_SPEC_H */
