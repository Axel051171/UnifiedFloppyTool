# 🎨 Inline Validation - Beispiele

## ✅ **Real-Time Validation in Tabs**

---

## 📊 **Beispiel 1: Tab 3 (Format) - Tracks**

### **Vor Validierung:**

```
┌─ Format Settings ──────────────────────┐
│                                        │
│ Number of Tracks:  [90]                │
│                                        │
└────────────────────────────────────────┘
```

### **Mit Validierung:**

```
┌─ Format Settings ──────────────────────┐
│                                        │
│ Number of Tracks:  [90]                │
│ ⚠️ Warning: Maximum tracks is 83       │
│ [Auto-Fix to 83]                       │
│                                        │
└────────────────────────────────────────┘
```

---

## 📊 **Beispiel 2: Tab 4 (Geometry) - Sector Size**

### **Ohne Validierung:**

```
Sector Size: [333] Bytes  ← Freie Eingabe (SCHLECHT!)
```

### **Mit Validierung:**

```
Sector Size: [512        ▼]  ← Dropdown!
             └─ 128
                256
                512  ✓
                1024
                2048
```

→ Keine falsche Eingabe möglich! ✓

---

## 📊 **Beispiel 3: Tab 5 (Protection) - Profile Conflict**

### **Konflikt erkannt:**

```
┌─ Protection Profile ──────────────────┐
│ Active Profile: [Amiga Standard  ▼]  │
└───────────────────────────────────────┘

Tab 1 (Workflow):
Format: "C64 (GCR)"  ← KONFLIKT!

⚠️ Warning: Amiga profile selected but C64 format active
Suggestion: Change profile to "C64 Standard"
[Auto-Fix] [Ignore]
```

---

## 📊 **Beispiel 4: Tab 8 (Hardware) - Greaseweazle**

### **Ohne Drive Type:**

```
┌─ Hardware Detection ──────────────────┐
│ Detected:                             │
│ ├── RPM: 300.1 ✓                     │
│ └── Type: Unknown ❌                  │
│                                       │
│ ⚠️ Drive type not set!                │
│ Operation may fail.                   │
│                                       │
│ [Enable Manual Override]              │
└───────────────────────────────────────┘
```

---

## 📊 **Beispiel 5: Tab 2 (Operations) - No File Selected**

### **Read Button Status:**

```
┌─ Operations ──────────────────────────┐
│                                       │
│ Image File: [________________]        │
│             ⚠️ No file specified      │
│                                       │
│ [Read] ← DISABLED (grau)              │
│                                       │
│ Tooltip: "Error: No output file"      │
└───────────────────────────────────────┘
```

### **Nach File-Auswahl:**

```
┌─ Operations ──────────────────────────┐
│                                       │
│ Image File: [/home/disk.d64] ✓       │
│                                       │
│ [Read] ← ENABLED (normal)             │
│                                       │
│ Tooltip: "Read disk to image file"   │
└───────────────────────────────────────┘
```

---

## 📊 **Beispiel 6: Tab 1 (Workflow) - XUM1541 + Non-C64**

### **Konflikt:**

```
┌─ Workflow ────────────────────────────┐
│ Hardware: XUM1541                     │
│ Format:   IBM PC (MFM)  ← FEHLER!    │
│                                       │
│ 🔴 ERROR: XUM1541 only supports C64   │
│                                       │
│ [Auto-Fix to C64 GCR]                 │
└───────────────────────────────────────┘
```

---

## 📊 **Beispiel 7: USB Floppy + Flux Capture**

### **Tab 7 (Advanced) - Expert:**

```
┌─ Expert Options ──────────────────────┐
│ ☐ Raw Mode                            │
│ ☐ Preserve Timestamps                │
│ ☑ Capture Raw Flux  ← FEHLER!        │
│                                       │
│ 🔴 ERROR: USB Floppy cannot capture   │
│    raw flux data                      │
│                                       │
│ Use Greaseweazle/SCP for flux capture│
│ OR disable this option                │
│                                       │
│ [Disable Flux Capture]                │
└───────────────────────────────────────┘
```

---

## 📊 **Beispiel 8: Capacity Mismatch (Warning)**

### **Tab 1 (Workflow) - Preview:**

```
┌─ Workflow Preview ────────────────────┐
│                                       │
│ Detected Drive: 3.5" HD (1.44M)       │
│ Selected Format: 720K DD              │
│                                       │
│ 🟡 WARNING: Capacity Mismatch         │
│                                       │
│ Drive:  1.44M (HD)                    │
│ Format:  720K (DD)                    │
│                                       │
│ This is OK if you're formatting an HD │
│ disk as DD (common practice).         │
│                                       │
│ [Continue] [Change Format to 1.44M]   │
└───────────────────────────────────────┘
```

