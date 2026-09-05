#include "diskanalyzerwindow.h"
#include "ui_diskanalyzer_window.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QDialog>
#include <QVBoxLayout>

extern "C" {
#include <uft/uft_core.h>   /* canonical disk API: open/close/get_geometry */
#include <uft/uft_format_plugin.h>  /* struct uft_disk field access */
#include <uft/uft_types.h>
}

#include "uft_sector_editor.h"

DiskAnalyzerWindow::DiskAnalyzerWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DiskAnalyzerWindow),
    m_currentTrack(0),
    m_currentSide(0)
{
    ui->setupUi(this);
    
    // Window stays with parent and moves together
    setWindowFlags(Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMaximizeButtonHint);
    
    // Connect signals
    connect(ui->spinTrackNumber, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DiskAnalyzerWindow::onTrackChanged);
    connect(ui->sliderTrack, &QSlider::valueChanged,
            ui->spinTrackNumber, &QSpinBox::setValue);
    connect(ui->spinTrackNumber, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->sliderTrack, &QSlider::setValue);
    
    connect(ui->spinSideNumber, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DiskAnalyzerWindow::onSideChanged);
    connect(ui->sliderSide, &QSlider::valueChanged,
            ui->spinSideNumber, &QSpinBox::setValue);
    connect(ui->spinSideNumber, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->sliderSide, &QSlider::setValue);
    
    connect(ui->radioTrackView, &QRadioButton::toggled,
            this, &DiskAnalyzerWindow::onViewModeChanged);
    connect(ui->radioDiskView, &QRadioButton::toggled,
            this, &DiskAnalyzerWindow::onViewModeChanged);
    
    connect(ui->btnExport, &QPushButton::clicked,
            this, &DiskAnalyzerWindow::onExportClicked);
    connect(ui->btnEditTools, &QPushButton::clicked,
            this, &DiskAnalyzerWindow::onEditToolsClicked);
    connect(ui->btnClose, &QPushButton::clicked,
            this, &QDialog::accept);
}

DiskAnalyzerWindow::~DiskAnalyzerWindow()
{
    delete ui;
}

// ============================================================================
// Was das Abbild ueber sich sagt  (MF-662)
// ============================================================================
//
// Bis hierher stand in beiden Zweigen dieser Datei
//
//     ui->labelSide0Format->setText("ISO MFM");
//
// fest verdrahtet — fuer JEDES Format. Ein D64 ist GCR, ein Amiga-ADF ist
// Amiga MFM, ein SD-ATR ist FM. Die Oberflaeche behauptete fuer alle
// dasselbe, und das Plugin wusste es oft besser: HFE, G64 und SCP
// beantworten read_metadata("encoding"), HFE zusaetzlich "version" (die
// Variante).
//
// Dieselbe Fehlerklasse wie die HFE-Interface-Tabelle (MF-659): eine
// Aussage ueber das Medium, die niemand gemessen hat.
//
// Die Regel dahinter steht in docs/plans/VARIANTEN_UND_FAEHIGKEITEN.md und
// ist nicht verhandelbar: wer nichts weiss, schreibt "nicht ermittelt" —
// nie etwas Geratenes. Aus Dateigroesse oder Endung eine Kodierung zu
// erschliessen waere die Fabrikations-Klasse FMT-2/3/10/11/12 in der
// Oberflaeche.

namespace {

/* Der Wert zu einem Schluessel, oder ein leerer QString wenn das Plugin
 * schweigt. Die Unterscheidung "kein Plugin" / "kein read_metadata" /
 * "kein Wert" faellt bewusst zusammen — sie hat dieselbe Folge. */
QString metadatum(const uft_disk_t *disk, const char *key)
{
    char buf[128];
    if (!uft_disk_metadata(disk, key, buf, sizeof(buf))) return QString();
    return QString::fromUtf8(buf).trimmed();
}

} // namespace

