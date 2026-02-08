/**
 * @file FloppyOtdrWidget.h
 * @brief Qt6 Widget for OTDR-Style Floppy Disk Signal Visualization
 *
 * Provides three visualization modes:
 *   1. Trace View  — OTDR-style quality curve for a single track
 *   2. Heatmap     — 2D quality overview of entire disk
 *   3. Histogram   — Timing distribution with peak markers
 *
 * Supports interactive zooming, event markers, sector highlighting,
 * and multi-revolution overlay.
 */

#ifndef FLOPPY_OTDR_WIDGET_H
#define FLOPPY_OTDR_WIDGET_H

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QScrollBar>
#include <QPushButton>
#include <QFileDialog>
#include <QApplication>

#include <cmath>
#include <algorithm>
#include <vector>

extern "C" {
#include "uft/analysis/floppy_otdr.h"
}

/* ═══════════════════════════════════════════════════════════════════════
 * Color Scheme (OTDR-inspired)
 * ═══════════════════════════════════════════════════════════════════════ */

namespace OtdrColors {
    // Background
    constexpr QColor BG_DARK     {20, 22, 28};
    constexpr QColor BG_GRID     {40, 44, 52};
    constexpr QColor BG_PANEL    {30, 33, 40};

    // Quality levels (signal trace)
    constexpr QColor EXCELLENT   {0, 210, 80};
    constexpr QColor GOOD        {100, 230, 0};
    constexpr QColor FAIR        {230, 220, 0};
    constexpr QColor POOR        {255, 140, 0};
    constexpr QColor CRITICAL    {255, 40, 0};
    constexpr QColor UNREADABLE  {180, 0, 0};

    // Event markers
    constexpr QColor EVT_STRUCT  {80, 160, 255};    // Sector headers etc.
    constexpr QColor EVT_WARN    {255, 200, 40};    // Warnings
    constexpr QColor EVT_ERROR   {255, 60, 60};     // Errors
    constexpr QColor EVT_PROT    {200, 80, 255};    // Copy protection

    // Heatmap gradient
    constexpr QColor HEAT_COLD   {0, 0, 120};       // Excellent
    constexpr QColor HEAT_COOL   {0, 150, 200};
    constexpr QColor HEAT_MID    {0, 220, 80};
    constexpr QColor HEAT_WARM   {255, 200, 0};
    constexpr QColor HEAT_HOT    {255, 0, 0};       // Unreadable

    // UI
    constexpr QColor TEXT        {200, 210, 220};
    constexpr QColor TEXT_DIM    {120, 130, 140};
    constexpr QColor CURSOR      {255, 255, 255, 100};
    constexpr QColor SELECTION   {80, 120, 200, 60};

    inline QColor qualityColor(otdr_quality_t q) {
        switch (q) {
            case OTDR_QUAL_EXCELLENT:  return EXCELLENT;
            case OTDR_QUAL_GOOD:       return GOOD;
            case OTDR_QUAL_FAIR:       return FAIR;
            case OTDR_QUAL_POOR:       return POOR;
            case OTDR_QUAL_CRITICAL:   return CRITICAL;
            case OTDR_QUAL_UNREADABLE: return UNREADABLE;
        }
        return TEXT_DIM;
    }

    inline QColor eventColor(otdr_severity_t sev) {
        switch (sev) {
            case OTDR_SEV_INFO:     return EVT_STRUCT;
            case OTDR_SEV_MINOR:    return GOOD;
            case OTDR_SEV_WARNING:  return EVT_WARN;
            case OTDR_SEV_ERROR:    return EVT_ERROR;
            case OTDR_SEV_CRITICAL: return CRITICAL;
        }
        return TEXT_DIM;
    }

