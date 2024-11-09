#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Analysis/StockAnalysis.h"

#include <QMainWindow>
#include <QStringList>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSearchButtonClicked();
    void displayStocks(const QStringList& stockInfoList);

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
