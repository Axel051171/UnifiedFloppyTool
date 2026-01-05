#ifndef NIBBLETAB_H
#define NIBBLETAB_H

#include <QWidget>

namespace Ui {
class TabNibble;
}

class NibbleTab : public QWidget
{
    Q_OBJECT

public:
    explicit NibbleTab(QWidget *parent = nullptr);
    ~NibbleTab();

private:
    Ui::TabNibble *ui;
};

#endif // NIBBLETAB_H
