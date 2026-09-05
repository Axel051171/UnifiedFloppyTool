/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_sector_editor.cpp
 * @brief Sector Editor Implementation (BONUS-GUI-002)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft_sector_editor.h"
/* MF-895: die Zonenaufteilung kommt aus dem einen Ort, an dem sie steht.
 * Der Kopf dieser Datei zaehlt auf, dass der Baum vorher 24 Kopien davon
 * trug; die Tabelle, die hier stand, war die 25. */
#include "uft/formats/cbm/uft_cbm_geometry.h"
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QScrollBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QKeyEvent>
#include <QFontMetrics>
#include <QApplication>
#include <QClipboard>

/* Die Formate dieses Editors, an EINER Stelle (MF-902).
 *
 * Vorher lag dieselbe Fallunterscheidung viermal im Widget: in der
 * Liste der Kombobox, in `applyFormat()`, in `sectorOffset()` und in
 * `sectorsPerTrack()`. Und die Kombobox gewann ihre Kennung aus dem
 * ANZEIGETEXT zurueck (`currentText().section(' ', 0, 0)`) — wer den
 * Text uebersetzt oder umformuliert, haette damit still die Geometrie
 * verstellt. Die Kennung steht jetzt als `itemData` daneben.
 *
 * `sektoren_je_spur == 0` heisst ZONIERT: die Sektorzahl haengt von der
 * Spur ab und kommt aus `uft_cbm_geometry.h`. */
struct FormatEintrag {
    const char *kennung;          /* "D64" — was `m_format` traegt      */
    const char *anzeige;          /* "D64 (C64)" — was der Benutzer sieht */
    int         sektorgroesse;
    int         spuren;           /* 0 = aus der Dateigroesse rechnen    */
    int         sektoren_je_spur; /* 0 = zoniert (CBM)                   */
    int         kopf_bytes;       /* Container-Kopf vor dem ersten Sektor */
};

static const FormatEintrag kFormate[] = {
    { "D64", "D64 (C64)",   256,  35,  0,  0 },
    { "D71", "D71 (C128)",  256,  70,  0,  0 },
    { "ADF", "ADF (Amiga)", 512, 160, 11,  0 },
    { "ATR", "ATR (Atari)", 128,  40, 18, 16 },
    { "Raw", "Raw",         256,   0,  1,  0 },
};
static const int kFormatCount =
    static_cast<int>(sizeof(kFormate) / sizeof(kFormate[0]));

/** @return Eintrag zur Kennung, oder der Rueckfall "Raw" (nie nullptr). */
static const FormatEintrag *formatEintrag(const QString &kennung)
{
    for (int i = 0; i < kFormatCount; i++) {
        if (kennung == QLatin1String(kFormate[i].kennung)) return &kFormate[i];
    }
    return &kFormate[kFormatCount - 1];   /* Raw */
}

/*===========================================================================
 * UftHexEdit
 *===========================================================================*/

UftHexEdit::UftHexEdit(QWidget *parent)
    : QWidget(parent)
    , m_readOnly(false)
    , m_bytesPerRow(16)
    , m_cursorPos(0)
    , m_selStart(-1)
    , m_selEnd(-1)
    , m_lowNibble(false)
    , m_scrollOffset(0)
    , m_lastSearchPos(-1)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(400, 200);
    
    QFont font("Courier New", 10);
    font.setStyleHint(QFont::Monospace);
    setFont(font);
    
    updateMetrics();
}

void UftHexEdit::updateMetrics()
{
    QFontMetrics fm(font());
    m_charWidth = fm.horizontalAdvance('0');
    m_charHeight = fm.height();
    m_addressWidth = m_charWidth * 8 + 10;  /* "00000000: " */
    m_hexWidth = m_charWidth * (3 * m_bytesPerRow + 2);  /* "XX " * 16 + gap */
    m_asciiWidth = m_charWidth * (m_bytesPerRow + 2);
}

void UftHexEdit::setData(const QByteArray &data)
{
    m_data = data;
    m_cursorPos = 0;
    m_selStart = m_selEnd = -1;
    m_scrollOffset = 0;
    update();
    emit dataChanged();
}

void UftHexEdit::setReadOnly(bool readonly)
{
    m_readOnly = readonly;
}

void UftHexEdit::setBytesPerRow(int count)
{
    m_bytesPerRow = qBound(8, count, 32);
    updateMetrics();
    update();
}

void UftHexEdit::setCursorPosition(int pos)
{
    m_cursorPos = qBound(0, pos, m_data.size());
    m_lowNibble = false;
    ensureVisible(m_cursorPos);
    update();
    emit cursorPositionChanged(m_cursorPos);
}

void UftHexEdit::setSelection(int start, int end)
{
    m_selStart = qBound(0, start, m_data.size());
    m_selEnd = qBound(0, end, m_data.size());
    update();
    emit selectionChanged(m_selStart, m_selEnd);
}

QByteArray UftHexEdit::selectedData() const
{
    if (m_selStart < 0 || m_selEnd < 0) return QByteArray();
    int start = qMin(m_selStart, m_selEnd);
    int end = qMax(m_selStart, m_selEnd);
    return m_data.mid(start, end - start + 1);
}