    /** Interpolate heatmap color from dB value (-40..0) */
    inline QColor heatmapColor(float db) {
        float t = (db + 40.0f) / 40.0f;
        t = std::clamp(t, 0.0f, 1.0f);

        // 5-stop gradient: cold → cool → mid → warm → hot
        QColor stops[] = {HEAT_HOT, HEAT_WARM, HEAT_MID, HEAT_COOL, HEAT_COLD};
        float pos = t * 4.0f;
        int idx = std::min((int)pos, 3);
        float frac = pos - idx;

        int r = stops[idx].red()   + (int)(frac * (stops[idx+1].red()   - stops[idx].red()));
        int g = stops[idx].green() + (int)(frac * (stops[idx+1].green() - stops[idx].green()));
        int b = stops[idx].blue()  + (int)(frac * (stops[idx+1].blue()  - stops[idx].blue()));
        return QColor(std::clamp(r,0,255), std::clamp(g,0,255), std::clamp(b,0,255));
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Trace View Widget — The main "OTDR display"
 * ═══════════════════════════════════════════════════════════════════════ */

class OtdrTraceView : public QWidget {
    Q_OBJECT

public:
    explicit OtdrTraceView(QWidget *parent = nullptr)
        : QWidget(parent) {
        setMinimumSize(600, 300);
        setMouseTracking(true);
        setFocusPolicy(Qt::WheelFocus);
    }

    void setTrack(const otdr_track_t *track) {
        m_track = track;
        m_viewStart = 0;
        m_viewEnd = track ? track->bitcell_count : 0;
        update();
    }

    void setShowSmoothed(bool on) { m_showSmoothed = on; update(); }
    void setShowEvents(bool on)   { m_showEvents = on; update(); }
    void setShowSectors(bool on)  { m_showSectors = on; update(); }
    void setShowRaw(bool on)      { m_showRaw = on; update(); }

signals:
    void cursorPositionChanged(uint32_t bitcell, float quality_db);
    void eventClicked(const otdr_event_t *event);
    void zoomChanged(uint32_t start, uint32_t end);

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        drawBackground(p);

        if (!m_track || m_track->bitcell_count == 0) {
            drawNoData(p);
            return;
        }

        drawGrid(p);
        drawTrace(p);

        if (m_showEvents)
            drawEvents(p);
        if (m_showSectors)
            drawSectors(p);

        drawCursor(p);
        drawScale(p);
        drawInfo(p);
    }

    void mouseMoveEvent(QMouseEvent *e) override {
        m_mouseX = e->pos().x();
        m_mouseY = e->pos().y();

        if (m_track && m_track->bitcell_count > 0) {
            uint32_t bc = xToBitcell(m_mouseX);
            float db = 0;
            if (m_track->quality_profile && bc < m_track->bitcell_count)
                db = m_showSmoothed && m_track->quality_smoothed
                    ? m_track->quality_smoothed[bc]
                    : m_track->quality_profile[bc];
            emit cursorPositionChanged(bc, db);

            // Tooltip for nearby events
            for (uint32_t i = 0; i < m_track->event_count; i++) {
                uint32_t epos = m_track->events[i].position;
                if (std::abs((int)epos - (int)bc) < (int)(viewRange() / width() * 10)) {
                    QToolTip::showText(e->globalPosition().toPoint(),
                        QString("[%1] %2\n%3")
                            .arg(otdr_severity_name(m_track->events[i].severity))
                            .arg(otdr_event_type_name(m_track->events[i].type))
                            .arg(m_track->events[i].desc));
                    break;
                }
            }
        }

        // Drag to pan
        if (m_dragging) {
            int dx = m_mouseX - m_dragStartX;
            double shift = -(double)dx / width() * viewRange();
            uint32_t total = m_track->bitcell_count;
            double newStart = m_dragViewStart + shift;
            double newEnd = m_dragViewEnd + shift;
            if (newStart >= 0 && newEnd <= total) {
                m_viewStart = (uint32_t)newStart;
                m_viewEnd = (uint32_t)newEnd;
                emit zoomChanged(m_viewStart, m_viewEnd);
            }
        }

        update();
    }

    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton) {
            m_dragging = true;
            m_dragStartX = e->pos().x();
            m_dragViewStart = m_viewStart;
            m_dragViewEnd = m_viewEnd;
        }
    }

    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton)
            m_dragging = false;
    }

    void wheelEvent(QWheelEvent *e) override {
        if (!m_track) return;

        double factor = (e->angleDelta().y() > 0) ? 0.8 : 1.25;
        double range = viewRange();
        double center = m_viewStart + range *
            ((double)e->position().x() / width());

        double newRange = range * factor;
        if (newRange < 100) newRange = 100;
        if (newRange > m_track->bitcell_count) newRange = m_track->bitcell_count;

        m_viewStart = (uint32_t)std::max(0.0, center - newRange / 2);
        m_viewEnd = m_viewStart + (uint32_t)newRange;
        if (m_viewEnd > m_track->bitcell_count)
            m_viewEnd = m_track->bitcell_count;

        emit zoomChanged(m_viewStart, m_viewEnd);
        update();
    }

