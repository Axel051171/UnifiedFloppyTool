/**
 * @file uft_input_validator.cpp
 * @brief UFT Input Validation Implementierung
 * @version 3.8.0
 */

#include "uft_input_validator.h"
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QStyle>

namespace UFT {

// ============================================================================
// Stil-Konstanten
// ============================================================================

const QString InputValidator::STYLE_VALID = 
    "border: 1px solid #4CAF50; background-color: #E8F5E9;";

const QString InputValidator::STYLE_WARNING = 
    "border: 1px solid #FFC107; background-color: #FFF8E1;";

const QString InputValidator::STYLE_ERROR = 
    "border: 2px solid #F44336; background-color: #FFEBEE;";

const QString InputValidator::STYLE_EMPTY = 
    "border: 1px solid #9E9E9E; background-color: #FAFAFA;";

// ============================================================================
// Konstruktor
// ============================================================================

InputValidator::InputValidator(QObject *parent)
    : QObject(parent)
{
}

// ============================================================================
// Integer-Bereich Validierung
// ============================================================================

ValidationError InputValidator::validateIntRange(
    const QString& fieldName,
    int value,
    int minVal,
    int maxVal,
    const QString& unit)
{
    ValidationError error;
    error.fieldName = fieldName;
    error.currentValue = value;
    
    QString unitStr = unit.isEmpty() ? "" : " " + unit;
    
    if (value < minVal) {
        error.result = ValidationResult::Error;
        error.message = QString("%1 ist zu klein").arg(fieldName);
        error.hint = QString("Minimum: %1%2\n"
                            "Aktuell: %3%2\n"
                            "Erhöhen Sie den Wert um mindestens %4.")
                     .arg(minVal).arg(unitStr).arg(value).arg(minVal - value);
        error.example = QString("Gültig: %1 - %2%3").arg(minVal).arg(maxVal).arg(unitStr);
        error.suggestedValue = minVal;
    }
    else if (value > maxVal) {
        error.result = ValidationResult::Error;
        error.message = QString("%1 ist zu groß").arg(fieldName);
        error.hint = QString("Maximum: %1%2\n"
                            "Aktuell: %3%2\n"
                            "Verringern Sie den Wert um mindestens %4.")
                     .arg(maxVal).arg(unitStr).arg(value).arg(value - maxVal);
        error.example = QString("Gültig: %1 - %2%3").arg(minVal).arg(maxVal).arg(unitStr);
        error.suggestedValue = maxVal;
    }
    else {
        error.result = ValidationResult::Valid;
        error.message = QString("%1 OK").arg(fieldName);
        
        // Warnungen für Grenzwerte
        if (value == minVal || value == maxVal) {
            error.result = ValidationResult::Warning;
            error.hint = QString("Grenzwert! %1 ist %2 %3%4")
                        .arg(fieldName)
                        .arg(value == minVal ? "am Minimum" : "am Maximum")
                        .arg(value)
                        .arg(unitStr);
        }
    }
    
    return error;
}

// ============================================================================
// Double-Bereich Validierung
// ============================================================================

ValidationError InputValidator::validateDoubleRange(
    const QString& fieldName,
    double value,
    double minVal,
    double maxVal,
    int decimals,
    const QString& unit)
{
    ValidationError error;
    error.fieldName = fieldName;
    error.currentValue = value;
    
    QString unitStr = unit.isEmpty() ? "" : " " + unit;
    
    if (value < minVal) {
        error.result = ValidationResult::Error;
        error.message = QString("%1 ist zu klein").arg(fieldName);
        error.hint = QString("Minimum: %1%2\n"
                            "Aktuell: %3%2\n"
                            "Tipp: Verwenden Sie einen Wert ≥ %1")
                     .arg(minVal, 0, 'f', decimals)
                     .arg(unitStr)
                     .arg(value, 0, 'f', decimals);
        error.example = QString("Beispiel: %1%2")
                       .arg((minVal + maxVal) / 2, 0, 'f', decimals)
                       .arg(unitStr);
        error.suggestedValue = minVal;
    }
    else if (value > maxVal) {
        error.result = ValidationResult::Error;
        error.message = QString("%1 ist zu groß").arg(fieldName);
        error.hint = QString("Maximum: %1%2\n"
                            "Aktuell: %3%2\n"
                            "Tipp: Verwenden Sie einen Wert ≤ %1")
                     .arg(maxVal, 0, 'f', decimals)
                     .arg(unitStr)
                     .arg(value, 0, 'f', decimals);
        error.example = QString("Beispiel: %1%2")
                       .arg((minVal + maxVal) / 2, 0, 'f', decimals)
                       .arg(unitStr);
        error.suggestedValue = maxVal;
    }
    else {
        error.result = ValidationResult::Valid;
        error.message = QString("%1 OK").arg(fieldName);
    }
    
    return error;
}

// ============================================================================
// Hex-Validierung
// ============================================================================

ValidationError InputValidator::validateHex(
    const QString& fieldName,
    const QString& value,
    int minBytes,
    int maxBytes)
{
    ValidationError error;
    error.fieldName = fieldName;
    error.currentValue = value;
    
    QString cleanValue = value.trimmed();
    
    // Entferne optionales "0x" Präfix
    if (cleanValue.startsWith("0x", Qt::CaseInsensitive)) {
        cleanValue = cleanValue.mid(2);
    }
    
    // Prüfe auf leere Eingabe
    if (cleanValue.isEmpty()) {
        error.result = ValidationResult::Empty;
        error.message = QString("%1 ist leer").arg(fieldName);
        error.hint = "Geben Sie einen Hex-Wert ein.";
        error.example = QString("Format: %1 (z.B. %2)")
                       .arg(minBytes == 1 ? "XX" : "XX XX ...")
                       .arg(minBytes == 1 ? "4E" : "4E 00 FF");
        return error;
    }
    
    // Prüfe auf gültige Hex-Zeichen
    QRegularExpression hexRegex("^[0-9A-Fa-f\\s]+$");
    if (!hexRegex.match(cleanValue).hasMatch()) {
        error.result = ValidationResult::Error;
        error.message = QString("%1: Ungültige Zeichen").arg(fieldName);
        error.hint = "Nur Hex-Zeichen erlaubt: 0-9, A-F\n"
                     "Ungültige Zeichen gefunden!";
        error.example = "Gültig: 00, 4E, FF, a5\n"
                       "Ungültig: GG, XY, -1";
        return error;
    }
    
    // Entferne Leerzeichen und prüfe Länge
    cleanValue.remove(' ');
    int byteCount = (cleanValue.length() + 1) / 2;
    
    if (cleanValue.length() % 2 != 0) {
        error.result = ValidationResult::Warning;
        error.message = QString("%1: Ungerade Zeichenzahl").arg(fieldName);
        error.hint = QString("Hex-Werte sollten paarweise sein.\n"
                            "Eingabe: '%1' (%2 Zeichen)\n"
                            "Tipp: Führende Null hinzufügen → '0%1'")
                     .arg(cleanValue).arg(cleanValue.length());
        error.suggestedValue = "0" + cleanValue;
        return error;
    }
    
    if (byteCount < minBytes) {
        error.result = ValidationResult::Error;
        error.message = QString("%1: Zu wenig Bytes").arg(fieldName);
        error.hint = QString("Minimum: %1 Byte(s)\n"
                            "Aktuell: %2 Byte(s)\n"
                            "Fügen Sie %3 weitere Byte(s) hinzu.")
                     .arg(minBytes).arg(byteCount).arg(minBytes - byteCount);
        error.example = QString("Beispiel: %1").arg(QString("00").repeated(minBytes));
        return error;
    }
    
    if (byteCount > maxBytes) {
        error.result = ValidationResult::Error;
        error.message = QString("%1: Zu viele Bytes").arg(fieldName);
        error.hint = QString("Maximum: %1 Byte(s)\n"
                            "Aktuell: %2 Byte(s)\n"
                            "Entfernen Sie %3 Byte(s).")
                     .arg(maxBytes).arg(byteCount).arg(byteCount - maxBytes);
        error.example = QString("Beispiel: %1").arg(QString("FF").repeated(maxBytes));
        return error;
    }
    
    error.result = ValidationResult::Valid;
    error.message = QString("%1 OK (0x%2)").arg(fieldName).arg(cleanValue.toUpper());
    return error;
}

// ============================================================================
// Dateipfad-Validierung
// ============================================================================

ValidationError InputValidator::validateFilePath(
    const QString& fieldName,
    const QString& path,
    bool mustExist,
    const QStringList& allowedExtensions)
{
    ValidationError error;
    error.fieldName = fieldName;
    error.currentValue = path;
    
    if (path.trimmed().isEmpty()) {
        error.result = ValidationResult::Empty;
        error.message = QString("%1: Kein Pfad angegeben").arg(fieldName);
        error.hint = "Bitte wählen Sie eine Datei aus.";
        return error;
    }
    
    QFileInfo fileInfo(path);
    
    if (mustExist && !fileInfo.exists()) {
        error.result = ValidationResult::Error;
        error.message = QString("%1: Datei nicht gefunden").arg(fieldName);
        error.hint = QString("Die Datei existiert nicht:\n%1\n\n"
                            "Prüfen Sie:\n"
                            "• Ist der Pfad korrekt?\n"
                            "• Existiert das Verzeichnis?\n"
                            "• Wurde die Datei verschoben?")
                     .arg(path);
        
        // Verzeichnis prüfen
        QDir dir = fileInfo.dir();
        if (!dir.exists()) {
            error.hint += QString("\n\nVerzeichnis existiert nicht:\n%1")
                         .arg(dir.absolutePath());
        }
        return error;
    }
    
    if (!allowedExtensions.isEmpty()) {
        QString ext = fileInfo.suffix().toLower();
        if (!allowedExtensions.contains(ext, Qt::CaseInsensitive)) {
            error.result = ValidationResult::Error;
            error.message = QString("%1: Ungültige Dateiendung").arg(fieldName);
            error.hint = QString("Erlaubte Endungen: %1\n"
                                "Aktuelle Endung: .%2\n\n"
                                "Bitte wählen Sie eine Datei mit\n"
                                "unterstütztem Format.")
                        .arg(allowedExtensions.join(", ."))
                        .arg(ext);
            error.example = QString("Beispiel: datei.%1").arg(allowedExtensions.first());
            return error;
        }
    }
    
    error.result = ValidationResult::Valid;
    error.message = QString("%1 OK").arg(fieldName);
    return error;
}

// ============================================================================
// Track-Validierung
// ============================================================================

ValidationError InputValidator::validateTrackRange(
    const QString& fieldName,
    int track,
    int maxTracks,
    bool allowHalfTracks)
{
    ValidationError error;
    error.fieldName = fieldName;
    error.currentValue = track;
    
    int effectiveMax = allowHalfTracks ? maxTracks * 2 : maxTracks;
    
    if (track < 0) {
        error.result = ValidationResult::Error;
        error.message = QString("%1: Negative Track-Nummer").arg(fieldName);
        error.hint = QString("Track-Nummern beginnen bei 0.\n"
                            "Eingabe: %1\n\n"
                            "Gültig: 0 - %2")
                     .arg(track).arg(effectiveMax - 1);
        error.suggestedValue = 0;
        return error;
    }
    
    if (track >= effectiveMax) {
        error.result = ValidationResult::Error;
        error.message = QString("%1: Track außerhalb des Bereichs").arg(fieldName);
        error.hint = QString("Maximale Track-Nummer: %1\n"
                            "Eingabe: %2\n\n"
                            "Standard-Disk-Typen:\n"
                            "• 5.25\" DD: 0-39 (40 Tracks)\n"
                            "• 5.25\" HD: 0-79 (80 Tracks)\n"
                            "• 3.5\" DD/HD: 0-79 (80 Tracks)\n"
                            "• C64 1541: 0-34 (35 Tracks)")
                     .arg(effectiveMax - 1).arg(track);
        error.suggestedValue = effectiveMax - 1;
        return error;
    }
    
    // Warnung für hohe Track-Nummern
    if (track > 79) {
        error.result = ValidationResult::Warning;
        error.message = QString("%1: Erweiterte Track-Nummer").arg(fieldName);
        error.hint = QString("Track %1 liegt im erweiterten Bereich.\n"
                            "Die meisten Disks haben max. 80 Tracks (0-79).\n\n"
                            "Prüfen Sie, ob Ihr Laufwerk\n"
                            "diesen Bereich unterstützt.")
                     .arg(track);
        return error;
    }
    
    error.result = ValidationResult::Valid;
    error.message = QString("%1 OK").arg(fieldName);
    return error;
}

// ============================================================================
// Sektor-Validierung
// ============================================================================

ValidationError InputValidator::validateSectorRange(
    const QString& fieldName,
    int sector,
    int maxSectors)
{
    ValidationError error;
    error.fieldName = fieldName;
    error.currentValue = sector;
    
    if (sector < 0) {
        error.result = ValidationResult::Error;
        error.message = QString("%1: Negative Sektor-Nummer").arg(fieldName);
        error.hint = QString("Sektoren beginnen bei 0 oder 1 (je nach Format).\n"
                            "Eingabe: %1\n\n"
                            "Gültig: 0/1 - %2")
                     .arg(sector).arg(maxSectors);
        error.suggestedValue = 0;
        return error;
    }
    
    if (sector > maxSectors) {
        error.result = ValidationResult::Error;
        error.message = QString("%1: Sektor außerhalb des Bereichs").arg(fieldName);
        error.hint = QString("Maximaler Sektor: %1\n"
                            "Eingabe: %2\n\n"
                            "Sektoren pro Track nach Format:\n"
                            "• Amiga DD: 11 Sektoren\n"
                            "• Amiga HD: 22 Sektoren\n"
                            "• PC 720KB: 9 Sektoren\n"
                            "• PC 1.44MB: 18 Sektoren\n"
                            "• C64: 17-21 Sektoren")
                     .arg(maxSectors).arg(sector);
        error.suggestedValue = maxSectors;
        return error;
    }
    
    error.result = ValidationResult::Valid;
    error.message = QString("%1 OK").arg(fieldName);
    return error;
}

// ============================================================================
// Bitrate-Validierung
// ============================================================================

ValidationError InputValidator::validateBitrate(
    const QString& fieldName,
    int bitrate)
{
    ValidationError error;
    error.fieldName = fieldName;
    error.currentValue = bitrate;
    
    // Standard Bitraten
    const QList<int> standardRates = {125000, 250000, 300000, 500000};
    
    if (bitrate < 100000) {
        error.result = ValidationResult::Error;
        error.message = QString("%1: Bitrate zu niedrig").arg(fieldName);
        error.hint = QString("Minimum: 100.000 bit/s\n"
                            "Eingabe: %1 bit/s\n\n"
                            "Standard-Bitraten:\n"
                            "• FM SD: 125.000\n"
                            "• MFM DD: 250.000\n"
                            "• MFM HD: 500.000")
                     .arg(bitrate);
        error.suggestedValue = 250000;
        return error;
    }
    
    if (bitrate > 1000000) {
        error.result = ValidationResult::Error;
        error.message = QString("%1: Bitrate zu hoch").arg(fieldName);
        error.hint = QString("Maximum: 1.000.000 bit/s\n"
                            "Eingabe: %1 bit/s\n\n"
                            "Hinweis: Standard-Floppy-Laufwerke\n"
                            "unterstützen max. 500.000 bit/s")
                     .arg(bitrate);
        error.suggestedValue = 500000;
        return error;
    }
    
    // Warnung für nicht-standard Bitraten
    if (!standardRates.contains(bitrate)) {
        int closest = standardRates.first();
        for (int rate : standardRates) {
            if (qAbs(rate - bitrate) < qAbs(closest - bitrate)) {
                closest = rate;
            }
        }
        
        error.result = ValidationResult::Warning;
        error.message = QString("%1: Nicht-Standard Bitrate").arg(fieldName);
        error.hint = QString("Eingabe: %1 bit/s\n\n"
                            "Nächste Standard-Rate: %2 bit/s\n\n"
                            "Nicht-Standard Bitraten können\n"
                            "Kompatibilitätsprobleme verursachen.")
                     .arg(bitrate).arg(closest);
        error.suggestedValue = closest;
        return error;
    }
    
    error.result = ValidationResult::Valid;
    error.message = QString("%1 OK (%2 kbit/s)").arg(fieldName).arg(bitrate / 1000);
    return error;
}

// ============================================================================
// RPM-Validierung
// ============================================================================

ValidationError InputValidator::validateRPM(
    const QString& fieldName,
    double rpm)
{
    ValidationError error;
    error.fieldName = fieldName;
    error.currentValue = rpm;
    
    if (rpm < 200.0) {
        error.result = ValidationResult::Error;
        error.message = QString("%1: RPM zu niedrig").arg(fieldName);
        error.hint = QString("Minimum: 200 RPM\n"
                            "Eingabe: %.1f RPM\n\n"
                            "Standard-Werte:\n"
                            "• 5.25\" DD/HD: 300 RPM\n"
                            "• 3.5\" DD/HD: 300 RPM\n"
                            "• Apple 3.5\": 394-590 RPM (CLV)")
                     .arg(rpm);
        error.suggestedValue = 300.0;
        return error;
    }
    
    if (rpm > 600.0) {
        error.result = ValidationResult::Error;
        error.message = QString("%1: RPM zu hoch").arg(fieldName);
        error.hint = QString("Maximum: 600 RPM\n"
                            "Eingabe: %.1f RPM\n\n"
                            "Höhere Werte können das\n"
                            "Laufwerk beschädigen!")
                     .arg(rpm);
        error.suggestedValue = 300.0;
        return error;
    }
    
    // Warnung für nicht-standard RPM
    if (qAbs(rpm - 300.0) > 10.0 && qAbs(rpm - 360.0) > 10.0) {
        error.result = ValidationResult::Warning;
        error.message = QString("%1: Nicht-Standard RPM").arg(fieldName);
        error.hint = QString("Eingabe: %.1f RPM\n\n"
                            "Standard-Werte:\n"
                            "• PC/Amiga/Atari: 300 RPM\n"
                            "• 8\" Laufwerke: 360 RPM\n\n"
                            "Nicht-Standard RPM kann zu\n"
                            "Lese-/Schreibfehlern führen.")
                     .arg(rpm);
        error.suggestedValue = 300.0;
        return error;
    }
    
    error.result = ValidationResult::Valid;
    error.message = QString("%1 OK (%.1f RPM)").arg(fieldName).arg(rpm);
    return error;
}

// ============================================================================
// Widget-Validierung
// ============================================================================

void InputValidator::validateLineEdit(
    QLineEdit* edit,
    std::function<ValidationError(const QString&)> validator)
{
    if (!edit) return;
    
    connect(edit, &QLineEdit::textChanged, this, [=](const QString& text) {
        ValidationError error = validator(text);
        applyValidationStyle(edit, error.result);
        
        if (error.result != ValidationResult::Valid) {
            edit->setToolTip(QString("<b>%1</b><br><br>%2<br><br><i>%3</i>")
                            .arg(error.message)
                            .arg(error.hint.replace("\n", "<br>"))
                            .arg(error.example));
        } else {
            edit->setToolTip(error.message);
        }
        
        m_fieldStates[error.fieldName] = error.result;
        emit validationChanged(error.fieldName, error.result);
        updateOverallState();
    });
}

void InputValidator::validateSpinBox(
    QSpinBox* spinBox,
    std::function<ValidationError(int)> validator)
{
    if (!spinBox) return;
    
    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int value) {
        ValidationError error = validator(value);
        applyValidationStyle(spinBox, error.result);
        
        if (error.result != ValidationResult::Valid) {
            spinBox->setToolTip(QString("<b>%1</b><br><br>%2")
                               .arg(error.message)
                               .arg(error.hint.replace("\n", "<br>")));
        } else {
            spinBox->setToolTip(error.message);
        }
        
        m_fieldStates[error.fieldName] = error.result;
        emit validationChanged(error.fieldName, error.result);
        updateOverallState();
    });
}