void UftHexEdit::setByteAt(int pos, uint8_t value)
{
    if (pos >= 0 && pos < m_data.size()) {
        uint8_t oldValue = (uint8_t)m_data[pos];
        m_data[pos] = (char)value;
        emit byteEdited(pos, oldValue, value);
        update();
    }
}

uint8_t UftHexEdit::byteAt(int pos) const
{
    if (pos >= 0 && pos < m_data.size()) {
        return (uint8_t)m_data[pos];
    }
    return 0;
}

int UftHexEdit::find(const QByteArray &pattern, int startPos)
{
    m_searchPattern = pattern;
    if (pattern.isEmpty()) return -1;
    
    int pos = m_data.indexOf(pattern, startPos);
    if (pos >= 0) {
        m_lastSearchPos = pos;
        setCursorPosition(pos);
        setSelection(pos, pos + pattern.size() - 1);
    }
    return pos;
}

int UftHexEdit::findNext()
{
    if (m_searchPattern.isEmpty()) return -1;
    return find(m_searchPattern, m_lastSearchPos + 1);
}

int UftHexEdit::findPrev()
{
    if (m_searchPattern.isEmpty() || m_lastSearchPos <= 0) return -1;
    
    int pos = m_data.lastIndexOf(m_searchPattern, m_lastSearchPos - 1);
    if (pos >= 0) {
        m_lastSearchPos = pos;
        setCursorPosition(pos);
        setSelection(pos, pos + m_searchPattern.size() - 1);
    }
    return pos;
}

QSize UftHexEdit::sizeHint() const
{
    int rows = (m_data.size() + m_bytesPerRow - 1) / m_bytesPerRow;
    return QSize(m_addressWidth + m_hexWidth + m_asciiWidth + 20,
                 rows * m_charHeight + 20);
}

void UftHexEdit::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setFont(font());
    
    int rows = (m_data.size() + m_bytesPerRow - 1) / m_bytesPerRow;
    int visibleRows = height() / m_charHeight + 1;
    int startRow = m_scrollOffset / m_charHeight;
    
    QColor bgColor = palette().window().color();
    QColor textColor = palette().windowText().color();
    QColor selColor = palette().highlight().color();
    QColor addrColor = Qt::darkBlue;
    QColor asciiColor = Qt::darkGreen;
    
    p.fillRect(rect(), bgColor);
    
    for (int row = startRow; row < startRow + visibleRows && row < rows; row++) {
        int y = (row - startRow) * m_charHeight + m_charHeight;
        int offset = row * m_bytesPerRow;
        
        /* Address */
        p.setPen(addrColor);
        p.drawText(5, y, QString("%1:").arg(offset, 6, 16, QChar('0')));
        
        int hexX = m_addressWidth;
        int asciiX = m_addressWidth + m_hexWidth;
        
        for (int col = 0; col < m_bytesPerRow && offset + col < m_data.size(); col++) {
            int pos = offset + col;
            uint8_t b = (uint8_t)m_data[pos];
            
            /* Check selection */
            bool selected = m_selStart >= 0 && m_selEnd >= 0 &&
                           pos >= qMin(m_selStart, m_selEnd) &&
                           pos <= qMax(m_selStart, m_selEnd);
            
            /* Check cursor */
            bool isCursor = (pos == m_cursorPos);
            
            /* Hex byte */
            QRect hexRect(hexX + col * 3 * m_charWidth, y - m_charHeight + 3,
                         2 * m_charWidth, m_charHeight);
            
            if (selected) {
                p.fillRect(hexRect, selColor);
                p.setPen(palette().highlightedText().color());
            } else {
                p.setPen(textColor);
            }
            
            p.drawText(hexRect, Qt::AlignCenter, 
                      QString("%1").arg(b, 2, 16, QChar('0')).toUpper());
            
            /* Cursor */
            if (isCursor && hasFocus()) {
                p.setPen(Qt::red);
                if (m_lowNibble) {
                    p.drawRect(hexRect.adjusted(m_charWidth, 0, 0, 0));
                } else {
                    p.drawRect(hexRect.adjusted(0, 0, -m_charWidth, 0));
                }
            }
            
            /* ASCII */
            QRect asciiRect(asciiX + col * m_charWidth, y - m_charHeight + 3,
                           m_charWidth, m_charHeight);
            
            if (selected) {
                p.fillRect(asciiRect, selColor);
                p.setPen(palette().highlightedText().color());
            } else {
                p.setPen(asciiColor);
            }
            
            QChar c = (b >= 0x20 && b < 0x7F) ? QChar(b) : '.';
            p.drawText(asciiRect, Qt::AlignCenter, c);
        }
    }
}

