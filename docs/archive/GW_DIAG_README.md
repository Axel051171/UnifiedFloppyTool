# Greaseweazle Connection Diagnostics

## Problem Summary
- Port opens successfully
- RESET command works (returns `10 00`)
- GET_INFO returns `00 01` (BAD_COMMAND)
- But official `gw info` tool works perfectly

## Root Cause Hypotheses

### Hypothesis 1: Incomplete Read
**Likelihood: HIGH**
The device sends more than 2 bytes, but our code was only reading 2 bytes.
The remaining data stays in the buffer and corrupts the next command.

**Evidence:**
- Response `00 01` could be partial (echo + ack only)
- Payload might still be waiting in buffer

### Hypothesis 2: Timing Issue
**Likelihood: MEDIUM**
Commands sent too fast after RESET, device not ready.

**Evidence:**
- Adding delays sometimes helps
- Official tool probably has better timing

### Hypothesis 3: State Contamination
**Likelihood: MEDIUM**
Previous UFT attempts left device in bad state, requires USB replug.

**Evidence:**
- First attempt often fails, subsequent attempts with `gw` work
- Suggests buffer/state cleanup issue

## Diagnostic Output Format

The new code outputs detailed diagnostics:

```
[GW DIAG] ========== GET_INFO SEQUENCE ==========
[GW DIAG] Step 1: Purging buffers
[GW DIAG] Step 2: Draining pending data
[GW DIAG]   Drained (X bytes): XX XX XX ...
[GW DIAG] Step 3: Sending RESET (cmd=0x10)
[GW DIAG]   TX (2 bytes): 10 02
[GW DIAG]   RX (2 bytes): 10 00
[GW DIAG] Step 4: Sending GET_INFO (cmd=0x00, subindex=0)
[GW DIAG]   TX (4 bytes): 00 04 00 00
[GW DIAG]   RX header (2 bytes): 00 00
[GW DIAG]   Reading payload...
[GW DIAG]   RX payload (X bytes): XX XX XX ...
[GW DIAG] Step 5: Parsed device info:
[GW DIAG]   Firmware: v1.6
[GW DIAG]   Main FW:  Yes
[GW DIAG]   Model:    4.0
[GW DIAG] ========== SUCCESS ==========
```

## Protocol Reference

### Greaseweazle Command Format (Firmware 1.x)
```
TX: cmd(1) + len(1) + params(len-2)
    where len = total message length including cmd and len bytes

RX: cmd_echo(1) + ack(1) + data(variable)
    ack=0x00 means success
    ack=0x01 means BAD_COMMAND
```

### GET_INFO Command
```
TX: 00 04 00 00
    cmd=0x00 (GET_INFO)
    len=0x04 (total 4 bytes)
    subindex=0x0000 (firmware info)

RX: 00 00 <fw_major> <fw_minor> <is_main> <max_cmd> <sample_freq:4> <model> <submodel> <usb_speed>
    Expected ~12 bytes total
```

### RESET Command
```
TX: 10 02
    cmd=0x10 (RESET)
    len=0x02 (total 2 bytes, no params)

RX: 10 00
    echo + ack (success)
```

## Fixes Applied

1. **serial_drain()** - Read all pending data (non-blocking)
2. **serial_read_exact()** - Read exact number of bytes with timeout
3. **Proper timeout configuration** - ReadIntervalTimeout + ReadTotalTimeoutConstant
4. **Full hexdump of TX/RX** - See exactly what's being sent/received
5. **Step-by-step diagnostics** - Know exactly where it fails

## Testing

1. Compile the new code
2. Run from command line to see diagnostics
3. Select Greaseweazle + COM4
4. Click Connect
5. Check console output

If still failing, the diagnostic output will show exactly:
- What bytes were sent
- What bytes were received
- Where the protocol failed
