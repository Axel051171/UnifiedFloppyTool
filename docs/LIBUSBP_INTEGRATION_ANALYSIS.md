# libusbp Integration Analysis für UnifiedFloppyTool

## Was ist libusbp?

**libusbp** (Pololu USB Library) ist eine Cross-Platform C-Bibliothek für USB-Geräte mit folgenden Features:

- **Cross-Platform**: Windows, Linux, macOS
- **Device Enumeration**: VID, PID, Revision, Serial ohne I/O
- **Serial Port Discovery**: Findet virtuelle COM-Ports (z.B. "COM5", "/dev/ttyACM0")
- **USB I/O**: Control, Bulk, Interrupt Transfers (sync + async)
- **C++ Wrapper**: Objektorientierte API verfügbar
- **Detaillierte Fehler**: Lesbare Fehlermeldungen mit Codes

---

## Relevanz für UFT

### ✅ HOHE RELEVANZ

| Feature | UFT Anwendung | Aktueller Stand |
|---------|---------------|-----------------|
| **Device Enumeration** | Greaseweazle, FluxEngine, KryoFlux finden | QSerialPortInfo (limitiert) |
| **Serial Port Discovery** | COM-Port Namen für CDC-Geräte | Hardcoded Pfade |
| **Bulk Transfers** | Direkte USB-Kommunikation | CLI-Wrapper |
| **Async I/O** | Non-blocking Reads | Nicht vorhanden |
| **Error Handling** | USB-Fehlermeldungen | Inkonsistent |

### Aktuelle USB-Implementierung in UFT

```
Hardware Provider          USB-Methode
─────────────────────────────────────────────────
Greaseweazle              CLI (gw) + QSerialPort
FluxEngine                CLI (fluxengine)
KryoFlux                  CLI (dtc)
SuperCard Pro             CLI + Serial
FC5025                    Stub (nicht impl.)
XUM1541                   Stub (nicht impl.)
```

**Problem**: Alle Provider nutzen CLI-Wrapper statt direkter USB-Kommunikation.

---

## Konkrete Vorteile von libusbp

### 1. Automatische Geräteerkennung

**Aktuell** (hardcoded):
```cpp
// greaseweazlehardwareprovider.cpp:668
out << QStringLiteral("/dev/ttyACM%1").arg(i);
out << QStringLiteral("/dev/ttyUSB%1").arg(i);
```

**Mit libusbp**:
```cpp
#include <libusbp.hpp>

std::vector<libusbp::device> find_greaseweazle_devices() {
    std::vector<libusbp::device> result;
    
    for (const auto& dev : libusbp::list_connected_devices()) {
        // Greaseweazle VID:PID = 1209:4d69
        if (dev.get_vendor_id() == 0x1209 && 
            dev.get_product_id() == 0x4d69) {
            result.push_back(dev);
        }
    }
    return result;
}

// Serial Port automatisch finden
std::string get_greaseweazle_port(const libusbp::device& dev) {
    auto serial_port = libusbp::serial_port(dev);
    return serial_port.get_name();  // "COM5" oder "/dev/ttyACM0"
}
```

### 2. Direkte USB-Kommunikation

**Aktuell** (CLI-Wrapper):
```cpp
// fluxenginehardwareprovider.cpp
QProcess process;
process.start("fluxengine", {"read", "ibm", "-o", output});
process.waitForFinished(60000);
```

**Mit libusbp** (direkt):
```cpp
#include <libusbp.hpp>

class FluxEngineUSB {
    libusbp::generic_interface iface;
    libusbp::generic_handle handle;
    
public:
    FluxEngineUSB(const libusbp::device& dev) {
        iface = libusbp::generic_interface(dev, 0, true);
        handle = libusbp::generic_handle(iface);
    }
    
    // Bulk Transfer für Flux-Daten
    std::vector<uint8_t> read_track(int cyl, int head) {
        // CMD_READ = 0x01
        uint8_t cmd[] = {0x01, (uint8_t)cyl, (uint8_t)head};
        
        // Control Transfer: Kommando senden
        handle.control_transfer(0x40, 0x00, 0, 0, cmd, sizeof(cmd));
        
        // Bulk Transfer: Daten empfangen
        std::vector<uint8_t> buffer(65536);
        size_t transferred;
        handle.read_pipe(0x81, buffer.data(), buffer.size(), &transferred);
        buffer.resize(transferred);
        
        return buffer;
    }
};
```

### 3. Asynchrone I/O

**Vorteil**: GUI friert nicht ein während USB-Operationen.

```cpp
// Asynchroner Read mit Callback
void FluxEngineUSB::read_track_async(int cyl, int head, 
                                      std::function<void(std::vector<uint8_t>)> callback) {
    libusbp::async_in_pipe pipe(handle, 0x81);
    
    pipe.submit();  // Non-blocking
    
    // In Event-Loop:
    while (!pipe.is_completed()) {
        // GUI bleibt responsive
        QCoreApplication::processEvents();
    }
    
    auto data = pipe.get_data();
    callback(data);
}
```

### 4. Cross-Platform Serial Port Discovery

**Problem**: Verschiedene Pfade auf verschiedenen OS.

