/**
 * @file LogViewerWidget.cpp
 * @brief Qt Widget for displaying UFT log entries - Implementation
 */

#include "LogViewerWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

LogViewerWidget::LogViewerWidget(QWidget *parent)
    : QWidget(parent)
    , m_categoryMask(LOG_DEFAULT)
    , m_minLevel(LOG_LEVEL_INFO)
    , m_autoScroll(true)
    , m_needsUpdate(false)
{
    setupUi();
    
    /* Update timer for batch updates */
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(100);
    connect(m_updateTimer, &QTimer::timeout, this, &LogViewerWidget::updateDisplay);
    m_updateTimer->start();
}

LogViewerWidget::~LogViewerWidget() {
    m_updateTimer->stop();
}

void LogViewerWidget::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    
    /* Top toolbar */
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    
    /* Category checkboxes */
    m_chkDevice = new QCheckBox("Device", this);
    m_chkRead = new QCheckBox("Read", this);
    m_chkCell = new QCheckBox("Cell", this);
    m_chkFormat = new QCheckBox("Format", this);
    m_chkWrite = new QCheckBox("Write", this);
    m_chkVerify = new QCheckBox("Verify", this);
    m_chkDebug = new QCheckBox("Debug", this);
    m_chkTrace = new QCheckBox("Trace", this);
    
    /* Set initial state */
    m_chkDevice->setChecked(m_categoryMask & LOG_DEVICE);
    m_chkRead->setChecked(m_categoryMask & LOG_READ);
    m_chkCell->setChecked(m_categoryMask & LOG_CELL);
    m_chkFormat->setChecked(m_categoryMask & LOG_FORMAT);
    m_chkWrite->setChecked(m_categoryMask & LOG_WRITE);
    m_chkVerify->setChecked(m_categoryMask & LOG_VERIFY);
    m_chkDebug->setChecked(m_categoryMask & LOG_DEBUG);
    m_chkTrace->setChecked(m_categoryMask & LOG_TRACE);
    
    toolbarLayout->addWidget(m_chkDevice);
    toolbarLayout->addWidget(m_chkRead);
    toolbarLayout->addWidget(m_chkCell);
    toolbarLayout->addWidget(m_chkFormat);
    toolbarLayout->addWidget(m_chkWrite);
    toolbarLayout->addWidget(m_chkVerify);
    toolbarLayout->addWidget(m_chkDebug);
    toolbarLayout->addWidget(m_chkTrace);
    
    /* Connect category checkboxes */
    connect(m_chkDevice, &QCheckBox::toggled, this, &LogViewerWidget::onCategoryFilterChanged);
    connect(m_chkRead, &QCheckBox::toggled, this, &LogViewerWidget::onCategoryFilterChanged);
    connect(m_chkCell, &QCheckBox::toggled, this, &LogViewerWidget::onCategoryFilterChanged);
    connect(m_chkFormat, &QCheckBox::toggled, this, &LogViewerWidget::onCategoryFilterChanged);
    connect(m_chkWrite, &QCheckBox::toggled, this, &LogViewerWidget::onCategoryFilterChanged);
    connect(m_chkVerify, &QCheckBox::toggled, this, &LogViewerWidget::onCategoryFilterChanged);
    connect(m_chkDebug, &QCheckBox::toggled, this, &LogViewerWidget::onCategoryFilterChanged);
    connect(m_chkTrace, &QCheckBox::toggled, this, &LogViewerWidget::onCategoryFilterChanged);
    
    toolbarLayout->addStretch();
    
    /* Level filter */
    m_cmbLevel = new QComboBox(this);
    m_cmbLevel->addItem("Error", LOG_LEVEL_ERROR);
    m_cmbLevel->addItem("Warning", LOG_LEVEL_WARNING);
    m_cmbLevel->addItem("Info", LOG_LEVEL_INFO);
    m_cmbLevel->addItem("Debug", LOG_LEVEL_DEBUG);
    m_cmbLevel->addItem("Trace", LOG_LEVEL_TRACE);
    m_cmbLevel->setCurrentIndex(m_minLevel);
    connect(m_cmbLevel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LogViewerWidget::onLevelFilterChanged);
    toolbarLayout->addWidget(m_cmbLevel);
    
    mainLayout->addLayout(toolbarLayout);
    
    /* Search bar */
    QHBoxLayout *searchLayout = new QHBoxLayout();
    
    m_txtSearch = new QLineEdit(this);
    m_txtSearch->setPlaceholderText("Search...");
    connect(m_txtSearch, &QLineEdit::textChanged, this, &LogViewerWidget::onSearchTextChanged);
    searchLayout->addWidget(m_txtSearch);
    
    m_chkAutoScroll = new QCheckBox("Auto-scroll", this);
    m_chkAutoScroll->setChecked(m_autoScroll);
    connect(m_chkAutoScroll, &QCheckBox::toggled, this, &LogViewerWidget::setAutoScroll);
    searchLayout->addWidget(m_chkAutoScroll);
    
    m_btnScrollBottom = new QPushButton("↓", this);
    m_btnScrollBottom->setFixedWidth(30);
    m_btnScrollBottom->setToolTip("Scroll to bottom");
    connect(m_btnScrollBottom, &QPushButton::clicked, this, &LogViewerWidget::scrollToBottom);
    searchLayout->addWidget(m_btnScrollBottom);
    
    m_btnClear = new QPushButton("Clear", this);
    connect(m_btnClear, &QPushButton::clicked, this, &LogViewerWidget::onClearClicked);
    searchLayout->addWidget(m_btnClear);
    
    m_btnExport = new QPushButton("Export...", this);
    connect(m_btnExport, &QPushButton::clicked, this, &LogViewerWidget::onExportClicked);
    searchLayout->addWidget(m_btnExport);
    
    mainLayout->addLayout(searchLayout);
    
    /* Table view */
    m_tableView = new QTableView(this);
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({"Time", "Category", "Level", "Message"});
    m_tableView->setModel(m_model);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    connect(m_tableView, &QTableView::doubleClicked, this, &LogViewerWidget::onTableDoubleClicked);
    
    mainLayout->addWidget(m_tableView);
}