void InputValidator::validateDoubleSpinBox(
    QDoubleSpinBox* spinBox,
    std::function<ValidationError(double)> validator)
{
    if (!spinBox) return;
    
    connect(spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [=](double value) {
        ValidationError error = validator(value);
        applyValidationStyle(spinBox, error.result);
        
        if (error.result != ValidationResult::Valid) {
            spinBox->setToolTip(QString("<b>%1</b><br><br>%2")
                               .arg(error.message)
                               .arg(error.hint.replace("\n", "<br>")));
        } else {
            spinBox->setToolTip(error.message);
        }
        
        m_fieldStates[error.fieldName] = error.result;
        emit validationChanged(error.fieldName, error.result);
        updateOverallState();
    });
}

// ============================================================================
// Visuelles Feedback
// ============================================================================

void InputValidator::applyValidationStyle(QWidget* widget, ValidationResult result)
{
    if (!widget) return;
    
    switch (result) {
        case ValidationResult::Valid:
            widget->setStyleSheet(STYLE_VALID);
            break;
        case ValidationResult::Warning:
            widget->setStyleSheet(STYLE_WARNING);
            break;
        case ValidationResult::Error:
            widget->setStyleSheet(STYLE_ERROR);
            break;
        case ValidationResult::Empty:
            widget->setStyleSheet(STYLE_EMPTY);
            break;
    }
}

