#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Analysis/PriceValidator.h"
#include "ThirdParty/alpaca-trade-api-cpp/alpaca/client.h"
#include "ThirdParty/alpaca-trade-api-cpp/alpaca/config.h"

#include <nlohmann/json.hpp>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDateTime>
#include <iostream>
#include <QMessageBox>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QDateTime>
#include <QStandardItemModel>
#include <QStandardItem>
#include <unordered_map>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::onSearchButtonClicked);

    // Create model for stocks table view
    QStandardItemModel* model = new QStandardItemModel(this);

    // Setup the table
    ui->stockList->setModel(model);
    model->setColumnCount(7);
    model->setHeaderData(0, Qt::Horizontal, "Name");
    model->setHeaderData(1, Qt::Horizontal, "Ticker");
    model->setHeaderData(2, Qt::Horizontal, "Price");
    model->setHeaderData(3, Qt::Horizontal, "MA Score");
    model->setHeaderData(4, Qt::Horizontal, "RSI Score");
    model->setHeaderData(5, Qt::Horizontal, "BB Score");
    model->setHeaderData(6, Qt::Horizontal, "Total Score");
    ui->stockList->setColumnWidth(0, 178);

    // Set up SQLite database connection
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("cache.db");

    if (!db.open()) {
        QMessageBox::critical(this, "Database Error", "Failed to open the SQLite database.");

        return;
    }

    // Create table for caching if it doesn't exist
    QSqlQuery createStocksDatabaseQuery;
    QSqlQuery createTradesDatabaseQuery;
    QSqlQuery createHistoricalDataDatabaseQuery;
    QSqlQuery createScoresDatabaseQuery;

    if (!createStocksDatabaseQuery.exec("CREATE TABLE IF NOT EXISTS stocks ("
                                        "id TEXT PRIMARY KEY, "         // Unique Asset ID (Defined by Alpaca)
                                        "name TEXT, "                   // Company name
                                        "symbol TEXT, "                 // Stock symbol
                                        "last_updated INTEGER)")) {     // Unix timestamp
        QMessageBox::critical(this, "Database Error", "Query execution failed:" + createStocksDatabaseQuery.lastError().text());

        return;
    }

    if (!createTradesDatabaseQuery.exec("CREATE TABLE IF NOT EXISTS trades ("
                                        "symbol TEXT PRIMARY KEY, "     // Stock symbol
                                        "price REAL, "                  // Last Trade Price
                                        "size INTEGER)")) {             // Size of Trade
        QMessageBox::critical(this, "Database Error", "Query execution failed:" + createTradesDatabaseQuery.lastError().text());

        return;
    }

    if (!createHistoricalDataDatabaseQuery.exec("CREATE TABLE IF NOT EXISTS historical_data ("
                                                "id INTEGER PRIMARY KEY AUTOINCREMENT, "  // Unique ID for each record
                                                "symbol TEXT NOT NULL, "                  // Stock symbol
                                                "timestamp TEXT NOT NULL, "               // Timestamp of the data point
                                                "open REAL NOT NULL, "                    // Open price
                                                "high REAL NOT NULL, "                    // High price
                                                "low REAL NOT NULL, "                     // Low price
                                                "close REAL NOT NULL, "                   // Close price
                                                "volume INTEGER NOT NULL, "               // Trade volume
                                                "FOREIGN KEY(symbol) REFERENCES stocks(symbol))")) {
        QMessageBox::critical(this, "Database Error", "Query execution failed:" + createHistoricalDataDatabaseQuery.lastError().text());

        return;
    }

    if (!createScoresDatabaseQuery.exec("CREATE TABLE IF NOT EXISTS scores ("
                                        "symbol TEXT PRIMARY KEY, "           // Stock symbol
                                        "ma_score REAL NOT NULL, "            // Moving Average Score
                                        "rsi_score REAL NOT NULL, "           // Relative Strength Index Score
                                        "bb_score REAL NOT NULL, "            // Bollinger Bands Score
                                        "total_score REAL NOT NULL)")) {      // Total Weighted Score
        QMessageBox::critical(this, "Database Error", "Query execution failed:" + createScoresDatabaseQuery.lastError().text());

        return;
    }
}

