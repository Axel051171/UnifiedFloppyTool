/**
 * @file UftFormatDetectionModel.cpp
 * @brief Format Detection Model Implementation
 */

#include "UftFormatDetectionModel.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

/* Include C header */
extern "C" {
#include "uft/uft_format_autodetect.h"
#include "uft/uft_types.h"
}

UftFormatDetectionModel::UftFormatDetectionModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

UftFormatDetectionModel::~UftFormatDetectionModel()
{
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * QAbstractTableModel Interface
 * ═══════════════════════════════════════════════════════════════════════════════ */

int UftFormatDetectionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_candidates.size();
}

int UftFormatDetectionModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant UftFormatDetectionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_candidates.size())
        return QVariant();
    
    const UftFormatCandidate &c = m_candidates[index.row()];
    
    switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
                case ColFormat: return c.formatId;
                case ColName: return c.formatName;
                case ColConfidence: return QString("%1%").arg(c.confidence);
                case ColHeuristics: return c.heuristics;
            }
            break;
            
        case Qt::ToolTipRole:
            return c.description;
            
        case Qt::ForegroundRole:
            if (index.column() == ColConfidence) {
                return c.confidenceColor();
            }
            break;
            
        case Qt::BackgroundRole:
            if (c.isBest) {
                return QColor(232, 245, 233);  /* Light green background */
            }
            break;
            
        case Qt::FontRole:
            if (c.isBest) {
                QFont font;
                font.setBold(true);
                return font;
            }
            break;
            
        case Qt::UserRole:  /* Raw confidence value */
            return c.confidence;
            
        case Qt::UserRole + 1:  /* Format ID enum */
            return c.formatId;
    }
    
    return QVariant();
}

QVariant UftFormatDetectionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    
    switch (section) {
        case ColFormat: return tr("Format");
        case ColName: return tr("Name");
        case ColConfidence: return tr("Confidence");
        case ColHeuristics: return tr("Matched By");
    }
    
    return QVariant();
}

