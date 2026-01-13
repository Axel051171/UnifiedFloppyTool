/**
 * @file UftParameterModel.h
 * @brief Qt Parameter Model for Bidirectional GUI-Backend Binding
 * 
 * P1-004: GUI Parameter Bidirektional
 * 
 * Features:
 * - Q_PROPERTY for all parameters with NOTIFY signals
 * - Automatic sync between C backend and Qt GUI
 * - Change tracking
 * - Undo/Redo support
 * 
 * Usage:
 *   UftParameterModel model;
 *   model.setCylinders(80);  // Notifies GUI and Backend
 *   connect(&model, &UftParameterModel::cylindersChanged, [](int v) { ... });
 */

#ifndef UFT_PARAMETER_MODEL_H
#define UFT_PARAMETER_MODEL_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QHash>
#include <QUndoStack>

extern "C" {
    struct uft_params;
    typedef struct uft_params uft_params_t;
}

/* Forward declare C struct */
extern "C" {
    struct uft_params;
    typedef struct uft_params uft_params_t;
}

/**
 * @brief Parameter change record for undo/redo
 */
struct UftParamChange {
    QString name;
    QVariant oldValue;
    QVariant newValue;
    qint64 timestamp;
};

/**
 * @brief Qt Model for UFT Parameters with full bidirectional binding
 */
class UftParameterModel : public QObject
{
    Q_OBJECT
    
    /* ═══════════════════════════════════════════════════════════════════════════════
     * General Parameters
     * ═══════════════════════════════════════════════════════════════════════════════ */
    Q_PROPERTY(QString inputPath READ inputPath WRITE setInputPath NOTIFY inputPathChanged)
    Q_PROPERTY(QString outputPath READ outputPath WRITE setOutputPath NOTIFY outputPathChanged)
    Q_PROPERTY(bool verbose READ verbose WRITE setVerbose NOTIFY verboseChanged)
    Q_PROPERTY(bool quiet READ quiet WRITE setQuiet NOTIFY quietChanged)
    
    /* ═══════════════════════════════════════════════════════════════════════════════
     * Format Parameters
     * ═══════════════════════════════════════════════════════════════════════════════ */
    Q_PROPERTY(QString format READ format WRITE setFormat NOTIFY formatChanged)
    Q_PROPERTY(int cylinders READ cylinders WRITE setCylinders NOTIFY cylindersChanged)
    Q_PROPERTY(int heads READ heads WRITE setHeads NOTIFY headsChanged)
    Q_PROPERTY(int sectors READ sectors WRITE setSectors NOTIFY sectorsChanged)
    Q_PROPERTY(QString encoding READ encoding WRITE setEncoding NOTIFY encodingChanged)
    
    /* ═══════════════════════════════════════════════════════════════════════════════
     * Hardware Parameters
     * ═══════════════════════════════════════════════════════════════════════════════ */
    Q_PROPERTY(QString hardware READ hardware WRITE setHardware NOTIFY hardwareChanged)
    Q_PROPERTY(QString devicePath READ devicePath WRITE setDevicePath NOTIFY devicePathChanged)
    Q_PROPERTY(int driveNumber READ driveNumber WRITE setDriveNumber NOTIFY driveNumberChanged)
    
    /* ═══════════════════════════════════════════════════════════════════════════════
     * Recovery Parameters
     * ═══════════════════════════════════════════════════════════════════════════════ */
    Q_PROPERTY(int retries READ retries WRITE setRetries NOTIFY retriesChanged)
    Q_PROPERTY(int revolutions READ revolutions WRITE setRevolutions NOTIFY revolutionsChanged)
    Q_PROPERTY(bool weakBits READ weakBits WRITE setWeakBits NOTIFY weakBitsChanged)
    
    /* ═══════════════════════════════════════════════════════════════════════════════
     * PLL Parameters (P1-006: PLL GUI Controls)
     * ═══════════════════════════════════════════════════════════════════════════════ */
    Q_PROPERTY(double pllPhaseGain READ pllPhaseGain WRITE setPllPhaseGain NOTIFY pllPhaseGainChanged)
    Q_PROPERTY(double pllFreqGain READ pllFreqGain WRITE setPllFreqGain NOTIFY pllFreqGainChanged)
    Q_PROPERTY(double pllWindowTolerance READ pllWindowTolerance WRITE setPllWindowTolerance NOTIFY pllWindowToleranceChanged)
    Q_PROPERTY(QString pllPreset READ pllPreset WRITE setPllPreset NOTIFY pllPresetChanged)
    
    /* ═══════════════════════════════════════════════════════════════════════════════
     * Write Parameters
     * ═══════════════════════════════════════════════════════════════════════════════ */
    Q_PROPERTY(bool verifyAfterWrite READ verifyAfterWrite WRITE setVerifyAfterWrite NOTIFY verifyAfterWriteChanged)
    Q_PROPERTY(int writeRetries READ writeRetries WRITE setWriteRetries NOTIFY writeRetriesChanged)
    
    /* ═══════════════════════════════════════════════════════════════════════════════
     * State
     * ═══════════════════════════════════════════════════════════════════════════════ */
    Q_PROPERTY(bool modified READ isModified NOTIFY modifiedChanged)
    Q_PROPERTY(bool valid READ isValid NOTIFY validChanged)

public:
    explicit UftParameterModel(QObject *parent = nullptr);
    ~UftParameterModel();
    
    /* ═══════════════════════════════════════════════════════════════════════════════
     * Getters
     * ═══════════════════════════════════════════════════════════════════════════════ */
    
    /* General */
    QString inputPath() const { return m_inputPath; }
    QString outputPath() const { return m_outputPath; }
    bool verbose() const { return m_verbose; }
    bool quiet() const { return m_quiet; }
    
