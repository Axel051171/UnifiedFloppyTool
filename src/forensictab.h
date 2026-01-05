#ifndef FORENSICTAB_H
#define FORENSICTAB_H

#include <QWidget>

namespace Ui {
class TabForensic;
}

class ForensicTab : public QWidget
{
    Q_OBJECT

public:
    explicit ForensicTab(QWidget *parent = nullptr);
    ~ForensicTab();

private slots:
    void onRunAnalysis();
    void onCompare();
    void onExportReport();

private:
    Ui::TabForensic *ui;
};

#endif // FORENSICTAB_H