void MainWindow::onSearchButtonClicked() {
    QString budgetText = ui->budgetInput->text();
    bool isNumber;
    double budget = budgetText.toDouble(&isNumber);
    PriceValidator validator(db); // Class to validate pricing data

    if (!isNumber || budget <= 0) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a valid budget.");

        return;
    }

    // Initialize Alpaca client for API request
    alpaca::Environment env;
    auto status = env.parse();

    if (!status.ok()) {
        QMessageBox::critical(this, "Environment Error", QString::fromStdString(status.getMessage()));

        return;
    }

    alpaca::Client client(env);

    // Fetch assets from Alpaca
    auto [fetchStatus, assets] = client.getAssets(alpaca::AssetClass::USEquity, alpaca::ActionStatus::Active, exchange, userAgent);

    if (!fetchStatus.ok()) {
        QMessageBox::critical(this, "API Error", QString::fromStdString(fetchStatus.getMessage()));

        return;
    }

    // Gather symbols
    std::vector<std::string> symbols;

    for (const auto& asset : assets)
        symbols.push_back(asset.symbol);

    std::vector<std::string> foundSymbols;
    std::vector<std::string> notFoundSymbols;

    for (const std::string& symbol : symbols) {
        QSqlQuery symbolSelectQuery;
        bool symbolFound = false;

        symbolSelectQuery.prepare("SELECT * FROM stocks WHERE symbol = :symbol LIMIT 1");
        symbolSelectQuery.bindValue(":symbol", QString::fromStdString(symbol));

        if (!symbolSelectQuery.exec()) {
            QMessageBox::critical(this, "Database Error", "Query execution failed:" + symbolSelectQuery.lastError().text());

            return;
        }

        // Check if we have results
        if (symbolSelectQuery.next()) {
            // Row found, check if it's up-to-date
            QVariant symbolVar = symbolSelectQuery.value(1);
            QVariant lastUpdated = symbolSelectQuery.value(2);

            qint64 lastUpdatedTimestamp = lastUpdated.toLongLong();
            qint64 currentTimestamp = QDateTime::currentSecsSinceEpoch();

            if (currentTimestamp - lastUpdatedTimestamp <= 86400) {
                // Data is up-to-date
                foundSymbols.push_back(symbolVar.toString().toStdString());
                symbolFound = true;
            }
        }

        if (!symbolFound)
            notFoundSymbols.push_back(symbol); // If symbol was not found or data was outdated, add to notFoundSymbols
    }

    // Retrieve fresh asset data from the API for these symbols
    if (!notFoundSymbols.empty()) {
        // Retrieve trade data
        auto [tradeStatus, lastTrades] = client.getLatestTrades(notFoundSymbols, userAgent);

        if (!tradeStatus.ok()) {
            QMessageBox::critical(this, "API Error", QString::fromStdString(tradeStatus.getMessage()));

            return;
        }

        for (const auto& [symbol, lastTrade] : lastTrades) {
            double price = lastTrade.price;
            QSqlQuery stocksInsertQuery;
            QSqlQuery tradesInsertQuery;

            // Only include stocks within the user's budget
            if (price <= budget) {
                // Retreieve basic stock data
                auto [fetchStatus, asset] = client.getAsset(symbol, userAgent);

                if (!fetchStatus.ok()) {
                    QMessageBox::critical(this, "API Error", QString::fromStdString(fetchStatus.getMessage()));

                    return;
                }

                // Update the database
                stocksInsertQuery.prepare("INSERT OR REPLACE INTO stocks (id, name, symbol, last_updated) VALUES (:id, :name, :symbol, :lastUpdated)");
                stocksInsertQuery.bindValue(":id", QString::fromStdString(asset.id));
                stocksInsertQuery.bindValue(":name", QString::fromStdString(asset.name));
                stocksInsertQuery.bindValue(":symbol", QString::fromStdString(symbol));
                stocksInsertQuery.bindValue(":lastUpdated", QDateTime::currentSecsSinceEpoch());

                if (!stocksInsertQuery.exec()) {
                    QMessageBox::critical(this, "Database Error", "Query execution failed:" + stocksInsertQuery.lastError().text());

                    return;
                }

                tradesInsertQuery.prepare("INSERT OR REPLACE INTO trades (symbol, price, size) VALUES (:symbol, :price, :size)");
                tradesInsertQuery.bindValue(":symbol", QString::fromStdString(symbol));
                tradesInsertQuery.bindValue(":price", price);
                tradesInsertQuery.bindValue(":size", lastTrade.size);

                if (!tradesInsertQuery.exec()) {
                    QMessageBox::critical(this, "Database Error", "Query execution failed:" + tradesInsertQuery.lastError().text());

                    return;
                }

                // Fetch historical data
                int period = 30; // 1 month
                std::vector<double> priceHistory;
                std::vector<std::string> symbolVec = {symbol}; // Prepare the symbol in a vector for getBars

                // Adjust date range to avoid recent SIP data
                QDateTime endDate = QDateTime::currentDateTime().addDays(-1); // Set endDate to 1 day ago
                QDateTime startDate = endDate.addDays(-period); // Start date is 30 days before the adjusted end date

                // Convert dates to ISO format strings with time (Alpaca expects "YYYY-MM-DDTHH:MM:SSZ")
                std::string end = endDate.toUTC().toString(Qt::ISODate).toStdString();
                std::string start = startDate.toUTC().toString(Qt::ISODate).toStdString();

                // Fetch the bars data using the adjusted date range
                auto [barStatus, bars] = client.getBars(symbolVec, start, end, "", "", "1D", period, userAgent);

                if (barStatus.ok()) {
                    // Check if the symbol exists in the bars map
                    auto it = bars.bars.find(symbol);

                    if (it != bars.bars.end()) {
                        // Update historical data in the database
                        QSqlQuery historicalDataInsertQuery;

                        historicalDataInsertQuery.prepare("INSERT OR REPLACE INTO historical_data (symbol, timestamp, open, high, low, close, volume)"
                                                          "VALUES (:symbol, :timestamp, :open, :high, :low, :close, :volume)");
                        historicalDataInsertQuery.bindValue(":symbol", QString::fromStdString(symbol));

                        // Iterate over the vector of Bar objects for this symbol
                        for (const auto& bar : it->second) {
                            priceHistory.push_back(bar.close_price); // Store closing prices
                            historicalDataInsertQuery.bindValue(":timestamp", bar.time);
                            historicalDataInsertQuery.bindValue(":open", bar.open_price);
                            historicalDataInsertQuery.bindValue(":high", bar.high_price);
                            historicalDataInsertQuery.bindValue(":low", bar.low_price);
                            historicalDataInsertQuery.bindValue(":close", bar.close_price);
                            historicalDataInsertQuery.bindValue(":volume", bar.volume);

                            if (!historicalDataInsertQuery.exec()) {
                                QMessageBox::critical(this, "Database Error", "Query execution failed:" + historicalDataInsertQuery.lastError().text());

                                return;
                            }
                        }
                    }
                    else
                        std::cerr << "No bars found for symbol " << symbol << std::endl;
                }
                else
                    std::cerr << "API Error fetching bars for " << symbol << ": " << barStatus.getMessage() << std::endl;

                validator.validateAndCorrectPrices();

                if (priceHistory.size() < 10) {
                    // Skip calculation since not enough data, insert 0.00 for the scores database
                    StockInformation info;
                    std::vector<double> scores = { 0.00, 0.00, 0.00, 0.00 };

                    info.Name = asset.name;
                    info.Price = price;
                    info.MA_Score = scores[0];
                    info.RSI_Score = scores[1];
                    info.BB_Score = scores[2];
                    info.Total_Score = scores[3];
                    stockInfoMap.emplace(symbol, info);
                    updateScoresDatabase(symbol, scores);
                    std::cerr << "Not enough historical data for " << symbol << std::endl;

                    continue;
                }

                // Calculate total score
                try {
                    std::vector<double> scores = StockAnalysis::calculateTotalScores(price, priceHistory, (priceHistory.size() - 1));

                    // Store score information for the UI
                    StockInformation info;
                    info.Name = asset.name;
                    info.Price = price;
                    info.MA_Score = scores[0];
                    info.RSI_Score = scores[1];
                    info.BB_Score = scores[2];
                    info.Total_Score = scores[3];
                    stockInfoMap.emplace(symbol, info);
                    updateScoresDatabase(symbol, scores);
                } catch (const std::exception& e) {
                    QMessageBox::warning(this, "Calculation Error", QString("Error calculating scores for symbol %1: %2").arg(QString::fromStdString(symbol), e.what()));

                    continue; // Skip to the next symbol
                }
            }

            // Log trades data to see if it's returning as expected
            std::cout << "Trade for symbol: " << symbol << " - Price: " << lastTrade.price << std::endl;
        }
    }

    // Symbols found valid in the cache
    if (!foundSymbols.empty()) {
        for (std::string foundSymbol : foundSymbols) {
            // Pull all trade and scores data from the cache for these symbols
            QSqlQuery stocksSelectQuery;
            QSqlQuery tradesSelectQuery;
            QSqlQuery scoresSelectQuery;
            std::string stockName;
            double stockPrice;
            double maScore;
            double rsiScore;
            double bbScore;
            double totalScore;

            // Gather stock details for the UI
            stocksSelectQuery.prepare("SELECT * FROM stocks WHERE symbol = :symbol LIMIT 1");
            stocksSelectQuery.bindValue(":symbol", QString::fromStdString(foundSymbol));

            if (stocksSelectQuery.exec() && stocksSelectQuery.next()) {
                stockName = stocksSelectQuery.value("name").toString().toStdString();

                tradesSelectQuery.prepare("SELECT * FROM trades WHERE symbol = :symbol LIMIT 1");
                tradesSelectQuery.bindValue(":symbol", QString::fromStdString(foundSymbol));

                if (tradesSelectQuery.exec() && tradesSelectQuery.next()) {
                    stockPrice = tradesSelectQuery.value("price").toDouble();
                    scoresSelectQuery.prepare("SELECT * FROM scores WHERE symbol = :symbol LIMIT 1");
                    scoresSelectQuery.bindValue(":symbol", QString::fromStdString(foundSymbol));

                    if (scoresSelectQuery.exec() && scoresSelectQuery.next()) {
                        maScore = scoresSelectQuery.value("ma_score").toDouble();
                        rsiScore = scoresSelectQuery.value("rsi_score").toDouble();
                        bbScore = scoresSelectQuery.value("bb_score").toDouble();
                        totalScore = scoresSelectQuery.value("total_score").toDouble();

                        // Put data into the stockInfoMap for this symbol
                        StockInformation info;

                        info.Name = stockName;
                        info.Price = stockPrice;
                        info.MA_Score = maScore;
                        info.RSI_Score = rsiScore;
                        info.BB_Score = bbScore;
                        info.Total_Score = totalScore;
                        stockInfoMap.emplace(foundSymbol, info);

                        // Initialize the scores vector with the appropriate scores
                        std::vector<double> scores = { info.MA_Score, info.RSI_Score, info.BB_Score, info.Total_Score };

                        updateScoresDatabase(foundSymbol, scores);
                    }
                    else {
                        QMessageBox::critical(this, "Database Error", "Query execution failed:" + scoresSelectQuery.lastError().text());

                        return;
                    }
                }
                else {
                    QMessageBox::critical(this, "Database Error", "Query execution failed:" + tradesSelectQuery.lastError().text());

                    return;
                }
            }
            else {
                QMessageBox::critical(this, "Database Error", "Query execution failed:" + stocksSelectQuery.lastError().text());

                return;
            }
        }
    }

    validator.revalidateSuspiciousScores();
    db.close(); // Close the database connection
    refreshStockList();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::refreshStockList() {
    // Clear the existing rows
    QStandardItemModel* model = qobject_cast<QStandardItemModel *>(ui->stockList->model());

    if (model)
        model->removeRows(0, model->rowCount());  // Clear any previous entries

    // Re-populate the table from the latest stockInfoMap data
    for (const auto& [symbol, stockData] : stockInfoMap) {
        addRowToTable(
            QString::fromStdString(stockData.Name),
            QString::fromStdString(symbol),
            stockData.Price,
            stockData.MA_Score,
            stockData.RSI_Score,
            stockData.BB_Score,
            stockData.Total_Score
        );
    }

    // Notify the model that data has been changed to refresh the UI
    if (model)
        emit model->layoutChanged();
}