void UftHexEdit::keyPressEvent(QKeyEvent *event)
{
    if (m_readOnly) {
        QWidget::keyPressEvent(event);
        return;
    }
    
    int key = event->key();
    
    /* Navigation */
    if (key == Qt::Key_Left) {
        setCursorPosition(m_cursorPos - 1);
    } else if (key == Qt::Key_Right) {
        setCursorPosition(m_cursorPos + 1);
    } else if (key == Qt::Key_Up) {
        setCursorPosition(m_cursorPos - m_bytesPerRow);
    } else if (key == Qt::Key_Down) {
        setCursorPosition(m_cursorPos + m_bytesPerRow);
    } else if (key == Qt::Key_Home) {
        setCursorPosition(0);
    } else if (key == Qt::Key_End) {
        setCursorPosition(m_data.size() - 1);
    } else if (key == Qt::Key_PageUp) {
        setCursorPosition(m_cursorPos - m_bytesPerRow * 16);
    } else if (key == Qt::Key_PageDown) {
        setCursorPosition(m_cursorPos + m_bytesPerRow * 16);
    }
    /* Hex input */
    else if ((key >= Qt::Key_0 && key <= Qt::Key_9) ||
             (key >= Qt::Key_A && key <= Qt::Key_F)) {
        int nibble;
        if (key >= Qt::Key_0 && key <= Qt::Key_9) {
            nibble = key - Qt::Key_0;
        } else {
            nibble = key - Qt::Key_A + 10;
        }
        
        if (m_cursorPos < m_data.size()) {
            uint8_t oldVal = (uint8_t)m_data[m_cursorPos];
            uint8_t newVal;
            
            if (!m_lowNibble) {
                newVal = (nibble << 4) | (oldVal & 0x0F);
                m_lowNibble = true;
            } else {
                newVal = (oldVal & 0xF0) | nibble;
                m_lowNibble = false;
                setCursorPosition(m_cursorPos + 1);
            }
            
            setByteAt(m_cursorPos, newVal);
        }
    }
    /* Copy/Paste */
    else if (event->matches(QKeySequence::Copy)) {
        QByteArray sel = selectedData();
        if (!sel.isEmpty()) {
            QString hex;
            for (int i = 0; i < sel.size(); i++) {
                hex += QString("%1 ").arg((uint8_t)sel[i], 2, 16, QChar('0'));
            }
            QApplication::clipboard()->setText(hex.trimmed());
        }
    }
    else {
        QWidget::keyPressEvent(event);
    }
}

void UftHexEdit::mousePressEvent(QMouseEvent *event)
{
    int pos = posFromPoint(event->pos());
    if (pos >= 0) {
        setCursorPosition(pos);
        m_selStart = pos;
        m_selEnd = pos;
    }
}

void UftHexEdit::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        int pos = posFromPoint(event->pos());
        if (pos >= 0) {
            m_selEnd = pos;
            update();
        }
    }
}

void UftHexEdit::wheelEvent(QWheelEvent *event)
{
    int delta = event->angleDelta().y();
    m_scrollOffset -= delta;
    m_scrollOffset = qBound(0, m_scrollOffset, 
                           qMax(0, (m_data.size() / m_bytesPerRow) * m_charHeight - height()));
    update();
}

void UftHexEdit::focusInEvent(QFocusEvent *)
{
    update();
}

void UftHexEdit::focusOutEvent(QFocusEvent *)
{
    update();
}

void UftHexEdit::resizeEvent(QResizeEvent *)
{
    update();
}

int UftHexEdit::posFromPoint(const QPoint &pt) const
{
    int row = (pt.y() + m_scrollOffset) / m_charHeight;
    int col = -1;
    
    /* Check hex area */
    int hexX = pt.x() - m_addressWidth;
    if (hexX >= 0 && hexX < m_hexWidth) {
        col = hexX / (3 * m_charWidth);
    }
    
    /* Check ASCII area */
    int asciiX = pt.x() - m_addressWidth - m_hexWidth;
    if (asciiX >= 0 && asciiX < m_asciiWidth) {
        col = asciiX / m_charWidth;
    }
    
    if (col >= 0 && col < m_bytesPerRow) {
        int pos = row * m_bytesPerRow + col;
        if (pos >= 0 && pos < m_data.size()) {
            return pos;
        }
    }
    
    return -1;
}

QPoint UftHexEdit::pointFromPos(int pos) const
{
    int row = pos / m_bytesPerRow;
    int col = pos % m_bytesPerRow;
    return QPoint(m_addressWidth + col * 3 * m_charWidth,
                  row * m_charHeight - m_scrollOffset);
}

void UftHexEdit::ensureVisible(int pos)
{
    int row = pos / m_bytesPerRow;
    int y = row * m_charHeight;
    
    if (y < m_scrollOffset) {
        m_scrollOffset = y;
    } else if (y + m_charHeight > m_scrollOffset + height()) {
        m_scrollOffset = y + m_charHeight - height();
    }
    
    update();
}

/*===========================================================================
 * UftHexEditCommand
 *===========================================================================*/