QString DiskAnalyzerWindow::formatBeschreibung(const uft_disk_t *disk)
{
    const QString kodierung = metadatum(disk, "encoding");
    const QString variante   = metadatum(disk, "version");

    if (kodierung.isEmpty() && variante.isEmpty()) {
        // Kein Rateschluss. Das Plugin sagt nichts, also sagen wir das.
        return tr("nicht ermittelt");
    }
    if (variante.isEmpty())  return kodierung;
    if (kodierung.isEmpty()) return variante;
    return QStringLiteral("%1 · %2").arg(kodierung, variante);
}

void DiskAnalyzerWindow::loadImage(const QString &filename)
{
    m_currentFile = filename;

    /* Open disk image via core API (canonical handle-returning form). */
    uft_disk_t *disk = uft_disk_open(filename.toUtf8().constData(), /*read_only=*/true);
    if (!disk) {
        /* Fall back to file-size based analysis */
        QFile f(filename);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, tr("Error"), tr("Cannot open file: %1").arg(filename));
            return;
        }
        QByteArray data = f.readAll();
        f.close();

        /* Compute CRC32 */
        uint32_t crc = 0xFFFFFFFF;
        for (char c : data) {
            crc ^= (uint8_t)c;
            for (int j = 0; j < 8; j++)
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
        crc = ~crc;

        /* Estimate geometry from file size */
        qint64 size = data.size();
        int sectorSize = 512;
        int totalSectors = static_cast<int>(size / sectorSize);
        int sides = 2;
        int sectorsPerTrack = 18;
        int totalTracks = totalSectors / (sides * sectorsPerTrack);
        if (totalTracks == 0) { totalTracks = 1; sides = 1; sectorsPerTrack = totalSectors; }

        int side0Sectors = totalTracks * sectorsPerTrack;
        int side1Sectors = (sides > 1) ? totalTracks * sectorsPerTrack : 0;

        // MF-662: In diesem Zweig konnte KEIN Plugin die Datei oeffnen.
        // Die Geometrie darueber ist aus der Dateigroesse GESCHAETZT
        // (512 B/Sektor, 2 Seiten, 18 Sektoren — feste Annahmen), und ueber
        // die Kodierung ist gar nichts bekannt. Frueher stand hier trotzdem
        // "ISO MFM". Eine Schaetzung als Messung auszugeben ist genau die
        // Fehlerklasse, gegen die dieses Werkzeug gebaut ist.
        ui->labelSide0Info->setText(tr("%1 Tracks, %2 Sectors, %3 Bytes "
                                       "(geschätzt aus der Dateigröße)")
            .arg(totalTracks).arg(side0Sectors).arg(side0Sectors * sectorSize));
        ui->labelSide0Format->setText(tr("nicht ermittelt"));

        if (sides > 1) {
            ui->labelSide1Info->setText(tr("%1 Tracks, %2 Sectors, %3 Bytes "
                                           "(geschätzt aus der Dateigröße)")
                .arg(totalTracks).arg(side1Sectors).arg(side1Sectors * sectorSize));
            ui->labelSide1Format->setText(tr("nicht ermittelt"));
        } else {
            ui->labelSide1Info->setText("N/A");
            ui->labelSide1Format->setText("-");
        }

        ui->labelCRC->setText(QString("CRC32: 0x%1").arg(crc, 8, 16, QChar('0')).toUpper());

        /* Configure slider/spin ranges */
        ui->spinTrackNumber->setMaximum(totalTracks > 0 ? totalTracks - 1 : 0);
        ui->sliderTrack->setMaximum(totalTracks > 0 ? totalTracks - 1 : 0);
        ui->spinSideNumber->setMaximum(sides - 1);
        ui->sliderSide->setMaximum(sides - 1);

        updateDiskView();
        return;
    }

    /* Successfully opened via core API */
    uft_geometry_t geom;
    uft_disk_get_geometry(disk, &geom);

    /* Compute CRC32 of entire image */
    uint32_t crc = 0xFFFFFFFF;
    if (disk->image_data && disk->image_size > 0) {
        for (size_t i = 0; i < disk->image_size; i++) {
            crc ^= disk->image_data[i];
            for (int j = 0; j < 8; j++)
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    crc = ~crc;

    int side0Sectors = geom.cylinders * geom.sectors;
    int side1Sectors = (geom.heads > 1) ? geom.cylinders * geom.sectors : 0;

    // MF-662: was das Plugin sagt, nicht was wir annehmen.
    const QString beschreibung = formatBeschreibung(disk);

    ui->labelSide0Info->setText(QString("%1 Tracks, %2 Sectors, %3 Bytes")
        .arg(geom.cylinders).arg(side0Sectors).arg(side0Sectors * geom.sector_size));
    ui->labelSide0Format->setText(beschreibung);

    if (geom.heads > 1) {
        ui->labelSide1Info->setText(QString("%1 Tracks, %2 Sectors, %3 Bytes")
            .arg(geom.cylinders).arg(side1Sectors).arg(side1Sectors * geom.sector_size));
        ui->labelSide1Format->setText(beschreibung);
    } else {
        ui->labelSide1Info->setText("N/A");
        ui->labelSide1Format->setText("-");
    }

    ui->labelCRC->setText(QString("CRC32: 0x%1").arg(crc, 8, 16, QChar('0')).toUpper());

    ui->spinTrackNumber->setMaximum(geom.cylinders > 0 ? geom.cylinders - 1 : 0);
    ui->sliderTrack->setMaximum(geom.cylinders > 0 ? geom.cylinders - 1 : 0);
    ui->spinSideNumber->setMaximum(geom.heads > 0 ? geom.heads - 1 : 0);
    ui->sliderSide->setMaximum(geom.heads > 0 ? geom.heads - 1 : 0);

    uft_disk_close(disk);   /* also frees the handle — see uft_core_stubs.c */

    updateDiskView();
}

void DiskAnalyzerWindow::onTrackChanged(int track)
{
    m_currentTrack = track;
    ui->labelTrackInfo->setText(QString("Track: %1 Side: %2").arg(track).arg(m_currentSide));
    updateSectorInfo(track, m_currentSide, 0);
}

void DiskAnalyzerWindow::onSideChanged(int side)
{
    m_currentSide = side;
    ui->labelTrackInfo->setText(QString("Track: %1 Side: %2").arg(m_currentTrack).arg(side));
    updateSectorInfo(m_currentTrack, side, 0);
}

void DiskAnalyzerWindow::onViewModeChanged()
{
    updateDiskView();
}

void DiskAnalyzerWindow::onExportClicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Export Analysis"),
        QString(),
        tr("HTML Report (*.html);;Text Report (*.txt);;PNG Image (*.png)"));
    
    if (!filename.isEmpty()) {
        if (filename.endsWith(".png", Qt::CaseInsensitive)) {
            /* Export disk view as PNG */
            QPixmap pixmap(ui->frameDisk0->size());
            pixmap.fill(Qt::black);
            ui->frameDisk0->render(&pixmap);
            pixmap.save(filename, "PNG");
        } else {
            /* Export as HTML or text report */
            QFile file(filename);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                if (filename.endsWith(".html", Qt::CaseInsensitive)) {
                    out << "<!DOCTYPE html>\n<html><head>\n";
                    out << "<title>Disk Analysis Report</title>\n";
                    out << "<style>body{font-family:monospace;background:#1a1a1a;color:#eee;}"
                           "table{border-collapse:collapse;} td,th{border:1px solid #444;padding:4px;}"
                           "th{background:#333;} .good{color:#0f0;} .bad{color:#f00;}</style>\n";
                    out << "</head><body>\n";
                    out << "<h1>Disk Analysis Report</h1>\n";
                    out << "<h2>File: " << QFileInfo(m_currentFile).fileName() << "</h2>\n";
                    out << "<p>" << ui->labelCRC->text() << "</p>\n";
                    out << "<h3>Side 0</h3><p>" << ui->labelSide0Info->text() << "</p>\n";
                    out << "<p>Format: " << ui->labelSide0Format->text().replace("\n", ", ") << "</p>\n";
                    out << "<h3>Side 1</h3><p>" << ui->labelSide1Info->text() << "</p>\n";
                    out << "<p>Format: " << ui->labelSide1Format->text().replace("\n", ", ") << "</p>\n";
                    out << "<h3>Current Sector</h3>\n<pre>" << ui->textSectorInfo->toPlainText() << "</pre>\n";
                    out << "<h3>Hex Dump</h3>\n<pre>" << ui->textHexDump->toPlainText() << "</pre>\n";
                    out << "</body></html>\n";
                } else {
                    /* Plain text */
                    out << "Disk Analysis Report\n";
                    out << "====================\n\n";
                    out << "File: " << QFileInfo(m_currentFile).fileName() << "\n";
                    out << ui->labelCRC->text() << "\n\n";
                    out << "Side 0: " << ui->labelSide0Info->text() << "\n";
                    out << "Format: " << ui->labelSide0Format->text().replace("\n", ", ") << "\n\n";
                    out << "Side 1: " << ui->labelSide1Info->text() << "\n";
                    out << "Format: " << ui->labelSide1Format->text().replace("\n", ", ") << "\n\n";
                    out << "--- Sector Info ---\n" << ui->textSectorInfo->toPlainText() << "\n\n";
                    out << "--- Hex Dump ---\n" << ui->textHexDump->toPlainText() << "\n";
                }
                file.close();
            }
        }
        QMessageBox::information(this, tr("Export"),
            tr("Export to %1 completed.").arg(filename));
    }
}

