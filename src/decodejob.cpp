/**
 * @file decodejob.cpp
 * @brief Worker Thread Implementation for Disk Decoding
 * 
 * P0-GUI-001 FIX: Real Backend Integration
 *
 * Uses DiskImageValidator for format detection and
 * performs actual file I/O for sector analysis.
 *
 * P0-17 (Tuer Stufe 2): the sector status comes from what the format
 * plugin actually delivered — `uft_disk_open()` (the path the rest of the
 * product uses, e.g. DiskAnalyzerWindow) -> `uft_d2_from_disk()` -> one
 * status per sector of the model. Before, flux files were reported as
 * "all sectors OK" and sector images were judged by reading bytes at a
 * flat offset, with no plugin, no layout and no checksum.
 */

#include "decodejob.h"

extern "C" {
#include "uft/uft_format_convert.h"
#include "uft/uft_format_plugin.h"   /* before uft_core.h: its prototypes
                                      * name struct uft_format_plugin */
#include "uft/uft_core.h"
#include "uft/core/uft_disk2_bridge.h"
}

#include <cstring>
#include <memory>

#include "disk_image_validator.h"
#include <QThread>
#include <QDebug>
#include <QFileInfo>
#include <QFile>
#include <QCryptographicHash>
#include <QList>
#include <QMap>
#include <QPair>

// ============================================================================
// Construction / Destruction
// ============================================================================

DecodeJob::DecodeJob(QObject *parent)
    : QObject(parent)
    , m_cancel(false)
{
    qDebug() << "DecodeJob created";
}

DecodeJob::~DecodeJob()
{
    closeDisk();
    qDebug() << "DecodeJob destroyed";
}

void DecodeJob::closeDisk()
{
    if (m_disk) {
        uft_disk_close(m_disk);   /* also frees the handle */
        m_disk = nullptr;
    }
}

// ============================================================================
// Configuration
// ============================================================================

void DecodeJob::setSourcePath(const QString& path)
{
    m_sourcePath = path;
}

void DecodeJob::setDestination(const QString& path, const QString& format)
{
    m_destPath = path;
    m_destFormat = format;
}

/* MF-1265: eine Kopie, gesetzt VOR `moveToThread()`/`run()`. Danach
 * gehoert der Auftrag einem anderen Faden, und niemand fasst ihn mehr
 * an. */
void DecodeJob::setCopyPlan(const uft_copy_plan_t& plan)
{
    m_plan = plan;
    m_planGesetzt = true;
}

void DecodeJob::requestCancel()
{
    m_cancel.store(true, std::memory_order_relaxed);
    qDebug() << "Cancel requested";
}

bool DecodeJob::isCancelled() const
{
    return m_cancel.load(std::memory_order_relaxed);
}

// ============================================================================
// Main Worker Function
// ============================================================================

void DecodeJob::run()
{
    qDebug() << "DecodeJob::run() started in thread" << QThread::currentThreadId();
    qDebug() << "Source:" << m_sourcePath;

    /* P0-17: the plugin handle opened in loadImage() is closed on EVERY
     * way out of run() — error, cancel and success alike. */
    struct DiskGuard {
        DecodeJob *job;
        ~DiskGuard() { job->closeDisk(); }
    } diskGuard{this};
    
    // Validate input
    if (m_sourcePath.isEmpty()) {
        emit error("No source file specified");
        return;
    }
    
    QFileInfo fileInfo(m_sourcePath);
    if (!fileInfo.exists()) {
        emit error(QString("File not found: %1").arg(m_sourcePath));
        return;
    }
    
    emit stageChanged("Initializing...");
    emit progress(0);
    
    if (isCancelled()) {
        emit error("Operation cancelled by user");
        return;
    }
    
    // Phase 1: Load and Validate Image (0-30%)
    emit stageChanged("Loading image...");
    emit progress(5);
    
    if (!loadImage()) {
        return; // Error already emitted
    }
    
    if (isCancelled()) {
        emit error("Operation cancelled during load");
        return;
    }
    
    // Phase 2: Verify Sectors (30-80%)
    emit stageChanged("Verifying sectors...");
    emit progress(30);
    
    if (!verifySectors()) {
        return; // Error already emitted
    }
    
    if (isCancelled()) {
        emit error("Operation cancelled during verification");
        return;
    }
    
    // Phase 3: Convert (80-95%) - optional
    if (!m_destPath.isEmpty()) {
        emit stageChanged("Converting...");
        emit progress(80);
        
        if (!convertImage()) {
            return; // Error already emitted
        }
    }
    
    // Phase 4: Finalize (95-100%)
    emit stageChanged("Finalizing...");
    emit progress(95);
    
    const QString resultMsg = resultMessage(m_result);

    emit progress(100);
    emit finished(resultMsg);

    qDebug() << "DecodeJob::run() completed successfully";
}

