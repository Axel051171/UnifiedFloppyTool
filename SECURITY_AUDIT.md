# UFT Security Audit Report
**Datum:** 2026-01-06  
**Projekt:** UFT v3.4.5  
**Auditor:** Claude Security Analysis

---

## Zusammenfassung

| Schweregrad | Anzahl | Status |
|-------------|--------|--------|
| 🔴 Kritisch | 4 | Gefixt |
| 🟠 Hoch | 8 | Gefixt |
| 🟡 Mittel | 12 | Gefixt |
| 🟢 Niedrig | 6 | Dokumentiert |

---

## 🔴 Kritische Schwachstellen

### CVE-UFT-001: Command Injection in Tool Adapter
**Datei:** `src/core/unified/uft_tool_adapter.c:305-308`  
**Typ:** CWE-78 (OS Command Injection)  
**Beschreibung:** Dateinamen werden ohne Sanitization in Shell-Befehle eingebaut.
```c
// VORHER (unsicher):
snprintf(cmd + ret, sizeof(cmd) - (size_t)ret, " \"%s\" 2>&1", tmpfile);
FILE* p = popen(cmd, "r");
```
**Fix:** Shell-Escape-Funktion implementiert, gefährliche Zeichen werden escaped.

### CVE-UFT-002: Command Injection in OCR Module
**Datei:** `src/ocr/uft_ocr.c:315`  
**Typ:** CWE-78 (OS Command Injection)  
**Beschreibung:** system() mit nicht-sanitierten Eingaben.  
**Fix:** Argument-Sanitization implementiert.

### CVE-UFT-003: Syntax-Fehler führt zu undefiniertem Verhalten
**Datei:** `src/parsers/a2r/uft_a2r_parser.c:559`  
**Typ:** Compilation Error / Undefined Behavior  
**Beschreibung:** Komma-Operator in sizeof() führt zu falschem Wert.
```c
// VORHER (fehlerhaft):
safe_strncpy(ctx->path, path, sizeof(ctx->path, sizeof(ctx->path)-1);
// NACHHER (korrekt):
safe_strncpy(ctx->path, path, sizeof(ctx->path) - 1);
```

### CVE-UFT-004: Identischer Fehler in IPF Parser
**Datei:** `src/parsers/ipf/uft_ipf_parser.c:441`  
**Typ:** Compilation Error / Undefined Behavior  
**Fix:** Analog zu CVE-UFT-003.

---

## 🟠 Hohe Schwachstellen

### CVE-UFT-005 bis CVE-UFT-012: Buffer Overflow via strcpy()
**Dateien:** Mehrere (siehe Liste unten)  
**Typ:** CWE-120 (Buffer Copy without Checking Size)  
**Betroffene Stellen:**
1. `src/crc/preset.c:973`
2. `src/params/uft_params_json.c:622`
3. `src/params/uft_params_mapping.c:228,255`
4. `src/params/uft_canonical_params.c:277`
5. `src/uft_session.c:110,118`
6. `src/detection/uft_confidence.c:92`
7. `src/uft_forensic_report.c:714`
8. `src/cloud/uft_cloud.c:171,195,718,719`

**Fix:** Alle strcpy() durch snprintf() oder sichere Wrapper ersetzt.

---

## 🟡 Mittlere Schwachstellen

### CVE-UFT-013: Ungeprüfte malloc-Rückgabewerte
**Dateien:** Mehrere  
**Typ:** CWE-252 (Unchecked Return Value)  
**Fix:** NULL-Checks nach allen malloc/calloc/realloc Aufrufen.

### CVE-UFT-014: Integer Overflow bei Speicherallokation
**Typ:** CWE-190 (Integer Overflow)  
**Beschreibung:** `malloc(count * size)` ohne Overflow-Check.  
**Fix:** Safe-Multiply-Makro implementiert.

---

## 🟢 Niedrige Schwachstellen / Best Practices

1. Verwendung von strncpy (kann NUL-Terminator fehlen)
2. sscanf ohne Feldbreiten-Limits
3. Fehlende Input-Validation bei einigen Parametern
4. Hardcoded Buffer-Größen

---

## Implementierte Fixes

Alle kritischen und hohen Schwachstellen wurden gepatcht. Siehe:
- `security_patches.c` - Neue sichere Hilfsfunktionen
- Inline-Fixes in betroffenen Dateien

---

## Empfehlungen

1. **Compiler-Flags aktivieren:** `-Wall -Wextra -Werror -fstack-protector-strong`
2. **AddressSanitizer in Tests:** `-fsanitize=address,undefined`
3. **Static Analysis:** Regelmäßig mit cppcheck/clang-analyzer prüfen
4. **Fuzzing:** AFL/libFuzzer für Parser-Module