UftFormatCandidate UftFormatDetectionModel::candidateAt(int index) const
{
    if (index >= 0 && index < m_candidates.size()) {
        return m_candidates[index];
    }
    return UftFormatCandidate();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftFormatDetectionModel::detectFromFile(const QString &path)
{
    emit detectionStarted(path);
    clear();
    
    QFileInfo fi(path);
    if (!fi.exists()) {
        emit errorOccurred(tr("File not found: %1").arg(path));
        emit detectionFinished(false);
        return;
    }
    
    m_fileSize = fi.size();
    
    /* Call C detection function */
    uft_detect_result_t result;
    uft_detect_result_init(&result);
    
    uft_error_t err = uft_detect_format_file(path.toUtf8().constData(), nullptr, &result);
    
    if (err != UFT_OK) {
        emit errorOccurred(tr("Detection failed with error %1").arg(err));
        emit detectionFinished(false);
        return;
    }
    
    populateFromResult(&result);
    uft_detect_result_free(&result);
    
    emit detectionFinished(true);
}

void UftFormatDetectionModel::detectFromBuffer(const QByteArray &data, const QString &extension)
{
    emit detectionStarted(extension.isEmpty() ? "<buffer>" : extension);
    clear();
    
    m_fileSize = data.size();
    
    uft_detect_result_t result;
    uft_detect_result_init(&result);
    
    uft_detect_options_t opts = UFT_DETECT_OPTIONS_DEFAULT;
    if (!extension.isEmpty()) {
        static QByteArray extBuf;
        extBuf = extension.toUtf8();
        opts.hint_extension = extBuf.constData();
    }
    
    uft_error_t err = uft_detect_format_buffer(
        reinterpret_cast<const uint8_t*>(data.constData()),
        data.size(), &opts, &result);
    
    if (err != UFT_OK) {
        emit errorOccurred(tr("Buffer detection failed"));
        emit detectionFinished(false);
        return;
    }
    
    populateFromResult(&result);
    uft_detect_result_free(&result);
    
    emit detectionFinished(true);
}

void UftFormatDetectionModel::clear()
{
    beginResetModel();
    m_candidates.clear();
    m_warnings.clear();
    m_bestFormat.clear();
    m_bestFormatName.clear();
    m_bestConfidence = 0;
    m_detectionTimeMs = 0;
    m_fileSize = 0;
    endResetModel();
    emit resultsChanged();
}

void UftFormatDetectionModel::updateFromResult(const uft_detect_result_t *result)
{
    if (!result) return;
    populateFromResult(result);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftFormatDetectionModel::populateFromResult(const uft_detect_result_t *result)
{
    beginResetModel();
    
    m_candidates.clear();
    m_warnings.clear();
    
    /* Populate candidates */
    for (int i = 0; i < result->candidate_count; i++) {
        const auto &c = result->candidates[i];
        
        UftFormatCandidate cand;
        cand.formatId = formatIdToString(c.format);
        cand.formatName = c.format_name ? QString::fromUtf8(c.format_name) 
                                         : formatIdToName(c.format);
        cand.description = c.format_desc ? QString::fromUtf8(c.format_desc) : "";
        cand.confidence = c.score;
        cand.heuristics = heuristicsToString(c.heuristics_matched);
        cand.isBest = (i == 0);  /* First is best (sorted by score) */
        
        m_candidates.append(cand);
    }
    
    /* Populate warnings */
    for (int i = 0; i < result->warning_count; i++) {
        UftDetectionWarning warn;
        warn.message = QString::fromUtf8(result->warnings[i].text);
        warn.severity = result->warnings[i].severity;
        m_warnings.append(warn);
    }
    
    /* Set best match info */
    if (result->candidate_count > 0) {
        m_bestFormat = formatIdToString(result->best_format);
        m_bestFormatName = result->best_name ? QString::fromUtf8(result->best_name)
                                              : formatIdToName(result->best_format);
        m_bestConfidence = result->best_score;
    }
    
    m_detectionTimeMs = result->detection_time_ms;
    m_fileSize = result->file_size;
    
    endResetModel();
    emit resultsChanged();
}

QString UftFormatDetectionModel::formatIdToString(int format) const
{
    switch (format) {
        case UFT_FORMAT_ADF: return "ADF";
        case UFT_FORMAT_D64: return "D64";
        case UFT_FORMAT_D71: return "D71";
        case UFT_FORMAT_D81: return "D81";
        case UFT_FORMAT_G64: return "G64";
        case UFT_FORMAT_SCP: return "SCP";
        case UFT_FORMAT_HFE: return "HFE";
        case UFT_FORMAT_IPF: return "IPF";
        case UFT_FORMAT_WOZ: return "WOZ";
        case UFT_FORMAT_A2R: return "A2R";
        case UFT_FORMAT_TD0: return "TD0";
        case UFT_FORMAT_IMD: return "IMD";
        case UFT_FORMAT_DMK: return "DMK";
        case UFT_FORMAT_ST: return "ST";
        case UFT_FORMAT_STX: return "STX";
        case UFT_FORMAT_MSA: return "MSA";
        case UFT_FORMAT_ATR: return "ATR";
        case UFT_FORMAT_XFD: return "XFD";
        case UFT_FORMAT_IMG: return "IMG";
        case UFT_FORMAT_DSK: return "DSK";
        case UFT_FORMAT_NIB: return "NIB";
        case UFT_FORMAT_DO: return "DO";
        case UFT_FORMAT_PO: return "PO";
        case UFT_FORMAT_2MG: return "2MG";
        case UFT_FORMAT_SSD: return "SSD";
        case UFT_FORMAT_DSD: return "DSD";
        case UFT_FORMAT_TRD: return "TRD";
        case UFT_FORMAT_SCL: return "SCL";
        case UFT_FORMAT_FDI: return "FDI";
        case UFT_FORMAT_CQM: return "CQM";
        case UFT_FORMAT_EDSK: return "EDSK";
        case UFT_FORMAT_D88: return "D88";
        case UFT_FORMAT_DC42: return "DC42";
        default: return "???";
    }
}

QString UftFormatDetectionModel::formatIdToName(int format) const
{
    switch (format) {
        case UFT_FORMAT_ADF: return tr("Amiga Disk File");
        case UFT_FORMAT_D64: return tr("C64 Disk Image");
        case UFT_FORMAT_D71: return tr("C128 D71 Image");
        case UFT_FORMAT_D81: return tr("C128 D81 Image");
        case UFT_FORMAT_G64: return tr("C64 GCR Image");
        case UFT_FORMAT_SCP: return tr("SuperCard Pro Flux");
        case UFT_FORMAT_HFE: return tr("HxC Floppy Emulator");
        case UFT_FORMAT_IPF: return tr("SPS/CAPS IPF");
        case UFT_FORMAT_WOZ: return tr("Apple II WOZ");
        case UFT_FORMAT_A2R: return tr("Applesauce A2R");
        case UFT_FORMAT_TD0: return tr("Teledisk");
        case UFT_FORMAT_IMD: return tr("ImageDisk");
        case UFT_FORMAT_DMK: return tr("TRS-80 DMK");
        case UFT_FORMAT_ST: return tr("Atari ST");
        case UFT_FORMAT_STX: return tr("Pasti STX");
        case UFT_FORMAT_MSA: return tr("Atari MSA/DMS");
        case UFT_FORMAT_ATR: return tr("Atari 8-bit ATR");
        case UFT_FORMAT_XFD: return tr("Atari XFD");
        case UFT_FORMAT_IMG: return tr("Raw Sector Image");
        case UFT_FORMAT_DSK: return tr("Generic DSK");
        case UFT_FORMAT_NIB: return tr("Apple Nibble");
        case UFT_FORMAT_DO: return tr("Apple DOS Order");
        case UFT_FORMAT_PO: return tr("Apple ProDOS Order");
        case UFT_FORMAT_2MG: return tr("Apple 2IMG");
        case UFT_FORMAT_SSD: return tr("BBC Micro SSD");
        case UFT_FORMAT_DSD: return tr("BBC Micro DSD");
        case UFT_FORMAT_TRD: return tr("TR-DOS");
        case UFT_FORMAT_SCL: return tr("Sinclair SCL");
        case UFT_FORMAT_FDI: return tr("Formatted Disk Image");
        case UFT_FORMAT_CQM: return tr("CopyQM");
        case UFT_FORMAT_EDSK: return tr("Extended DSK");
        case UFT_FORMAT_D88: return tr("PC-98/X68000 D88");
        case UFT_FORMAT_DC42: return tr("DiskCopy 4.2");
        default: return tr("Unknown Format");
    }
}

QString UftFormatDetectionModel::heuristicsToString(uint32_t flags) const
{
    QStringList parts;
    
    if (flags & UFT_HEURISTIC_MAGIC_BYTES) parts << tr("Magic");
    if (flags & UFT_HEURISTIC_EXTENSION) parts << tr("Ext");
    if (flags & UFT_HEURISTIC_FILE_SIZE) parts << tr("Size");
    if (flags & UFT_HEURISTIC_BOOT_SECTOR) parts << tr("Boot");
    if (flags & UFT_HEURISTIC_GEOMETRY) parts << tr("Geom");
    if (flags & UFT_HEURISTIC_ENCODING) parts << tr("Enc");
    if (flags & UFT_HEURISTIC_FILESYSTEM) parts << tr("FS");
    if (flags & UFT_HEURISTIC_FLUX_TIMING) parts << tr("Flux");
    
    return parts.join(", ");
}
