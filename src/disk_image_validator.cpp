/**
 * @file disk_image_validator.cpp
 * @brief Disk Image Validation Implementation
 */

#include "disk_image_validator.h"
#include <QFile>
#include <QFileInfo>
#include <QSet>

bool DiskImageValidator::isSupported(const QString& filePath)
{
    QFileInfo fi(filePath);
    return isSupportedExtension(fi.suffix());
}

bool DiskImageValidator::isSupportedExtension(const QString& extension)
{
    QString ext = extension.toLower();
    if (!ext.startsWith('.')) {
        ext = '.' + ext;
    }
    
    for (int i = 0; i < NUM_DISK_FORMATS; ++i) {
        if (DISK_FORMATS[i].extension == ext) {
            return true;
        }
    }
    
    return false;
}

DiskImageInfo DiskImageValidator::validate(const QString& filePath)
{
    DiskImageInfo info;
    info.filePath = filePath;
    
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        info.errorMessage = "File not found";
        return info;
    }
    
    if (!fi.isFile()) {
        info.errorMessage = "Not a file";
        return info;
    }
    
    info.extension = fi.suffix().toLower();
    info.fileSize = fi.size();
    
    // Finde passendes Format
    QString ext = '.' + info.extension;
    const DiskFormatDef* bestMatch = nullptr;
    
    for (int i = 0; i < NUM_DISK_FORMATS; ++i) {
        if (DISK_FORMATS[i].extension == ext) {
            // Bei fester Größe: prüfe ob sie passt
            if (DISK_FORMATS[i].expectedSize > 0) {
                if (info.fileSize == DISK_FORMATS[i].expectedSize) {
                    bestMatch = &DISK_FORMATS[i];
                    break;
                }
            } else {
                // Variable Größe (Flux etc.)
                if (!bestMatch) {
                    bestMatch = &DISK_FORMATS[i];
                }
            }
        }
    }
    
    if (bestMatch) {
        info.formatName = bestMatch->name;
        info.platform = bestMatch->platform;
        info.isFluxFormat = bestMatch->isFlux;
        info.tracks = bestMatch->tracks;
        info.heads = bestMatch->heads;
        info.sectorsPerTrack = bestMatch->sectorsPerTrack;
        info.sectorSize = bestMatch->sectorSize;
        info.isValid = true;
    } else {
        // Versuche Magic-Byte-Erkennung
        QString detected = detectByMagic(filePath);
        if (!detected.isEmpty()) {
            info.formatName = detected;
            info.isValid = true;
        } else {
            info.errorMessage = QString("Unknown format: .%1").arg(info.extension);
        }
    }
    
    // Versuche Geometrie zu erkennen wenn nicht gesetzt
    if (info.isValid && info.tracks < 0) {
        detectGeometry(info);
    }
    
    return info;
}

QStringList DiskImageValidator::supportedExtensions()
{
    QSet<QString> exts;
    
    for (int i = 0; i < NUM_DISK_FORMATS; ++i) {
        exts.insert(DISK_FORMATS[i].extension);
    }
    
    QStringList list = exts.values();
    list.sort();
    return list;
}

QString DiskImageValidator::fileDialogFilter()
{
    QStringList allExts;
    QMap<QString, QStringList> byPlatform;
    
    for (int i = 0; i < NUM_DISK_FORMATS; ++i) {
        QString ext = "*" + DISK_FORMATS[i].extension;
        if (!allExts.contains(ext)) {
            allExts << ext;
        }
        byPlatform[DISK_FORMATS[i].platform] << ext;
    }
    
    // Entferne Duplikate in Platform-Listen
    for (auto it = byPlatform.begin(); it != byPlatform.end(); ++it) {
        it.value().removeDuplicates();
    }
    
    QString filter;
    
    // Alle Formate
    filter += QString("All Disk Images (%1);;").arg(allExts.join(' '));
    
    // Nach Plattform
    for (auto it = byPlatform.constBegin(); it != byPlatform.constEnd(); ++it) {
        filter += QString("%1 Images (%2);;").arg(it.key()).arg(it.value().join(' '));
    }
    
    // Alle Dateien
    filter += "All Files (*)";
    
    return filter;
}