UftHexEditCommand::UftHexEditCommand(UftHexEdit *editor, int pos, 
                                      uint8_t oldVal, uint8_t newVal,
                                      QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_editor(editor)
    , m_pos(pos)
    , m_oldValue(oldVal)
    , m_newValue(newVal)
{
    setText(QString("Edit byte at %1").arg(pos, 0, 16));
}

void UftHexEditCommand::undo()
{
    m_editor->setByteAt(m_pos, m_oldValue);
}

void UftHexEditCommand::redo()
{
    m_editor->setByteAt(m_pos, m_newValue);
}

bool UftHexEditCommand::mergeWith(const QUndoCommand *other)
{
    const UftHexEditCommand *cmd = dynamic_cast<const UftHexEditCommand*>(other);
    if (!cmd) return false;
    if (m_pos != cmd->m_pos) return false;
    
    m_newValue = cmd->m_newValue;
    return true;
}

/*===========================================================================
 * UftSectorEditor
 *===========================================================================*/

UftSectorEditor::UftSectorEditor(QWidget *parent)
    : QWidget(parent)
    , m_currentTrack(0)
    , m_currentSector(0)
    , m_sectorSize(256)
    , m_totalTracks(35)
    , m_modified(false)
{
    m_undoStack = new QUndoStack(this);
    setupUi();
}

void UftSectorEditor::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    
    createToolbar();
    mainLayout->addWidget(m_toolbar);
    
    QHBoxLayout *contentLayout = new QHBoxLayout();
    
    /* Left: Navigation + Info */
    QVBoxLayout *leftLayout = new QVBoxLayout();
    createNavigationPanel();
    createInfoPanel();
    leftLayout->addWidget(m_navGroup);
    leftLayout->addWidget(m_infoGroup);
    leftLayout->addStretch();
    
    contentLayout->addLayout(leftLayout);
    
    /* Center: Hex editor */
    m_hexEdit = new UftHexEdit();
    contentLayout->addWidget(m_hexEdit, 1);
    
    mainLayout->addLayout(contentLayout);
    
    /* Connections */
    connect(m_hexEdit, &UftHexEdit::cursorPositionChanged, 
            this, &UftSectorEditor::onCursorMoved);
    connect(m_hexEdit, &UftHexEdit::byteEdited,
            this, &UftSectorEditor::onByteEdited);
    connect(m_trackSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftSectorEditor::onTrackChanged);
    connect(m_sectorSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftSectorEditor::onSectorChanged);
}

void UftSectorEditor::createToolbar()
{
    m_toolbar = new QToolBar();
    
    QPushButton *openBtn = new QPushButton(tr("Open..."));
    m_toolbar->addWidget(openBtn);
    
    m_saveAction = m_toolbar->addAction(tr("Save"));
    m_saveAction->setEnabled(false);
    
    m_toolbar->addSeparator();
    
    m_undoAction = m_undoStack->createUndoAction(this, tr("Undo"));
    m_redoAction = m_undoStack->createRedoAction(this, tr("Redo"));
    m_toolbar->addAction(m_undoAction);
    m_toolbar->addAction(m_redoAction);
    
    m_toolbar->addSeparator();
    
    m_findAction = m_toolbar->addAction(tr("Find"));
    m_goToAction = m_toolbar->addAction(tr("Go To"));
    
    connect(openBtn, &QPushButton::clicked, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Open Disk Image"));
        if (!path.isEmpty()) loadDisk(path);
    });
    connect(m_saveAction, &QAction::triggered, this, &UftSectorEditor::save);
    connect(m_findAction, &QAction::triggered, this, &UftSectorEditor::find);
    connect(m_goToAction, &QAction::triggered, this, &UftSectorEditor::goTo);
}

void UftSectorEditor::createNavigationPanel()
{
    m_navGroup = new QGroupBox(tr("Navigation"));
    QGridLayout *grid = new QGridLayout(m_navGroup);
    
    grid->addWidget(new QLabel(tr("Track:")), 0, 0);
    m_trackSpin = new QSpinBox();
    m_trackSpin->setObjectName("sectorEditorTrackSpin");
    m_trackSpin->setRange(0, 84);
    grid->addWidget(m_trackSpin, 0, 1);
    
    grid->addWidget(new QLabel(tr("Sector:")), 1, 0);
    m_sectorSpin = new QSpinBox();
    m_sectorSpin->setObjectName("sectorEditorSectorSpin");
    m_sectorSpin->setRange(0, 20);
    grid->addWidget(m_sectorSpin, 1, 1);
    
    m_prevButton = new QPushButton(tr("◀ Prev"));
    m_nextButton = new QPushButton(tr("Next ▶"));
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addWidget(m_prevButton);
    btnLayout->addWidget(m_nextButton);
    grid->addLayout(btnLayout, 2, 0, 1, 2);
    
    grid->addWidget(new QLabel(tr("Format:")), 3, 0);
    m_formatCombo = new QComboBox();
    m_formatCombo->setObjectName("sectorEditorFormatCombo");
    /* MF-895: "DSK (Apple)" ist raus. Das Widget hatte fuer diesen Eintrag
     * NIE eine Geometrie — er fiel in den Rueckfallzweig, der `sector`
     * ueberhaupt nicht beruecksichtigte. Ein Eintrag, der etwas anderes
     * anzeigt, als er verspricht, gehoert nicht in die Liste. */
    for (int i = 0; i < kFormatCount; i++) {
        m_formatCombo->addItem(QString::fromLatin1(kFormate[i].anzeige),
                               QString::fromLatin1(kFormate[i].kennung));
    }
    grid->addWidget(m_formatCombo, 3, 1);

    /* MF-895: die Auswahl war angelegt, gefuellt, eingehaengt — und im
     * ganzen Widget weder gelesen noch gesetzt noch verbunden (gemessen).
     * Nach dem Laden einer .d71 stand dort weiter "D64 (C64)". Jetzt
     * spiegelt sie das geladene Format UND wirkt: die Endung schlaegt vor,
     * der Benutzer entscheidet. */
    /* MF-902: die Kennung kommt aus `itemData`, nicht mehr aus dem
     * ersten Wort des Anzeigetextes. */
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        applyFormat(m_formatCombo->currentData().toString());
    });
    
    connect(m_prevButton, &QPushButton::clicked, [this]() {
        if (m_currentSector > 0) {
            m_sectorSpin->setValue(m_currentSector - 1);
        } else if (m_currentTrack > 0) {
            m_trackSpin->setValue(m_currentTrack - 1);
            m_sectorSpin->setValue(qMax(0, sectorsPerTrack(m_currentTrack - 1) - 1));
        }
    });
    
    connect(m_nextButton, &QPushButton::clicked, [this]() {
        if (m_currentSector < sectorsPerTrack(m_currentTrack) - 1) {
            m_sectorSpin->setValue(m_currentSector + 1);
        } else if (m_currentTrack < m_totalTracks - 1) {
            m_trackSpin->setValue(m_currentTrack + 1);
            m_sectorSpin->setValue(0);
        }
    });
}

