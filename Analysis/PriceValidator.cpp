#include "PriceValidator.h"
#include "StockAnalysis.h"
#include "iostream"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

// Constructor
PriceValidator::PriceValidator(QSqlDatabase& database) : db(database) {}

// Public method to validate and correct trade prices
void PriceValidator::validateAndCorrectPrices() const {
    QSqlQuery query(db);

    // Fetch all trades
    if (!query.exec("SELECT symbol, price FROM trades")) {
        std::cerr << "Error fetching trades: " << query.lastError().text().toStdString() << std::endl;

        return;
    }

    while (query.next()) {
        QString symbol = query.value("symbol").toString();
        double tradePrice = query.value("price").toDouble();

        // Get the latest closing price
        double latestClose = getLatestClosingPrice(symbol);

        // If there's a significant discrepancy, update the trade price
        if (latestClose > 0 && std::abs(tradePrice - latestClose) / latestClose > 0.005)  // 0.5% tolerance
            updateTradePrice(symbol, latestClose);
    }
}

// Public method to double-check all total scores >=1.3 against their historical data then recalculate their scores
void PriceValidator::revalidateSuspiciousScores() const {
    QSqlQuery query(db);

    // Step 1: Identify all stocks with total_score >= 1.3
    if (!query.exec("SELECT symbol, total_score FROM scores WHERE total_score >= 1.3")) {
        std::cerr << "Failed to fetch suspicious scores: " << query.lastError().text().toStdString() << std::endl;

        return;
    }

    while (query.next()) {
        QString symbol = query.value("symbol").toString();
        double totalScore = query.value("total_score").toDouble();

        // Step 2: Fetch the historical data for this stock
        QSqlQuery historicalQuery(db);

        historicalQuery.prepare("SELECT close FROM historical_data WHERE symbol = :symbol ORDER BY timestamp DESC LIMIT 30");
        historicalQuery.bindValue(":symbol", symbol);

        if (!historicalQuery.exec()) {
            std::cerr << "Failed to fetch historical data for " << symbol.toStdString() << ":" << historicalQuery.lastError().text().toStdString() << std::endl;

            continue;
        }

        std::vector<double> priceHistory;

        while (historicalQuery.next())
            priceHistory.push_back(historicalQuery.value("close").toDouble());

        // Ensure we have enough data for meaningful analysis
        if (priceHistory.size() < 10) {
            std::cerr << "Insufficient historical data for " << symbol.toStdString() << std::endl;

            continue;
        }

        // Step 3: Fetch the latest trade price
        QSqlQuery tradeQuery(db);

        tradeQuery.prepare("SELECT price FROM trades WHERE symbol = :symbol LIMIT 1");
        tradeQuery.bindValue(":symbol", symbol);

        if (!tradeQuery.exec() || !tradeQuery.next()) {
            std::cerr << "Failed to fetch trade price for " << symbol.toStdString() << ":" << tradeQuery.lastError().text().toStdString() << std::endl;

            continue;
        }

        double tradePrice = tradeQuery.value("price").toDouble();

        // Step 4: Recalculate the scores using available historical data
        try {
            std::vector<double> recalculatedScores = StockAnalysis::calculateTotalScores(tradePrice, priceHistory, priceHistory.size());
            double recalculatedTotalScore = recalculatedScores[3];

            // Step 5: Compare and update the scores if necessary
            if (std::abs(recalculatedTotalScore - totalScore) > 0.05) { // Threshold for revalidation
                QSqlQuery updateQuery(db);

                updateQuery.prepare("UPDATE scores SET ma_score = :ma_score, rsi_score = :rsi_score, bb_score = :bb_score, total_score = :total_score WHERE symbol = :symbol");
                updateQuery.bindValue(":ma_score", recalculatedScores[0]);
                updateQuery.bindValue(":rsi_score", recalculatedScores[1]);
                updateQuery.bindValue(":bb_score", recalculatedScores[2]);
                updateQuery.bindValue(":total_score", recalculatedScores[3]);
                updateQuery.bindValue(":symbol", symbol);

                if (!updateQuery.exec())
                    std::cerr << "Failed to update scores for " << symbol.toStdString() << ":" << updateQuery.lastError().text().toStdString() << std::endl;
                else
                    std::cout << "Revalidated and updated scores for " << symbol.toStdString() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error recalculating scores for " << symbol.toStdString() << ": " << e.what() << std::endl;
        }
    }
}

// Private helper method to fetch the latest closing price for a stock
double PriceValidator::getLatestClosingPrice(const QString& symbol) const {
    QSqlQuery query(db);

    query.prepare("SELECT close FROM historical_data WHERE symbol = :symbol ORDER BY timestamp DESC LIMIT 1");
    query.bindValue(":symbol", symbol);

    if (!query.exec()) {
        std::cerr << "Error fetching latest closing price for " << symbol.toStdString() << ": " << query.lastError().text().toStdString() << std::endl;

        return -1;
    }

    if (query.next())
        return query.value("close").toDouble();

    return -1; // No data found
}

// Private helper method to update a trade price in the trades table
void PriceValidator::updateTradePrice(const QString& symbol, double correctedPrice) const {
    QSqlQuery query(db);

    query.prepare("UPDATE trades SET price = :price WHERE symbol = :symbol");
    query.bindValue(":price", correctedPrice);
    query.bindValue(":symbol", symbol);

    if (!query.exec())
        std::cerr << "Error updating trade price for " << symbol.toStdString() << ": " << query.lastError().text().toStdString() << std::endl;
    else
        std::cout << "Updated trade price for " << symbol.toStdString() << " to " << correctedPrice << std::endl;
}
