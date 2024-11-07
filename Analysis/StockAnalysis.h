#ifndef STOCK_ANALYSIS_H
#define STOCK_ANALYSIS_H

#include <vector>

class StockAnalysis {
public:
    static double calculateMovingAverage(const std::vector<double>& prices, int period);
    static double calculateRSI(const std::vector<double>& prices, int period = 14);
    static std::pair<double, double> calculateBollingerBands(const std::vector<double>& prices, int period = 20, double numStdDev = 2.0);
};

#endif // STOCK_ANALYSIS_H