private:
    // ── Drawing ────────────────────────────────────────────

    void drawBackground(QPainter &p) {
        p.fillRect(rect(), OtdrColors::BG_DARK);
    }

    void drawNoData(QPainter &p) {
        p.setPen(OtdrColors::TEXT_DIM);
        p.setFont(QFont("Monospace", 14));
        p.drawText(rect(), Qt::AlignCenter, "No track data loaded");
    }

    void drawGrid(QPainter &p) {
        QRect area = traceArea();
        p.setPen(QPen(OtdrColors::BG_GRID, 1));

        // Horizontal lines (dB levels)
        for (int db = 0; db >= -40; db -= 5) {
            int y = dbToY(db, area);
            p.drawLine(area.left(), y, area.right(), y);
        }

        // Vertical lines (position)
        double range = viewRange();
        double step = pow(10, floor(log10(range / 5)));
        if (range / step > 20) step *= 5;
        else if (range / step > 10) step *= 2;

        for (double pos = ceil(m_viewStart / step) * step;
             pos < m_viewEnd; pos += step) {
            int x = bitcellToX((uint32_t)pos, area);
            p.drawLine(x, area.top(), x, area.bottom());
        }
    }

    void drawTrace(QPainter &p) {
        QRect area = traceArea();
        const float *profile = m_showSmoothed && m_track->quality_smoothed
            ? m_track->quality_smoothed : m_track->quality_profile;
        if (!profile) return;

        // Draw trace as colored polyline
        uint32_t step = std::max(1u, viewRange() / (uint32_t)area.width());

        QPainterPath path;
        bool first = true;
        QColor prevColor;

        for (int px = 0; px < area.width(); px++) {
            uint32_t bc = xToBitcell(area.left() + px);
            if (bc >= m_track->bitcell_count) break;

            // Average over step width
            float sum = 0, n = 0;
            for (uint32_t s = 0; s < step && (bc + s) < m_track->bitcell_count; s++) {
                sum += profile[bc + s];
                n++;
            }
            float db = (n > 0) ? sum / n : -40.0f;

            int y = dbToY(db, area);
            QColor c = OtdrColors::heatmapColor(db);

            if (first) {
                path.moveTo(area.left() + px, y);
                first = false;
            } else {
                path.lineTo(area.left() + px, y);
            }
        }

        // Gradient fill under curve
        QLinearGradient grad(0, area.top(), 0, area.bottom());
        grad.setColorAt(0.0, QColor(0, 220, 80, 80));
        grad.setColorAt(0.5, QColor(255, 200, 0, 60));
        grad.setColorAt(1.0, QColor(255, 0, 0, 40));

        QPainterPath fillPath = path;
        fillPath.lineTo(area.right(), area.bottom());
        fillPath.lineTo(area.left(), area.bottom());
        fillPath.closeSubpath();
        p.fillPath(fillPath, grad);

        // Trace line
        p.setPen(QPen(OtdrColors::EXCELLENT, 1.5));
        p.drawPath(path);

        // If showing raw (non-smoothed) as overlay
        if (m_showRaw && m_showSmoothed && m_track->quality_profile) {
            QPainterPath rawPath;
            bool rawFirst = true;
            for (int px = 0; px < area.width(); px += 2) {
                uint32_t bc = xToBitcell(area.left() + px);
                if (bc >= m_track->bitcell_count) break;
                float db = m_track->quality_profile[bc];
                int y = dbToY(db, area);
                if (rawFirst) { rawPath.moveTo(area.left() + px, y); rawFirst = false; }
                else rawPath.lineTo(area.left() + px, y);
            }
            p.setPen(QPen(QColor(255, 255, 255, 40), 0.5));
            p.drawPath(rawPath);
        }
    }

    void drawEvents(QPainter &p) {
        if (!m_track) return;
        QRect area = traceArea();

        for (uint32_t i = 0; i < m_track->event_count; i++) {
            const otdr_event_t *evt = &m_track->events[i];
            if (evt->position < m_viewStart || evt->position > m_viewEnd)
                continue;

            int x = bitcellToX(evt->position, area);
            QColor c = OtdrColors::eventColor(evt->severity);

            // Vertical marker line
            p.setPen(QPen(c, 1, Qt::DashLine));
            p.drawLine(x, area.top(), x, area.bottom());

            // Event triangle at top
            QPolygon tri;
            tri << QPoint(x - 5, area.top())
                << QPoint(x + 5, area.top())
                << QPoint(x, area.top() + 8);
            p.setBrush(c);
            p.setPen(Qt::NoPen);
            p.drawPolygon(tri);

            // Affected region (if has length)
            if (evt->length > 1) {
                int x2 = bitcellToX(evt->position + evt->length, area);
                p.fillRect(x, area.top(), x2 - x, area.height(),
                           QColor(c.red(), c.green(), c.blue(), 25));
            }
        }
    }

    void drawSectors(QPainter &p) {
        if (!m_track) return;
        QRect area = traceArea();

        p.setFont(QFont("Monospace", 8));

        for (uint8_t s = 0; s < m_track->sector_count; s++) {
            uint32_t pos = m_track->sectors[s].header_pos;
            if (pos < m_viewStart || pos > m_viewEnd) continue;

            int x = bitcellToX(pos, area);
            QColor c = m_track->sectors[s].data_ok
                ? OtdrColors::EVT_STRUCT : OtdrColors::EVT_ERROR;

            p.setPen(QPen(c, 1, Qt::DotLine));
            p.drawLine(x, area.top(), x, area.bottom());

            // Sector ID label
            p.setPen(c);
            p.drawText(x + 3, area.bottom() - 4,
                       QString("S%1").arg(m_track->sectors[s].id));
        }
    }

    void drawCursor(QPainter &p) {
        if (m_mouseX < 0) return;
        QRect area = traceArea();
        if (!area.contains(m_mouseX, m_mouseY)) return;

        // Crosshair
        p.setPen(QPen(OtdrColors::CURSOR, 1, Qt::DashLine));
        p.drawLine(m_mouseX, area.top(), m_mouseX, area.bottom());
        p.drawLine(area.left(), m_mouseY, area.right(), m_mouseY);

        // Position readout at cursor
        uint32_t bc = xToBitcell(m_mouseX);
        float db = yToDb(m_mouseY, area);

        p.setPen(OtdrColors::TEXT);
        p.setFont(QFont("Monospace", 9));
        QString info = QString("BC:%1  %2 dB")
            .arg(bc).arg(db, 0, 'f', 1);
        p.drawText(m_mouseX + 10, m_mouseY - 5, info);
    }

    void drawScale(QPainter &p) {
        QRect area = traceArea();
        p.setFont(QFont("Monospace", 9));
        p.setPen(OtdrColors::TEXT_DIM);

        // Y-axis (dB)
        for (int db = 0; db >= -40; db -= 10) {
            int y = dbToY(db, area);
            p.drawText(2, y + 4, QString("%1 dB").arg(db));
        }

        // X-axis (bitcell position)
        double range = viewRange();
        double step = pow(10, floor(log10(range / 5)));
        if (range / step > 20) step *= 5;
        else if (range / step > 10) step *= 2;

        for (double pos = ceil(m_viewStart / step) * step;
             pos < m_viewEnd; pos += step) {
            int x = bitcellToX((uint32_t)pos, area);
            QString label;
            if (step >= 1000)
                label = QString("%1k").arg(pos / 1000, 0, 'f', 0);
            else
                label = QString::number((int)pos);
            p.drawText(x - 15, area.bottom() + 14, label);
        }
    }

    void drawInfo(QPainter &p) {
        if (!m_track) return;

        // Top-right info panel
        int x = width() - 240;
        int y = 10;

        p.fillRect(x - 5, y - 2, 235, 95, QColor(0, 0, 0, 160));
        p.setPen(OtdrColors::TEXT);
        p.setFont(QFont("Monospace", 9));

        QColor qc = OtdrColors::qualityColor(m_track->stats.overall);

        p.drawText(x, y += 12, QString("Track %1 (Cyl %2, Head %3)")
            .arg(m_track->track_num).arg(m_track->cylinder).arg(m_track->head));
        p.setPen(qc);
        p.drawText(x, y += 14, QString("Quality: %1 (%2)")
            .arg(otdr_quality_name(m_track->stats.overall))
            .arg(m_track->stats.quality_mean_db, 0, 'f', 1));
        p.setPen(OtdrColors::TEXT);
        p.drawText(x, y += 14, QString("Jitter:  RMS %1%  Peak %2%")
            .arg(m_track->stats.jitter_rms, 0, 'f', 1)
            .arg(m_track->stats.jitter_peak, 0, 'f', 1));
        p.drawText(x, y += 14, QString("SNR:     %1 dB")
            .arg(m_track->stats.snr_estimate, 0, 'f', 1));
        p.drawText(x, y += 14, QString("Events:  %1  |  Zoom: %2x")
            .arg(m_track->event_count)
            .arg((float)m_track->bitcell_count / viewRange(), 0, 'f', 1));
    }

    // ── Coordinate mapping ─────────────────────────────────

    QRect traceArea() const {
        return QRect(55, 10, width() - 65, height() - 30);
    }

    uint32_t viewRange() const {
        return (m_viewEnd > m_viewStart) ? (m_viewEnd - m_viewStart) : 1;
    }

    int bitcellToX(uint32_t bc, const QRect &area) const {
        double frac = (double)(bc - m_viewStart) / viewRange();
        return area.left() + (int)(frac * area.width());
    }

    uint32_t xToBitcell(int x) const {
        QRect area = traceArea();
        double frac = (double)(x - area.left()) / area.width();
        return m_viewStart + (uint32_t)(frac * viewRange());
    }

    int dbToY(float db, const QRect &area) const {
        // 0 dB at top, -40 dB at bottom
        float frac = -db / 40.0f;
        frac = std::clamp(frac, 0.0f, 1.0f);
        return area.top() + (int)(frac * area.height());
    }

    float yToDb(int y, const QRect &area) const {
        float frac = (float)(y - area.top()) / area.height();
        return -frac * 40.0f;
    }

    // ── Member data ────────────────────────────────────────

    const otdr_track_t *m_track = nullptr;
    uint32_t m_viewStart = 0;
    uint32_t m_viewEnd = 0;

    int m_mouseX = -1, m_mouseY = -1;
    bool m_dragging = false;
    int m_dragStartX = 0;
    double m_dragViewStart = 0, m_dragViewEnd = 0;

    bool m_showSmoothed = true;
    bool m_showEvents = true;
    bool m_showSectors = true;
    bool m_showRaw = false;
};

