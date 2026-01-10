# UFT Deactivated & Incomplete Modules Report

Generated: 2025-01-10

## Summary

| Category | Count | Description |
|----------|-------|-------------|
| HAL Disabled | 3 | API mismatch with current structures |
| C64 Tests Disabled | 2 | Test API doesn't match implementation |
| libflux_format Stubs | 37+ | Only probe(), no read/write |
| Format Parser Unused | 30+ | Not included in CMake build |
| Legacy Code | 10 | Moved to legacy/ directory |

---

## 1. HAL Modules (Disabled in CMakeLists)

```
src/hal/CMakeLists.txt:
    # uft_hal_profiles.c    - Drive profiles, struct mismatch
    # uft_greaseweazle_full.c - GW full impl, missing types
    # uft_hal_v3.c          - Duplicate of v2 (identical)
```

**Fix Required:**
- Update structs in uft_hal_profiles.c to match current uft_drive.h
- Add missing types to uft_greaseweazle.h (uft_gw_info_t, etc.)

---

## 2. C64 Tests (Disabled)

```
src/c64/CMakeLists.txt:
    # test_6502_disasm.c - Wrong argument count for uft_6502_disasm_range()
    # test_drive_scan.c  - Missing struct members (has_halftrack, etc.)
```

**Fix Required:**
- Align test APIs with current implementation
- Add missing struct members or update tests

---

## 3. libflux_format Stubs (probe only)

These files have `probe()` implemented but `read()`/`write()` return ENOSYS:

```
1dd.c, 2d.c, a2r.c, bpb.c, cfi.c, cpm.c, cqm.c, cwtool.c,
d2m.c, d4m.c, d80.c, d88.c, dfi.c, do.c, ds2.c, dsc.c,
dti.c, fd.c, fdi.c, ipf.c, lif.c, mbd.c, mfi.c, mgt.c,
opd.c, pdi.c, qdos.c, s24.c, sad.c, sap.c, sbt.c, scl.c,
scp.c, sdf.c, st.c, stream.c, trd.c, udi.c
```

**Priority for Implementation:**
1. scl.c - ✅ DONE (new implementation integrated)
2. trd.c - TR-DOS (ZX Spectrum)
3. d88.c - PC-88/98
4. cpm.c - CP/M
5. ipf.c - SPS/IPF

---

## 4. Format Parsers Not in Build

Located in src/formats/ but not included in CMakeLists:

### Amiga
- uft_adf_serial.c
- amiga.c, amiga_hw.c, amigadosfs_loader.c
- bootblock.c, fs_amigados.c

### Amstrad
- dsk.c, dsk_mfm.c, edsk_extdsk.c
- mgt_sad_sdf.c, trd_scl.c

### Apple
- 2mg.c, apple2_2mg_loader.c
- apple2_do_loader.c, apple2_nib_loader.c
- mac_dsk.c, nib.c, nib_nbz.c
- prodos_po_do.c, uft_2mg_parser.c, woz.c

### Atari
- atari_st.c, atom.c, atr.c, atx.c, st.c

---

## 5. Legacy Code (Moved)

Moved to `legacy/hfe_hxc/`:
- hfe_loader/ (6 files)
- parsers/hfe/ (1 file)
- hfe.c, hfev3_trackgen.c

**Reason:** Duplicate of src/formats/hfe/uft_hfe.c, uses external LIBFLUX API

---

## 6. Macro Warnings (Minor)

Build generates warnings for redefined macros:
```
UFT_PACKED - Defined in uft_common.h AND uft_compiler.h
UFT_INLINE - Defined in uft_common.h AND uft_compiler.h
```

**Fix:** Remove duplicate definitions from uft_common.h

---

## Next Steps (P3 Priority)

### High Priority
1. Fix C64 test APIs
2. Fix HAL module struct mismatches
3. Resolve macro redefinitions

### Medium Priority
4. Implement TRD format (TR-DOS)
5. Implement D88 format (PC-88/98)
6. Add more format parsers to build

### Low Priority
7. Review and potentially restore legacy HFE code
8. Implement remaining libflux_format stubs

---

## Completed This Session

✅ **SCL Format** - Full implementation integrated from uft_drive_trx_scl_pack_v2
- Parse/Validate/GetFile/Build operations
- 16 unit tests passing
- Zero-copy data access
- TR-DOS compatibility

✅ **Z80 Disassembler** - Ported from tzxtools (Python → C)
- All standard Z80 instructions (256 base opcodes)
- CB prefix (bit operations)
- ED prefix (extended instructions)
- DD/FD prefix (IX/IY register versions)
- Undocumented instructions (IXH, IXL, IYH, IYL)
- 53 unit tests passing
- Source: https://codeberg.org/shred/tzxtools (GPL-3.0)

---

## Learned from tzxtools

The tzxtools Python library provided:
1. **Complete TZX block definitions** (29 block types)
2. **Z80 disassembler** with ZX Spectrum Next support
3. **ZX BASIC tokenization** for text extraction
4. **ZX Spectrum screen conversion** to PNG

UFT was missing these TZX block types that tzxtools has:
- 0x16: C64 Data
- 0x17: C64 Turbo Data
- 0x26: Call Sequence
- 0x27: Return
- 0x34: Emulation Info
- 0x40: Snapshot
- 0x4B: Kansas City Standard (TSX/MSX)