void DiskAnalyzerWindow::onEditToolsClicked()
{
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Sector Editor - %1").arg(QFileInfo(m_currentFile).fileName()));
    dlg->resize(800, 600);

    QVBoxLayout *layout = new QVBoxLayout(dlg);
    UftSectorEditor *editor = new UftSectorEditor(dlg);
    layout->addWidget(editor);

    if (!m_currentFile.isEmpty()) {
        editor->loadDisk(m_currentFile);
        editor->goToSector(m_currentTrack, 0);
    }

    dlg->exec();
    dlg->deleteLater();
}

void DiskAnalyzerWindow::updateDiskView()
{
    /* Render disk visualization on frameDisk0 (and frameDisk1 if applicable) */
    auto renderDisk = [this](QFrame *frame, int side) {
        if (!frame) return;

        QPixmap pixmap(frame->size());
        pixmap.fill(Qt::black);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        int w = pixmap.width();
        int h = pixmap.height();
        int size = qMin(w, h) - 10;
        int cx = w / 2;
        int cy = h / 2;

        /* Estimate track count from label text */
        QString info = (side == 0) ? ui->labelSide0Info->text() : ui->labelSide1Info->text();
        int numTracks = 80;
        if (info.contains("Tracks")) {
            QRegularExpression re("(\\d+)\\s+Tracks");
            QRegularExpressionMatch match = re.match(info);
            if (match.hasMatch()) numTracks = match.captured(1).toInt();
        }
        if (numTracks <= 0) numTracks = 1;

        double outerRadius = size / 2.0;
        double innerRadius = size / 6.0;
        double trackWidth = (outerRadius - innerRadius) / numTracks;

        bool isDiskMode = ui->radioDiskView->isChecked();

        if (isDiskMode) {
            /* Concentric rings for each track */
            for (int t = 0; t < numTracks; t++) {
                double r1 = outerRadius - t * trackWidth;
                double r2 = r1 - trackWidth + 0.5;

                /* Color based on track position: gradient green->yellow->red for variation */
                int hue = 120 - (t * 120 / numTracks);  /* green to red */
                if (t == m_currentTrack && side == m_currentSide)
                    hue = 60; /* yellow for selected track */
                QColor color = QColor::fromHsv(qBound(0, hue, 359), 200, 180);

                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                painter.drawEllipse(QPointF(cx, cy), r1, r1);
            }
            /* Punch center hole */
            painter.setBrush(Qt::black);
            painter.setPen(QPen(QColor(80, 0, 0), 2));
            painter.drawEllipse(QPointF(cx, cy), innerRadius, innerRadius);

            /* Highlight current track */
            if (m_currentTrack >= 0 && m_currentTrack < numTracks && side == m_currentSide) {
                double r = outerRadius - m_currentTrack * trackWidth - trackWidth / 2.0;
                painter.setPen(QPen(Qt::white, 2));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(QPointF(cx, cy), r, r);
            }
        } else {
            /* Track grid view */
            int cols = 10;
            int rows = (numTracks + cols - 1) / cols;
            double cellW = (double)w / cols;
            double cellH = (double)h / rows;

            for (int t = 0; t < numTracks; t++) {
                int col = t % cols;
                int row = t / cols;
                QRectF rect(col * cellW + 1, row * cellH + 1, cellW - 2, cellH - 2);

                QColor color = QColor(0, 200, 0);
                if (t == m_currentTrack && side == m_currentSide)
                    color = QColor(255, 255, 0);

                painter.fillRect(rect, color);
                painter.setPen(QPen(Qt::black, 0.5));
                painter.drawRect(rect);

                painter.setPen(Qt::white);
                painter.setFont(QFont("Arial", qMax(6, (int)(cellH / 3))));
                painter.drawText(rect, Qt::AlignCenter, QString::number(t));
            }
        }

        /* Side label */
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 10, QFont::Bold));
        painter.drawText(5, 15, QString("Side %1").arg(side));

        painter.end();

        /* Apply pixmap as frame background */
        QPalette pal = frame->palette();
        pal.setBrush(QPalette::Window, pixmap);
        frame->setPalette(pal);
        frame->setAutoFillBackground(true);
    };

    renderDisk(ui->frameDisk0, 0);
    renderDisk(ui->frameDisk1, 1);
}