/* ═══════════════════════════════════════════════════════════════════════
 * Heatmap Widget — Disk-wide quality overview
 * ═══════════════════════════════════════════════════════════════════════ */

class OtdrHeatmapView : public QWidget {
    Q_OBJECT

public:
    explicit OtdrHeatmapView(QWidget *parent = nullptr)
        : QWidget(parent) {
        setMinimumSize(400, 200);
        setMouseTracking(true);
    }

    void setDisk(const otdr_disk_t *disk) {
        m_disk = disk;
        rebuildImage();
        update();
    }

signals:
    void trackClicked(uint16_t trackNum);

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), OtdrColors::BG_DARK);

        if (!m_disk || m_image.isNull()) {
            p.setPen(OtdrColors::TEXT_DIM);
            p.drawText(rect(), Qt::AlignCenter, "No disk data");
            return;
        }

        // Scale heatmap to widget
        QRect area(60, 10, width() - 70, height() - 40);
        p.drawImage(area, m_image);

        // Track labels (Y axis)
        p.setPen(OtdrColors::TEXT_DIM);
        p.setFont(QFont("Monospace", 8));
        int trackStep = std::max(1, m_disk->track_count / 20);
        for (int t = 0; t < m_disk->track_count; t += trackStep) {
            int y = area.top() + (int)((float)t / m_disk->track_count * area.height());
            p.drawText(5, y + 4, QString("T%1").arg(t));
        }

        // Position labels (X axis)
        p.drawText(area.left(), area.bottom() + 14, "0");
        p.drawText(area.right() - 20, area.bottom() + 14, "360°");

        // Title
        p.setPen(OtdrColors::TEXT);
        p.setFont(QFont("Monospace", 10));
        p.drawText(area.left(), area.bottom() + 28,
                   QString("Disk Quality Heatmap — %1")
                       .arg(otdr_quality_name(m_disk->stats.overall)));

        // Color legend
        int lx = area.right() - 120;
        int ly = area.top() + 5;
        for (int i = 0; i <= 40; i++) {
            QColor c = OtdrColors::heatmapColor(-40.0f + i);
            p.fillRect(lx + i * 3, ly, 3, 10, c);
        }
        p.setPen(OtdrColors::TEXT_DIM);
        p.drawText(lx - 30, ly + 9, "-40dB");
        p.drawText(lx + 125, ly + 9, "0dB");
    }

    void mousePressEvent(QMouseEvent *e) override {
        if (!m_disk) return;
        QRect area(60, 10, width() - 70, height() - 40);
        if (area.contains(e->pos())) {
            int t = (int)((float)(e->pos().y() - area.top()) /
                          area.height() * m_disk->track_count);
            if (t >= 0 && t < m_disk->track_count)
                emit trackClicked((uint16_t)t);
        }
    }

