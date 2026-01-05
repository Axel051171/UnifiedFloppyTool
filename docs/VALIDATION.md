# ⚠️ Validation & Error Handling - COMPLETE!

## 🎯 **Pre-Flight Validation System**

### **Checks VOR jeder Operation:**

```
[Read] Button clicked
    ↓
┌─ PRE-FLIGHT CHECKS ──────────────────────────┐
│                                               │
│ 1. Hardware Connected?                       │
│    └── ❌ "No flux hardware detected!"       │
│                                               │
│ 2. Disk Inserted?                            │
│    └── ❌ "No disk in drive!"                │
│                                               │
│ 3. Format Set?                               │
│    └── ⚠️ "No format selected - auto-detect?"│
│                                               │
│ 4. Drive Type Matches Disk Type?             │
│    └── ❌ "3.5" disk in 5.25" drive!"        │
│                                               │
│ 5. Density Matches?                          │
│    └── ❌ "HD disk in DD-only drive!"        │
│                                               │
│ 6. Output File Path Set?                     │
│    └── ❌ "No output file specified!"        │
│                                               │
│ 7. Output File Exists?                       │
│    └── ⚠️ "File exists - overwrite?"         │
│                                               │
│ 8. Protection Settings Valid?                │
│    └── ⚠️ "C64 Nibbler active but Format = PC"│
│                                               │
│ 9. Workflow Consistent?                      │
│    └── ❌ "Source/Dest both set to Disk!"    │
│                                               │
│ 10. Disk Space Available?                    │
│     └── ❌ "Not enough disk space!"          │
│                                               │
└───────────────────────────────────────────────┘
    ↓
ALL CHECKS PASSED? → [Read] Operation starts ✓
ANY FAILED?        → Show Error/Warning Dialog ⚠️
```

---

## 📋 **Error/Warning Dialog System:**

### **Dialog Types:**

```
ERROR (Red ❌):
├── Blocks operation completely
├── User MUST fix before continuing
└── Example: "No hardware connected!"

WARNING (Orange ⚠️):
├── Operation can continue (risky)
├── User can proceed or cancel
└── Example: "Format unknown - proceed anyway?"

INFO (Blue ℹ️):
├── Information only
├── User confirms understanding
└── Example: "This will take ~5 minutes"

CONFIRMATION (Yellow ❓):
├── User must confirm action
├── Cancel or Proceed
└── Example: "Overwrite existing file?"
```

---

## 🎨 **Validation Dialog Examples:**

### **Example 1: No Format Set**

```
┌─ WARNING: No Format Selected ──────────────────┐
│                                                 │
│ ⚠️ You haven't selected a disk format!         │
│                                                 │
│ Options:                                        │
│                                                 │
│ ⚫ Auto-Detect Format (Recommended)             │
│    → Will analyze disk and detect format       │
│                                                 │
│ ◯ Set Format Manually                          │
│    → Go to Tab 1 (Workflow) and select format  │
│                                                 │
│ ◯ Proceed Anyway (Advanced Users)              │
│    → May result in incorrect image!            │
│                                                 │
│ [Auto-Detect] [Set Manually] [Cancel]          │
└─────────────────────────────────────────────────┘
```

### **Example 2: Drive/Disk Mismatch**

```
┌─ ERROR: Drive Type Mismatch ────────────────────┐
│                                                  │
│ ❌ 3.5" disk detected in 5.25" drive!            │
│                                                  │
│ Detected Drive:                                  │
│ ├── Type: 5.25" DD (40 Tracks)                  │
│ └── Size: 5.25 inches                           │
│                                                  │
│ Detected Disk:                                   │
│ ├── Type: 3.5" HD (80 Tracks)                   │
│ └── Size: 3.5 inches                            │
│                                                  │
│ ⚠️ This will NOT work!                           │
│                                                  │
│ Solutions:                                       │
│ 1. Change to correct drive                      │
│ 2. Insert correct disk                          │
│ 3. Check Tab 8 (Hardware) settings              │
│                                                  │
│ [Go to Hardware Tab] [Cancel]                   │
└──────────────────────────────────────────────────┘
```

### **Example 3: Density Mismatch**

