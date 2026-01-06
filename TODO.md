# UFT TODO List

Consolidated task list for UnifiedFloppyTool development.
Priority: P0 (Critical) > P1 (High) > P2 (Medium) > P3 (Polish)

---

## P0 - Critical (Crashes / Data Corruption)

All P0 tasks completed in v3.3.0 ✅

---

## P1 - High Priority (Incorrect Results / Format Errors)

### P1-FMT-001: Extended D88 Support
- **Scope**: PC-98 D88 format edge cases
- **Modules**: `src/formats/d88/`
- **Acceptance**: All NEC PC-98 D88 variants parse correctly
- **Status**: 80% complete

### P1-FMT-002: Apple II RWTS Variations
- **Scope**: Custom RWTS disk formatting
- **Modules**: `src/formats/apple/`
- **Acceptance**: Detect and handle non-standard sector skewing
- **Status**: Not started

### P1-HW-001: KryoFlux Stream Index Sync
- **Scope**: Multi-revolution index alignment
- **Modules**: `src/flux/kryoflux/`
- **Acceptance**: Accurate revolution boundaries
- **Status**: 60% complete

---

## P2 - Medium Priority (Performance / Refactoring)

### P2-PERF-001: SIMD Flux Analysis
- **Scope**: AVX2/NEON acceleration for PLL
- **Modules**: `src/flux/pll/`
- **Acceptance**: 2x speedup on large flux files
- **Status**: Prototype ready

### P2-REF-001: Parser Factory Consolidation
- **Scope**: Unify parser instantiation
- **Modules**: `src/formats/*/`
- **Acceptance**: Single registration point
- **Status**: 40% complete

### P2-GUI-001: Batch Processing Dialog
- **Scope**: Multi-file conversion wizard
- **Modules**: `src/gui/`
- **Acceptance**: Queue management, progress tracking
- **Status**: Planned

---

## P3 - Polish / Documentation

### P3-DOC-001: API Documentation
- **Scope**: Doxygen for public API
- **Modules**: `include/`
- **Acceptance**: 100% public API documented
- **Status**: 70% complete

### P3-GUI-001: Keyboard Shortcuts
- **Scope**: Comprehensive keyboard navigation
- **Modules**: `src/gui/`
- **Acceptance**: All actions have shortcuts
- **Status**: 50% complete

### P3-TEST-001: Integration Test Suite
- **Scope**: End-to-end conversion tests
- **Modules**: `tests/integration/`
- **Acceptance**: All format pairs tested
- **Status**: 30% complete

---

## Completed Tasks (v3.3.0)

- [x] P0-SEC-001: Buffer overflow protection
- [x] P0-SEC-002: Integer overflow checks
- [x] P0-SEC-003: Format string safety
- [x] P0-SEC-004: Memory safety audit
- [x] P1-IO-001: I/O return value checks (1,370 fixes)
- [x] P1-IO-002: File handle leak prevention
- [x] P1-THR-001: Thread safety fixes
- [x] P1-MEM-001: Memory leak fixes
- [x] P2-ARCH-001-007: Core unification
- [x] P2-GUI-001-010: GUI components

---

## Issue Tracking

For bug reports and feature requests, please use [GitHub Issues](https://github.com/Axel051171/UnifiedFloppyTool/issues).

Template:
```
**Type**: Bug / Feature / Enhancement
**Priority**: P0 / P1 / P2 / P3
**Module**: [affected module]
**Description**: [clear description]
**Steps to Reproduce**: [if bug]
**Expected Behavior**: [what should happen]
**Actual Behavior**: [what happens]
```