private:
    void rebuildImage() {
        if (!m_disk || !m_disk->heatmap) return;

        uint32_t w = m_disk->heatmap_cols;
        uint16_t h = m_disk->heatmap_rows;
        m_image = QImage(w, h, QImage::Format_RGB888);

        for (uint16_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                float db = m_disk->heatmap[y * w + x];
                QColor c = OtdrColors::heatmapColor(db);
                m_image.setPixelColor(x, y, c);
            }
        }
    }

    const otdr_disk_t *m_disk = nullptr;
    QImage m_image;
};

/* ═══════════════════════════════════════════════════════════════════════
 * Histogram Widget — Timing distribution
 * ═══════════════════════════════════════════════════════════════════════ */

class OtdrHistogramView : public QWidget {
    Q_OBJECT

public:
    explicit OtdrHistogramView(QWidget *parent = nullptr)
        : QWidget(parent) { setMinimumSize(300, 150); }

    void setTrack(const otdr_track_t *track) {
        m_track = track;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), OtdrColors::BG_DARK);

        if (!m_track) return;

        QRect area(40, 10, width() - 50, height() - 30);

        // Find max bin value
        uint32_t maxVal = 1;
        for (int i = 0; i < 256; i++)
            if (m_track->histogram.bins[i] > maxVal)
                maxVal = m_track->histogram.bins[i];

        // Draw bars
        float barW = (float)area.width() / 128; // show 0-12800ns range
        for (int i = 0; i < 128; i++) {
            uint32_t v = m_track->histogram.bins[i];
            if (v == 0) continue;

            float h = (float)v / maxVal * area.height();
            int x = area.left() + (int)(i * barW);
            int y = area.bottom() - (int)h;

            // Color by timing zone
            QColor c;
            int ns = i * 100;
            if (ns < 3000) c = OtdrColors::FAIR;      // below 2T
            else if (ns < 5000) c = OtdrColors::EXCELLENT; // 2T zone
            else if (ns < 7000) c = OtdrColors::GOOD;      // 3T zone
            else if (ns < 9000) c = OtdrColors::EVT_STRUCT; // 4T zone
            else c = OtdrColors::POOR;                       // above 4T

            p.fillRect(x, y, std::max(1, (int)barW - 1), (int)h, c);
        }

        // Peak markers
        auto drawPeak = [&](uint32_t ns, const char *label) {
            int bin = ns / 100;
            if (bin >= 128) return;
            int x = area.left() + (int)(bin * barW);
            p.setPen(QPen(OtdrColors::CRITICAL, 1, Qt::DashLine));
            p.drawLine(x, area.top(), x, area.bottom());
            p.setPen(OtdrColors::TEXT);
            p.setFont(QFont("Monospace", 8));
            p.drawText(x + 2, area.top() + 12, label);
        };

        if (m_track->histogram.peak_2t > 0)
            drawPeak(m_track->histogram.peak_2t, "2T");
        if (m_track->histogram.peak_3t > 0)
            drawPeak(m_track->histogram.peak_3t, "3T");
        if (m_track->histogram.peak_4t > 0)
            drawPeak(m_track->histogram.peak_4t, "4T");

        // X-axis labels
        p.setPen(OtdrColors::TEXT_DIM);
        p.setFont(QFont("Monospace", 8));
        for (int us = 0; us <= 12; us += 2) {
            int x = area.left() + (int)(us * 10 * barW);
            p.drawText(x - 5, area.bottom() + 12, QString("%1µs").arg(us));
        }
    }