/* Build result message.
 *
 * P0-17: here stood "%good/%total sectors OK", where %total came from a
 * geometry (for flux formats a default of 80 x 2 x 9) and %good was set
 * equal to it for every flux file. The message now says what was READ, in
 * the classes of DecodeResult, and nothing more.
 *
 * P0-17 (second version): with no sector in the model the text used to
 * begin "Decode complete!" — and StatusTab put "✓ Decode Complete" above
 * it — while it went on to say "nicht dekodiert". Nothing was decoded, so
 * nothing is complete; the run is over, and that is all the text says. */
QString DecodeJob::resultMessage(const DecodeResult& r)
{
    QString msg;
    if (r.totalSectors == 0) {
        msg = QString("%1 — keine Sektoraussage: %2 (%3) nicht "
                      "dekodiert")
            .arg(r.readAborted ? QStringLiteral("Lesen abgebrochen")
                               : QStringLiteral("Lauf beendet"))
            .arg(r.formatName)
            .arg(r.platformName);
        if (r.readerName.isEmpty() && r.rejectedSectors == 0) {
            msg += QString(" (kein Plugin hat die Datei geoeffnet — "
                           "unbekannt oder mehrdeutig)");
        } else if (r.rejectedSectors == 0) {
            msg += QString(" (Plugin \"%1\" lieferte fuer %2 von %3 "
                           "Spuren keinen Sektor)")
                .arg(r.readerName)
                .arg(r.tracksWithoutSectors)
                .arg(r.tracksAsked);
        } else if (r.tracksWithoutSectors > 0) {
            msg += QString("; %1 von %2 Spuren ohne Sektoraussage")
                .arg(r.tracksWithoutSectors)
                .arg(r.tracksAsked);
        }
    } else {
        /* A partial model is not a finished decode: say so up front. */
        msg = QString("%1 %2 (%3): %4 Sektoren gelesen — "
                      "%5 Pruefsumme stimmt, %6 Pruefsumme falsch, "
                      "%7 ohne Pruefsummenangabe")
            .arg(r.readAborted
                     ? QStringLiteral("Lesen abgebrochen — Teilergebnis:")
                     : QStringLiteral("Decode complete!"))
            .arg(r.formatName)
            .arg(r.platformName)
            .arg(r.totalSectors)
            .arg(r.goodSectors)
            .arg(r.badSectors)
            .arg(r.uncheckedSectors);
        if (r.missingSectors > 0)
            msg += QString(", %1 ohne Datenfeld").arg(r.missingSectors);
        if (r.weakSectors > 0)
            msg += QString(", %1 mit Flackerbits").arg(r.weakSectors);
        if (r.deletedSectors > 0)
            msg += QString(", %1 geloescht markiert").arg(r.deletedSectors);
        if (r.tracksWithoutSectors > 0)
            msg += QString("; %1 von %2 Spuren ohne Sektoraussage")
                .arg(r.tracksWithoutSectors)
                .arg(r.tracksAsked);
    }
    if (r.tracksFailed > 0) {
        msg += QString(" (davon %1 mit Fehlercode von read_track — ob "
                       "unlesbar, im Behaelter fehlend oder unformatiert, "
                       "sagt der Rueckgabewert nicht)")
            .arg(r.tracksFailed);
    }
    /* Delivered by the plugin, refused by the model: not in any class
     * above, and not silently gone either (Kein Bit verloren). */
    if (r.rejectedSectors > 0) {
        msg += QString("; %1 vom Modell abgewiesen (geliefert, aber "
                       "Zuversicht ohne Beleg oder kein Speicher — weder gut "
                       "noch schlecht gezaehlt")
            .arg(r.rejectedSectors);
        if (r.rejectedWithoutPlace > 0)
            msg += QString(", %1 davon ohne Spurangabe").arg(r.rejectedWithoutPlace);
        msg += QStringLiteral(")");
    }
    return msg;
}

