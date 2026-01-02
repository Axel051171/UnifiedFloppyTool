/**
 * @file formattab.h
 * @brief Format/Settings Tab with format-dependent UI activation
 * 
 * This implements a smart UI where only relevant options for the
 * selected disk format are enabled. Prevents misconfiguration.
 */

#pragma once

#include <QWidget>
#include <QMap>
#include <QSet>
#include <QStringList>

QT_BEGIN_NAMESPACE
namespace Ui { class TabFormat; }
QT_END_NAMESPACE

/**
 * @brief Format configuration defining which UI elements are relevant
 */
struct FormatConfig {
    QString system;           // e.g. "Commodore", "Amiga"
    QString format;           // e.g. "D64", "ADF"
    QStringList versions;     // Available versions/variants
    
    // Which UI groups are enabled for this format
    bool enableFlux;
    bool enablePLL;
    bool enableGeometry;
    bool enableEncoding;
    bool enableTiming;
    bool enableRetry;
    bool enableVerify;
    bool enableNibble;
    bool enableGCR;           // Commodore/Apple GCR
    bool enableMFM;           // Amiga/PC MFM
    
    // Specific parameters
    int defaultTracks;
    int defaultSectors;
    int defaultSides;
    int defaultRPM;
    QString defaultEncoding;
};

class FormatTab : public QWidget
{
    Q_OBJECT

public:
    explicit FormatTab(QWidget *parent = nullptr);
    ~FormatTab();
    
    // Get current format configuration
    QString currentSystem() const;
    QString currentFormat() const;
    FormatConfig currentConfig() const;
    
    // Set format programmatically (e.g., when file is loaded)
    void setFormat(const QString &system, const QString &format);

signals:
    // Emitted when format changes - other tabs can connect to this
    void formatChanged(const QString &system, const QString &format);
    void configurationChanged();

private slots:
    void onSystemChanged(int index);
    void onFormatChanged(int index);
    void onVersionChanged(int index);

private:
    void setupConnections();
    void initializeFormatDatabase();
    void updateFormatsForSystem(const QString &system);
    void updateVersionsForFormat(const QString &format);
    void applyFormatConfig(const FormatConfig &config);
    void setGroupEnabled(QWidget *group, bool enabled);
    void updateUIForCurrentFormat();
    
    Ui::TabFormat *ui;
    
    // Format database: system -> list of formats
    QMap<QString, QList<FormatConfig>> m_formatDB;
    
    // Current configuration
    FormatConfig m_currentConfig;
};