private:
    const otdr_track_t *m_track = nullptr;
};

/* ═══════════════════════════════════════════════════════════════════════
 * Main OTDR Widget — Combines all views
 * ═══════════════════════════════════════════════════════════════════════ */

class FloppyOtdrWidget : public QWidget {
    Q_OBJECT

public:
    explicit FloppyOtdrWidget(QWidget *parent = nullptr)
        : QWidget(parent) {
        setupUi();
        connectSignals();
    }

    /** Load disk analysis for display */
    void setDisk(otdr_disk_t *disk) {
        m_disk = disk;
        m_heatmap->setDisk(disk);

        m_trackSelector->clear();
        if (disk) {
            for (uint16_t t = 0; t < disk->track_count; t++) {
                if (disk->tracks[t].sample_count > 0) {
                    m_trackSelector->addItem(
                        QString("Track %1 (C%2:H%3) — %4")
                            .arg(t)
                            .arg(disk->tracks[t].cylinder)
                            .arg(disk->tracks[t].head)
                            .arg(otdr_quality_name(disk->tracks[t].stats.overall)),
                        t);
                }
            }
        }
    }

    /** Select a specific track for trace view */
    void selectTrack(uint16_t trackNum) {
        if (m_disk && trackNum < m_disk->track_count) {
            m_trace->setTrack(&m_disk->tracks[trackNum]);
            m_histogram->setTrack(&m_disk->tracks[trackNum]);

            // Update selector
            int idx = m_trackSelector->findData(trackNum);
            if (idx >= 0)
                m_trackSelector->setCurrentIndex(idx);
        }
    }

private slots:
    void onTrackSelected(int index) {
        if (index >= 0) {
            uint16_t t = m_trackSelector->itemData(index).toUInt();
            selectTrack(t);
        }
    }