// ============================================================================
// Load Image using DiskImageValidator
// ============================================================================

bool DecodeJob::loadImage()
{
    // Use existing DiskImageValidator for format detection
    DiskImageInfo info = DiskImageValidator::validate(m_sourcePath);
    
    if (!info.isValid) {
        emit error(QString("Invalid image: %1").arg(info.errorMessage));
        return false;
    }
    
    emit progress(15);
    
    // Populate result from validated info
    m_result.formatName = info.formatName;
    m_result.platformName = info.platform;
    m_result.totalSize = info.fileSize;
    m_result.hasErrors = false;

    /* P0-17: the geometry is what the plugin that OPENED the file says.
     *
     * Here stood `info.tracks > 0 ? info.tracks : 80` and the same for
     * heads (2), sectors (9) and size (512). The validator knows no
     * geometry for flux formats, so every SCP/HFE/KryoFlux file became an
     * 80 x 2 x 9 disk, and `totalSectors` = 1440 became the "1440/1440
     * sectors OK" of the finished message. The job now opens the file the
     * way the rest of the product does (`uft_disk_open`, the plugin
     * registry — no second reader), and without a plugin the geometry
     * stays 0 = "nicht ermittelt" instead of a default. */
    closeDisk();
    m_disk = uft_disk_open(m_sourcePath.toUtf8().constData(), /*read_only=*/true);
    uft_geometry_t g;
    memset(&g, 0, sizeof(g));
    if (m_disk && uft_disk_get_geometry(m_disk, &g) == UFT_OK) {
        const uft_format_plugin_t *plugin = uft_disk_plugin(m_disk);
        m_result.readerName = QString::fromUtf8(
            plugin && plugin->name ? plugin->name : "(unbenannt)");
        m_result.tracks = g.cylinders;
        m_result.heads = g.heads;
        m_result.sectorsPerTrack = g.sectors;
        m_result.sectorSize = g.sector_size;
    } else {
        m_result.readerName.clear();
        m_result.tracks = info.tracks > 0 ? info.tracks : 0;
        m_result.heads = info.heads > 0 ? info.heads : 0;
        m_result.sectorsPerTrack = info.sectorsPerTrack > 0 ? info.sectorsPerTrack : 0;
        m_result.sectorSize = info.sectorSize > 0 ? info.sectorSize : 0;
    }
    /* Measured in verifySectors(), not planned from a geometry. */
    m_result.totalSectors = 0;
    
    qDebug() << "Loaded:" << m_result.formatName << m_result.platformName
             << m_result.tracks << "tracks" << m_result.heads << "heads"
             << m_result.sectorsPerTrack << "spt";
    
    // Emit image info signal
    emit imageInfo(m_result);
    
    emit progress(25);
    return true;
}

// ============================================================================
// Verify Sectors by Reading and Checking Data
// ============================================================================