| OS | Greaseweazle Port |
|----|-------------------|
| Windows | COM3, COM5, ... |
| Linux | /dev/ttyACM0, /dev/ttyUSB0 |
| macOS | /dev/cu.usbmodem* |

**libusbp Lösung**:
```cpp
std::string find_port_by_vid_pid(uint16_t vid, uint16_t pid) {
    for (const auto& dev : libusbp::list_connected_devices()) {
        if (dev.get_vendor_id() == vid && dev.get_product_id() == pid) {
            auto port = libusbp::serial_port(dev);
            return port.get_name();  // Automatisch richtig für OS
        }
    }
    return "";
}
```

---

## Hardware VID/PID Referenz

| Gerät | VID | PID | Interface |
|-------|-----|-----|-----------|
| Greaseweazle F7 | 0x1209 | 0x4d69 | CDC (Serial) |
| FluxEngine | 0x1209 | 0x6e00 | WinUSB/libusb |
| KryoFlux | 0x03eb | 0x6124 | Bulk |
| SuperCard Pro | 0x16d0 | 0x0d61 | CDC (Serial) |
| FC5025 | 0xda05 | 0xfc52 | Bulk |
| XUM1541 | 0x16d0 | 0x0504 | Bulk |

---

## Integration in UFT

### CMakeLists.txt
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSBP REQUIRED libusbp-1)

target_include_directories(UFT PRIVATE ${LIBUSBP_INCLUDE_DIRS})
target_link_libraries(UFT PRIVATE ${LIBUSBP_LIBRARIES})
```

### UnifiedFloppyTool.pro
```qmake
unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += libusbp-1
}

win32 {
    INCLUDEPATH += $$PWD/3rdparty/libusbp/include
    LIBS += -L$$PWD/3rdparty/libusbp/lib -llibusbp-1
}
```

### Unified Hardware Layer

```cpp
// uft_usb_device.h
class UFTUSBDevice {
public:
    static std::vector<UFTUSBDevice> enumerate();
    static std::optional<UFTUSBDevice> find_by_vid_pid(uint16_t vid, uint16_t pid);
    
    uint16_t vid() const;
    uint16_t pid() const;
    std::string serial() const;
    std::string port_name() const;  // Falls CDC
    
    // I/O
    void control_transfer(uint8_t type, uint8_t request, uint16_t value, 
                         uint16_t index, void* data, size_t len);
    size_t bulk_read(uint8_t endpoint, void* data, size_t len, int timeout_ms);
    size_t bulk_write(uint8_t endpoint, const void* data, size_t len, int timeout_ms);
    
private:
    libusbp::device m_device;
    std::unique_ptr<libusbp::generic_handle> m_handle;
};
```

---

## Vergleich: libusbp vs. libusb vs. Qt

| Feature | libusbp | libusb-1.0 | Qt Serial |
|---------|---------|------------|-----------|
| Cross-Platform | ✅ | ✅ | ✅ |
| Serial Port Discovery | ✅ | ❌ | ⚠️ (nur Serial) |
| Serial Number ohne I/O | ✅ | ❌ | ❌ |
| Async I/O | ✅ | ✅ | ✅ |
| C++ Wrapper | ✅ | ❌ (C only) | ✅ (Qt) |
| Windows WinUSB | ✅ | ⚠️ (Driver) | ❌ |
| Fehlermeldungen | ✅ Detailliert | ⚠️ Codes | ✅ |
| Dependency | Minimal | Minimal | Qt Framework |

**Empfehlung**: libusbp für USB-Geräte, Qt Serial als Fallback für reine Serial-Ports.

---

## Implementierungsplan

### Phase 1: Device Enumeration (P1)
- [ ] libusbp als Dependency hinzufügen
- [ ] UFTUSBDevice Wrapper implementieren
- [ ] Hardware-Tab: Automatische Geräteerkennung

### Phase 2: Direct USB für Greaseweazle (P2)
- [ ] Greaseweazle USB-Protokoll implementieren
- [ ] CLI-Fallback behalten
- [ ] Performance-Vergleich

### Phase 3: Weitere Hardware (P3)
- [ ] FluxEngine direkt
- [ ] KryoFlux direkt
- [ ] SuperCard Pro direkt

---

## Risiken & Mitigationen

| Risiko | Mitigation |
|--------|------------|
| Windows WinUSB Driver | Zadig-Anleitung dokumentieren |
| Unterschiedliche FW-Versionen | Versionserkennung implementieren |
| Breaking Changes | CLI als Fallback behalten |
| Build-Komplexität | Optional compile mit `UFT_USE_LIBUSBP` |

---

## Fazit

**libusbp ist sehr gut geeignet für UFT**, besonders für:
- Automatische Geräteerkennung (kein hardcoded /dev/ttyACM0)
- Cross-Platform Serial Port Discovery
- Zukünftige direkte USB-Kommunikation (ohne CLI)
- Bessere Fehlermeldungen

**Empfehlung**: 
1. Phase 1 starten: Device Enumeration für Hardware-Tab
2. CLI-Wrapper behalten als Fallback
3. Schrittweise direkte USB-Kommunikation hinzufügen