---

## 📊 **Beispiel 9: Batch Queue Validation**

### **Tab 7 (Advanced) - Batch:**

```
┌─ Batch Queue ─────────────────────────┐
│ # | Source   | Dest    | Status      │
│───┼──────────┼─────────┼─────────────│
│ 1 | disk.adf | out.adf | ✓ Ready     │
│ 2 | game.d64 | [NONE]  | ❌ No dest  │
│ 3 | data.img | out.img | ✓ Ready     │
└───────────────────────────────────────┘

⚠️ Item 2 has no destination file!
Fix required before starting batch.

[Fix Item 2] [Remove Item 2] [Cancel]
```

---

## 🎨 **Visual Cues:**

### **Color Coding:**

```
✅ Green:  Valid / OK
🟡 Orange: Warning (non-critical)
🔴 Red:    Error (critical)
🔵 Blue:   Info
⚪ Gray:   Disabled
```

### **Icons:**

```
✓  Valid
⚠️  Warning
❌  Error
ℹ️  Info
⏸️  Disabled
🔒  Locked (read-only)
```

### **Label Styles:**

```css
/* Valid */
color: rgb(0, 128, 0);
font-weight: normal;

/* Warning */
color: rgb(255, 128, 0);
font-weight: bold;

/* Error */
color: rgb(255, 0, 0);
font-weight: bold;

/* Info */
color: rgb(0, 100, 200);
font-weight: normal;

/* Disabled */
color: rgb(128, 128, 128);
font-weight: normal;
```

---

## 💡 **Best Practices:**

### **1. Inline Validation (Real-Time):**

```
✅ Show warning IMMEDIATELY after input
✅ Keep warning visible until fixed
✅ Offer Auto-Fix button if possible
✅ Clear warning when value is valid
```

### **2. Pre-Operation Validation (Dialog):**

```
✅ Validate before [Read]/[Write]/[Convert]
✅ Show ALL errors/warnings in one dialog
✅ Provide clear suggestions
✅ Allow "Continue Anyway" for warnings only
✅ Block operation for critical errors
```

### **3. Button States:**

```
✅ Disable buttons when validation fails
✅ Show tooltip explaining why disabled
✅ Re-enable when validation passes
✅ Visual feedback (color, cursor)
```

### **4. Progressive Validation:**

```
Level 1: Input validation (real-time)
Level 2: Tab-level validation (on tab change)
Level 3: Global validation (on operation start)
```

---

## 📋 **Validation Checklist (für jeden Tab):**

### **Tab 1 (Workflow):**

```
✅ Hardware Type selected?
✅ Format Type selected?
✅ Source/Dest compatible?
✅ Hardware supports format?
```

### **Tab 2 (Operations):**

```
✅ File path valid?
✅ File exists (for Write)?
✅ Output path writable?
✅ Disk present (for Read)?
```

### **Tab 3 (Format):**

```
✅ Tracks: 35 ≤ x ≤ 83?
✅ Sectors: 1 ≤ x ≤ 64?
✅ Sector Size: valid power-of-2?
✅ Track Type matches format?
```

### **Tab 4 (Geometry):**

```
✅ CHS values reasonable?
✅ Preset matches manual values?
✅ Format hint matches settings?
```

### **Tab 5 (Protection):**

```
✅ Profile matches format?
✅ C64 Nibbler only for C64?
✅ X-Copy only for Amiga?
✅ DiskDupe only for Amiga?
```

### **Tab 6 (Flux):**

```
✅ Max Revs < 10 (warning)?
✅ DPM + Error Policy compatible?
✅ Flux Capture supported by hardware?
```

### **Tab 7 (Advanced):**

```
✅ Batch queue items valid?
✅ Log file path writable?
✅ Expert options compatible?
```

### **Tab 8 (Hardware):**

```
✅ Device path exists?
✅ Drive type set (if Greaseweazle)?
✅ Manual override values valid?
```

---

## 🚀 **Implementation Steps:**

```
1. ✅ dialog_validation.ui created
2. Add validation labels to all tabs
3. Implement validation logic (C++)
4. Connect to all input fields
5. Update button states dynamically
6. Add Auto-Fix buttons where possible
7. Test all error scenarios
```

---

**© 2025 - UnifiedFloppyTool v3.1 - Inline Validation Examples**
