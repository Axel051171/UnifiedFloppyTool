# UnifiedFloppyTool v3.7.0 - TODO

> **Status:** GUI-Only Release - Major Progress ✅
> **Bei uns geht kein Bit verloren**

---

## ✅ Completed P0 Blockers

### ✅ P0-GUI-001: DecodeJob Backend Integration [DONE]
- **OLD:** 8 simulated code blocks, 0 backend calls
- **NEW:** 0 simulated blocks, 14+ backend calls
- Uses DiskImageValidator for format detection
- Real file I/O for sector verification

### ✅ P0-GUI-002: openFile() [DONE]
- Uses DiskImageValidator for validation
- Updates UI with format info
- Tracks recent files

### ✅ P0-GUI-003: HardwareTab [DONE]
- 308 lines implemented
- Serial port detection via QSerialPortInfo
- VID/PID matching for Greaseweazle, SCP, KryoFlux
- Note: HAL calls simulated (requires real hardware)

### ✅ P0-GUI-004: CatalogTab [DONE]
- 325 lines implemented
- Directory listing with file info

### ✅ P0-GUI-005: ForensicTab [DONE]
- 452 lines implemented
- Hash calculation, sector analysis

### ✅ P0-GUI-006: ToolsTab [DONE]
- 410 lines implemented
- Conversion, batch operations

### ✅ P0-GUI-007: NibbleTab [DONE]
- 353 lines implemented
- Track editing, hex view

### ✅ P0-GUI-008: XCopyTab [DONE]
- 315 lines implemented
- Disk copy operations

---

## 🟡 P1 - Remaining Work

### P1-1: Real HAL Integration
- **File:** src/hardwaretab.cpp
- **Issue:** HAL calls are simulated (uft_hal_* commented)
- **Reason:** Requires real hardware for testing
- **Action:** Document as "Hardware Required" feature

### P1-2: StatusTab Data Source
- **File:** src/statustab.cpp (99 lines)
- **Issue:** No data source connected
- **Action:** Connect to DecodeJob signals

### P1-3: Format Conversion
- **Current:** Simple file copy
- **Needed:** Real format conversion via uft_convert()
- **Priority:** Medium

---

## 🟢 P2 - Polish

### P2-1: Error Handling
- Add more descriptive error messages
- Log errors to file

### P2-2: Settings Persistence
- Save/restore window state
- Remember last used paths

### P2-3: Localization
- German translation complete
- Add more languages

---

## ✅ Build Status

| Platform | GUI | Tests | Status |
|----------|-----|-------|--------|
| Linux x64 | ✅ | ✅ 20/20 | Working |
| macOS ARM64 | ⏳ | ⏳ | CI Pending |
| Windows x64 | ⏳ | ⏳ | CI Pending |

---

## 📊 Metrics

| Metric | Old | Current |
|--------|-----|---------|
| P0 Blockers | 8 | 0 ✅ |
| Simulated Code | 8 blocks | 3 (HAL only) |
| Backend Calls | 3 | 100+ |
| Tab Coverage | 0% | 100% |
| Unit Tests | 20/20 | 20/20 ✅ |

---

## 🎯 Release Criteria

- [x] All P0 closed
- [x] Open File works (D64, ADF, SCP, etc.)
- [x] Decode shows real results
- [ ] Hardware Scan (requires real hardware)
- [x] No CLI required for user workflows
- [x] 20/20 Unit Tests pass

---

*Last Updated: 2026-01-12*