    void onHeatmapTrackClicked(uint16_t trackNum) {
        selectTrack(trackNum);
    }

    void onExportReport() {
        if (!m_disk) return;
        QString path = QFileDialog::getSaveFileName(this,
            "Export OTDR Report", "otdr_report.txt", "Text (*.txt)");
        if (!path.isEmpty())
            otdr_disk_export_report(m_disk, path.toUtf8().constData());
    }

    void onExportHeatmap() {
        if (!m_disk) return;
        QString path = QFileDialog::getSaveFileName(this,
            "Export Heatmap", "heatmap.pgm", "PGM Image (*.pgm)");
        if (!path.isEmpty())
            otdr_disk_export_heatmap_pgm(m_disk, path.toUtf8().constData());
    }

    void onCursorPosition(uint32_t bitcell, float db) {
        m_statusLabel->setText(
            QString("Position: %1  |  Quality: %2 dB  |  %3")
                .arg(bitcell)
                .arg(db, 0, 'f', 1)
                .arg(otdr_quality_name(otdr_db_to_quality(db))));
    }

private:
    void setupUi() {
        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(4, 4, 4, 4);
        mainLayout->setSpacing(4);

        // ── Toolbar ──
        auto *toolbar = new QHBoxLayout;
        toolbar->addWidget(new QLabel("Track:"));

        m_trackSelector = new QComboBox;
        m_trackSelector->setMinimumWidth(250);
        toolbar->addWidget(m_trackSelector);

        m_cbSmoothed = new QCheckBox("Smoothed");
        m_cbSmoothed->setChecked(true);
        toolbar->addWidget(m_cbSmoothed);

        m_cbEvents = new QCheckBox("Events");
        m_cbEvents->setChecked(true);
        toolbar->addWidget(m_cbEvents);

        m_cbSectors = new QCheckBox("Sectors");
        m_cbSectors->setChecked(true);
        toolbar->addWidget(m_cbSectors);

        m_cbRawOverlay = new QCheckBox("Raw Overlay");
        toolbar->addWidget(m_cbRawOverlay);

        toolbar->addStretch();

        m_btnReport = new QPushButton("Export Report");

        toolbar->addWidget(m_btnReport);
        m_btnHeatmap = new QPushButton("Export Heatmap");

        toolbar->addWidget(m_btnHeatmap);

        mainLayout->addLayout(toolbar);

        // ── Trace View (main) ──
        m_trace = new OtdrTraceView;
        mainLayout->addWidget(m_trace, 3);

        // ── Bottom panel: Heatmap + Histogram ──
        auto *bottomLayout = new QHBoxLayout;

        m_heatmap = new OtdrHeatmapView;
        bottomLayout->addWidget(m_heatmap, 2);

        m_histogram = new OtdrHistogramView;
        bottomLayout->addWidget(m_histogram, 1);

        mainLayout->addLayout(bottomLayout, 2);

        // ── Status bar ──
        m_statusLabel = new QLabel("Ready");
        m_statusLabel->setStyleSheet("color: #8899aa; font-family: monospace;");
        mainLayout->addWidget(m_statusLabel);
    }

