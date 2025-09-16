#ifndef STOCK_ANALYSIS_H
#define STOCK_ANALYSIS_H

#include <vector>
#include <QString>
#include <QSqlDatabase>

class StockAnalysis {
private:
    QSqlDatabase& db;

    static std::pair<double, double> calculateBollingerBands(const std::vector<double>& prices, int period = 20, double numStdDev = 2.0);
    static double calculateMovingAverage(const std::vector<double>& prices, int period);
    static double calculateRSI(const std::vector<double>& prices, int period = 14);
    static double calculateMAScore(double price, double movingAverage);
    static double calculateRSIScore(double rsi);
    static double calculateBBScore(double price, double lowerBand, double upperBand);

public:
    // Constructor
    explicit StockAnalysis(QSqlDatabase& database);

    std::vector<double> calculateTotalScores(std::string symbol, double price, const std::vector<double>& prices, int period);
};

#endif // STOCK_ANALYSIS_H