void LogViewerWidget::addEntry(const GuiLogEntry &entry) {
    QMutexLocker locker(&m_mutex);
    m_allEntries.append(entry);
    m_needsUpdate = true;
}

void LogViewerWidget::clear() {
    QMutexLocker locker(&m_mutex);
    m_allEntries.clear();
    m_model->removeRows(0, m_model->rowCount());
}

void LogViewerWidget::updateDisplay() {
    if (!m_needsUpdate) return;
    
    QMutexLocker locker(&m_mutex);
    m_needsUpdate = false;
    
    /* Rebuild visible entries */
    applyFilters();
    
    if (m_autoScroll) {
        scrollToBottom();
    }
}

void LogViewerWidget::applyFilters() {
    m_model->removeRows(0, m_model->rowCount());
    
    for (const GuiLogEntry &entry : m_allEntries) {
        /* Category filter */
        if (!(entry.category & m_categoryMask)) continue;
        
        /* Level filter */
        if (entry.level > m_minLevel) continue;
        
        /* Text filter */
        if (!m_textFilter.isEmpty()) {
            if (!entry.message.contains(m_textFilter, Qt::CaseInsensitive)) {
                continue;
            }
        }
        
        /* Add to model */
        int row = m_model->rowCount();
        m_model->insertRow(row);
        
        /* Time */
        double sec = entry.timestampUs / 1000000.0;
        QStandardItem *timeItem = new QStandardItem(QString::number(sec, 'f', 3));
        m_model->setItem(row, 0, timeItem);
        
        /* Category */
        QStandardItem *catItem = new QStandardItem(categoryToString(entry.category));
        m_model->setItem(row, 1, catItem);
        
        /* Level */
        QStandardItem *levelItem = new QStandardItem(levelToString(entry.level));
        QColor color = levelToColor(entry.level);
        levelItem->setForeground(color);
        m_model->setItem(row, 2, levelItem);
        
        /* Message */
        QStandardItem *msgItem = new QStandardItem(entry.message);
        msgItem->setForeground(color);
        m_model->setItem(row, 3, msgItem);
    }
}

