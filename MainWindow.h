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

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSearchButtonClicked();

private:
    Ui::MainWindow *ui;

    const std::string exchange = "NYSE";
    const std::string userAgent = "StockHound/1.0";

    void addRowToTable(const QString& name, const QString& ticker, double price, double ma_score, double rsi_score, double bb_score, double total_score);

    struct StockInformation {
        std::string Name;
        double Price;
        double MA_Score;
        double RSI_Score;
        double BB_Score;
        double Total_Score;
    };
};

#endif // MAINWINDOW_H
