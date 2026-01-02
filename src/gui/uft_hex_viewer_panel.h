/**
 * @file uft_hex_viewer_panel.h
 * @brief Hex Viewer Panel - Bootloader and Sector Hex View
 */

#ifndef UFT_HEX_VIEWER_PANEL_H
#define UFT_HEX_VIEWER_PANEL_H

#include <QWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QScrollBar>
#include <QFont>

/* Custom Hex View Widget */
class UftHexView : public QWidget
{
    Q_OBJECT

public:
    explicit UftHexView(QWidget *parent = nullptr);

    void setData(const QByteArray &data);
    QByteArray getData() const;
    
    void setOffset(qint64 offset);
    qint64 getOffset() const;
    
    void setSelection(qint64 start, qint64 end);
    void getSelection(qint64 &start, qint64 &end) const;
    
    void setBytesPerLine(int bytes);
    int getBytesPerLine() const;
    
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;

signals:
    void selectionChanged(qint64 start, qint64 end);
    void dataChanged();
    void cursorPositionChanged(qint64 pos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateScrollBar();
    int positionToOffset(const QPoint &pos) const;
    QRect offsetToRect(qint64 offset) const;

    QByteArray m_data;
    qint64 m_offset;
    qint64 m_selectionStart;
    qint64 m_selectionEnd;
    qint64 m_cursorPos;
    int m_bytesPerLine;
    bool m_readOnly;
    QFont m_font;
    QScrollBar *m_scrollBar;
};

/* Hex Viewer Panel */
class UftHexViewerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit UftHexViewerPanel(QWidget *parent = nullptr);

    void loadBootblock(const uint8_t *data, size_t size);
    void loadSector(int track, int side, int sector, const uint8_t *data, size_t size);
    void loadTrack(int track, int side, const uint8_t *data, size_t size);

signals:
    void dataModified();

public slots:
    void onSectorSelected(int track, int side, int sector);
    void onTrackSelected(int track, int side);
    void onSearch();
    void onGotoOffset();
    void onExportSelection();
    void onImportData();

private:
    void setupUi();
    void createNavigationGroup();
    void createSearchGroup();
    void createInfoGroup();
    void createHexView();
    void updateInfo();

    /* Navigation */
    QGroupBox *m_navigationGroup;
    QComboBox *m_viewMode;          /* Bootblock, Sector, Track, Raw */
    QSpinBox *m_trackSelect;
    QComboBox *m_sideSelect;
    QSpinBox *m_sectorSelect;
    QPushButton *m_prevButton;
    QPushButton *m_nextButton;
    QPushButton *m_gotoButton;
    
    /* Search */
    QGroupBox *m_searchGroup;
    QLineEdit *m_searchHex;
    QLineEdit *m_searchAscii;
    QCheckBox *m_caseSensitive;
    QPushButton *m_findNextButton;
    QPushButton *m_findPrevButton;
    QLabel *m_searchResult;
    
    /* Info */
    QGroupBox *m_infoGroup;
    QLabel *m_offsetLabel;
    QLabel *m_sizeLabel;
    QLabel *m_selectionLabel;
    QLabel *m_checksumLabel;
    
    /* Hex View */
    UftHexView *m_hexView;
    QComboBox *m_bytesPerLine;
    QCheckBox *m_showAscii;
    QCheckBox *m_showOffset;
    
    /* Actions */
    QPushButton *m_exportButton;
    QPushButton *m_importButton;
    QPushButton *m_editButton;
    QPushButton *m_saveButton;
    
    /* State */
    int m_currentTrack;
    int m_currentSide;
    int m_currentSector;
    QByteArray m_currentData;
};

#endif /* UFT_HEX_VIEWER_PANEL_H */