```
┌─ WARNING: Density Mismatch ─────────────────────┐
│                                                  │
│ ⚠️ HD (High Density) disk in DD-only drive!     │
│                                                  │
│ Drive Capabilities:                              │
│ └── Max Density: DD (Double Density)            │
│                                                  │
│ Disk Requirements:                               │
│ └── Required: HD (High Density)                 │
│                                                  │
│ ⚠️ This may result in:                           │
│ ├── Read errors                                 │
│ ├── Incomplete data                             │
│ └── Failed verification                         │
│                                                  │
│ Recommendations:                                 │
│ 1. Use HD-capable drive                         │
│ 2. Use DD disk instead                          │
│                                                  │
│ [Change Drive] [Change Disk] [Proceed Anyway] [Cancel] │
└──────────────────────────────────────────────────┘
```

### **Example 4: Unknown Disk Type**

```
┌─ WARNING: Unknown Disk Type ────────────────────┐
│                                                  │
│ ⚠️ Cannot determine disk type!                  │
│                                                  │
│ Detection Results:                               │
│ ├── Drive Type: 3.5" (detected ✓)              │
│ ├── Disk Inserted: Yes (detected ✓)            │
│ ├── Disk Size: Unknown ❌                       │
│ ├── Format: Unknown ❌                          │
│ └── Density: Unknown ❌                         │
│                                                  │
│ Possible Reasons:                                │
│ ├── Non-standard disk                           │
│ ├── Copy-protected disk                         │
│ ├── Damaged disk                                │
│ └── Blank/unformatted disk                      │
│                                                  │
│ What would you like to do?                      │
│                                                  │
│ ⚫ Try Manual Detection                          │
│    → Test disk with different formats           │
│                                                  │
│ ◯ Set Type Manually                             │
│    → Specify disk type in Tab 1                 │
│                                                  │
│ ◯ Proceed with Flux Capture                     │
│    → Capture raw flux (safest for unknown)      │
│                                                  │
│ [Try Detection] [Set Manually] [Flux Capture] [Cancel] │
└──────────────────────────────────────────────────┘
```

### **Example 5: 3.5" vs 5.25" Detection**

```
┌─ INFO: Disk Size Detected ──────────────────────┐
│                                                  │
│ ℹ️ Physical disk size detected!                 │
│                                                  │
│ Detected:                                        │
│ ├── Physical Size: 3.5 inches ✓                │
│ ├── Drive Type: 3.5" HD ✓                      │
│ └── Compatible: Yes ✓                          │
│                                                  │
│ Auto-Configuration:                              │
│ ├── Format: IBM PC (MFM) ✓                     │
│ ├── Tracks: 80 ✓                               │
│ ├── Density: HD ✓                              │
│ └── Expected Capacity: 1.44 MB                  │
│                                                  │
│ Is this correct?                                 │
│                                                  │
│ [Yes, Continue] [No, Change Settings] [Cancel]  │
└──────────────────────────────────────────────────┘
```

### **Example 6: Protection Settings Mismatch**

```
┌─ WARNING: Configuration Mismatch ───────────────┐
│                                                  │
│ ⚠️ Protection settings don't match format!      │
│                                                  │
│ Current Settings:                                │
│ ├── Format (Tab 1): IBM PC (MFM)               │
│ └── Protection (Tab 5): C64 Nibbler ACTIVE ❌  │
│                                                  │
│ ⚠️ Conflict Detected:                            │
│ C64 Nibbler is for GCR format, but you          │
│ selected IBM PC (MFM) format!                   │
│                                                  │
│ Auto-Fix Options:                                │
│                                                  │
│ ⚫ Change Format to "C64 (GCR)"                  │
│    → Matches C64 Nibbler protection             │
│                                                  │
│ ◯ Disable C64 Nibbler                           │
│    → Keep IBM PC (MFM) format                   │
│                                                  │
│ ◯ Ignore Warning (Advanced)                     │
│    → Proceed with conflicting settings          │
│                                                  │
│ [Change to C64] [Disable Nibbler] [Ignore] [Cancel] │
└──────────────────────────────────────────────────┘
```

### **Example 7: No Hardware Connected**

```
┌─ ERROR: No Hardware Connected ──────────────────┐
│                                                  │
│ ❌ No flux hardware detected!                    │
│                                                  │
│ Expected Hardware:                               │
│ └── Greaseweazle / SCP / KryoFlux / etc.        │
│                                                  │
│ Troubleshooting:                                 │
│ ├── Is hardware connected? (USB cable)          │
│ ├── Is hardware powered on?                     │
│ ├── Correct drivers installed?                  │
│ └── Check Tab 8 (Hardware) settings             │
│                                                  │
│ [Auto-Detect Hardware] [Manual Setup] [Cancel]  │
└──────────────────────────────────────────────────┘
```

