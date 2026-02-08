/**
 * @file uft_input_validator.h
 * @brief UFT Input Validation mit detaillierten Fehlermeldungen
 * @version 3.8.0
 * 
 * Features:
 * - Echtzeit-Validierung mit visuellem Feedback
 * - Kontextbezogene, hilfreiche Fehlermeldungen
 * - Unterstützung für verschiedene Eingabetypen
 * - Mehrsprachige Hinweise (DE/EN)
 */

#ifndef UFT_INPUT_VALIDATOR_H
#define UFT_INPUT_VALIDATOR_H

#include <QObject>
#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QToolTip>
#include <QRegularExpression>
#include <QMap>
#include <functional>

namespace UFT {

/**
 * @enum ValidationResult
 * @brief Ergebnis der Validierung
 */
enum class ValidationResult {
    Valid,          // Eingabe ist gültig
    Warning,        // Gültig, aber ungewöhnlich
    Error,          // Ungültig
    Empty           // Leer (kann erlaubt sein)
};

/**
 * @struct ValidationError
 * @brief Detaillierte Fehlerbeschreibung
 */
struct ValidationError {
    ValidationResult result;
    QString message;        // Kurze Fehlermeldung
    QString hint;           // Hilfreicher Hinweis
    QString example;        // Beispiel für gültige Eingabe
    QString fieldName;      // Name des Feldes
    QVariant currentValue;  // Aktueller Wert
    QVariant suggestedValue;// Vorgeschlagener Wert
};

/**
 * @class InputValidator
 * @brief Zentrale Eingabevalidierung mit visuellem Feedback
 */
class InputValidator : public QObject
{
    Q_OBJECT

public:
    explicit InputValidator(QObject *parent = nullptr);
    virtual ~InputValidator() = default;

    // ========================================================================
    // Validierungs-Regeln
    // ========================================================================
    
    /**
     * @brief Integer-Bereich validieren
     */
    static ValidationError validateIntRange(
        const QString& fieldName,
        int value,
        int minVal,
        int maxVal,
        const QString& unit = QString());
    
    /**
     * @brief Double-Bereich validieren
     */
    static ValidationError validateDoubleRange(
        const QString& fieldName,
        double value,
        double minVal,
        double maxVal,
        int decimals = 2,
        const QString& unit = QString());
    
    /**
     * @brief Hex-Wert validieren
     */
    static ValidationError validateHex(
        const QString& fieldName,
        const QString& value,
        int minBytes = 1,
        int maxBytes = 4);
    
    /**
     * @brief Dateipfad validieren
     */
    static ValidationError validateFilePath(
        const QString& fieldName,
        const QString& path,
        bool mustExist = true,
        const QStringList& allowedExtensions = QStringList());
    
    /**
     * @brief Track/Sektor-Bereich validieren
     */
    static ValidationError validateTrackRange(
        const QString& fieldName,
        int track,
        int maxTracks = 84,
        bool allowHalfTracks = false);
    
    /**
     * @brief Sektorbereich validieren
     */
    static ValidationError validateSectorRange(
        const QString& fieldName,
        int sector,
        int maxSectors = 21);
    
    /**
     * @brief Bitrate validieren
     */
    static ValidationError validateBitrate(
        const QString& fieldName,
        int bitrate);
    
    /**
     * @brief RPM validieren
     */
    static ValidationError validateRPM(
        const QString& fieldName,
        double rpm);

    // ========================================================================
    // Widget-Validierung mit visuellem Feedback
    // ========================================================================
    
    /**
     * @brief QLineEdit validieren und Feedback anzeigen
     */
    void validateLineEdit(
        QLineEdit* edit,
        std::function<ValidationError(const QString&)> validator);
    
    /**
     * @brief QSpinBox validieren
     */
    void validateSpinBox(
        QSpinBox* spinBox,
        std::function<ValidationError(int)> validator);
    
    /**
     * @brief QDoubleSpinBox validieren
     */
    void validateDoubleSpinBox(
        QDoubleSpinBox* spinBox,
        std::function<ValidationError(double)> validator);

    // ========================================================================
    // Visuelles Feedback
    // ========================================================================
    
    /**
     * @brief Validierungsstatus auf Widget anwenden
     */
    static void applyValidationStyle(QWidget* widget, ValidationResult result);
    
    /**
     * @brief Fehlerhinweis als Tooltip anzeigen
     */
    static void showErrorTooltip(QWidget* widget, const ValidationError& error);
    
    /**
     * @brief Validierungslabel aktualisieren
     */
    static void updateValidationLabel(
        QLabel* label, 
        const ValidationError& error);

    // ========================================================================
    // Stil-Konstanten
    // ========================================================================
    
    static const QString STYLE_VALID;
    static const QString STYLE_WARNING;
    static const QString STYLE_ERROR;
    static const QString STYLE_EMPTY;

signals:
    void validationChanged(const QString& fieldName, ValidationResult result);
    void allFieldsValid(bool valid);

private:
    QMap<QString, ValidationResult> m_fieldStates;
    void updateOverallState();
};

// ============================================================================
// Vordefinierte Validatoren für häufige UFT-Felder
// ============================================================================

namespace Validators {

/**
 * @brief Track-Nummer (0-83, optional Half-Tracks)
 */
inline ValidationError trackNumber(const QString& name, int value, bool allowHalf = false) {
    return InputValidator::validateTrackRange(name, value, 84, allowHalf);
}

/**
 * @brief Sektor-Nummer (1-21 für C64, 1-11 für Amiga DD)
 */
inline ValidationError sectorNumber(const QString& name, int value, int max = 21) {
    return InputValidator::validateSectorRange(name, value, max);
}

/**
 * @brief Revolutions (1-20)
 */
inline ValidationError revolutions(int value) {
    return InputValidator::validateIntRange(
        "Revolutions", value, 1, 20, "rev");
}

/**
 * @brief Retries (0-50)
 */
inline ValidationError retries(int value) {
    return InputValidator::validateIntRange(
        "Retries", value, 0, 50, "");
}

/**
 * @brief Bitrate (125000-500000)
 */
inline ValidationError bitrate(int value) {
    return InputValidator::validateBitrate("Bitrate", value);
}

/**
 * @brief RPM (270-360)
 */
inline ValidationError rpm(double value) {
    return InputValidator::validateRPM("RPM", value);
}

/**
 * @brief PLL-Faktor (0.01-1.0)
 */
inline ValidationError pllFactor(double value) {
    return InputValidator::validateDoubleRange(
        "PLL Factor", value, 0.01, 1.0, 2, "");
}

/**
 * @brief Hex-Byte (00-FF)
 */
inline ValidationError hexByte(const QString& name, const QString& value) {
    return InputValidator::validateHex(name, value, 1, 1);
}

/**
 * @brief Fill-Byte Validierung
 */
inline ValidationError fillByte(const QString& value) {
    ValidationError err = InputValidator::validateHex("Fill Byte", value, 1, 1);
    if (err.result == ValidationResult::Valid) {
        bool ok;
        int val = value.toInt(&ok, 16);
        if (ok && (val == 0x00 || val == 0xFF || val == 0x4E || val == 0xE5)) {
            err.hint = QString("Standard-Wert: 0x%1").arg(val, 2, 16, QChar('0')).toUpper();
        }
    }
    return err;
}

} // namespace Validators

} // namespace UFT

#endif // UFT_INPUT_VALIDATOR_H
