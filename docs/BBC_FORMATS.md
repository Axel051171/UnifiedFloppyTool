# BBC Micro Disk & Tape Format Reference

## DFS Catalog Structure

### Sector 0 (Filenames)
- Bytes 0-7: Disk title (first 8 chars)
- Bytes 8-255: File entries (31 max, 8 bytes each)
  - Bytes 0-6: Filename (space-padded)
  - Byte 7: Directory + locked (bit 7)

### Sector 1 (File Info)
- Bytes 0-3: Title (last 4 chars)
- Byte 4: Sequence (BCD)
- Byte 5: Entries × 8
- Byte 6: Boot (bits 4-5) + sectors high (bits 0-1)
- Byte 7: Sectors low
- Bytes 8+: File info (8 bytes each)

### Mixed Bits (Sector 1, Entry Byte 6)
```
Bits 7-6: Exec addr bits 17-16
Bits 5-4: Length bits 17-16
Bits 3-2: Load addr bits 17-16
Bits 1-0: Start sector bits 9-8
```

## BBC Tape Format

### Audio Encoding
- 1200 baud standard
- '0' = 1 cycle @ 1200 Hz
- '1' = 2 cycles @ 2400 Hz

### Block Structure
1. Leader: ~5s of '1' bits
2. Sync: 0x2A
3. Filename: null-terminated
4. Header: 19 bytes
5. Data + CRC

### CRC-16 (Non-standard)
```c
if (crc & 0x8000) {
    crc ^= 0x0810;
    crc = (crc << 1) | 1;
} else {
    crc <<= 1;
}
```