/**
 * Was WIRKLICH auf der Spur liegt — und was dieses Format nicht sagen kann.
 *
 * ── Was hier stand (MF-890) ──────────────────────────────────────────────
 *
 * Der ganze Bericht war eine feste Zeichenkette. Eingesetzt wurden GENAU
 * drei Werte: Sektor, Spur, Seite. Alles andere stand fuer jede Spur jeder
 * Diskette gleich da:
 *
 *     Data checksum: 0x5600 (OK)
 *     Head CRC: 0x3FFF (BAD CRC!)
 *     Data CRC: 0xFFFF (BAD CRC!)
 *     Start sector cell: 95821   ... Number of cells: 4896
 *
 * Zwei erfundene CRC-FEHLER und vier erfundene Zellpositionen. Und der
 * Text ging woertlich in den Bericht: `onExportClicked()` schreibt
 * `ui->textSectorInfo->toPlainText()` unter „Current Sector" in die HTML-
 * UND die Textfassung. Das war kein Anzeigefehler, sondern ein falscher
 * Befund in einem ausgelieferten Dokument.
 *
 * ── Warum hier keine CRC-Zeile fuer jedes Format steht ───────────────────
 *
 * Gemessen am Korpus-Abbild ueber den echten Plugin-Pfad:
 *
 *     D64, Zylinder 0: 21 Sektoren, C=0 H=0 N=1, je 256 Byte
 *     crc_ok = 1,  crc_stored = 0x0,  crc_calculated = 0x0
 *
 * Das Plugin setzt `crc_ok` auf wahr, OHNE dass eine Pruefsumme existiert
 * — ein D64 ist ein Sektorabbild und traegt keine. „Data CRC: OK" waere
 * dort dieselbe Erfindung wie „BAD CRC!", nur in die andere Richtung.
 * Genannt werden CRC-Werte darum nur, wenn das Plugin welche geliefert
 * hat; sonst steht da, dass dieses Format keine fuehrt.
 *
 * Die Zellpositionen (`Start sector cell`, `Number of cells`) sind
 * Fluss-Groessen. Ein Sektorabbild kennt sie nicht, und der Weg hierher
 * liefert sie nicht — sie sind ersatzlos weg statt gefuellt.
 */
