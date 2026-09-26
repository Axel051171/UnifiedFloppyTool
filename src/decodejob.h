#pragma once
/**
 * @file decodejob.h
 * @brief Worker Thread for Disk Decoding Operations
 * 
 * P0-GUI-001 FIX: Real Backend Integration
 * 
 * Uses UFT Unified API for actual disk image processing:
 * - uft_load() for image loading
 * - uft_verify() for sector verification  
 * - uft_get_track_info() for track analysis
 * 
 * USAGE:
 *   auto* thread = new QThread(this);
 *   auto* job = new DecodeJob();
 *   job->setSourcePath("/path/to/disk.adf");
 *   job->moveToThread(thread);
 *   
 *   connect(thread, &QThread::started, job, &DecodeJob::run);
 *   connect(job, &DecodeJob::progress, this, &MainWindow::onProgress);
 *   connect(job, &DecodeJob::finished, thread, &QThread::quit);
 *   connect(job, &DecodeJob::finished, job, &QObject::deleteLater);
 *   connect(thread, &QThread::finished, thread, &QObject::deleteLater);
 *   
 *   thread->start();
 */

#ifndef DECODEJOB_H
#define DECODEJOB_H

#include <QObject>
#include <QString>
#include <atomic>
/* MF-1265: der Kopierplan gilt auch hier (`P3-509`, zweiter Halbsatz
 * der Vorgabe: „FluxCopy + DeepCopy + Protected + Evidence ist EIN
 * Vorgang"). */
#include "uft/core/uft_copy_plan.h"
/* P0-17 (Tuer Stufe 2): der Sektorstatus kommt aus dem Modell. */
#include "uft/core/uft_disk2.h"

/** Gesetzt, seit `sectorUpdate` den Kopf traegt und der Status aus dem
 *  Modell kommt (P0-17). `tests/test_decode_job_no_fiction.cpp` fragt ihn
 *  ab, damit derselbe Test gegen den Vorzustand uebersetzbar bleibt und
 *  dort ROT wird, statt gar nicht zu bauen. */
#define UFT_DECODEJOB_MODEL_STATUS 1

/**
 * @brief Image information returned after decode
 *
 * P0-17: die Sektorzahlen sind GEMESSEN — gezaehlt aus den Sektoren, die
 * das Plugin geliefert hat (`uft_d2_from_disk()`), nicht aus einer
 * Geometrie. Die Klassen sind disjunkt und summieren sich zu
 * `totalSectors`:
 *
 *   goodSectors       Pruefsumme bekannt und stimmt
 *   badSectors        Pruefsumme (Daten ODER Kennfeld) bekannt und falsch
 *   weakSectors       Flackerbits gemeldet, Pruefsumme nicht falsch
 *   deletedSectors    geloeschte Datenmarke, Pruefsumme nicht falsch
 *   missingSectors    kein Datenfeld (Fuellmaterial / nur Kennfeld)
 *   uncheckedSectors  Daten liegen vor, das Format traegt KEINE
 *                     Pruefsummenangabe (IMG, D64, ADF, MSA ...) — weder
 *                     gut noch schlecht
 *
 * `tracksWithoutSectors` zaehlt Spuren der Plugin-Geometrie, fuer die das
 * Modell keinen Sektor hat — das Plugin hat sie nicht geliefert, nur als
 * Fluss/Bitstrom geliefert, oder `read_track()` scheiterte. Welches davon,
 * sagt der Rueckgabewert nicht (TRACK_UNREADABLE in der Bruecke); diese
 * Spuren zaehlen NIE als gute und NIE als schlechte Sektoren.
 *
 * `rejectedSectors` steht NEBEN den Klassen, nicht in `totalSectors`: das
 * Plugin hat diese Sektoren geliefert, `uft_d2_add_sector()` hat sie
 * abgewiesen (Befund CONF_UNEARNED oder NO_MEMORY) — sie sind nicht im
 * Modell, also weder gut noch schlecht, aber sie werden GENANNT (Kein Bit
 * verloren). Eine Spur, deren Sektoren alle abgewiesen wurden, ist keine
 * Spur ohne Sektoren. `rejectedWithoutPlace` ist der Teil davon, fuer den
 * das Modell keinen Befund mit Spurangabe mehr fuehren konnte (die
 * Befundliste ist auf UFT_D2_MAX_DIAG begrenzt).
 */
