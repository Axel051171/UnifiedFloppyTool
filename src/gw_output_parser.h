/**
 * @file gw_output_parser.h
 * @brief Greaseweazle Output Parser - basierend auf Sovox Disk Master Analyse
 * @version 1.0.0
 * 
 * Extrahierte Regex-Patterns und Error-Keywords aus Sovox Decompilation.
 */

#ifndef GW_OUTPUT_PARSER_H
#define GW_OUTPUT_PARSER_H

#include <QObject>
#include <QString>
#include <QRegularExpression>
#include <QStringList>

/**
 * @brief Track Status vom GW-Output
 */
struct GWTrackStatus {
    int track = -1;
    int head = -1;
    bool isWriting = false;
    bool isWritten = false;
    bool hasError = false;
    QString errorMessage;
};

/**
 * @brief Parser für Greaseweazle stdout/stderr Output
 * 
 * Basierend auf den extrahierten Patterns aus Sovox Disk Master:
 * - Track Status: ^T(\d{1,2})\.(\d)
 * - Error Keywords: error, fail, crc, verify failure, etc.
 */
class GWOutputParser : public QObject
{
    Q_OBJECT

public:
    explicit GWOutputParser(QObject* parent = nullptr);
    
    /**
     * @brief Parse eine Output-Zeile von gw.exe
     * @param line Die zu parsende Zeile
     * @param isStderr true wenn von stderr
     * @return Parsed Track Status
     */
    GWTrackStatus parseLine(const QString& line, bool isStderr = false);
    
    /**
     * @brief Prüft ob die Zeile einen Fehler enthält
     */
    bool isErrorLine(const QString& line) const;
    
    /**
     * @brief Prüft ob die Operation erfolgreich abgeschlossen wurde
     */
    bool isSuccessLine(const QString& line) const;
    
    /**
     * @brief Extrahiert Error-Message aus Zeile
     */
    QString extractErrorMessage(const QString& line) const;

signals:
    void trackWriting(int track, int head);
    void trackWritten(int track, int head);
    void trackError(int track, int head, const QString& message);
    void generalError(const QString& message);
    void operationComplete(bool success);
    void logMessage(const QString& message);

private:
    // Extrahierte Regex-Patterns aus Sovox
    QRegularExpression m_trackPattern;      // ^T(\d{1,2})\.(\d)
    QRegularExpression m_trackCleanPattern; // ^T\d{1,2}\.\d\s*([:*\-]+\s*)?
    
    // Error-Keywords (aus Sovox Decompilation)
    QStringList m_errorKeywords;
    QStringList m_successKeywords;
    
    // GW-spezifische Prefixe zum Entfernen
    QStringList m_gwPrefixes;
};

#endif // GW_OUTPUT_PARSER_H
