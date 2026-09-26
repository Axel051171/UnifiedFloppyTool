/**
 * @file disk_image_validator.cpp
 * @brief Disk Image Validation Implementation
 */

#include "disk_image_validator.h"
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <cstring>
#include <vector>

#include <uft/uft_core.h>           /* uft_disk_open_ranked / close / get_geometry */
#include <uft/uft_format_plugin.h>  /* registry, uft_disk_plugin, capabilities */
#include <uft/uft_types.h>

namespace {

/* P2-2 (MF-1351): the registry's extensions as ".ext", lower case,
 * de-duplicated, in registration order.
 *
 * Deliberately NOT a second walk over `plugin->extensions`: the split
 * rule (';', ',' and whitespace, 14 entries use commas) and the
 * de-duplication live in `uft_format_endungen_sammeln()` (MF-1245), and
 * a second copy would be "one quantity, two calculations" (MF-1177).
 *
 * Empty when the registry is empty (not registered yet, MF-447) or the
 * collection refuses — the callers then fall back to the fixed list
 * alone instead of claiming a format list that does not hold. */
QStringList registryExtensions()
{
    size_t noetig = 0;
    (void)uft_format_endungen_sammeln(nullptr, 0, &noetig);
    if (noetig <= 1) {
        return QStringList();
    }
    std::vector<char> puffer(noetig);
    if (!uft_format_endungen_sammeln(puffer.data(), puffer.size(), nullptr)) {
        return QStringList();
    }

    QStringList out;
    const QStringList teile =
        QString::fromLatin1(puffer.data()).split(' ', Qt::SkipEmptyParts);
    for (const QString& t : teile) {
        if (t.startsWith(QLatin1String("*."))) {
            out << t.mid(1);                /* "*.imd" -> ".imd" */
        }
    }
    out.removeDuplicates();
    return out;
}

/* "description (name)" — both come from the plugin. The description is
 * the human-readable label the fixed list also uses ("Amiga Disk File
 * (DD)"); the name is the registry key the rest of UFT uses (tiers,
 * logs, uft_get_format_plugin_by_name). Several plugins share generic
 * descriptions (the 49 DSK_PLUGIN() tables), so the name is what makes
 * the label unambiguous. */
QString pluginLabel(const uft_format_plugin_t* p)
{
    if (!p) {
        return QString();
    }
    const QString name = p->name ? QString::fromUtf8(p->name) : QString();
    const QString desc = p->description ? QString::fromUtf8(p->description)
                                        : QString();
    if (desc.isEmpty()) {
        return name;
    }
    if (name.isEmpty()) {
        return desc;
    }
    return QString("%1 (%2)").arg(desc, name);
}

/* The plugin's geometry value; 0 means "not stated" and becomes -1,
 * the DiskImageInfo convention for unknown. Nothing is filled in. */
int reported(unsigned v)
{
    return v > 0 ? static_cast<int>(v) : -1;
}

} // namespace

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
    
    /* P2-2: an extension a registered plugin claims is supported too. */
    return registryExtensions().contains(ext);
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
            /* P2-2: list and magic both reject — ask the registry. It
             * comes LAST on purpose: for the list's own extensions the
             * result must not change (a 720K .st stays "Atari ST Image";
             * the registry's probe race gives that size to MSX). And the
             * registry's answer is not passed through detectGeometry():
             * the geometry is what the opened disk reports, not a guess
             * from the file size. */
            validateByRegistry(info);
            return info;
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
    /* P2-2: plus what the registry claims (empty registry: none). */
    for (const QString& ext : registryExtensions()) {
        exts.insert(ext);
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
    
    /* P2-2: validate() opens what the registry opens, so the filter
     * offers it too. The list's own entries stay first and unchanged;
     * registry extensions the list already has are not repeated. With an
     * empty registry this adds nothing — no group that matches nothing. */
    QStringList registryExts;
    for (const QString& ext : registryExtensions()) {
        const QString pattern = "*" + ext;
        registryExts << pattern;
        if (!allExts.contains(pattern)) {
            allExts << pattern;
        }
    }

    QString filter;
    
    // Alle Formate
    filter += QString("All Disk Images (%1);;").arg(allExts.join(' '));
    
    // Nach Plattform
    for (auto it = byPlatform.constBegin(); it != byPlatform.constEnd(); ++it) {
        filter += QString("%1 Images (%2);;").arg(it.key()).arg(it.value().join(' '));
    }
    
    // Was die registrierten Plugins beanspruchen (keine Plattform-Zuordnung:
    // die Registry nennt keine, und hier wird keine erfunden)
    if (!registryExts.isEmpty()) {
        filter += QString("Registry Formats (%1);;").arg(registryExts.join(' '));
    }

    // Alle Dateien
    filter += "All Files (*)";
    
    return filter;
}

bool DiskImageValidator::nativePath(const QString& path, QByteArray* out)
{
    /* One place for the path the C layer gets (MF-1177): the validator and
     * DecodeJob both open the same file through it. */
    const QByteArray native = QFile::encodeName(path);
    if (QFile::decodeName(native) != path) {
        return false;
    }
    if (out) {
        *out = native;
    }
    return true;
}

