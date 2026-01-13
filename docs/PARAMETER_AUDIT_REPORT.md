# UFT Parameter Konsistenz Audit
## Version: 3.9.0 | Datum: 2026-01-13

---

## 🚨 KRITISCHES PROBLEM

**Die GUI ist NICHT mit dem C-Backend verbunden!**

Die `syncToBackend()` und `syncFromBackend()` Funktionen in `UftParameterModel.cpp` 
sind leere STUBS, die nur qDebug() ausgeben aber KEINE echte Synchronisation durchführen.

### Was fehlt:

```cpp
// AKTUELL (STUB):
void UftParameterModel::syncToBackend()
{
    qDebug() << "Syncing to backend...";  // <-- NUR DEBUG OUTPUT!
    emit backendSynced();
}

// SOLLTE SEIN:
void UftParameterModel::syncToBackend()
{
    uft_params_set_int(m_backendParams, "cylinders", m_cylinders);
    uft_params_set_int(m_backendParams, "heads", m_heads);
    // ... etc
}
```

---

## Schichten-Analyse

```
┌────────────────────────────────────────────────────────────┐
│  Qt GUI Widgets                                             │
│  (QSpinBox, QComboBox, etc.)                               │
├────────────────────────────────────────────────────────────┤
│  UftWidgetBinder  →  UftParameterModel (Q_PROPERTY)        │  ✅ OK
├────────────────────────────────────────────────────────────┤
│  syncToBackend() / syncFromBackend()                       │  🚨 STUB!
├────────────────────────────────────────────────────────────┤
│  uft_params_set_*() / uft_params_get_*()                   │  ✅ API existiert
├────────────────────────────────────────────────────────────┤
│  C Backend (uft_params_t)                                   │  ✅ OK
└────────────────────────────────────────────────────────────┘
```

---

## Parameter Mapping

| GUI Parameter | Backend Key | Typ | Status |
|---------------|-------------|-----|--------|
| cylinders | "cylinders" | int | ⚠️ Nicht verbunden |
| heads | "heads" | int | ⚠️ Nicht verbunden |
| sectors | "sectors" | int | ⚠️ Nicht verbunden |
| format | "format" | string | ⚠️ Nicht verbunden |
| encoding | "encoding" | string | ⚠️ Nicht verbunden |
| hardware | "hardware" | string | ⚠️ Nicht verbunden |
| devicePath | "devicePath" | string | ⚠️ Nicht verbunden |
| driveNumber | "driveNumber" | int | ⚠️ Nicht verbunden |
| retries | "retries" | int | ⚠️ Nicht verbunden |
| revolutions | "revolutions" | int | ⚠️ Nicht verbunden |
| weakBits | "weakBits" | bool | ⚠️ Nicht verbunden |
| pllPhaseGain | "pllPhaseGain" | float | ⚠️ Nicht verbunden |
| pllFreqGain | "pllFreqGain" | float | ⚠️ Nicht verbunden |
| pllWindowTolerance | "pllWindowTolerance" | float | ⚠️ Nicht verbunden |
| verifyAfterWrite | "verifyAfterWrite" | bool | ⚠️ Nicht verbunden |
| writeRetries | "writeRetries" | int | ⚠️ Nicht verbunden |

---

## Backend API (existiert!)

```c
// Diese Funktionen existieren in uft_param_bridge.h:
uft_params_set_int(params, "cylinders", value);
uft_params_set_bool(params, "weakBits", value);
uft_params_set_float(params, "pllPhaseGain", value);
uft_params_set_string(params, "format", value);

uft_params_get_int(params, "cylinders");
uft_params_get_bool(params, "weakBits");
uft_params_get_float(params, "pllPhaseGain");
uft_params_get_string(params, "format");
```

---

## FIX ERFORDERLICH

### 1. Header ändern (UftParameterModel.h)

```cpp
extern "C" {
    #include "uft/uft_param_bridge.h"
}

private:
    uft_params_t *m_backendParams = nullptr;
```

### 2. Constructor/Destructor

```cpp
UftParameterModel::UftParameterModel(QObject *parent)
    : QObject(parent)
{
    m_backendParams = uft_params_create_defaults();
}

UftParameterModel::~UftParameterModel()
{
    uft_params_free(m_backendParams);
}
```

### 3. syncToBackend() implementieren

Siehe Fix in: `/tmp/parameter_model_fix.cpp`

---

## Auswirkung

**OHNE diesen Fix:**
- GUI-Änderungen haben KEINE Auswirkung auf das Backend
- Disk-Operationen verwenden NICHT die GUI-Einstellungen
- Alle Parameter werden ignoriert

**MIT diesem Fix:**
- Vollständige bidirektionale Synchronisation
- GUI-Einstellungen werden an Backend übergeben
- Backend-Änderungen werden in GUI reflektiert

---

## Priorität: P0 - KRITISCH

Dieser Fix MUSS implementiert werden, bevor die GUI produktiv genutzt werden kann!