QString DecodeJob::statusOf(const uft_d2_sector_t& s)
{
    /* Order = weight. A known-wrong checksum outweighs weak bits and a
     * deleted mark; "known" is required before anything counts as right
     * or wrong — a zeroed `id_crc_ok`/`data_crc_ok` without the matching
     * `*_known` is the default of memset, not a measurement (the rule of
     * uft_disk2_bridge.h: `UFT_SECTOR_OK` is set unconditionally by
     * uft_format_add_sector() and is therefore no evidence). */
    if (!s.has_data)                              return QStringLiteral("MISSING");
    if (s.id_crc_known && !s.id_crc_ok)           return QStringLiteral("ID_CRC_BAD");
    if (s.data_crc_known && !s.data_crc_ok)       return QStringLiteral("CRC_BAD");
    if (s.weak_bits > 0)                          return QStringLiteral("WEAK");
    if (s.dam == 0xF8)                            return QStringLiteral("DELETED");
    if (s.data_crc_known && s.data_crc_ok)        return QStringLiteral("OK");
    return QStringLiteral("UNCHECKED");
}

bool DecodeJob::tallyModel(const uft_disk2_t *d2, int cylinders, int heads,
                           int tracksAsked, int tracksFailed, int sectorsRejected)
{
    m_result.goodSectors = m_result.badSectors = 0;
    m_result.weakSectors = m_result.deletedSectors = 0;
    m_result.missingSectors = m_result.uncheckedSectors = 0;
    m_result.totalSectors = 0;
    m_result.tracksWithoutSectors = 0;
    m_result.tracksAsked = tracksAsked;
    m_result.tracksFailed = tracksFailed;
    m_result.rejectedSectors = sectorsRejected;
    m_result.rejectedWithoutPlace = 0;
    if (!d2) return !isCancelled();

    /* Where the model refused a sector, it says so in a finding WITH the
     * place: uft_d2_add_sector() writes CONF_UNEARNED or NO_MEMORY on the
     * sector layer with cylinder, head and sector ID. Measured over the
     * producers in uft_disk2.c: these two codes on that layer with a
     * cylinder >= 0 come from add_sector only. The finding list is capped
     * (UFT_D2_MAX_DIAG); what has no finding left is counted apart. */
    QMap<QPair<int, int>, QList<int>> rejectedAt;
    int located = 0;
    const size_t nd = uft_d2_diag_count(d2);
    for (size_t i = 0; i < nd; ++i) {
        const uft_d2_diag_t *f = uft_d2_diag_at(d2, i);
        if (!f || f->layer != UFT_D2_LAYER_SECTORS || f->cyl < 0 || f->head < 0)
            continue;
        if (std::strcmp(f->code, "CONF_UNEARNED") != 0 &&
            std::strcmp(f->code, "NO_MEMORY") != 0)
            continue;
        rejectedAt[qMakePair(int(f->cyl), int(f->head))].append(f->sector);
        located++;
    }
    m_result.rejectedWithoutPlace = qMax(0, sectorsRejected - located);

    for (int c = 0; c < cylinders; ++c) {
        for (int h = 0; h < heads; ++h) {
            if (isCancelled()) return false;
            const uft_d2_track_t *t = uft_d2_track_get(d2,
                                                       static_cast<uint16_t>(c),
                                                       static_cast<uint8_t>(h));
            const size_t n = t ? t->sectors.count : 0u;
            const QList<int> rej = rejectedAt.value(qMakePair(c, h));
            if (n == 0 && rej.isEmpty()) {
                /* Not a good sector, not a bad sector: the model holds no
                 * sector for this track, and refused none. Whether
                 * read_track failed, the track is absent/unformatted in the
                 * container, or it came as flux/bitstream only, is not said
                 * by the plugin's return value (TRACK_UNREADABLE in the
                 * bridge). */
                m_result.tracksWithoutSectors++;
                emit sectorUpdate(c, h, -1, QStringLiteral("NO_SECTORS"));
                continue;
            }
            for (size_t i = 0; i < n; ++i) {
                if (isCancelled()) return false;   /* per sector */
                const uft_d2_sector_t &s = t->sectors.items[i];
                const QString status = statusOf(s);
                if (status == "OK")                                  m_result.goodSectors++;
                else if (status == "CRC_BAD" || status == "ID_CRC_BAD") m_result.badSectors++;
                else if (status == "WEAK")                           m_result.weakSectors++;
                else if (status == "DELETED")                        m_result.deletedSectors++;
                else if (status == "MISSING")                        m_result.missingSectors++;
                else                                                 m_result.uncheckedSectors++;
                m_result.totalSectors++;
                emit sectorUpdate(c, h, s.id_sec, status);
            }
            /* A track whose sectors were all refused is NOT a track without
             * sectors — the plugin delivered them. One update each, with
             * the ID the finding names. */
            for (int id : rej) {
                if (isCancelled()) return false;
                emit sectorUpdate(c, h, id, QStringLiteral("REJECTED"));
            }
        }
        if (cylinders > 0)
            emit progress(30 + (c * 50 / cylinders));
    }
    return !isCancelled();
}

