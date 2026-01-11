# TZX Block Types Reference

Source: rtzx (Rust TZX Player, MIT License)
Reference: https://worldofspectrum.net/TZXformat.html

## Block Type Overview

| ID   | Name | Description |
|------|------|-------------|
| 0x10 | Standard Speed Data Block | Standard data (1000 T-states pilot) |
| 0x11 | Turbo Speed Data Block | Variable speed data |
| 0x12 | Pure Tone | Single frequency tone |
| 0x13 | Pulse Sequence | Sequence of pulses |
| 0x14 | Pure Data Block | Data without pilot/sync |
| 0x15 | Direct Recording | Sampled audio data |
| 0x18 | CSW Recording | Compressed audio |
| 0x19 | Generalized Data Block | Flexible data format |
| 0x20 | Pause or Stop the Tape | Pause in ms |
| 0x21 | Group Start | Begin named group |
| 0x22 | Group End | End named group |
| 0x23 | Jump to Block | Jump relative |
| 0x24 | Loop Start | Begin loop |
| 0x25 | Loop End | End loop |
| 0x26 | Call Sequence | Call block sequence |
| 0x27 | Return from Sequence | Return from call |
| 0x28 | Select Block | User selection menu |
| 0x2A | Stop Tape if in 48K Mode | Conditional stop |
| 0x2B | Set Signal Level | Set output level |
| 0x30 | Text Description | ASCII text |
| 0x31 | Message Block | Message with duration |
| 0x32 | Archive Info | Metadata |
| 0x33 | Hardware Type | Hardware requirements |
| 0x35 | Custom Info Block | Application-specific |
| 0x40 | Snapshot Block | Embedded snapshot |
| 0x5A | Glue Block | Concatenation marker |

## Standard Timing Constants

```c
// ZX Spectrum standard loader
#define TZX_PILOT_PULSE_LENGTH    2168  // T-states
#define TZX_SYNC1_PULSE_LENGTH     667  // T-states
#define TZX_SYNC2_PULSE_LENGTH     735  // T-states
#define TZX_ZERO_PULSE_LENGTH      855  // T-states
#define TZX_ONE_PULSE_LENGTH      1710  // T-states
#define TZX_PILOT_PULSES_HEADER   8063  // Pilot pulses for header
#define TZX_PILOT_PULSES_DATA     3223  // Pilot pulses for data

// Amstrad CPC (CDT)
#define CDT_PILOT_PULSE_LENGTH    2000  // T-states
#define CDT_SYNC1_PULSE_LENGTH     855  // T-states
#define CDT_SYNC2_PULSE_LENGTH     855  // T-states
#define CDT_ZERO_PULSE_LENGTH      855  // T-states
#define CDT_ONE_PULSE_LENGTH      1710  // T-states
```

## Block 0x10: Standard Speed Data Block

```
Offset  Size  Description
0x00    1     Block ID (0x10)
0x01    2     Pause after block (ms)
0x03    2     Data length (N)
0x05    N     Data bytes
```

## Block 0x11: Turbo Speed Data Block

```
Offset  Size  Description
0x00    1     Block ID (0x11)
0x01    2     Pilot pulse length
0x03    2     Sync first pulse length
0x05    2     Sync second pulse length
0x07    2     Zero bit pulse length
0x09    2     One bit pulse length
0x0B    2     Pilot tone pulses
0x0D    1     Used bits in last byte
0x0E    2     Pause after block (ms)
0x10    3     Data length (N)
0x13    N     Data bytes
```

## Block 0x19: Generalized Data Block

Most flexible block type, supports:
- Variable pilot/sync patterns
- Multiple data encoding schemes
- Per-symbol timing definitions

---

*Part of UnifiedFloppyTool Reference Documentation*
