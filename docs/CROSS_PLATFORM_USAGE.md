# CROSS-PLATFORM FIXES - USAGE EXAMPLES

## 📚 HOW TO USE THE NEW UTILITIES

### 1. ENDIANNESS-SAFE BINARY I/O

**Reading SCP File Header:**
```c
#include "uft/uft_endian.h"

// ❌ OLD WAY (WRONG - breaks on big-endian, alignment issues):
struct SCPHeader {
    uint32_t magic;
    uint8_t version;
    uint8_t disk_type;
    uint32_t num_revolutions;
} __attribute__((packed));

uint8_t file_data[1024];
fread(file_data, 1, 1024, fp);
SCPHeader* header = (SCPHeader*)file_data;  // ❌ WRONG!


// ✅ NEW WAY (CORRECT - works everywhere):
#include "uft/uft_endian.h"

uint8_t file_data[1024];
fread(file_data, 1, 1024, fp);

uint32_t magic = uft_read_le32(file_data);           // ✅ CORRECT!
uint8_t version = file_data[4];                       // ✅ CORRECT!
uint8_t disk_type = file_data[5];                     // ✅ CORRECT!
uint32_t num_revolutions = uft_read_le32(file_data + 8); // ✅ CORRECT!
```

**Writing IMD File:**
```c
#include "uft/uft_endian.h"

uint8_t header[16];

// Write little-endian values
uft_write_le32(header, 0x494D4420);  // "IMD "
uft_write_le16(header + 4, 0x0100);  // Version 1.0

fwrite(header, 1, 16, fp);
```

---

### 2. CROSS-PLATFORM PATH HANDLING

**In Qt/C++ Code:**
```cpp
#include "pathutils.h"

// ❌ OLD WAY (WRONG):
QString output = "C:/Users/" + username + "/Documents/" + filename;  // ❌ Linux?
QString output = dir + "\\" + filename;  // ❌ macOS?


// ✅ NEW WAY (CORRECT):
QString output = PathUtils::join(dir, filename);  // ✅ All platforms!

// Get default output directory
QString outputDir = PathUtils::defaultOutputDir();
// Windows: C:/Users/USERNAME/Documents/UnifiedFloppyTool
// macOS: ~/Documents/UnifiedFloppyTool
// Linux: ~/Documents/UnifiedFloppyTool

// Build complex path
QString imagePath = PathUtils::join({outputDir, "archives", "disk001.scp"});

// Check if file exists
if (!PathUtils::exists(imagePath)) {
    // Create directories
    PathUtils::mkdirs(PathUtils::dirname(imagePath));
}

// Pass to C core (UTF-8)
std::string utf8path = PathUtils::toUtf8(imagePath);
decode_file(utf8path.c_str());
```

---

### 3. INPUT VALIDATION

**In GUI Code (Before Calling C Core):**
```cpp
#include "inputvalidation.h"

void MyDialog::onDecodeClicked()
{
    // Get values from UI
    int tracks = ui->spinTracks->value();
    int sectors = ui->spinSectors->value();
    QString inputFile = ui->editInputFile->text();
    QString encoding = ui->comboEncoding->currentText();
    
    // ❌ OLD WAY (WRONG - no validation):
    decode_disk(tracks, sectors);  // ❌ Crashes if values invalid!
    
    
    // ✅ NEW WAY (CORRECT - validate everything):
    
    // Validate geometry
    if (!InputValidation::validateTracks(tracks)) {
        QMessageBox::warning(this, "Error", InputValidation::lastError());
        return;
    }
    
    if (!InputValidation::validateSectors(sectors)) {
        QMessageBox::warning(this, "Error", InputValidation::lastError());
        return;
    }
    
    // Validate file
    if (!InputValidation::validateInputFile(inputFile)) {
        QMessageBox::warning(this, "Error", InputValidation::lastError());
        return;
    }
    
    // Validate encoding
    if (!InputValidation::validateEncoding(encoding)) {
        QMessageBox::warning(this, "Error", InputValidation::lastError());
        return;
    }
    
    // All validated - safe to proceed!
    decode_disk(tracks, sectors);  // ✅ Safe!
}
```

---

### 4. SETTINGS MANAGER

**Application Startup:**
```cpp
#include "settingsmanager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Load settings
    SettingsManager* settings = SettingsManager::instance();
    settings->load();
    
    // Create main window
    MainWindow window;
    
    // Apply settings to UI
    window.ui->spinTracks->setValue(settings->tracks());
    window.ui->spinSectors->setValue(settings->sectors());
    window.ui->comboEncoding->setCurrentText(settings->encoding());
    
    // Connect UI to settings (bidirectional!)
    QObject::connect(window.ui->spinTracks, QOverload<int>::of(&QSpinBox::valueChanged),
                     settings, &SettingsManager::setTracks);
    
    QObject::connect(settings, &SettingsManager::tracksChanged,
                     window.ui->spinTracks, &QSpinBox::setValue);
    
    window.show();
    
    int result = app.exec();
    
    // Save on exit
    settings->save();
    
    return result;
}
```