    /* Format */
    QString format() const { return m_format; }
    int cylinders() const { return m_cylinders; }
    int heads() const { return m_heads; }
    int sectors() const { return m_sectors; }
    QString encoding() const { return m_encoding; }
    
    /* Hardware */
    QString hardware() const { return m_hardware; }
    QString devicePath() const { return m_devicePath; }
    int driveNumber() const { return m_driveNumber; }
    
    /* Recovery */
    int retries() const { return m_retries; }
    int revolutions() const { return m_revolutions; }
    bool weakBits() const { return m_weakBits; }
    
    /* PLL */
    double pllPhaseGain() const { return m_pllPhaseGain; }
    double pllFreqGain() const { return m_pllFreqGain; }
    double pllWindowTolerance() const { return m_pllWindowTolerance; }
    QString pllPreset() const { return m_pllPreset; }
    
    /* Write */
    bool verifyAfterWrite() const { return m_verifyAfterWrite; }
    int writeRetries() const { return m_writeRetries; }
    
    /* State */
    bool isModified() const { return m_modified; }
    bool isValid() const;
    
    /* ═══════════════════════════════════════════════════════════════════════════════
     * Setters (emit signals for bidirectional binding)
     * ═══════════════════════════════════════════════════════════════════════════════ */
    
public slots:
    /* General */
    void setInputPath(const QString &path);
    void setOutputPath(const QString &path);
    void setVerbose(bool v);
    void setQuiet(bool q);
    
    /* Format */
    void setFormat(const QString &fmt);
    void setCylinders(int c);
    void setHeads(int h);
    void setSectors(int s);
    void setEncoding(const QString &enc);
    
    /* Hardware */
    void setHardware(const QString &hw);
    void setDevicePath(const QString &path);
    void setDriveNumber(int d);
    
    /* Recovery */
    void setRetries(int r);
    void setRevolutions(int r);
    void setWeakBits(bool w);
    
    /* PLL */
    void setPllPhaseGain(double g);
    void setPllFreqGain(double g);
    void setPllWindowTolerance(double t);
    void setPllPreset(const QString &preset);
    
    /* Write */
    void setVerifyAfterWrite(bool v);
    void setWriteRetries(int r);
    
    /* ═══════════════════════════════════════════════════════════════════════════════
     * Actions
     * ═══════════════════════════════════════════════════════════════════════════════ */
    
    void reset();
    void loadDefaults();
    bool loadFromFile(const QString &path);
    bool saveToFile(const QString &path);
    
    /* Sync with C backend */
    void syncToBackend();
    void syncFromBackend();
    
    /* Undo/Redo */
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    
    /* Generic access */
    QVariant getValue(const QString &name) const;
    bool setValue(const QString &name, const QVariant &value);
    QStringList parameterNames() const;
    
signals:
    /* ═══════════════════════════════════════════════════════════════════════════════
     * Change Signals (for GUI binding)
     * ═══════════════════════════════════════════════════════════════════════════════ */
    
    /* General */
    void inputPathChanged(const QString &path);
    void outputPathChanged(const QString &path);
    void verboseChanged(bool v);
    void quietChanged(bool q);
    
    /* Format */
    void formatChanged(const QString &fmt);
    void cylindersChanged(int c);
    void headsChanged(int h);
    void sectorsChanged(int s);
    void encodingChanged(const QString &enc);
    
    /* Hardware */
    void hardwareChanged(const QString &hw);
    void devicePathChanged(const QString &path);
    void driveNumberChanged(int d);
    
    /* Recovery */
    void retriesChanged(int r);
    void revolutionsChanged(int r);
    void weakBitsChanged(bool w);
    
    /* PLL */
    void pllPhaseGainChanged(double g);
    void pllFreqGainChanged(double g);
    void pllWindowToleranceChanged(double t);
    void pllPresetChanged(const QString &preset);
    
    /* Write */
    void verifyAfterWriteChanged(bool v);
    void writeRetriesChanged(int r);
    
    /* State */
    void modifiedChanged(bool modified);
    void validChanged(bool valid);
    
    /* Generic */
    void parameterChanged(const QString &name, const QVariant &oldValue, const QVariant &newValue);
    void backendSynced();
    void errorOccurred(const QString &error);

private:
    void recordChange(const QString &name, const QVariant &oldVal, const QVariant &newVal);
    void markModified();
    
    /* ═══════════════════════════════════════════════════════════════════════════════
     * Member Variables
     * ═══════════════════════════════════════════════════════════════════════════════ */
    
    /* General */
    QString m_inputPath;
    QString m_outputPath;
    bool m_verbose = false;
    bool m_quiet = false;
    
    /* Format */
    QString m_format = "auto";
    int m_cylinders = 80;
    int m_heads = 2;
    int m_sectors = 18;
    QString m_encoding = "auto";
    
    /* Hardware */
    QString m_hardware = "auto";
    QString m_devicePath;
    int m_driveNumber = 0;
    
    /* Recovery */
    int m_retries = 3;
    int m_revolutions = 3;
    bool m_weakBits = true;
    
    /* PLL */
    double m_pllPhaseGain = 0.10;
    double m_pllFreqGain = 0.05;
    double m_pllWindowTolerance = 0.40;
    QString m_pllPreset = "Amiga DD";
    
    /* Write */
    bool m_verifyAfterWrite = true;
    int m_writeRetries = 3;
    
    /* State */
    bool m_modified = false;
    QList<UftParamChange> m_history;
    int m_historyIndex = -1;
    
    /* Backend pointer */
    uft_params_t *m_backendParams = nullptr;
};

#endif /* UFT_PARAMETER_MODEL_H */