struct DecodeResult {
    QString formatName;
    QString platformName;
    QString volumeName;
    QString readerName;        ///< Plugin, das die Datei geoeffnet hat; leer = keins
    int tracks = 0;            ///< 0 = nicht ermittelt
    int heads = 0;
    int sectorsPerTrack = 0;
    int sectorSize = 0;
    qint64 totalSize = 0;
    int goodSectors = 0;
    int badSectors = 0;
    int weakSectors = 0;
    int deletedSectors = 0;
    int missingSectors = 0;
    int uncheckedSectors = 0;
    int totalSectors = 0;      ///< Sektoren im Modell, also GELESEN
    int tracksAsked = 0;       ///< read_track-Aufrufe der Bruecke
    int tracksFailed = 0;      ///< davon mit Fehlercode
    int tracksWithoutSectors = 0;
    int rejectedSectors = 0;       ///< geliefert, vom Modell abgewiesen (Bruecke)
    int rejectedWithoutPlace = 0;  ///< davon ohne Befund mit Spurangabe
    bool hasErrors = false;
    /** The bridge asked fewer tracks than the plugin geometry has: the model
     *  stopped partway (e.g. no memory for a track). NOT the return value of
     *  uft_d2_from_disk() — that is `sectors > 0 || bitstreams > 0` and
     *  false for every completely read flux file. The counts are a PARTIAL
     *  result, and the job ends in error() — never in "Decode complete". */
    bool readAborted = false;
};

/** Gesetzt, seit abgewiesene Sektoren genannt werden, das Ende ohne
 *  Sektoraussage neutral lautet und der Abbruch je Sektor greift (P0-17,
 *  Tuer Stufe 2, zweite Fassung). Der Test fragt ihn ab, damit er gegen die
 *  erste Fassung uebersetzbar bleibt und dort ROT wird. */
#define UFT_DECODEJOB_STAGE2_V2 1

/**
 * @brief Worker class for disk decode operations
 * 
 * Runs in separate thread to prevent UI freeze.
 * Uses UFT C backend for actual decoding.
 * Provides progress updates and cancel capability.
 *
 * CANCEL GRANULARITY (P0-17): a cancel request is honoured before loading,
 * after loading, and before EVERY `sectorUpdate` of the emission loop — the
 * job stops within the track and ends in error(), never in finished().
 * One step is NOT interruptible: `uft_d2_from_disk()` reads every track of
 * the plugin geometry in a single call. A cancel that arrives during that
 * call takes effect when it returns. Measured 2026-09-26: the whole job
 * over all 110 files of tests/corpus_free took 404 ms together; on a large
 * flux file a plugin actually decodes it is NOT measured (open, P3-556 in
 * docs/OPEN_ITEMS.md).
 */
class DecodeJob : public QObject
{
    Q_OBJECT
    
public:
    explicit DecodeJob(QObject *parent = nullptr);
    ~DecodeJob();
    
    /**
     * @brief Set source file path
     * @param path Path to disk image (ADF, D64, SCP, etc.)
     */
    void setSourcePath(const QString& path);
    
    /**
     * @brief Set destination path for conversion (optional)
     * @param path Output path
     * @param format Output format (e.g., "ADF", "D64")
     */
    void setDestination(const QString& path, const QString& format = QString());

    /**
     * @brief Den Kopierplan fuer diesen Auftrag setzen (MF-1265).
     *
     * Gespeichert wird eine **Kopie**, und das ist kein Zufall: dieser
     * Auftrag laeuft nach `moveToThread()` in einem eigenen Faden. Ein
     * Zeiger auf einen Reiter waere ein Zugriff auf die Oberflaeche aus
     * einem fremden Faden; eine Rueckruffunktion ebenso. `uft_copy_plan_t`
     * ist ein reiner Wertetyp, also ist die Kopie billig und sicher.
     *
     * Wer ihn nicht setzt, bekommt die Vorgaben wie bisher — `run()`
     * fragt `m_planGesetzt`, nicht den Inhalt. Ein genullter Plan ist
     * naemlich ein GUELTIGER Plan (FILE/FAST/LOGICAL/NORMAL), und ihn von
     * „kein Plan" zu unterscheiden ist genau der Punkt.
     */
    void setCopyPlan(const uft_copy_plan_t& plan);