void InputValidator::showErrorTooltip(QWidget* widget, const ValidationError& error)
{
    if (!widget || error.result == ValidationResult::Valid) return;
    
    QString html = QString(
        "<div style='max-width: 300px;'>"
        "<h3 style='color: %1; margin: 0;'>%2</h3>"
        "<p style='margin: 8px 0;'>%3</p>"
        "%4"
        "</div>")
        .arg(error.result == ValidationResult::Error ? "#F44336" : "#FFC107")
        .arg(error.message)
        .arg(error.hint.replace("\n", "<br>"))
        .arg(error.example.isEmpty() ? "" : 
             QString("<p style='color: #666; font-style: italic;'>%1</p>")
             .arg(error.example));
    
    QToolTip::showText(widget->mapToGlobal(QPoint(0, widget->height())), 
                       html, widget);
}

void InputValidator::updateValidationLabel(QLabel* label, const ValidationError& error)
{
    if (!label) return;
    
    QString icon;
    QString color;
    
    switch (error.result) {
        case ValidationResult::Valid:
            icon = "✓";
            color = "#4CAF50";
            break;
        case ValidationResult::Warning:
            icon = "⚠";
            color = "#FFC107";
            break;
        case ValidationResult::Error:
            icon = "✕";
            color = "#F44336";
            break;
        case ValidationResult::Empty:
            icon = "○";
            color = "#9E9E9E";
            break;
    }
    
    label->setText(QString("<span style='color: %1; font-weight: bold;'>%2 %3</span>")
                  .arg(color).arg(icon).arg(error.message));
    label->setToolTip(error.hint);
}

void InputValidator::updateOverallState()
{
    bool allValid = true;
    
    for (auto it = m_fieldStates.begin(); it != m_fieldStates.end(); ++it) {
        if (it.value() == ValidationResult::Error) {
            allValid = false;
            break;
        }
    }
    
    emit allFieldsValid(allValid);
}

} // namespace UFT
