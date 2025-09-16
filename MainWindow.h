#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Analysis/StockAnalysis.h"

#include <QSqlDatabase>
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
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onSearchButtonClicked();

private:
    Ui::MainWindow* ui;

    QSqlDatabase db;

    struct StockInformation {
        std::string Name;
        double Price;
        double MA_Score;
        double RSI_Score;
        double BB_Score;
        double Total_Score;
    };

    const std::string exchange = "NYSE";
    const std::string userAgent = "StockHound/1.0";

    // Map to store all stock information
    std::unordered_map<std::string, StockInformation> stockInfoMap;

    void refreshStockList();
    void addRowToTable(const QString& name, const QString& ticker, double price, double ma_score, double rsi_score, double bb_score, double total_score);
    void updateScoresDatabase(const std::string symbol, const std::vector<double> scores);
    void excludeSuspiciousScores();
};

#endif // MAINWINDOW_H
