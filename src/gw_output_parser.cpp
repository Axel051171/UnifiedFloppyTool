/**
 * @file gw_output_parser.cpp
 * @brief Greaseweazle Output Parser Implementation
 * 
 * Patterns und Keywords extrahiert aus Sovox Disk Master v1.0
 */

#include "gw_output_parser.h"
#include <QDebug>

GWOutputParser::GWOutputParser(QObject* parent)
    : QObject(parent)
{
    // Extrahierte Regex-Patterns aus Sovox
    // Pattern für Track-Status: T00.0, T01.1, etc.
    m_trackPattern = QRegularExpression(R"(^T(\d{1,2})\.(\d))");
    
    // Pattern zum Bereinigen von Track-Prefixen
    m_trackCleanPattern = QRegularExpression(R"(^T\d{1,2}\.\d\s*([:*\-]+\s*)?)");
    
    // Error-Keywords (extrahiert aus Sovox stream_reader)
    m_errorKeywords = {
        "error",
        "fail",
        "failed", 
        "verify failure",
        "verification failed",
        "cannot",
        "unable",
        "invalid",
        "no index",
        "no data",
        "crc",
        "bad",
        "timeout",
        "not found",
        "missing"
    };
    
    // Success-Keywords (extrahiert aus Sovox process_output_queue)
    m_successKeywords = {
        "success",
        "completed",
        "all tracks verified",
        "successo",      // Italian
        "completato",    // Italian
        "verified"
    };
    
    // GW-Prefixe zum Entfernen (aus Sovox stream_reader)
    m_gwPrefixes = {
        "gw.exe ",
        "gw.exe:",
        "gw.exe: ",
        "gw: ",
        "GW ERROR: "
    };
}

GWTrackStatus GWOutputParser::parseLine(const QString& line, bool isStderr)
{
    GWTrackStatus status;
    QString trimmed = line.trimmed();
    
    if (trimmed.isEmpty()) {
        return status;
    }
    
    // Log-Nachricht emittieren
    emit logMessage(trimmed);
    
    // Prüfe auf Track-Pattern: T00.0, T01.1, etc.
    QRegularExpressionMatch match = m_trackPattern.match(trimmed);
    if (match.hasMatch()) {
        status.track = match.captured(1).toInt();
        status.head = match.captured(2).toInt();
        
        // Extrahiere den Rest nach dem Track-Prefix
        QString remainder = trimmed.mid(match.capturedEnd()).trimmed();
        
        // Bereinige Prefixe wie ": " oder "* "
        QRegularExpressionMatch cleanMatch = m_trackCleanPattern.match(trimmed);
        if (cleanMatch.hasMatch()) {
            remainder = trimmed.mid(cleanMatch.capturedEnd()).trimmed();
        }
        
        // Prüfe auf Fehler in dieser Track-Zeile
        if (isErrorLine(remainder) || isStderr) {
            status.hasError = true;
            status.errorMessage = extractErrorMessage(remainder);
            emit trackError(status.track, status.head, status.errorMessage);
        } else if (remainder.isEmpty() || remainder == "OK" || 
                   remainder.contains("writing", Qt::CaseInsensitive)) {
            // Track wird gerade geschrieben
            status.isWriting = true;
            emit trackWriting(status.track, status.head);
        } else {
            // Track erfolgreich geschrieben
            status.isWritten = true;
            emit trackWritten(status.track, status.head);
        }
        
        return status;
    }
    
    // Keine Track-Zeile - prüfe auf allgemeine Fehler
    if (isStderr || isErrorLine(trimmed)) {
        QString errorMsg = extractErrorMessage(trimmed);
        emit generalError(errorMsg);
        status.hasError = true;
        status.errorMessage = errorMsg;
    }
    
    // Prüfe auf Success/Completion
    if (isSuccessLine(trimmed)) {
        emit operationComplete(true);
    }
    
    return status;
}

bool GWOutputParser::isErrorLine(const QString& line) const
{
    QString lower = line.toLower();
    
    for (const QString& keyword : m_errorKeywords) {
        if (lower.contains(keyword)) {
            return true;
        }
    }
    
    return false;
}

bool GWOutputParser::isSuccessLine(const QString& line) const
{
    QString lower = line.toLower();
    
    for (const QString& keyword : m_successKeywords) {
        if (lower.contains(keyword)) {
            return true;
        }
    }
    
    return false;
}

QString GWOutputParser::extractErrorMessage(const QString& line) const
{
    QString cleaned = line;
    
    // Entferne GW-Prefixe
    for (const QString& prefix : m_gwPrefixes) {
        if (cleaned.startsWith(prefix, Qt::CaseInsensitive)) {
            cleaned = cleaned.mid(prefix.length());
            break;
        }
    }
    
    return cleaned.trimmed();
}