void UftSectorEditor::createInfoPanel()
{
    m_infoGroup = new QGroupBox(tr("Sector Info"));
    QGridLayout *grid = new QGridLayout(m_infoGroup);
    
    grid->addWidget(new QLabel(tr("Offset:")), 0, 0);
    m_offsetLabel = new QLabel("-");
    m_offsetLabel->setObjectName("sectorEditorOffsetLabel");
    grid->addWidget(m_offsetLabel, 0, 1);
    
    grid->addWidget(new QLabel(tr("Value:")), 1, 0);
    m_valueLabel = new QLabel("-");
    grid->addWidget(m_valueLabel, 1, 1);
    
    grid->addWidget(new QLabel(tr("Checksum:")), 2, 0);
    m_checksumLabel = new QLabel("-");
    grid->addWidget(m_checksumLabel, 2, 1);
    
    grid->addWidget(new QLabel(tr("Encoding:")), 3, 0);
    m_encodingLabel = new QLabel("-");
    grid->addWidget(m_encodingLabel, 3, 1);
}

void UftSectorEditor::loadDisk(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Error"), 
            tr("Cannot open file: %1").arg(path));
        return;
    }
    
    m_diskPath = path;
    m_diskData = file.readAll();
    file.close();
    
    /* Die Dateiendung SCHLAEGT das Format VOR — sie beweist es nicht.
     * Die Auswahl daneben zeigt es an und laesst es umstellen (MF-895). */
    const QString ext = QFileInfo(path).suffix().toLower();
    m_currentTrack = 0;
    m_currentSector = 0;

    if (ext == "d64")      applyFormat(QStringLiteral("D64"));
    else if (ext == "d71") applyFormat(QStringLiteral("D71"));
    else if (ext == "adf") applyFormat(QStringLiteral("ADF"));
    else if (ext == "atr") applyFormat(QStringLiteral("ATR"));
    else                   applyFormat(QStringLiteral("Raw"));

    m_modified = false;
    m_saveAction->setEnabled(true);
}

void UftSectorEditor::applyFormat(const QString &format)
{
    const FormatEintrag *e = formatEintrag(format);
    m_format      = QString::fromLatin1(e->kennung);
    m_sectorSize  = e->sektorgroesse;
    m_totalTracks = e->spuren > 0
                  ? e->spuren
                  : static_cast<int>(m_diskData.size() / e->sektorgroesse);
    if (m_totalTracks < 1) m_totalTracks = 1;

    /* Die Auswahl spiegelt das Format, ohne sich selbst erneut auszuloesen. */
    if (m_formatCombo) {
        const QSignalBlocker blocked(m_formatCombo);
        const int i = m_formatCombo->findData(m_format);
        if (i >= 0) m_formatCombo->setCurrentIndex(i);
    }

    if (m_currentTrack > m_totalTracks - 1) m_currentTrack = 0;
    {
        const QSignalBlocker blocked(m_trackSpin);
        m_trackSpin->setRange(0, m_totalTracks - 1);
        m_trackSpin->setValue(m_currentTrack);
    }

    const int perTrack = sectorsPerTrack(m_currentTrack);
    if (m_currentSector > perTrack - 1) m_currentSector = 0;
    {
        const QSignalBlocker blocked(m_sectorSpin);
        m_sectorSpin->setRange(0, perTrack > 0 ? perTrack - 1 : 0);
        m_sectorSpin->setValue(m_currentSector);
    }

    loadSector(m_currentTrack, m_currentSector);
    updateSectorInfo();
}

