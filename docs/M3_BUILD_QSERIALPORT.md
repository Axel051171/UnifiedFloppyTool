# Building UFT with QSerialPort support (M3.3+)

**Status:** Required for production transports of Applesauce + ADFCopy.
**Last updated:** 2026-05-24

The M3.3 production transport for Applesauce (`MF-249`,
`src/hardware_providers/qserial_applesauce_transport.cpp`) depends on
Qt6's `SerialPort` module. The default Qt 6.10.x install (the
`qt.qt6.6102.win64_mingw` MaintenanceTool component) does NOT include
SerialPort — it is a separate optional Qt module.

When Qt6::SerialPort is absent, the build still succeeds: the
`src/hardware_providers/CMakeLists.txt` excludes the QSerialPort
transport sources from the static library and the affected providers
(Applesauce, ADFCopy) continue to operate as honest-stubs with
`nullptr` runners. This is the same forensically-safe behaviour the
codebase had before MF-249.

To enable the production transport you need to install Qt6 SerialPort.

---

## Option A — Official Qt installer (recommended)

1. Open Qt MaintenanceTool (Start Menu → Qt → Maintenance Tool, or
   `C:\Qt\MaintenanceTool.exe`).
2. Sign in to your Qt account.
3. Select **"Add or remove components"**.
4. Expand **Qt → Qt 6.10.2 → MinGW 13.1.0 64-bit** (or whichever Qt
   version your build uses — check
   `build/CMakeCache.txt` for `Qt6_DIR`).
5. Check the **"Qt Serial Port"** module checkbox.
6. Click Next → Update.
7. Re-run `cmake .` in the build directory. You should see:
   ```
   -- UFT hardware_providers: Qt6::SerialPort FOUND —
      QSerialPort transports ENABLED
   ```

---

## Option B — Build Qt SerialPort from source

Useful when working offline or with a custom Qt build.

```powershell
cd C:\src  # any work directory
git clone --branch v6.10.2 https://code.qt.io/qt/qtserialport.git
cd qtserialport
cmake -B build -G "MinGW Makefiles" `
      -DCMAKE_PREFIX_PATH="C:\Qt\6.10.2\mingw_64" `
      -DCMAKE_INSTALL_PREFIX="C:\Qt\6.10.2\mingw_64"
cmake --build build --parallel
cmake --install build
```

Then re-run cmake on UFT — the module should be discovered
automatically.

---

## Option C — Linux distribution package

Most distributions ship Qt6 SerialPort as a separate package:

| Distribution | Package |
|---|---|
| Debian / Ubuntu | `qt6-serialport-dev` |
| Fedora          | `qt6-qtserialport-devel` |
| Arch            | `qt6-serialport` |

Install, then reconfigure cmake.

---

## Verification

After installation, the CMake configure output should say:

```
-- UFT hardware_providers: Qt6::SerialPort FOUND —
   QSerialPort transports ENABLED
```

The static library `hardware_providers` will then include
`qserial_applesauce_transport.cpp` and link against `Qt6::SerialPort`.

Without the module the same configure step says NOT FOUND and silently
omits the file — no error.

---

## Hardware verification

Compiling with QSerialPort enabled is necessary but NOT sufficient.
Before considering Applesauce production-wired, the
`docs/M3_APPLESAUCE_TRANSPORT.md` §5 bench-verification plan must run
green against a real Applesauce device. Until then HardwareTab still
shows the orange "Disconnect (Preview)" styling for Applesauce
connections.
