/**
 * @file uft_sector_editor.h
 * @brief Sector Editor (BONUS-GUI-002)
 * 
 * Hex editor for disk sectors with undo/redo support.
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_SECTOR_EDITOR_H
#define UFT_SECTOR_EDITOR_H

#include <QWidget>
#include <QScrollArea>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QToolBar>
#include <QUndoStack>
#include <QLineEdit>

/*===========================================================================
 * Hex Editor Widget
 *===========================================================================*/

/**
 * @brief Hex editor core widget
 */
class UftHexEdit : public QWidget
{
    Q_OBJECT
    
public:
    explicit UftHexEdit(QWidget *parent = nullptr);
    
    void setData(const QByteArray &data);
    QByteArray data() const { return m_data; }
    
    void setReadOnly(bool readonly);
    bool isReadOnly() const { return m_readOnly; }
    
    void setBytesPerRow(int count);
    int bytesPerRow() const { return m_bytesPerRow; }
    
    void setCursorPosition(int pos);
    int cursorPosition() const { return m_cursorPos; }
    
    void setSelection(int start, int end);
    QByteArray selectedData() const;
    
    /* Edit operations */
    void setByteAt(int pos, uint8_t value);
    uint8_t byteAt(int pos) const;
    
    /* Search */
    int find(const QByteArray &pattern, int startPos = 0);
    int findNext();
    int findPrev();

signals:
    void dataChanged();
    void cursorPositionChanged(int pos);
    void selectionChanged(int start, int end);
    void byteEdited(int pos, uint8_t oldValue, uint8_t newValue);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    
    QSize sizeHint() const override;

private:
    int posFromPoint(const QPoint &pt) const;
    QPoint pointFromPos(int pos) const;
    void ensureVisible(int pos);
    void updateMetrics();
    
    QByteArray m_data;
    bool m_readOnly;
    int m_bytesPerRow;
    
    int m_cursorPos;
    int m_selStart;
    int m_selEnd;
    bool m_lowNibble;  /* Editing low nibble? */
    
    /* Display metrics */
    int m_charWidth;
    int m_charHeight;
    int m_addressWidth;
    int m_hexWidth;
    int m_asciiWidth;
    int m_scrollOffset;
    
    /* Search */
    QByteArray m_searchPattern;
    int m_lastSearchPos;
};

/*===========================================================================
 * Edit Command (for Undo/Redo)
 *===========================================================================*/

class UftHexEditCommand : public QUndoCommand
{
public:
    UftHexEditCommand(UftHexEdit *editor, int pos, uint8_t oldVal, uint8_t newVal,
                      QUndoCommand *parent = nullptr);
    
    void undo() override;
    void redo() override;
    int id() const override { return 1; }
    bool mergeWith(const QUndoCommand *other) override;

private:
    UftHexEdit *m_editor;
    int m_pos;
    uint8_t m_oldValue;
    uint8_t m_newValue;
};

/*===========================================================================
 * Sector Editor Widget
 *===========================================================================*/

/**
 * @brief Sector Editor Main Widget
 */
class UftSectorEditor : public QWidget
{
    Q_OBJECT
    
public:
    explicit UftSectorEditor(QWidget *parent = nullptr);
    
    void loadDisk(const QString &path);
    void clear();
    
    bool isModified() const { return m_modified; }
    QString diskPath() const { return m_diskPath; }

signals:
    void sectorChanged(int track, int sector);
    void diskModified();

public slots:
    void goToSector(int track, int sector);
    void goToOffset(int offset);
    
    void save();
    void saveAs();
    void revert();
    
    void undo();
    void redo();
    
    void find();
    void findNext();
    void replace();
    void goTo();

private slots:
    void onTrackChanged(int track);
    void onSectorChanged(int sector);
    void onByteEdited(int pos, uint8_t oldVal, uint8_t newVal);
    void onCursorMoved(int pos);
    void updateSectorInfo();

private:
    void setupUi();
    void createToolbar();
    void createNavigationPanel();
    void createInfoPanel();
    
    void loadSector(int track, int sector);
    void saveSector();
    int sectorOffset(int track, int sector) const;
    int sectorsPerTrack(int track) const;
    
    /* Toolbar */
    QToolBar *m_toolbar;
    QAction *m_saveAction;
    QAction *m_undoAction;
    QAction *m_redoAction;
    QAction *m_findAction;
    QAction *m_goToAction;
    
    /* Navigation */
    QGroupBox *m_navGroup;
    QSpinBox *m_trackSpin;
    QSpinBox *m_sectorSpin;
    QPushButton *m_prevButton;
    QPushButton *m_nextButton;
    QComboBox *m_formatCombo;
    
    /* Editor */
    UftHexEdit *m_hexEdit;
    QUndoStack *m_undoStack;
    
    /* Info panel */
    QGroupBox *m_infoGroup;
    QLabel *m_offsetLabel;
    QLabel *m_valueLabel;
    QLabel *m_checksumLabel;
    QLabel *m_encodingLabel;
    
    /* Data */
    QString m_diskPath;
    QByteArray m_diskData;
    int m_currentTrack;
    int m_currentSector;
    int m_sectorSize;
    int m_totalTracks;
    bool m_modified;
    
    /* Format info */
    QString m_format;
};

/*===========================================================================
 * Find/Replace Dialog
 *===========================================================================*/

class UftFindReplaceDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit UftFindReplaceDialog(QWidget *parent = nullptr);
    
    QByteArray searchPattern() const;
    QByteArray replacePattern() const;
    bool isHexMode() const;

signals:
    void findRequested(const QByteArray &pattern);
    void replaceRequested(const QByteArray &search, const QByteArray &replace);
    void replaceAllRequested(const QByteArray &search, const QByteArray &replace);

private:
    QLineEdit *m_searchEdit;
    QLineEdit *m_replaceEdit;
    QCheckBox *m_hexCheck;
    QPushButton *m_findBtn;
    QPushButton *m_replaceBtn;
    QPushButton *m_replaceAllBtn;
};

#endif /* UFT_SECTOR_EDITOR_H */