void UftSectorEditor::loadSector(int track, int sector)
{
    const int offset = sectorOffset(track, sector);
    if (offset < 0 || offset + m_sectorSize > m_diskData.size()) {
        /* MF-895: hier stand `QByteArray(m_sectorSize, 0)` — 256 Nullbytes,
         * von einem echt leeren Sektor nicht zu unterscheiden. Dieselbe
         * Klasse wie die feste Rampe aus MF-892. Ausserhalb des Abbilds
         * gibt es keinen Inhalt, also wird auch keiner gezeigt, und die
         * Anzeige sagt warum. */
        m_hexEdit->setData(QByteArray());
        m_currentTrack  = track;
        m_currentSector = sector;
        if (m_offsetLabel) {
            /* MF-902: war deutsch, in einem Panel, dessen uebrige
             * tr()-Zeichenketten englisch sind ("Track:", "Sector:",
             * "Offset:"). */
            m_offsetLabel->setText(
                tr("Track %1 / sector %2 is outside the image")
                    .arg(track).arg(sector));
        }
        emit sectorChanged(track, sector);
        return;
    }
    
    QByteArray sectorData = m_diskData.mid(offset, m_sectorSize);
    m_hexEdit->setData(sectorData);
    
    m_currentTrack = track;
    m_currentSector = sector;
    
    emit sectorChanged(track, sector);
}

void UftSectorEditor::saveSector()
{
    const int offset = sectorOffset(m_currentTrack, m_currentSector);
    if (offset < 0 || offset + m_hexEdit->data().size() > m_diskData.size()) {
        /* MF-895: `if (offset < 0) return;` hat die Aenderung still
         * verworfen — die Hex-Ansicht zeigte sie weiter, `m_modified`
         * blieb falsch, und beim Speichern fehlte sie. Lieber laut. */
        qWarning("Sektoreditor: Spur %d / Sektor %d liegt ausserhalb des "
                 "Abbilds - die Aenderung wird NICHT uebernommen",
                 m_currentTrack, m_currentSector);
        return;
    }
    
    QByteArray data = m_hexEdit->data();
    for (int i = 0; i < data.size() && offset + i < m_diskData.size(); i++) {
        m_diskData[offset + i] = data[i];
    }
    
    m_modified = true;
    emit diskModified();
}

/* Spuren je Seite bei den Commodore-GCR-Laufwerken. Eine D71 ist zweimal
 * diese Seite hintereinander — der SSOT beschreibt EINE Seite
 * (`UFT_CBM_1571`: max_track 42, heads 2), nicht 70 fortlaufende Spuren.
 * Nicht aus dem SSOT ableitbar: `uft_cbm_max_track(UFT_CBM_1571)` ist 42
 * (die erweiterten Spuren), waehrend eine D71-SEITE 35 traegt. Das ist
 * eine Eigenschaft des Datei-Containers, nicht des Laufwerks. */
static const int kCbmSpurenJeSeite = 35;


int UftSectorEditor::sectorOffset(int track, int sector) const
{
    /* MF-895: Vorher gab es diese Pruefung NICHT. `sector` ging ungeprueft
     * in die Rechnung, und die Spursumme war mit `t < 35` gedeckelt —
     * bei einer D71 landeten damit alle 35 Spuren der zweiten Seite auf
     * demselben Versatz 0x2AB00. Da `saveSector()` denselben Versatz
     * benutzt und `save()` `m_diskData` zurueckschreibt, war das stille
     * Veraenderung auf einem Schreibpfad. */
    if (track < 0 || sector < 0 || track >= m_totalTracks) return -1;
    const int perTrack = sectorsPerTrack(track);
    if (perTrack <= 0 || sector >= perTrack) return -1;

    const FormatEintrag *e = formatEintrag(m_format);

    if (e->sektoren_je_spur == 0) {
        /* Zoniert: die Sektorzahl haengt von der Spur ab, und eine D71 ist
         * zweimal die 1541-Seite hintereinander. */
        const int seite = track / kCbmSpurenJeSeite;              /* 0 oder 1 */
        const int spur  = track % kCbmSpurenJeSeite + 1;          /* 1-basiert */
        const int blockversatz = uft_cbm_block_offset(UFT_CBM_1541, spur);
        if (blockversatz < 0) return -1;
        const int seitenversatz =
            seite * uft_cbm_total_blocks(UFT_CBM_1541, kCbmSpurenJeSeite);
        return e->kopf_bytes
             + (seitenversatz + blockversatz + sector) * e->sektorgroesse;
    }

    /* Flach: gleich viele Sektoren auf jeder Spur. Vorher stand im
     * Rueckfall `track * m_sectorSize` — `sector` fiel unter den Tisch. */
    return e->kopf_bytes + (track * perTrack + sector) * e->sektorgroesse;
}

int UftSectorEditor::sectorsPerTrack(int track) const
{
    if (track < 0 || track >= m_totalTracks) return 0;
    const FormatEintrag *e = formatEintrag(m_format);
    if (e->sektoren_je_spur > 0) return e->sektoren_je_spur;
    /* MF-895: der D71-Zweig fehlte ganz. `sectorsPerTrack()` kannte nur
     * "D64" und fiel sonst auf `return 1` durch — der Sektor-Zaehler
     * stand bei einer D71 auf 0..0, von 21 Sektoren war einer erreichbar. */
    return uft_cbm_sectors_per_track(UFT_CBM_1541,
                                     track % kCbmSpurenJeSeite + 1);
}