void MainWindow::addRowToTable(const QString& name, const QString& ticker, double price, double ma_score, double rsi_score, double bb_score, double total_score) {
    QList<QStandardItem*> row;

    row << new QStandardItem(name);
    row << new QStandardItem(ticker);
    row << new QStandardItem(QString::number(price, 'f', 2));
    row << new QStandardItem(QString::number(ma_score, 'f', 2));
    row << new QStandardItem(QString::number(rsi_score, 'f', 2));
    row << new QStandardItem(QString::number(bb_score, 'f', 2));
    row << new QStandardItem(QString::number(total_score, 'f', 2));

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->stockList->model());

    if (model)
        model->appendRow(row);
}

void MainWindow::updateScoresDatabase(const std::string symbol, const std::vector<double> scores) {
    // Ensure that the scores vector has the expected number of elements
    if (scores.size() != 4) {
        QMessageBox::critical(this, "Database Error", "Scores vector does not contain the correct number of elements.");

        return;
    }

    QSqlQuery query;

    query.prepare("INSERT OR REPLACE INTO scores (symbol, ma_score, rsi_score, bb_score, total_score) "
                  "VALUES (:symbol, :ma_score, :rsi_score, :bb_score, :total_score)"
    );

    // Bind values to the query
    query.bindValue(":symbol", QString::fromStdString(symbol));
    query.bindValue(":ma_score", scores[0]);
    query.bindValue(":rsi_score", scores[1]);
    query.bindValue(":bb_score", scores[2]);
    query.bindValue(":total_score", scores[3]);

    // Execute the query and check for errors
    if (!query.exec()) {
        QMessageBox::critical(this, "Database Error", "Failed to update scores database: " + query.lastError().text());
        std::cout << "Query Error:" << query.lastError().text().toStdString() << std::endl;
    }
}