    /**
     * @brief Get decode results after completion
     */
    DecodeResult result() const { return m_result; }
    
    /**
     * @brief Request cancellation of running job
     */
    void requestCancel();
    
    /**
     * @brief Check if job was cancelled
     */
    bool isCancelled() const;

    /**
     * @brief Status of ONE sector of the model, as `sectorUpdate` sends it.
     *
     * Uses exactly what `uft_d2_sector_t` carries, first match wins:
     *   "MISSING"     no data field (`has_data` false)
     *   "ID_CRC_BAD"  ID checksum known and wrong
     *   "CRC_BAD"     data checksum known and wrong
     *   "WEAK"        weak bits reported
     *   "DELETED"     deleted data mark (0xF8)
     *   "OK"          data checksum known and right
     *   "UNCHECKED"   data present, the format carries no checksum
     */
    static QString statusOf(const uft_d2_sector_t& s);

    /**
     * @brief The step from the model to counts and signals.
     *
     * Called by verifySectors() after `uft_d2_from_disk()`; public so the
     * rule can be tested with a synthetic model (a rejected sector is not
     * reachable through any corpus file, see P0-17). Resets the sector
     * classes of result(), emits one `sectorUpdate` per model sector, one
     * "REJECTED" per sector the model refused (place from its findings),
     * and one "NO_SECTORS" per track with neither. Checks isCancelled()
     * before every emission and returns false when it stopped for it.
     *
     * @param tracksAsked / tracksFailed / sectorsRejected  the bridge stats
     */
    bool tallyModel(const uft_disk2_t *d2, int cylinders, int heads,
                    int tracksAsked, int tracksFailed, int sectorsRejected);

    /**
     * @brief The text of finished(), built from a result and nothing else.
     *
     * No sector in the model -> "Lauf beendet — keine Sektoraussage ...",
     * never "Decode complete" (nothing was decoded). Otherwise the classes
     * of DecodeResult, each named, and the rejected sectors when > 0.
     */
    static QString resultMessage(const DecodeResult& r);

signals:
    /**
     * @brief Progress update (0-100%)
     */
    void progress(int percentage);
    
    /**
     * @brief Current operation stage changed
     */
    void stageChanged(const QString& stage);
    
    /**
     * @brief Track/sector update for visualization
     *
     * P0-17: carries the HEAD. Before, side 0 and side 1 of a cylinder
     * arrived as the same (track, sector) pair.
     *
     * @param track  physical cylinder the plugin delivered
     * @param head   head (side)
     * @param sector sector ID as read; -1 = the whole track ("NO_SECTORS":
     *               the model holds no sector for it)
     * @param status one of the strings of statusOf(), "NO_SECTORS", or
     *               "REJECTED" (delivered by the plugin, refused by the
     *               model — neither good nor bad, see DecodeResult)
     */
    void sectorUpdate(int track, int head, int sector, const QString& status);
    
    /**
     * @brief Image information available
     */
    void imageInfo(const DecodeResult& info);
    
    /**
     * @brief Job completed successfully
     */
    void finished(const QString& resultMessage);
    
    /**
     * @brief Job failed with error
     */
    void error(const QString& errorMessage);
    
public slots:
    /**
     * @brief Main worker function - runs in worker thread
     */
    void run();
    
private:
    std::atomic_bool m_cancel{false};
    QString m_sourcePath;
    QString m_destPath;
    QString m_destFormat;
    /* MF-1265: eine KOPIE des Plans, kein Zeiger — siehe setCopyPlan(). */
    uft_copy_plan_t m_plan{};
    bool m_planGesetzt = false;
    DecodeResult m_result;
    /* P0-17: opened ONCE through the plugin path the rest of the product
     * uses (`uft_disk_open`), owned by the worker thread, closed at the
     * end of run() on every path. */
    uft_disk_t *m_disk = nullptr;
    void closeDisk();

    /**
     * @brief Load and analyze image using UFT backend
     */
    bool loadImage();
    
    /**
     * @brief Verify all sectors
     */
    bool verifySectors();
    
    /**
     * @brief Convert to destination format (if set)
     */
    bool convertImage();
};

#endif // DECODEJOB_H
