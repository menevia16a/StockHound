#include "StockAnalysis.h"

#include <numeric>
#include <cmath>
#include <stdexcept>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

// Constructor implementation
StockAnalysis::StockAnalysis(QSqlDatabase& database)
    : db(database) {}

std::pair<double, double> StockAnalysis::calculateBollingerBands(const std::vector<double>& prices, int period, double numStdDev) {
    // If not enough data, mark symbol as excluded to prevent it from being rendered.
    if (prices.size() < static_cast<size_t>((period - 10)))
        return { static_cast<double>(-1), static_cast<double>(-1)}; // Return invalid to the calling function

    // Compute moving average over the last period points
    double sum = std::accumulate(prices.end() - period, prices.end(), 0.0);
    double movingAverage = sum / period;

    // Compute (population) std-dev
    double sumSquares = 0.0;

    for (auto it = prices.end() - period; it != prices.end(); ++it) {
        double diff = *it - movingAverage;
        sumSquares += diff * diff;
    }

    double stdDev = std::sqrt(sumSquares / period);
    double upperBand = movingAverage + numStdDev * stdDev;
    double lowerBand = movingAverage - numStdDev * stdDev;

    return { upperBand, lowerBand };
}

double StockAnalysis::calculateMovingAverage(const std::vector<double>& prices, int period) {
    // If not enough data, mark symbol as excluded to prevent it from being rendered.
    if (prices.size() < static_cast<size_t>((period - 10)))
        return static_cast<double>(-1); // Return invalid to the calling function

    double sum = std::accumulate(prices.end() - period, prices.end(), 0.0);

    return sum / period;
}

double StockAnalysis::calculateRSI(const std::vector<double>& prices, int period) {
    // If not enough data, mark symbol as excluded to prevent it from being rendered.
    if (prices.size() < static_cast<size_t>((period - 10)))
        return static_cast<double>(-1); // Return invalid to the calling function

    double gain = 0.0, loss = 0.0;

    for (size_t i = prices.size() - period; i < prices.size() - 1; ++i) {
        double change = prices[i + 1] - prices[i];

        if (change > 0)
            gain += change;
        else
            loss -= change;
    }

    double avgGain = gain / period;
    double avgLoss = loss / period;

    if (avgLoss == 0)
        return 100.0;

    double rs = avgGain / avgLoss;

    return 100.0 - (100.0 / (1.0 + rs));
}

double StockAnalysis::calculateMAScore(double price, double movingAverage) {
    return 1.0 - std::abs(price - movingAverage) / movingAverage;
}

double StockAnalysis::calculateRSIScore(double rsi) {
    return 1.0 - std::abs(rsi - 30) / (70 - 30); // Normalizes RSI score with a range from 30 to 70
}

double StockAnalysis::calculateBBScore(double price, double lowerBand, double upperBand) {
    return 1.0 - (price - lowerBand) / (upperBand - lowerBand);
}

std::vector<double> StockAnalysis::calculateTotalScores(std::string symbol, double price, const std::vector<double>& prices, int period) {
    // Calculate indicators
    double movingAverage = calculateMovingAverage(prices, period);
    double rsi = calculateRSI(prices, period);
    auto [upperBand, lowerBand] = calculateBollingerBands(prices, period);

    // If any scores are invalid, mark as excluded
    if (movingAverage == static_cast<double>(-1) ||
        rsi == static_cast<double>(-1) ||
        (upperBand == static_cast<double>(-1) && lowerBand == static_cast<double>(-1))) {
        QSqlQuery markExcludedQuery(db);

        markExcludedQuery.prepare("UPDATE stocks SET excluded = 1 WHERE symbol = :symbol");
        markExcludedQuery.bindValue(":symbol", QVariant(QString::fromStdString(symbol)));

        if (!markExcludedQuery.exec())
            QMessageBox::critical(nullptr, "Database Error", "Query execution failed:" + markExcludedQuery.lastError().text());

        return {}; // Return empty vector
    }

    // Calculate individual scores
    double maScore = calculateMAScore(price, movingAverage) * 0.4;
    double rsiScore = calculateRSIScore(rsi) * 0.3;
    double bbScore = calculateBBScore(price, lowerBand, upperBand) * 0.3;

    // Calculate total weighted score
    double totalScore = maScore + rsiScore + bbScore;

    // Return all values
    std::vector<double> scores = { maScore, rsiScore, bbScore, totalScore };

    return scores;
}