**In Your Dialog:**
```cpp
void FormatTab::onEncodingChanged(const QString& encoding)
{
    // ❌ OLD WAY (WRONG - settings verpuffen):
    ui->labelEncoding->setText(encoding);
    // Backend weiß nichts! ❌
    
    
    // ✅ NEW WAY (CORRECT - synchronized):
    SettingsManager* settings = SettingsManager::instance();
    settings->setEncoding(encoding);  // ← Saves automatically!
    
    // UI updates via signal/slot connection
    // Backend can read via settings->encoding()
}

void FormatTab::setupConnections()
{
    SettingsManager* settings = SettingsManager::instance();
    
    // UI → Settings
    connect(ui->comboEncoding, &QComboBox::currentTextChanged,
            settings, &SettingsManager::setEncoding);
    
    // Settings → UI (for external changes)
    connect(settings, &SettingsManager::encodingChanged,
            ui->comboEncoding, &QComboBox::setCurrentText);
}
```

---

## 🎯 COMPLETE EXAMPLE: File Selection Dialog

```cpp
#include "pathutils.h"
#include "inputvalidation.h"
#include "settingsmanager.h"

void WorkflowTab::onSourceFileClicked()
{
    SettingsManager* settings = SettingsManager::instance();
    
    // Get last used directory (or default)
    QString lastDir = settings->outputDir();
    if (lastDir.isEmpty()) {
        lastDir = PathUtils::defaultOutputDir();
    }
    
    // Show file dialog
    QString filename = QFileDialog::getOpenFileName(
        this,
        "Select Source File",
        lastDir,
        "Flux Files (*.scp *.hfe *.kfraw);;All Files (*.*)"
    );
    
    if (filename.isEmpty()) {
        return;  // Cancelled
    }
    
    // Validate file
    if (!InputValidation::validateInputFile(filename)) {
        QMessageBox::warning(this, "Error", InputValidation::lastError());
        return;
    }
    
    // Validate extension
    if (!InputValidation::validateExtension(filename, {"scp", "hfe", "kfraw"})) {
        QMessageBox::warning(this, "Error", InputValidation::lastError());
        return;
    }
    
    // Save directory for next time
    settings->setOutputDir(PathUtils::dirname(filename));
    
    // Update UI
    ui->labelSourceStatus->setText(QString(
        "Mode: Image File\n"
        "File: %1\n"
        "Status: Ready"
    ).arg(PathUtils::filename(filename)));
    
    // Pass to C core (UTF-8)
    m_currentFile = PathUtils::toUtf8(filename);
}
```

---

## 📋 MIGRATION CHECKLIST

When updating existing code:

### For Paths:
- [ ] Replace all `"/" +` and `"\\" +` with `PathUtils::join()`
- [ ] Use `PathUtils::defaultOutputDir()` for default paths
- [ ] Use `PathUtils::toUtf8()` when passing to C core
- [ ] Use `QDir::toNativeSeparators()` for display only

### For Binary I/O:
- [ ] Replace all struct casts with `uft_read_le*()` calls
- [ ] Document endianness in comments
- [ ] Test on both little and big-endian (or emulator)

### For Validation:
- [ ] Add validation before EVERY C core call
- [ ] Show error messages to user
- [ ] Never trust UI values directly

### For Settings:
- [ ] Connect ALL UI widgets to SettingsManager
- [ ] Bidirectional connections (UI ↔ Settings)
- [ ] Load on startup, save on exit
- [ ] Use auto-save for critical settings

---

## ✅ TESTING

**Test Cross-Platform Paths:**
```cpp
// Create test file
QString testPath = PathUtils::join({"test", "subdir", "file.txt"});
qDebug() << testPath;
// Windows: test/subdir/file.txt (Qt always uses /)
// Linux: test/subdir/file.txt
// macOS: test/subdir/file.txt

QString native = PathUtils::toNative(testPath);
qDebug() << native;
// Windows: test\subdir\file.txt
// Linux: test/subdir/file.txt
// macOS: test/subdir/file.txt
```

**Test Endianness:**
```c
uint8_t buf[4] = {0x78, 0x56, 0x34, 0x12};
uint32_t value = uft_read_le32(buf);
assert(value == 0x12345678);  // Works on ALL platforms!
```

---

**ALL UTILITIES ARE HEADER-ONLY - JUST #include THEM!**