QString DiskImageValidator::detectByMagic(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    
    QByteArray header = file.read(16);
    file.close();
    
    if (header.size() < 4) {
        return QString();
    }
    
    // SCP Magic: "SCP"
    if (header.startsWith("SCP")) {
        return "SuperCard Pro";
    }
    
    // IPF Magic: "CAPS"
    if (header.startsWith("CAPS")) {
        return "SPS Interchangeable";
    }
    
    // HFE Magic: "HXCPICFE" oder "HXCHFE3"
    if (header.startsWith("HXCPICFE") || header.startsWith("HXCHFE3")) {
        return "HxC Floppy Emulator";
    }
    
    // G64 Magic: "GCR-1541"
    if (header.startsWith("GCR-1541")) {
        return "C64 GCR Image";
    }
    
    // ADF/IMG: Kein eindeutiges Magic, prüfe Bootsektor
    // Amiga Bootblock: "DOS\0", "DOS\1", "DOS\2", etc.
    if (header.size() >= 4 && header[0] == 'D' && header[1] == 'O' && header[2] == 'S') {
        return "Amiga Disk File";
    }
    
    // PC Bootsektor: 0x55 0xAA am Ende des ersten Sektors
    // (müssten Position 510-511 prüfen)
    
    return QString();
}

void DiskImageValidator::detectGeometry(DiskImageInfo& info)
{
    // Versuche Geometrie aus Dateigröße zu berechnen
    if (info.fileSize <= 0 || info.isFluxFormat) {
        return;
    }
    
    qint64 size = info.fileSize;
    
    // Bekannte Größen
    struct KnownSize {
        qint64 size;
        int tracks;
        int heads;
        int spt;
        int ss;
    };
    
    static const KnownSize KNOWN_SIZES[] = {
        // Amiga
        {901120, 80, 2, 11, 512},    // Amiga DD
        {1802240, 80, 2, 22, 512},   // Amiga HD
        
        // PC
        {163840, 40, 1, 8, 512},     // 160K
        {184320, 40, 1, 9, 512},     // 180K
        {327680, 40, 2, 8, 512},     // 320K
        {368640, 40, 2, 9, 512},     // 360K
        {737280, 80, 2, 9, 512},     // 720K
        {1228800, 80, 2, 15, 512},   // 1.2M
        {1474560, 80, 2, 18, 512},   // 1.44M
        {2949120, 80, 2, 36, 512},   // 2.88M
        
        // C64
        {174848, 35, 1, 17, 256},    // D64 (variable SPT)
        {175531, 35, 1, 17, 256},    // D64 with error info
        
        // Apple
        {143360, 35, 1, 16, 256},    // Apple DOS 3.3
        
        // Atari ST
        {737280, 80, 2, 9, 512},     // 720K
        {819200, 82, 2, 10, 512},    // 800K
    };
    
    for (const auto& ks : KNOWN_SIZES) {
        if (size == ks.size) {
            info.tracks = ks.tracks;
            info.heads = ks.heads;
            info.sectorsPerTrack = ks.spt;
            info.sectorSize = ks.ss;
            return;
        }
    }
    
    // Generische Berechnung für 512-byte Sektoren
    if (size % 512 == 0) {
        qint64 totalSectors = size / 512;
        
        // Versuche typische Geometrien
        struct Geom { int t; int h; int s; };
        static const Geom GEOMS[] = {
            {80, 2, 18}, {80, 2, 9}, {80, 1, 18}, {80, 1, 9},
            {40, 2, 9}, {40, 1, 9}, {35, 2, 9}, {35, 1, 9},
        };
        
        for (const auto& g : GEOMS) {
            if (totalSectors == g.t * g.h * g.s) {
                info.tracks = g.t;
                info.heads = g.h;
                info.sectorsPerTrack = g.s;
                info.sectorSize = 512;
                return;
            }
        }
    }
}