void DiskAnalyzerWindow::updateSectorInfo(int track, int side, int sector)
{
    Q_UNUSED(sector);   /* der Bericht zeigt die ganze Spur, nicht einen Sektor */

    if (m_currentFile.isEmpty()) {
        ui->textSectorInfo->setPlainText(tr("Kein Abbild geladen."));
        return;
    }

    uft_disk_t *disk = uft_disk_open(m_currentFile.toUtf8().constData(), true);
    if (!disk) {
        ui->textSectorInfo->setPlainText(
            tr("Spur %1, Seite %2\n\n"
               "Nicht lesbar: kein Plugin konnte dieses Abbild oeffnen.")
                .arg(track).arg(side));
        return;
    }

    const uft_format_plugin_t *pl = uft_disk_plugin(disk);
    if (!pl || !pl->read_track) {
        ui->textSectorInfo->setPlainText(
            tr("Spur %1, Seite %2\n\n"
               "Das Plugin \"%3\" liest keine ganzen Spuren; ueber die "
               "Sektoren dieser Spur ist hier nichts bekannt.")
                .arg(track).arg(side)
                .arg(pl && pl->name ? QString::fromUtf8(pl->name)
                                    : tr("(unbenannt)")));
        uft_disk_close(disk);
        return;
    }

    uft_track_t spur;
    memset(&spur, 0, sizeof(spur));
    const uft_error_t rc = pl->read_track(disk, track, side, &spur);

    QStringList z;
    z << tr("Spur %1, Seite %2   (Plugin: %3)")
             .arg(track).arg(side)
             .arg(pl->name ? QString::fromUtf8(pl->name) : tr("(unbenannt)"));
    z << QString();

    if (rc != UFT_OK) {
        z << tr("Nicht lesbar (Fehler %1).").arg(static_cast<int>(rc));
    } else if (spur.sector_count == 0) {
        z << tr("Keine Sektoren gefunden.");
        z << tr("Das ist eine Beobachtung, keine Diagnose: eine leere Spur "
                "und eine nicht dekodierbare sehen hier gleich aus.");
    } else {
        z << tr("%1 Sektoren gefunden.").arg(spur.sector_count);
        z << QString();

        bool crc_gemeldet = false;
        for (size_t i = 0; i < spur.sector_count; i++) {
            const uft_sector_t *s = &spur.sectors[i];
            if (s->crc_stored != 0 || s->crc_calculated != 0)
                crc_gemeldet = true;

            QStringList merkmale;
            if (s->deleted) merkmale << tr("geloescht-Marke");
            if (s->weak)    merkmale << tr("schwache Bits");

            z << tr("  Sektor %1   C=%2 H=%3 N=%4   %5 Byte%6")
                     .arg(s->id.sector, 3)
                     .arg(s->id.cylinder).arg(s->id.head).arg(s->id.size_code)
                     .arg(s->data_len, 5)
                     .arg(merkmale.isEmpty() ? QString()
                                             : "   " + merkmale.join(", "));
        }

        z << QString();
        if (crc_gemeldet) {
            z << tr("Pruefsummen:");
            for (size_t i = 0; i < spur.sector_count; i++) {
                const uft_sector_t *s = &spur.sectors[i];
                if (s->crc_stored == 0 && s->crc_calculated == 0) continue;
                z << tr("  Sektor %1   gespeichert 0x%2   gerechnet 0x%3   %4")
                         .arg(s->id.sector, 3)
                         .arg(s->crc_stored, 4, 16, QChar('0'))
                         .arg(s->crc_calculated, 4, 16, QChar('0'))
                         .arg(s->crc_ok ? tr("stimmt") : tr("weicht ab"));
            }
        } else {
            z << tr("Keine Pruefsumme: dieses Format speichert keine, und "
                    "das Plugin hat keine gemeldet. Ein Urteil \"gut\" oder "
                    "\"fehlerhaft\" waere hier erfunden.");
        }
    }

    for (size_t i = 0; i < spur.sector_count; i++) free(spur.sectors[i].data);
    free(spur.sectors);
    uft_disk_close(disk);

    ui->textSectorInfo->setPlainText(z.join("\n"));
}

void DiskAnalyzerWindow::updateHexDump(const QByteArray &data)
{
    QString hexDump;
    for (int i = 0; i < data.size() && i < 256; i += 16) {
        QString line = QString("%1  ").arg(i, 5, 16, QChar('0')).toUpper();
        QString ascii;
        
        for (int j = 0; j < 16 && (i + j) < data.size(); ++j) {
            unsigned char c = static_cast<unsigned char>(data[i + j]);
            line += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();
            ascii += (c >= 32 && c < 127) ? QChar(c) : '.';
        }
        
        hexDump += line + " " + ascii + "\n";
    }
    
    ui->textHexDump->setPlainText(hexDump);
}