### **Example 8: File Exists - Overwrite?**

```
┌─ CONFIRMATION: File Exists ─────────────────────┐
│                                                  │
│ ❓ Output file already exists!                   │
│                                                  │
│ File: game_001.d64                              │
│ ├── Size: 174,848 bytes                        │
│ ├── Modified: 2025-12-27 15:32:11              │
│ └── Format: D64 (C64 1541)                     │
│                                                  │
│ What would you like to do?                      │
│                                                  │
│ ⚫ Overwrite                                     │
│    → Replace existing file                      │
│                                                  │
│ ◯ Auto-Rename                                   │
│    → Save as "game_001_v2.d64"                  │
│                                                  │
│ ◯ Choose New Name                               │
│    → Specify different filename                 │
│                                                  │
│ ◯ Compare First                                 │
│    → Compare disk with existing file            │
│                                                  │
│ ☐ Remember my choice for this session           │
│                                                  │
│ [Overwrite] [Auto-Rename] [New Name] [Compare] [Cancel] │
└──────────────────────────────────────────────────┘
```

---

## 🔧 **Smart Auto-Fix System:**

### **Auto-Fix Examples:**

```
Problem: C64 Nibbler active but Format = IBM PC
Auto-Fix: Change Format to "C64 (GCR)" ✓

Problem: 3.5" disk detected but Drive = 5.25"
Auto-Fix: Update Drive Type to 3.5" ✓

Problem: No output file specified
Auto-Fix: Generate name "disk_001.img" ✓

Problem: Manual Override enabled but Detection succeeded
Auto-Fix: Suggest using detected values ✓

Problem: Workflow = Disk→Disk but only 1 drive
Auto-Fix: Suggest Disk→File→Disk workflow ✓
```

---

## 📊 **Validation Flow:**

### **Complete Validation Sequence:**

```
User clicks [Read]
    ↓
┌─ STEP 1: Hardware Check ──────────────────┐
│ ✓ Hardware connected?                     │
│ ✓ Hardware responding?                    │
│ ✓ Firmware version OK?                    │
└───────────────────────────────────────────┘
    ↓ PASS
┌─ STEP 2: Drive Check ─────────────────────┐
│ ✓ Drive detected?                         │
│ ✓ Drive type known?                       │
│ ✓ Drive ready?                            │
└───────────────────────────────────────────┘
    ↓ PASS
┌─ STEP 3: Disk Check ──────────────────────┐
│ ✓ Disk inserted?                          │
│ ✓ Disk type matches drive?                │
│ ✓ Disk readable?                          │
└───────────────────────────────────────────┘
    ↓ PASS
┌─ STEP 4: Format Check ────────────────────┐
│ ✓ Format selected?                        │
│ ✓ Format matches disk?                    │
│ ✓ Protection settings consistent?         │
└───────────────────────────────────────────┘
    ↓ PASS
┌─ STEP 5: Output Check ────────────────────┐
│ ✓ Output path specified?                  │
│ ✓ Directory exists?                       │
│ ✓ Disk space available?                   │
│ ✓ File exists (confirm overwrite)?        │
└───────────────────────────────────────────┘
    ↓ ALL PASSED
┌─ STEP 6: START OPERATION ─────────────────┐
│ ✓ All checks passed!                      │
│ → Starting read operation...              │
└───────────────────────────────────────────┘
```

---

## 🎨 **Status Messages System:**

### **Real-Time Status Updates:**

```
Status Bar (Bottom of Window):

Before Operation:
└── "Ready - Waiting for operation"

During Hardware Check:
└── "Checking hardware... (1/6)"

During Drive Detection:
└── "Detecting drive type... (2/6)"

During Disk Analysis:
└── "Analyzing disk format... (3/6)"

During Read:
├── "Reading Track 05/80 (6%) - 00:02:15 remaining"
├── Progress Bar: [██░░░░░░░░░░░░░░░░] 6%
└── "Speed: 1.2 KB/s, Errors: 0, Retries: 2"

After Success:
└── "✓ Operation completed successfully! Saved: game_001.d64"

After Error:
└── "❌ Operation failed! Click for details."
```