quint32 LogViewerWidget::getFilterMask() const {
    return m_categoryMask;
}

void LogViewerWidget::setFilterMask(quint32 mask) {
    m_categoryMask = mask;
    
    m_chkDevice->setChecked(mask & LOG_DEVICE);
    m_chkRead->setChecked(mask & LOG_READ);
    m_chkCell->setChecked(mask & LOG_CELL);
    m_chkFormat->setChecked(mask & LOG_FORMAT);
    m_chkWrite->setChecked(mask & LOG_WRITE);
    m_chkVerify->setChecked(mask & LOG_VERIFY);
    m_chkDebug->setChecked(mask & LOG_DEBUG);
    m_chkTrace->setChecked(mask & LOG_TRACE);
    
    m_needsUpdate = true;
}

void LogViewerWidget::setMinLevel(int level) {
    m_minLevel = level;
    m_cmbLevel->setCurrentIndex(level);
    m_needsUpdate = true;
}

void LogViewerWidget::setAutoScroll(bool enabled) {
    m_autoScroll = enabled;
    m_chkAutoScroll->setChecked(enabled);
}

int LogViewerWidget::visibleEntryCount() const {
    return m_model->rowCount();
}

int LogViewerWidget::totalEntryCount() const {
    return m_allEntries.size();
}

void LogViewerWidget::onCategoryFilterChanged() {
    m_categoryMask = 0;
    if (m_chkDevice->isChecked()) m_categoryMask |= LOG_DEVICE;
    if (m_chkRead->isChecked()) m_categoryMask |= LOG_READ;
    if (m_chkCell->isChecked()) m_categoryMask |= LOG_CELL;
    if (m_chkFormat->isChecked()) m_categoryMask |= LOG_FORMAT;
    if (m_chkWrite->isChecked()) m_categoryMask |= LOG_WRITE;
    if (m_chkVerify->isChecked()) m_categoryMask |= LOG_VERIFY;
    if (m_chkDebug->isChecked()) m_categoryMask |= LOG_DEBUG;
    if (m_chkTrace->isChecked()) m_categoryMask |= LOG_TRACE;
    
    m_needsUpdate = true;
    emit filterChanged(m_categoryMask, m_minLevel);
}

void LogViewerWidget::onLevelFilterChanged(int index) {
    m_minLevel = index;
    m_needsUpdate = true;
    emit filterChanged(m_categoryMask, m_minLevel);
}

void LogViewerWidget::onSearchTextChanged(const QString &text) {
    m_textFilter = text;
    m_needsUpdate = true;
}

void LogViewerWidget::setTextFilter(const QString &text) {
    m_txtSearch->setText(text);
}

void LogViewerWidget::scrollToBottom() {
    m_tableView->scrollToBottom();
}

void LogViewerWidget::scrollToTop() {
    m_tableView->scrollToTop();
}

void LogViewerWidget::onExportClicked() {
    QString filename = QFileDialog::getSaveFileName(
        this, "Export Log",
        QString(),
        "Text files (*.txt);;JSON files (*.json);;HTML files (*.html)"
    );
    
    if (filename.isEmpty()) return;
    
    if (filename.endsWith(".json", Qt::CaseInsensitive)) {
        exportToJson(filename);
    } else if (filename.endsWith(".html", Qt::CaseInsensitive)) {
        exportToHtml(filename);
    } else {
        exportToFile(filename);
    }
}

void LogViewerWidget::onClearClicked() {
    clear();
}

void LogViewerWidget::onTableDoubleClicked(const QModelIndex &index) {
    int row = index.row();
    if (row >= 0 && row < m_allEntries.size()) {
        emit entryDoubleClicked(m_allEntries[row]);
    }
}

void LogViewerWidget::exportToFile(const QString &filename) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    
    QTextStream out(&file);
    for (const GuiLogEntry &entry : m_allEntries) {
        out << QString::number(entry.timestampUs / 1000000.0, 'f', 3) << "\t"
            << categoryToString(entry.category) << "\t"
            << levelToString(entry.level) << "\t"
            << entry.message << "\n";
    }
}

