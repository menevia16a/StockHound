#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ThirdParty/alpaca-trade-api-cpp/alpaca/client.h"
#include "ThirdParty/alpaca-trade-api-cpp/alpaca/config.h"

#include <nlohmann/json.hpp>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
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
    QStandardItemModel *model = new QStandardItemModel(this);

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
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
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
                                        "symbol TEXT PRIMARY KEY, "     // Stock symbol
                                        "ma_score INTEGER, "            // Moving Average Score
                                        "rsi_score INTEGER, "           // Relative Strength Index Score
                                        "bb_score INTEGER, "            // Bollinger Bands Score
                                        "total_score INTEGER)")) {      // Total Weighted Score
        QMessageBox::critical(this, "Database Error", "Query execution failed:" + createScoresDatabaseQuery.lastError().text());

        return;
    }
}

void MainWindow::onSearchButtonClicked() {
    QString budgetText = ui->budgetInput->text();
    bool isNumber;
    double budget = budgetText.toDouble(&isNumber);

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

    // Map to store all stock information
    std::unordered_map<std::string, StockInformation> stockInfoMap;
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
                int period = 20; // 20 Day period
                std::vector<double> priceHistory;
                std::vector<std::string> symbolVec = {symbol}; // Prepare the symbol in a vector for getBars

                // Calculate the date range
                QDateTime endDate = QDateTime::currentDateTime();
                QDateTime startDate = endDate.addDays(-period);

                // Convert dates to ISO format strings (Alpaca expects "YYYY-MM-DD")
                std::string end = endDate.toString("yyyy-MM-dd").toStdString();
                std::string start = startDate.toString("yyyy-MM-dd").toStdString();

                // Fetch the bars data using the date range
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
                    else {
                        std::cerr << "No bars found for symbol " << symbol << std::endl;

                        continue; // Skip this stock if no bars are found
                    }
                }
                else {
                    std::cerr << "API Error fetching bars for " << symbol << ": " << barStatus.getMessage() << std::endl;

                    continue; // Skip this stock if historical data fails
                }

                if (priceHistory.size() < period) {
                    std::cerr << "Not enough historical data for " << symbol << std::endl;

                    continue; // Skip stocks without enough data
                }

                // Calculate total score
                std::vector<double> scores = StockAnalysis::calculateTotalScores(price, priceHistory, period);

                // Store score information for the UI
                StockInformation info;

                info.Name = asset.name;
                info.Price = price;
                info.MA_Score = scores[0];
                info.RSI_Score = scores[1];
                info.BB_Score = scores[2];
                info.Total_Score = scores[3];
                stockInfoMap.emplace(symbol, info);
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

            if (stocksSelectQuery.exec()) {
                stockName = stocksSelectQuery.value(1).toString().toStdString();

                tradesSelectQuery.prepare("SELECT * FROM trades WHERE symbol = :symbol LIMIT 1");
                tradesSelectQuery.bindValue(":symbol", QString::fromStdString(foundSymbol));

                if (tradesSelectQuery.exec()) {
                    stockPrice = tradesSelectQuery.value(1).toDouble();

                    scoresSelectQuery.prepare("SELECT * FROM scores WHERE symbol = :symbol LIMIT 1");
                    scoresSelectQuery.bindValue(":symbol", QString::fromStdString(foundSymbol));

                    if (scoresSelectQuery.exec()) {
                        maScore = scoresSelectQuery.value(1).toDouble();
                        rsiScore = scoresSelectQuery.value(2).toDouble();
                        bbScore = scoresSelectQuery.value(3).toDouble();
                        totalScore = scoresSelectQuery.value(4).toDouble();

                        // Put data into the stockInfoMap for this symbol
                        StockInformation info;

                        info.Name = stockName;
                        info.Price = stockPrice;
                        info.MA_Score = maScore;
                        info.RSI_Score = rsiScore;
                        info.BB_Score = bbScore;
                        info.Total_Score = totalScore;
                        stockInfoMap.emplace(foundSymbol, info);
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

    // Look for the symbols in the map to find the stock's data
    for (const std::string& symbol : symbols) {
        auto it = stockInfoMap.find(symbol);

        if (it != stockInfoMap.end()) {
            // Entry found in map, add row to the UI
            StockInformation stockData = it->second;

            addRowToTable(QString::fromStdString(stockData.Name), QString::fromStdString(symbol), stockData.Price, stockData.MA_Score, stockData.RSI_Score, stockData.BB_Score, stockData.Total_Score);
        }
    }
}

MainWindow::~MainWindow() {
    delete ui;
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

    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(ui->stockList->model());

    if (model)
        model->appendRow(row);
}