bool DecodeJob::verifySectors()
{
    /* The flux hash stays (it is a statement about the FILE, and a true
     * one). What is gone is the line after it: "For flux, mark all as OK
     * (we can't verify individual sectors)" — which then set
     * goodSectors = totalSectors, badSectors = 0. */
    QFileInfo fi(m_sourcePath);
    const QString ext = fi.suffix().toLower();
    const bool isFluxFormat = (ext == "scp" || ext == "raw" || ext == "g64" ||
                               ext == "nib" || ext == "hfe" || ext == "ipf");
    if (isFluxFormat) {
        emit stageChanged("Verifying flux data...");
        QFile file(m_sourcePath);
        if (!file.open(QIODevice::ReadOnly)) {
            emit error(QString("Cannot open file: %1").arg(file.errorString()));
            return false;
        }
        const QByteArray data = file.readAll();
        file.close();
        if (data.isEmpty()) {
            emit error("Failed to read flux data");
            return false;
        }
        const QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Md5);
        qDebug() << "MD5:" << hash.toHex();
    }

    m_result.goodSectors = m_result.badSectors = 0;
    m_result.weakSectors = m_result.deletedSectors = 0;
    m_result.missingSectors = m_result.uncheckedSectors = 0;
    m_result.totalSectors = 0;
    m_result.tracksAsked = m_result.tracksFailed = 0;
    m_result.tracksWithoutSectors = 0;
    m_result.hasErrors = false;

    if (!m_disk) {
        /* No plugin opened the file (unknown, or ambiguous since MF-1251).
         * Then there is nothing to say about any sector — and that is what
         * the result says, instead of reading bytes at a flat offset. */
        emit stageChanged("Keine Sektoraussage: kein Plugin hat die Datei "
                          "geoeffnet — nichts dekodiert.");
        if (isCancelled()) {
            emit error("Verification cancelled before completion — counts not finalised.");
            return false;
        }
        emit progress(80);
        return true;
    }

    emit stageChanged(QString("Reading tracks via plugin \"%1\"...")
                          .arg(m_result.readerName));

    /* The model. It belongs to this (worker) thread and dies with this
     * function; see "NEBENLAEUFIGKEIT" in uft_disk2.h. */
    std::unique_ptr<uft_disk2_t, void (*)(uft_disk2_t *)> d2(uft_d2_create(),
                                                            uft_d2_destroy);
    if (!d2) {
        emit error("Kein Speicher fuer das Diskettenmodell — nichts gelesen.");
        return false;
    }
    uft_d2_bridge_stats_t st;
    memset(&st, 0, sizeof(st));
    /* One call reads every track of the plugin geometry. It is NOT
     * interruptible (named in the class header of decodejob.h); the
     * cancel request is honoured right after it and before every
     * emission in tallyModel(). */
    uft_d2_from_disk(d2.get(), m_disk, uft_disk_plugin(m_disk), &st);

    uft_geometry_t g;
    memset(&g, 0, sizeof(g));
    uft_disk_get_geometry(m_disk, &g);

    /* Is the model complete? NOT from the return value: it is
     * `st.sectors > 0 || st.bitstreams > 0` (uft_disk2_bridge.c), i.e.
     * false for every flux-only file that was read in full (measured: the
     * KFX and SCP corpus files) and ALSO false when uft_d2_track() failed
     * partway. The stats separate the two: a bridge that asked fewer
     * tracks than the geometry has stopped early, and the tracks it never
     * reached must not count as "without sectors" under a "Decode
     * complete" (review of stage 2, MF-1350). */
    const long spuren_geometrie = static_cast<long>(g.cylinders) * g.heads;
    m_result.readAborted = static_cast<long>(st.tracks_asked) < spuren_geometrie;

    tallyModel(d2.get(), g.cylinders, g.heads,
               static_cast<int>(st.tracks_asked),
               static_cast<int>(st.tracks_failed),
               static_cast<int>(st.sectors_rejected));

    // MF-118: previously this line read
    //     m_result.goodSectors = goodCount > 0 ? goodCount : m_result.totalSectors;
    // which silently turned "user cancelled before any sector was checked"
    // into "verification claimed all sectors OK". That violated
    // "Keine erfundenen Daten" — the result struct then went out via
    // finished() and ended up in forensic reports.
    //
    // Honest behaviour: the flux branch above already set goodSectors =
    // totalSectors with an explicit "we cannot verify individual sectors"
    // intent; we leave that alone. The sector branch trusts whatever was
    // actually counted; cancel makes verifySectors() return false so the
    // job emits an error rather than a finished("X/Y good") signal.
    //
    // P0-17: the second paragraph above is withdrawn and stays quoted. The
    // flux branch was left alone by MF-118 — and was the same invention in
    // a different branch. There is no flux branch for the counts any more:
    // every file goes through the model, and cancel still ends in error().
    m_result.hasErrors = (m_result.badSectors > 0);

    qDebug() << "Verify:" << m_result.totalSectors << "read,"
             << m_result.goodSectors << "crc ok," << m_result.badSectors << "crc bad,"
             << m_result.uncheckedSectors << "unchecked,"
             << m_result.tracksWithoutSectors << "tracks without sectors";

    if (isCancelled()) {
        emit error("Verification cancelled before completion — counts not finalised.");
        return false;
    }

    if (m_result.readAborted) {
        emit error(resultMessage(m_result));
        return false;
    }

    emit progress(80);
    return true;
}

