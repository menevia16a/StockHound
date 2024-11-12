#include "StockAnalysis.h"

#include <numeric>
#include <cmath>
#include <stdexcept>

std::pair<double, double> StockAnalysis::calculateBollingerBands(const std::vector<double>& prices, int period, double numStdDev) {
    if (prices.size() < period) {
        throw std::invalid_argument("Not enough data to calculate Bollinger Bands");
    }

    double movingAverage = calculateMovingAverage(prices, period);
    double sumSquares = 0.0;

    for (size_t i = prices.size() - period; i < prices.size(); ++i) {
        double diff = prices[i] - movingAverage;
        sumSquares += diff * diff;
    }

    double stdDev = std::sqrt(sumSquares / period);
    double upperBand = movingAverage + numStdDev * stdDev;
    double lowerBand = movingAverage - numStdDev * stdDev;

    return {upperBand, lowerBand};
}

double StockAnalysis::calculateMovingAverage(const std::vector<double>& prices, int period) {
    if (prices.size() < period) {
        throw std::invalid_argument("Not enough data to calculate Moving Average");
    }

    double sum = std::accumulate(prices.end() - period, prices.end(), 0.0);

    return sum / period;
}

double StockAnalysis::calculateRSI(const std::vector<double>& prices, int period) {
    if (prices.size() <= period) {
        throw std::invalid_argument("Not enough data to calculate RSI");
    }

    double gain = 0.0, loss = 0.0;

    for (size_t i = prices.size() - period; i < prices.size() - 1; ++i) {
        double change = prices[i + 1] - prices[i];
        if (change > 0) {
            gain += change;
        } else {
            loss -= change;
        }
    }

    double avgGain = gain / period;
    double avgLoss = loss / period;

    if (avgLoss == 0) return 100.0;

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

std::vector<double> StockAnalysis::calculateTotalScores(double price, const std::vector<double>& prices, int period) {
    // Calculate indicators
    double movingAverage = calculateMovingAverage(prices, period);
    double rsi = calculateRSI(prices, period);
    auto [upperBand, lowerBand] = calculateBollingerBands(prices, period);

    // Calculate individual scores
    double maScore = calculateMAScore(price, movingAverage) * 0.4;
    double rsiScore = calculateRSIScore(rsi) * 0.3;
    double bbScore = calculateBBScore(price, lowerBand, upperBand) * 0.3;

    // Calculate total weighted score
    double totalScore = maScore + rsiScore + bbScore;

    // Return all values
    std::vector<double> scores = {maScore, rsiScore, bbScore, totalScore};
    return scores;
}

