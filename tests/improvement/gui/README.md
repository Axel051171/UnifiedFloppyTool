# improvement/gui/

Tests that prove UFT's GUI behaviour — `gw` is CLI-only, with no GUI
and no capability model.

**Form: C++ QtTest, not pytest-qt.** The original plan here was Python
`pytest-qt` tests. That does not work: pytest-qt drives Qt apps written
*in Python* (PyQt/PySide), and UFT's GUI is C++ Qt with no Python
bindings — pytest-qt cannot reach `HardwareTab`/`MainWindow` at all.
The GUI improvement tests are therefore C++ QtTest executables in
`tests/test_*_gui.cpp` (same shape as `tests/test_wiring_runtime.cpp`),
linked against `Qt6::Test` + `Qt6::Widgets`. This directory keeps the
README/category marker; the tests live in `tests/`.

| Test | Proves | Status |
|------|--------|--------|
| `tests/test_hardware_tab_gui.cpp` | the Hardware tab greys out exactly the capability actions a connected provider's TYPE does not satisfy, and enables exactly the ones it does — for all 9 V2 providers; plus the "no provider connected → every capability button disabled" affordance | ✅ MF-220 — drives the real `onConnect()` path; the expected button state is derived from the capability CONCEPT applied to the provider type, never a hard-coded table |
| main-window smoke (builds + shows without crashing) | the GUI constructs cleanly | ⬜ open — candidate follow-up |
| format-tab workflow walk-through | the conversion tab walks a path end-to-end | ⬜ open — candidate follow-up |

`test_hardware_tab_gui.cpp` is the executable proof of the type-driven
HAL refactor's GUI half: a provider whose type does not satisfy
`WritesRawFlux` / `ControlsMotor` / ... has the matching button
structurally disabled, because `wire_action<cap::X>` (codegen) gates on
the concept. It also covers MF-205/MF-206 (variant routing of all 9
providers through `HardwareTab`) and MF-210 (the
`updateTestButtonsEnabled` / `updateMotorControlsEnabled` fixes that
stopped clobbering the codegen gating).

Run headless: the test sets `QT_QPA_PLATFORM=offscreen` under ctest, so
no display is needed — only the Qt runtime DLLs on PATH (same as every
Qt-Test target in this repo).