bool DiskImageValidator::validateByRegistry(DiskImageInfo& info)
{
    const QString ext = QString(".%1").arg(info.extension);

    /* Not registered (MF-447: only main() fills it). Say so instead of
     * "kein Plugin", which would be a statement about the file. */
    if (uft_registered_format_plugin_count() == 0) {
        info.errorMessage = QString("Unknown format: %1 — Format-Registry leer "
                                    "(nicht registriert), nur die feste Liste "
                                    "gefragt").arg(ext);
        return false;
    }

    /* The C layer opens with a narrow fopen(). A path the system code page
     * cannot represent must not reach it: measured (MF-1351, Windows, code
     * page 1252) "Ωα.imd" encodes by best-fit to "Oa.imd", and with an IMD
     * lying under that name the validator reported 4099 random bytes as
     * "ImageDisk (IMD)" — a DIFFERENT file. Without one it said "kein
     * Plugin oeffnet die Datei", a statement about a file it never read.
     * Both are statements about the path; say that. */
    QByteArray pfad;
    if (!nativePath(info.filePath, &pfad)) {
        info.errorMessage = QString("Unknown format: %1 — Pfad in der "
                                    "Systemcodepage nicht darstellbar; die "
                                    "Format-Schicht kann ihn nicht oeffnen "
                                    "(keine Aussage ueber die Datei)").arg(ext);
        return false;
    }
    /* An empty file: the cause is known, "kein Plugin" would hide it. */
    if (info.fileSize == 0) {
        info.errorMessage = QString("Unknown format: %1 — Datei leer (0 Byte)")
                                .arg(ext);
        return false;
    }

    /* One decision point (MF-1251/1252): highest confidence, on a tie the
     * extension, otherwise NULL — and the ranking says why. */
    uft_probe_ranking_t rang;
    std::memset(&rang, 0, sizeof(rang));
    uft_disk_t* disk = uft_disk_open_ranked(pfad.constData(), true, &rang);

    if (!disk) {
        if (rang.tied > 1) {
            QStringList namen;
            for (size_t i = 0; i < rang.tied_listed && i < 4; ++i) {
                const uft_format_plugin_t* p = rang.tied_with[i];
                namen << (p && p->name ? QString::fromUtf8(p->name)
                                       : QString("?"));
            }
            if (rang.tied > rang.tied_listed) {
                namen << QString("und %1 weitere")
                             .arg(rang.tied - rang.tied_listed);
            }
            info.errorMessage =
                QString("Unknown format: %1 — mehrdeutig: %2 Plugins gleichauf "
                        "bei Konfidenz %3 (%4); kein Plugin oeffnet die Datei "
                        "eindeutig")
                    .arg(ext).arg(rang.tied).arg(rang.confidence)
                    .arg(namen.join(", "));
        } else if (rang.winner && rang.winner->name) {
            info.errorMessage =
                QString("Unknown format: %1 — kein Plugin oeffnet die Datei "
                        "(%2 erkennt sie mit Konfidenz %3, oeffnet sie aber "
                        "nicht)")
                    .arg(ext, QString::fromUtf8(rang.winner->name))
                    .arg(rang.confidence);
        } else {
            info.errorMessage =
                QString("Unknown format: %1 — kein Plugin oeffnet die Datei")
                    .arg(ext);
        }
        return false;
    }

    const uft_format_plugin_t* plugin = uft_disk_plugin(disk);
    uft_geometry_t g;
    std::memset(&g, 0, sizeof(g));
    const bool haveGeometry = (uft_disk_get_geometry(disk, &g) == 0);
    uft_disk_close(disk);

    info.isValid = true;
    info.formatName = pluginLabel(plugin);
    if (rang.tied > 1) {
        /* The extension broke a tie (MF-1252 rule 2b). That is a
         * narrowing, not evidence — keep it visible. */
        info.formatName += QString(" — gleichauf mit %1 weiteren, die Endung "
                                   "%2 hat entschieden")
                               .arg(rang.tied - 1).arg(ext);
    }
    if (rang.band == UFT_PROBE_BAND_SIZE) {
        /* MF-729: 30..49 means "only the size fits". A PC-360K image
         * opens as MSX, a TI-99 image as XFD — the registry's decision,
         * but not an identification. Say what it rests on. */
        info.formatName += QString(" — erkannt nur an der Dateigroesse "
                                   "(Konfidenz %1, %2 Bewerber in diesem "
                                   "Band)")
                               .arg(rang.confidence)
                               .arg(rang.band_claimants);
    }
    /* The registry names no platform; none is invented. */
    info.platform.clear();
    /* UFT_FORMAT_CAP_FLUX ("Hat Flux-Daten") is the plugin's own
     * declaration — the only flag the registry carries for it. */
    info.isFluxFormat =
        plugin && (plugin->capabilities & UFT_FORMAT_CAP_FLUX) != 0;
    if (haveGeometry) {
        info.tracks = reported(g.cylinders);
        info.heads = reported(g.heads);
        info.sectorsPerTrack = reported(g.sectors);
        info.sectorSize = reported(g.sector_size);
    }
    return true;
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
