#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ThirdParty/alpaca-trade-api-cpp/alpaca/client.h"
#include "ThirdParty/alpaca-trade-api-cpp/alpaca/config.h"
#include <nlohmann/json.hpp>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
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
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <unordered_map>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::onSearchButtonClicked);

    // Create model for stocks table view
    QStandardItemModel* model = new QStandardItemModel(this);

    // Wrap it in a proxy model for sorting
    QSortFilterProxyModel* proxyModel = new QSortFilterProxyModel(this);

    proxyModel->setSourceModel(model);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setDynamicSortFilter(true);

    // Setup the table
    ui->stockList->setModel(proxyModel);
    model->setColumnCount(7);
    model->setHeaderData(0, Qt::Horizontal, "Name");
    model->setHeaderData(1, Qt::Horizontal, "Ticker");
    model->setHeaderData(2, Qt::Horizontal, "Price");
    model->setHeaderData(3, Qt::Horizontal, "MA Score");
    model->setHeaderData(4, Qt::Horizontal, "RSI Score");
    model->setHeaderData(5, Qt::Horizontal, "BB Score");
    model->setHeaderData(6, Qt::Horizontal, "Total Score");
    ui->stockList->setColumnWidth(0, 178);
    ui->stockList->setSortingEnabled(true);

    // Set up SQLite database in the same folder as the executable
    QString exeDir = QCoreApplication::applicationDirPath();  // Folder containing the exe
    QDir dir(exeDir);

    // Ensure the directory exists (should always exist, just a safety check)
    if (!dir.exists()) {
        QMessageBox::critical(this, "Database Error", "Executable directory does not exist: " + exeDir);
        return;
    }

    QString dbFilePath = dir.filePath("cache.db"); // Correct path separator
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbFilePath);

    if (!db.open()) {
        QMessageBox::critical(this, "Database Error", "Failed to open SQLite database: " + dbFilePath);
        return;
    }

    // Create table for caching if it doesn't exist
    QSqlQuery createStocksDatabaseQuery(db);
    QSqlQuery createTradesDatabaseQuery(db);
    QSqlQuery createHistoricalDataDatabaseQuery(db);
    QSqlQuery createScoresDatabaseQuery(db);

    if (!createStocksDatabaseQuery.exec("CREATE TABLE IF NOT EXISTS stocks ("
                                        "id TEXT PRIMARY KEY, "         // Unique Asset ID (Defined by Alpaca)
                                        "name TEXT, "                   // Company name
                                        "symbol TEXT, "                 // Stock symbol
                                        "excluded TINYINT DEFAULT 0, "  // Excluded from analysis flag (either 0 or 1)
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

    for (const auto& asset : assets) {
        if (asset.tradable)
            symbols.push_back(asset.symbol); // Only add assets tradable on Alpaca
    }

    std::vector<std::string> foundSymbols;
    std::vector<std::string> notFoundSymbols;

    for (const std::string& symbol : symbols) {
        QSqlQuery symbolSelectQuery(db);
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

            if (currentTimestamp - lastUpdatedTimestamp <= 172800) { // Data is only considerd valid for 48 hours
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
            QSqlQuery stocksInsertQuery(db);
            QSqlQuery tradesInsertQuery(db);

            // Only include stocks within the user's budget
            if (price <= budget) {
                // Retreieve basic stock data
                auto [individualFetchStatus, asset] = client.getAsset(symbol, userAgent);

                if (!individualFetchStatus.ok()) {
                    QMessageBox::critical(this, "API Error", QString::fromStdString(individualFetchStatus.getMessage()));

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
                int period = 40; // 40 days
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
                        QSqlQuery historicalDataInsertQuery(db);

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
                else {
                    std::string msg = barStatus.getMessage();

                    if (msg.find("empty response") != std::string::npos) {
                        std::cerr << "Insufficent data for symbol " << symbol << ", removing from analysis." << std::endl;

                        QSqlQuery markExcludedQuery(db);

                        markExcludedQuery.prepare("UPDATE stocks SET excluded = 1 WHERE symbol = :symbol");
                        markExcludedQuery.bindValue(":symbol", QString::fromStdString(symbol));

                        if (!markExcludedQuery.exec())
                            QMessageBox::critical(this, "Database Error", "Query execution failed:" + markExcludedQuery.lastError().text());
                    }
                    else {
                        std::cerr << "API Error fetching bars for " << symbol << ": " << msg << std::endl;

                        return;
                    }
                }

                // Calculate scores
                try {
                    QSqlQuery getStockExclusionStatusQuery(db);
                    StockAnalysis analyzer(db);
                    bool isExcluded = false;

                    getStockExclusionStatusQuery.prepare("SELECT * FROM stocks WHERE symbol = :symbol LIMIT 1");
                    getStockExclusionStatusQuery.bindValue(":symbol", QString::fromStdString(symbol));

                    if (!getStockExclusionStatusQuery.exec()) {
                        QMessageBox::critical(this, "Database Error", "Query execution failed:" + getStockExclusionStatusQuery.lastError().text());

                        return;
                    }

                    while (getStockExclusionStatusQuery.next())
                        isExcluded = (getStockExclusionStatusQuery.value("excluded").toInt() != 0);

                    if (!isExcluded) {
                        std::vector<double> scores = analyzer.calculateTotalScores(symbol, price, priceHistory, static_cast<int>(priceHistory.size()));

                        // If the analyzer returns empty, skip the symbol
                        if (scores.empty())
                            continue; // Skip to the next symbol

                        // Store score information for the UI
                        StockInformation info;
                        info.Name = asset.name;
                        info.Price = price;
                        info.MA_Score = scores[0];
                        info.RSI_Score = scores[1];
                        info.BB_Score = scores[2];
                        info.Total_Score = scores[3];

                        updateScoresDatabase(symbol, scores);
                        excludeSuspiciousScores();

                        // Last exclusion flag check before adding to the dataset
                        QSqlQuery finalExclusionCheckQuery(db);
                        isExcluded = false; // Reset the bool just to be safe

                        finalExclusionCheckQuery.prepare("SELECT * FROM stocks WHERE symbol = :symbol LIMIT 1");
                        finalExclusionCheckQuery.bindValue(":symbol", QString::fromStdString(symbol));

                        if (!finalExclusionCheckQuery.exec()) {
                            QMessageBox::critical(this, "Database Error", "Query execution failed:" + finalExclusionCheckQuery.lastError().text());

                            continue;
                        }

                        while (finalExclusionCheckQuery.next())
                            isExcluded = (finalExclusionCheckQuery.value("excluded").toInt() != 0);

                        if (!isExcluded)
                            stockInfoMap.emplace(symbol, info);
                    }
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
            QSqlQuery stocksSelectQuery(db);
            QSqlQuery tradesSelectQuery(db);
            QSqlQuery scoresSelectQuery(db);
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

                        // Initialize the scores vector with the appropriate scores
                        std::vector<double> scores = { info.MA_Score, info.RSI_Score, info.BB_Score, info.Total_Score };

                        updateScoresDatabase(foundSymbol, scores);
                        excludeSuspiciousScores();

                        // Last exclusion flag check before adding to the dataset
                        QSqlQuery finalExclusionCheckQuery(db);
                        bool isExcluded = false;

                        finalExclusionCheckQuery.prepare("SELECT * FROM stocks WHERE symbol = :symbol LIMIT 1");
                        finalExclusionCheckQuery.bindValue(":symbol", QString::fromStdString(foundSymbol));

                        if (!finalExclusionCheckQuery.exec()) {
                            QMessageBox::critical(this, "Database Error", "Query execution failed:" + finalExclusionCheckQuery.lastError().text());

                            continue;
                        }

                        while (finalExclusionCheckQuery.next())
                            isExcluded = (finalExclusionCheckQuery.value("excluded").toInt() != 0);

                        if (!isExcluded)
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

    refreshStockList();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::refreshStockList() {
    // Clear the existing rows
    QSortFilterProxyModel* proxy = qobject_cast<QSortFilterProxyModel*>(ui->stockList->model());

    if (!proxy) return;

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(proxy->sourceModel());

    if (!model) return;

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
    if (model) {
        emit model->layoutChanged();

        // Force sorting by Total Score (column 6), descending
        if (proxy)
            proxy->sort(6, Qt::DescendingOrder);
    }
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

    QSortFilterProxyModel* proxy = qobject_cast<QSortFilterProxyModel*>(ui->stockList->model());

    if (!proxy) return;

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(proxy->sourceModel());

    if (!model) return;

    model->appendRow(row);
}

void MainWindow::updateScoresDatabase(const std::string symbol, const std::vector<double> scores) {
    // Ensure that the scores vector has the expected number of elements
    if (scores.size() != 4) {
        QMessageBox::critical(this, "Database Error", "Scores vector does not contain the correct number of elements.");

        return;
    }

    QSqlQuery query(db);

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

        return;
    }
}

void MainWindow::excludeSuspiciousScores() {
    // Mark any symbol with a total score >=1.1 as excluded
    QSqlQuery finalScoreCheckQuery(db);

    finalScoreCheckQuery.prepare("SELECT * FROM scores WHERE total_score >= 1.1"); // Any score over 1.1 is considered erroneous

    if (!finalScoreCheckQuery.exec()) {
        QMessageBox::critical(this, "Database Error", "Failed to gather score data: " + finalScoreCheckQuery.lastError().text());
        std::cout << "Query Error:" << finalScoreCheckQuery.lastError().text().toStdString() << std::endl;

        return;
    }

    while (finalScoreCheckQuery.next()) {
        QString symbol = finalScoreCheckQuery.value("symbol").toString();
        double totalScore = finalScoreCheckQuery.value("total_score").toDouble();

        if (totalScore >= static_cast<double>(1.1)) {
            QSqlQuery markExcludedQuery(db);

            markExcludedQuery.prepare("UPDATE stocks SET excluded = 1 WHERE symbol = :symbol");
            markExcludedQuery.bindValue(":symbol", symbol);

            if (!markExcludedQuery.exec()) {
                QMessageBox::critical(this, "Database Error", "Failed to update stocks database: " + markExcludedQuery.lastError().text());
                std::cout << "Query Error:" << markExcludedQuery.lastError().text().toStdString() << std::endl;

                return;
            }
        }
    }
}