void UftSectorEditor::onTrackChanged(int track)
{
    const int proSpur = sectorsPerTrack(track);
    m_sectorSpin->setRange(0, proSpur > 0 ? proSpur - 1 : 0);
    loadSector(track, m_sectorSpin->value());
    updateSectorInfo();
}

void UftSectorEditor::onSectorChanged(int sector)
{
    loadSector(m_currentTrack, sector);
    updateSectorInfo();
}

void UftSectorEditor::onByteEdited(int pos, uint8_t oldVal, uint8_t newVal)
{
    m_undoStack->push(new UftHexEditCommand(m_hexEdit, pos, oldVal, newVal));
    saveSector();
}

void UftSectorEditor::onCursorMoved(int pos)
{
    if (pos >= 0 && pos < m_hexEdit->data().size()) {
        int globalOffset = sectorOffset(m_currentTrack, m_currentSector) + pos;
        uint8_t value = m_hexEdit->byteAt(pos);
        
        m_offsetLabel->setText(QString("0x%1 (local: 0x%2)")
            .arg(globalOffset, 6, 16, QChar('0'))
            .arg(pos, 2, 16, QChar('0')));
        m_valueLabel->setText(QString("0x%1 (%2) '%3'")
            .arg(value, 2, 16, QChar('0'))
            .arg(value)
            .arg((value >= 0x20 && value < 0x7F) ? QChar(value) : '.'));
    }
}

void UftSectorEditor::updateSectorInfo()
{
    /* Calculate checksum */
    QByteArray data = m_hexEdit->data();
    uint8_t xorSum = 0;
    uint16_t addSum = 0;
    for (int i = 0; i < data.size(); i++) {
        xorSum ^= (uint8_t)data[i];
        addSum += (uint8_t)data[i];
    }
    m_checksumLabel->setText(QString("XOR: %1, ADD: %2")
        .arg(xorSum, 2, 16, QChar('0'))
        .arg(addSum, 4, 16, QChar('0')));
    
    m_encodingLabel->setText(m_format);
}

void UftSectorEditor::goToSector(int track, int sector)
{
    m_trackSpin->setValue(track);
    m_sectorSpin->setValue(sector);
}

void UftSectorEditor::goToOffset(int offset)
{
    /* MF-895: hier stand eine feste 18 als Sektoren-je-Spur — fuer JEDES
     * Format, mit dem Kommentar "Simplified for now". Bei einer D64 fuehrte
     * jede Eingabe im "Go To"-Dialog damit auf die falsche Spur. Jetzt wird
     * die Zuordnung ueber dieselbe Rechnung gesucht, die auch anzeigt. */
    if (offset < 0) return;
    for (int t = 0; t < m_totalTracks; t++) {
        const int proSpur = sectorsPerTrack(t);
        for (int s = 0; s < proSpur; s++) {
            const int o = sectorOffset(t, s);
            if (o >= 0 && offset >= o && offset < o + m_sectorSize) {
                goToSector(t, s);
                return;
            }
        }
    }
    /* Kein Sektor deckt diesen Versatz. Raten waere eine Erfindung. */
    if (m_offsetLabel) {
        m_offsetLabel->setText(
            tr("0x%1 is in no sector of this image")
                .arg(offset, 6, 16, QChar('0')));
    }
}

void UftSectorEditor::save()
{
    if (m_diskPath.isEmpty()) {
        saveAs();
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        tr("Save Changes"),
        tr("Save changes to %1?\n\nThis will overwrite the original file. "
           "For forensic preservation, consider using 'Save As' instead.")
            .arg(QFileInfo(m_diskPath).fileName()),
        QMessageBox::Save | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (reply != QMessageBox::Save) return;

    /* Create .bak backup before overwriting */
    QString bakPath = m_diskPath + ".bak";
    QFile::copy(m_diskPath, bakPath);

    QFile file(m_diskPath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Cannot save file: %1").arg(m_diskPath));
        return;
    }

    file.write(m_diskData);
    file.close();

    m_modified = false;
    QMessageBox::information(this, tr("Saved"),
        tr("Disk image saved successfully."));
}

void UftSectorEditor::saveAs()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save As"));
    if (path.isEmpty()) return;
    
    m_diskPath = path;
    save();
}

void UftSectorEditor::revert()
{
    if (m_modified) {
        if (QMessageBox::question(this, tr("Revert"),
            tr("Discard all changes?")) != QMessageBox::Yes) {
            return;
        }
    }
    loadDisk(m_diskPath);
}

void UftSectorEditor::undo()
{
    m_undoStack->undo();
}

void UftSectorEditor::redo()
{
    m_undoStack->redo();
}

void UftSectorEditor::find()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Find"),
        tr("Enter hex bytes (e.g., 4C 00 10):"), QLineEdit::Normal, "", &ok);
    
    if (!ok || text.isEmpty()) return;
    
    QByteArray pattern;
    QStringList parts = text.split(' ', Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        bool ok;
        uint8_t b = p.toUInt(&ok, 16);
        if (ok) pattern.append((char)b);
    }
    
    if (m_hexEdit->find(pattern) < 0) {
        QMessageBox::information(this, tr("Find"), tr("Pattern not found."));
    }
}

