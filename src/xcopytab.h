#ifndef XCOPYTAB_H
#define XCOPYTAB_H

#include <QWidget>

namespace Ui {
class TabXCopy;
}

class XCopyTab : public QWidget
{
    Q_OBJECT

public:
    explicit XCopyTab(QWidget *parent = nullptr);
    ~XCopyTab();

private slots:
    void onBrowseSource();
    void onBrowseDest();
    void onStartCopy();
    void onStopCopy();

private:
    Ui::TabXCopy *ui;
};

#endif // XCOPYTAB_H
