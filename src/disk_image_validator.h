#pragma once
/**
 * @file disk_image_validator.h
 * @brief Disk Image Validation und Format-Erkennung
 * 
 * Unterstützte Formate (aus Sovox handle_drop):
 * - .adf (Amiga Disk File)
 * - .ipf (Interchangeable Preservation Format)
 * - .scp (SuperCard Pro)
 * - .img (Raw Sector Image)
 * - .hfe (UFT HFE Format)
 * 
 * Zusätzliche UFT-Formate:
 * - .raw (Raw Flux)
 * - .d64 (C64)
 * - .g64 (C64 GCR)
 * - .nib (Apple II)
 * - .dsk (Various)
 * - .st (Atari ST)
 *
 * P2-2 (MF-1351): die Liste oben ist nicht mehr die Grenze. Lehnen Liste
 * und Magic-Bytes ab, fragt validate() die Plugin-Registry
 * (uft_register_all_formats(), src/main.cpp) — die Liste bleibt fuer
 * ihre eigenen Endungen ZUERST, ihr Ergebnis dort ist unveraendert.
 * isSupportedExtension(), supportedExtensions() und fileDialogFilter()
 * nehmen die Endungen der Registry dazu. Leere Registry: Liste allein.
 */

#ifndef DISK_IMAGE_VALIDATOR_H
#define DISK_IMAGE_VALIDATOR_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QMap>

/**
 * @brief Erkanntes Disk-Image-Format
 */
struct DiskImageInfo {
    QString filePath;
    QString extension;
    QString formatName;
    QString platform;           // Amiga, PC, C64, Apple, Atari
    qint64 fileSize = 0;
    bool isValid = false;
    bool isFluxFormat = false;  // SCP, RAW vs. Sektor-Image
    QString errorMessage;
    
    // Erkannte Geometrie (falls möglich)
    int tracks = -1;
    int heads = -1;
    int sectorsPerTrack = -1;
    int sectorSize = -1;
};

/**
 * @brief Validiert und erkennt Disk-Image-Formate
 */
class DiskImageValidator
{
public:
    /**
     * @brief Prüft ob eine Datei ein unterstütztes Format hat
     */
    static bool isSupported(const QString& filePath);
    
    /**
     * @brief Prüft nur die Extension
     */
    static bool isSupportedExtension(const QString& extension);
    
    /**
     * @brief Validiert eine Disk-Image-Datei
     */
    static DiskImageInfo validate(const QString& filePath);
    
    /**
     * @brief Gibt Liste aller unterstützten Extensions zurück
     */
    static QStringList supportedExtensions();
    
    /**
     * @brief Gibt Filter-String für QFileDialog zurück
     */
    static QString fileDialogFilter();

    /**
     * @brief Der Pfad in der Form, die die Format-Schicht oeffnen kann
     *        (P2-2, MF-1351).
     *
     * Die C-Schicht oeffnet mit einem schmalen fopen(), also in der Form
     * von QFile::encodeName() — unter Windows die ANSI-Codepage, sonst
     * UTF-8. Gibt false zurueck, wenn der Pfad darin NICHT darstellbar ist
     * (der Rueckweg ergibt nicht denselben Pfad). Dann darf ihn niemand an
     * die C-Schicht geben: fopen scheitert, oder eine Ersatzabbildung
     * trifft eine ANDERE Datei. Wer false bekommt, sagt, dass es am Pfad
     * liegt — nicht „kein Plugin", das waere ein Urteil ueber die Datei.
     *
     * @param out  bei true die Bytes fuer die C-Schicht; darf nullptr sein
     */
    static bool nativePath(const QString& path, QByteArray* out);

private:
    /**
     * @brief Erkennt Format anhand von Magic Bytes
     */
    static QString detectByMagic(const QString& filePath);

    /**
     * @brief Letzte Stufe (P2-2): fragt die Plugin-Registry, wenn Liste
     *        und Magic-Bytes ablehnen.
     *
     * Oeffnet die Datei read-only ueber uft_disk_open_ranked(); gueltig
     * nur, wenn ein Plugin sie oeffnet. Name aus dem Plugin, Geometrie
     * aus der geoeffneten Scheibe (0 -> -1, nichts dazugerechnet),
     * Fluss aus UFT_FORMAT_CAP_FLUX, keine Plattform. Hat die Endung
     * einen Gleichstand entschieden oder traegt nur die Dateigroesse
     * (Band 30..49, MF-729), sagt der Name es dazu. Bei Gleichstand
     * ohne Entscheidung: ungueltig, Meldung "mehrdeutig" mit Namen.
     * Leere Registry: ungueltig, Meldung sagt es.
     */
    static bool validateByRegistry(DiskImageInfo& info);
    
    /**
     * @brief Berechnet erwartete Geometrie für bekannte Formate
     */
    static void detectGeometry(DiskImageInfo& info);
};

// ============================================================================
// Bekannte Formate und ihre Eigenschaften
// ============================================================================

/**
 * @brief Format-Definition
 */
struct DiskFormatDef {
    QString extension;
    QString name;
    QString platform;
    bool isFlux;
    qint64 expectedSize;        // 0 = variable
    int tracks;
    int heads;
    int sectorsPerTrack;
    int sectorSize;
};

// Sovox-kompatible Formate
static const DiskFormatDef DISK_FORMATS[] = {
    // Amiga
    {".adf", "Amiga Disk File (DD)", "Amiga", false, 901120, 80, 2, 11, 512},
    {".adf", "Amiga Disk File (HD)", "Amiga", false, 1802240, 80, 2, 22, 512},
    
    // Flux Formate
    {".scp", "SuperCard Pro", "Universal", true, 0, -1, -1, -1, -1},
    {".ipf", "SPS Interchangeable", "Universal", true, 0, -1, -1, -1, -1},
    {".hfe", "UFT HFE Format", "Universal", true, 0, -1, -1, -1, -1},
    {".raw", "Raw Flux Dump", "Universal", true, 0, -1, -1, -1, -1},
    
    // PC
    {".img", "Raw Sector Image (720K)", "PC", false, 737280, 80, 2, 9, 512},
    {".img", "Raw Sector Image (1.44M)", "PC", false, 1474560, 80, 2, 18, 512},
    {".ima", "Raw Sector Image", "PC", false, 0, -1, -1, -1, -1},
    
    // C64
    {".d64", "C64 Disk Image", "C64", false, 174848, 35, 1, -1, 256},
    {".g64", "C64 GCR Image", "C64", true, 0, 42, 1, -1, -1},
    
    // Apple
    {".nib", "Apple II Nibble", "Apple", true, 232960, 35, 1, -1, -1},
    {".dsk", "Apple II DOS 3.3", "Apple", false, 143360, 35, 1, 16, 256},
    {".do", "Apple II DOS Order", "Apple", false, 143360, 35, 1, 16, 256},
    {".po", "Apple II ProDOS Order", "Apple", false, 143360, 35, 1, 16, 256},
    
    // Atari ST
    {".st", "Atari ST Image", "Atari", false, 737280, 80, 2, 9, 512},
    {".msa", "Atari MSA Archive", "Atari", false, 0, -1, -1, -1, -1},
    
    // BBC Micro
    {".ssd", "BBC Micro Single", "BBC", false, 102400, 40, 1, 10, 256},
    {".dsd", "BBC Micro Double", "BBC", false, 204800, 40, 2, 10, 256},
};

static const int NUM_DISK_FORMATS = sizeof(DISK_FORMATS) / sizeof(DiskFormatDef);

#endif // DISK_IMAGE_VALIDATOR_H