// ============================================================================
// Convert Image (ueber uft_convert_file, MF-568)
// ============================================================================

bool DecodeJob::convertImage()
{
    if (m_destPath.isEmpty()) {
        return true; // No conversion requested
    }

    emit progress(85);

    /* -- MF-568: hier stand eine Kopie in 64-kB-Bloecken ----------------
     *
     * Der Kopf der Funktion sagte es selbst: "Convert Image (Simple Copy
     * for now)", und daneben stand "Real implementation would use format
     * conversion APIs". Die Oberflaeche meldete waehrenddessen
     * "Converting...".
     *
     * Wer also eine SCP-Aufnahme mit Ziel `.d64` dekodieren liess, bekam
     * die SCP-Datei unter dem Namen `.d64` zurueck -- ohne Warnung, ohne
     * Verlustmeldung, mit Fortschrittsbalken bis 95 %.
     *
     * Bemerkenswert daran: MF-122 hat INNERHALB dieser Kopie den
     * Rueckgabewert von `write()` gehaertet, weil eine volle Platte sonst
     * still eine abgeschnittene Datei erzeugt haette. Die Sorgfalt galt
     * dem Detail; dass die Funktion gar nicht wandelt, blieb stehen.
     *
     * Jetzt geht auch dieser Weg durch `uft_convert_file()` -- denselben
     * Engpass wie die Konvertieren-Schaltflaeche (ToolsTab) und derselbe,
     * den MF-567 fuer den Speicher-Weg dichtgemacht hat. */
    uft_format_t dstFmt = uft_format_from_name(
        QFileInfo(m_destPath).suffix().toUtf8().constData());
    if (dstFmt == UFT_FORMAT_UNKNOWN) {
        emit error(QString("Unknown target format for '%1' - nothing was "
                           "written.").arg(QFileInfo(m_destPath).fileName()));
        return false;
    }

/* Vorgaben holen, nicht nullen (MF-672).
     *
     * Hier stand `memset(&opts, 0, sizeof(opts))`. Das ist nicht dasselbe
     * wie "keine besonderen Wuensche": `uft_convert_default_options()`
     * setzt zehn Werte, und einer davon aendert das Ergebnis.
     *
     *   `use_multiple_revs` steht per Vorgabe auf TRUE. Genullt ist es
     *   false, und `uft_format_convert_flux.c:964` liest
     *   `(!opts || opts->use_multiple_revs)` — ein Aufrufer, der NULL
     *   uebergibt, bekommt die Verschmelzung ueber alle Umdrehungen; ein
     *   Aufrufer, der eine genullte Struktur uebergibt, bekommt sie
     *   nicht.
     *
     * Die Oberflaeche war damit schlechter als gar keine Angabe: sie hat
     * SCP-Abbilder mit einer Umdrehung dekodiert, wo fuenf vorlagen, und
     * niemand konnte es sehen.
     *
     * `accept_data_loss` bleibt ausdruecklich aus — die Vorgabe laesst es
     * absichtlich ungesetzt (UFT-A05), und das gilt hier weiter. */
    uft_convert_options_t opts = uft_convert_default_options();

    /* MF-1265 (`P3-509`, zweiter Halbsatz): der Kopierplan gilt auch
     * fuer den Hintergrundauftrag. Bis dahin erreichte er nur den
     * Speicherpfad (MF-1263) — „EIN Vorgang" war damit erst zur
     * Haelfte wahr.
     *
     * Der Plan steht als KOPIE bereit (`setCopyPlan()`); hier wird
     * nichts an der Oberflaeche gefragt, weil dieser Faden sie nicht
     * anfassen darf. */
    /* MF-1309: erst das Tor, dann die Uebersetzung. Ein Plan, der sich
     * selbst widerspricht, darf keinen Hintergrundauftrag starten. */
    if (m_planGesetzt) {
        const char *grund = nullptr;
        if (uft_copy_plan_gate(&m_plan, &grund) == UFT_COPY_DENY) {
            emit error(QStringLiteral("Kopierplan nicht ausfuehrbar: %1")
                           .arg(QString::fromUtf8(grund ? grund : "unbekannt")));
            return false;
        }
        uft_copy_plan_to_convert_options(&m_plan, &opts);
    }

    /* Ein Hintergrundauftrag hat niemanden zu fragen. Verlustbehaftete
     * Wandlungen weist das Tor hier also ab, statt sie stillschweigend
     * zu erlauben -- die Zustimmung gehoert an die Oberflaeche, wo
     * jemand sie geben kann (ToolsTab).
     *
     * MF-1265: steht NACH dem Plan. Kein Plan erteilt sich selbst eine
     * Zustimmung (UFT-A05) — und im Hintergrund erst recht nicht. */
    opts.accept_data_loss = false;

    uft_convert_result_t res;
    memset(&res, 0, sizeof(res));
    uft_error_t rc = uft_convert_file(m_sourcePath.toUtf8().constData(),
                                      m_destPath.toUtf8().constData(),
                                      dstFmt, &opts, &res);

    if (rc != UFT_OK || !res.success) {
        QString msg = QString("Conversion to %1 refused or failed "
                              "(error %2)")
                          .arg(QString::fromUtf8(uft_format_get_name(dstFmt)))
                          .arg((int)rc);
        for (int i = 0; i < res.warning_count && i < 8; i++) {
            msg += "\n  " + QString::fromUtf8(res.warnings[i]);
        }
        emit error(msg);
        return false;
    }

    for (int i = 0; i < res.warning_count && i < 8; i++) {
        qDebug() << "convert:" << res.warnings[i];
    }
    qDebug() << "Converted to:" << m_destPath
             << res.tracks_converted << "tracks,"
             << res.sectors_converted << "sectors,"
             << res.sectors_failed << "failed";

    emit progress(95);
    return true;
}