void UftSectorEditor::findNext()
{
    if (m_hexEdit->findNext() < 0) {
        QMessageBox::information(this, tr("Find"), tr("No more occurrences."));
    }
}

void UftSectorEditor::replace()
{
    /* Open find/replace dialog for in-place sector editing */
    bool ok;
    QString findText = QInputDialog::getText(this, tr("Find"),
        tr("Enter hex bytes to find (e.g., 4C 00 10):"), QLineEdit::Normal, "", &ok);
    if (!ok || findText.isEmpty()) return;

    QString replText = QInputDialog::getText(this, tr("Replace"),
        tr("Enter hex bytes to replace with:"), QLineEdit::Normal, "", &ok);
    if (!ok || replText.isEmpty()) return;

    /* Parse find pattern */
    QByteArray findPattern, replPattern;
    QStringList findParts = findText.split(' ', Qt::SkipEmptyParts);
    for (const QString &p : findParts) {
        bool ok2;
        uint8_t b = static_cast<uint8_t>(p.toUInt(&ok2, 16));
        if (ok2) findPattern.append(static_cast<char>(b));
    }
    QStringList replParts = replText.split(' ', Qt::SkipEmptyParts);
    for (const QString &p : replParts) {
        bool ok2;
        uint8_t b = static_cast<uint8_t>(p.toUInt(&ok2, 16));
        if (ok2) replPattern.append(static_cast<char>(b));
    }

    if (findPattern.isEmpty() || replPattern.isEmpty()) {
        QMessageBox::warning(this, tr("Replace"), tr("Invalid hex pattern."));
        return;
    }

    /* Search and replace in current sector data */
    QByteArray data = m_hexEdit->data();
    int pos = data.indexOf(findPattern);
    int replCount = 0;

    while (pos >= 0) {
        /* Replace bytes at position */
        for (int i = 0; i < replPattern.size() && pos + i < data.size(); i++) {
            m_hexEdit->setByteAt(pos + i, static_cast<uint8_t>(replPattern[i]));
        }
        replCount++;
        pos = data.indexOf(findPattern, pos + replPattern.size());
    }

    if (replCount > 0) {
        saveSector();
        QMessageBox::information(this, tr("Replace"),
            tr("Replaced %1 occurrence(s).").arg(replCount));
    } else {
        QMessageBox::information(this, tr("Replace"), tr("Pattern not found."));
    }
}

void UftSectorEditor::goTo()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Go To"),
        tr("Enter offset (hex):"), QLineEdit::Normal, "", &ok);
    
    if (!ok || text.isEmpty()) return;
    
    int offset = text.toInt(&ok, 16);
    if (ok) {
        goToOffset(offset);
    }
}

void UftSectorEditor::clear()
{
    m_diskPath.clear();
    m_diskData.clear();
    m_hexEdit->setData(QByteArray());
    m_undoStack->clear();
    m_modified = false;
    m_saveAction->setEnabled(false);
}

/*===========================================================================
 * UftFindReplaceDialog
 *===========================================================================*/

UftFindReplaceDialog::UftFindReplaceDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Find/Replace"));
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    QFormLayout *form = new QFormLayout();
    m_searchEdit = new QLineEdit();
    m_replaceEdit = new QLineEdit();
    form->addRow(tr("Find:"), m_searchEdit);
    form->addRow(tr("Replace:"), m_replaceEdit);
    layout->addLayout(form);
    
    m_hexCheck = new QCheckBox(tr("Hex mode"));
    layout->addWidget(m_hexCheck);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_findBtn = new QPushButton(tr("Find Next"));
    m_replaceBtn = new QPushButton(tr("Replace"));
    m_replaceAllBtn = new QPushButton(tr("Replace All"));
    btnLayout->addWidget(m_findBtn);
    btnLayout->addWidget(m_replaceBtn);
    btnLayout->addWidget(m_replaceAllBtn);
    layout->addLayout(btnLayout);
    
    connect(m_findBtn, &QPushButton::clicked, [this]() {
        emit findRequested(searchPattern());
    });
    connect(m_replaceBtn, &QPushButton::clicked, [this]() {
        emit replaceRequested(searchPattern(), replacePattern());
    });
    connect(m_replaceAllBtn, &QPushButton::clicked, [this]() {
        emit replaceAllRequested(searchPattern(), replacePattern());
    });
}

QByteArray UftFindReplaceDialog::searchPattern() const
{
    if (m_hexCheck->isChecked()) {
        QByteArray result;
        QStringList parts = m_searchEdit->text().split(' ', Qt::SkipEmptyParts);
        for (const QString &p : parts) {
            result.append((char)p.toUInt(nullptr, 16));
        }
        return result;
    }
    return m_searchEdit->text().toLatin1();
}

QByteArray UftFindReplaceDialog::replacePattern() const
{
    if (m_hexCheck->isChecked()) {
        QByteArray result;
        QStringList parts = m_replaceEdit->text().split(' ', Qt::SkipEmptyParts);
        for (const QString &p : parts) {
            result.append((char)p.toUInt(nullptr, 16));
        }
        return result;
    }
    return m_replaceEdit->text().toLatin1();
}

bool UftFindReplaceDialog::isHexMode() const
{
    return m_hexCheck->isChecked();
}