void LogViewerWidget::exportToJson(const QString &filename) {
    QJsonArray entries;
    for (const GuiLogEntry &entry : m_allEntries) {
        QJsonObject obj;
        obj["timestamp_us"] = (qint64)entry.timestampUs;
        obj["category"] = categoryToString(entry.category);
        obj["level"] = levelToString(entry.level);
        obj["message"] = entry.message;
        entries.append(obj);
    }
    
    QJsonDocument doc;
    QJsonObject root;
    root["log_entries"] = entries;
    doc.setObject(root);
    
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Indented));
    }
}

void LogViewerWidget::exportToHtml(const QString &filename) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    
    QTextStream out(&file);
    out << "<!DOCTYPE html>\n<html><head>\n";
    out << "<title>UFT Log</title>\n";
    out << "<style>\n";
    out << "body { font-family: monospace; background: #1e1e1e; color: #d4d4d4; }\n";
    out << ".error { color: #f44747; }\n";
    out << ".warning { color: #cca700; }\n";
    out << ".info { color: #4ec9b0; }\n";
    out << ".debug { color: #569cd6; }\n";
    out << ".trace { color: #808080; }\n";
    out << "table { border-collapse: collapse; width: 100%; }\n";
    out << "td, th { padding: 4px 8px; border-bottom: 1px solid #333; }\n";
    out << "</style></head><body>\n";
    out << "<h1>UFT Log Export</h1>\n";
    out << "<table><tr><th>Time</th><th>Category</th><th>Level</th><th>Message</th></tr>\n";
    
    for (const GuiLogEntry &entry : m_allEntries) {
        QString cssClass;
        switch (entry.level) {
            case LOG_LEVEL_ERROR: cssClass = "error"; break;
            case LOG_LEVEL_WARNING: cssClass = "warning"; break;
            case LOG_LEVEL_INFO: cssClass = "info"; break;
            case LOG_LEVEL_DEBUG: cssClass = "debug"; break;
            case LOG_LEVEL_TRACE: cssClass = "trace"; break;
        }
        
        out << QString("<tr class=\"%1\"><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>\n")
               .arg(cssClass)
               .arg(QString::number(entry.timestampUs / 1000000.0, 'f', 3))
               .arg(categoryToString(entry.category))
               .arg(levelToString(entry.level))
               .arg(entry.message.toHtmlEscaped());
    }
    
    out << "</table></body></html>\n";
}

QColor LogViewerWidget::levelToColor(int level) const {
    switch (level) {
        case LOG_LEVEL_ERROR:   return QColor(244, 71, 71);   /* Red */
        case LOG_LEVEL_WARNING: return QColor(204, 167, 0);   /* Yellow */
        case LOG_LEVEL_INFO:    return QColor(78, 201, 176);  /* Teal */
        case LOG_LEVEL_DEBUG:   return QColor(86, 156, 214);  /* Blue */
        case LOG_LEVEL_TRACE:   return QColor(128, 128, 128); /* Gray */
        default: return QColor(212, 212, 212);
    }
}

QString LogViewerWidget::categoryToString(quint32 category) const {
    switch (category) {
        case LOG_DEVICE: return "DEVICE";
        case LOG_READ:   return "READ";
        case LOG_CELL:   return "CELL";
        case LOG_FORMAT: return "FORMAT";
        case LOG_WRITE:  return "WRITE";
        case LOG_VERIFY: return "VERIFY";
        case LOG_DEBUG:  return "DEBUG";
        case LOG_TRACE:  return "TRACE";
        default: return "?";
    }
}

QString LogViewerWidget::levelToString(int level) const {
    switch (level) {
        case LOG_LEVEL_ERROR:   return "ERROR";
        case LOG_LEVEL_WARNING: return "WARN";
        case LOG_LEVEL_INFO:    return "INFO";
        case LOG_LEVEL_DEBUG:   return "DEBUG";
        case LOG_LEVEL_TRACE:   return "TRACE";
        default: return "?";
    }
}