    void connectSignals() {
        connect(m_trackSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &FloppyOtdrWidget::onTrackSelected);
        connect(m_heatmap, &OtdrHeatmapView::trackClicked,
                this, &FloppyOtdrWidget::onHeatmapTrackClicked);
        connect(m_trace, &OtdrTraceView::cursorPositionChanged,
                this, &FloppyOtdrWidget::onCursorPosition);

        connect(m_cbSmoothed, &QCheckBox::toggled, m_trace, &OtdrTraceView::setShowSmoothed);
        connect(m_cbEvents, &QCheckBox::toggled, m_trace, &OtdrTraceView::setShowEvents);
        connect(m_cbSectors, &QCheckBox::toggled, m_trace, &OtdrTraceView::setShowSectors);
        connect(m_cbRawOverlay, &QCheckBox::toggled, m_trace, &OtdrTraceView::setShowRaw);

        connect(m_btnReport, &QPushButton::clicked, this, &FloppyOtdrWidget::onExportReport);
        connect(m_btnHeatmap, &QPushButton::clicked, this, &FloppyOtdrWidget::onExportHeatmap);
    }

    // ── Member data ──
    otdr_disk_t *m_disk = nullptr;

    OtdrTraceView    *m_trace = nullptr;
    OtdrHeatmapView  *m_heatmap = nullptr;
    OtdrHistogramView *m_histogram = nullptr;

    QComboBox  *m_trackSelector = nullptr;
    QCheckBox  *m_cbSmoothed = nullptr;
    QCheckBox  *m_cbEvents = nullptr;
    QCheckBox  *m_cbSectors = nullptr;
    QCheckBox  *m_cbRawOverlay = nullptr;
    QLabel     *m_statusLabel = nullptr;
    QPushButton *m_btnReport = nullptr;
    QPushButton *m_btnHeatmap = nullptr;
};

#endif /* FLOPPY_OTDR_WIDGET_H */