---

## 🔍 **Detailed Error Codes:**

### **Error Code System:**

```
UFT-001: Hardware Not Found
UFT-002: Hardware Communication Error
UFT-003: Firmware Version Incompatible
UFT-004: Drive Not Detected
UFT-005: Drive Type Mismatch
UFT-006: No Disk Inserted
UFT-007: Disk Type Unknown
UFT-008: Density Mismatch
UFT-009: Format Not Selected
UFT-010: Protection Settings Invalid
UFT-011: Output Path Invalid
UFT-012: File Exists (Overwrite?)
UFT-013: Insufficient Disk Space
UFT-014: Read Error (Track N)
UFT-015: Write Error (Track N)
UFT-016: Verification Failed
UFT-017: Timeout
UFT-018: User Cancelled
UFT-019: Unknown Error

Each error has:
├── Code (UFT-XXX)
├── Short Description
├── Long Description
├── Possible Causes
├── Solutions
└── Help Link
```

---

## 📋 **Validation Checklist UI:**

### **Pre-Flight Checklist (Optional Display):**

```
┌─ Pre-Flight Checklist ────────────────────┐
│                                            │
│ ✓ Hardware Connected (Greaseweazle F7)    │
│ ✓ Drive Detected (3.5" HD, 80 Tracks)     │
│ ✓ Disk Inserted (3.5" 1.44M)              │
│ ✓ Format Selected (IBM PC MFM)            │
│ ✓ Output File (game_001.img)              │
│ ⚠️ File Exists (will overwrite)            │
│ ✓ Disk Space (1.5 MB available)           │
│                                            │
│ [Continue] [Cancel] [Details]             │
└────────────────────────────────────────────┘
```

---

## 🛠️ **Implementation Notes:**

### **Where to Add Validation:**

```
Tab 2 (Operations):
├── Before [Read]: validate_read_operation()
├── Before [Write]: validate_write_operation()
├── Before [Verify]: validate_verify_operation()
└── Before [Format]: validate_format_operation()

Each validates:
├── Hardware State
├── Drive State
├── Disk State
├── Format Settings
├── Protection Settings
└── Output Settings
```

### **Validation Functions (Pseudo-Code):**

```cpp
bool validate_read_operation() {
    // Step 1: Hardware
    if (!hardware_connected()) {
        show_error("UFT-001: No Hardware Connected");
        return false;
    }
    
    // Step 2: Drive
    if (!drive_detected()) {
        show_error("UFT-004: Drive Not Detected");
        return false;
    }
    
    // Step 3: Disk
    if (!disk_inserted()) {
        show_error("UFT-006: No Disk Inserted");
        return false;
    }
    
    // Step 4: Drive/Disk Match
    if (!drive_disk_compatible()) {
        show_error("UFT-005: Drive Type Mismatch");
        return false;
    }
    
    // Step 5: Format
    if (!format_selected() && !auto_detect_enabled()) {
        int result = show_warning_with_options(
            "UFT-009: No Format Selected",
            {"Auto-Detect", "Set Manually", "Proceed Anyway"}
        );
        if (result == CANCEL) return false;
    }
    
    // Step 6: Protection Consistency
    if (!protection_settings_valid()) {
        show_warning("UFT-010: Protection Settings Invalid");
        // Offer auto-fix...
    }
    
    // Step 7: Output
    if (!output_file_specified()) {
        show_error("UFT-011: No Output File");
        return false;
    }
    
    // Step 8: File Exists
    if (file_exists(output_file)) {
        int result = show_confirmation(
            "UFT-012: File Exists",
            {"Overwrite", "Auto-Rename", "Cancel"}
        );
        if (result == CANCEL) return false;
    }
    
    // All checks passed!
    return true;
}
```

---

## ✅ **Summary:**

```
✅ Pre-Flight Validation (10 Checks)
✅ Error Dialogs (Red ❌)
✅ Warning Dialogs (Orange ⚠️)
✅ Info Dialogs (Blue ℹ️)
✅ Confirmation Dialogs (Yellow ❓)
✅ Auto-Fix System
✅ Smart Suggestions
✅ Error Codes (UFT-001 to UFT-019)
✅ Status Messages
✅ Progress Tracking
✅ Detailed Error Descriptions
```

---

**© 2025 - UnifiedFloppyTool v3.1 - Validation Edition**
