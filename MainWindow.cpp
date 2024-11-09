#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Analysis/StockAnalysis.h"
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::onSearchButtonClicked);

    // Set up SQLite database connection
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("stock_cache.db");

    if (!db.open()) {
        QMessageBox::critical(this, "Database Error", "Failed to open the SQLite database.");
        return;
    }

    // Create table for caching if it doesn't exist
    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS stock_cache ("
               "symbol TEXT PRIMARY KEY, "
               "price REAL, "
               "last_updated INTEGER)"); // last_updated in Unix timestamp
}

void MainWindow::onSearchButtonClicked() {
    QString budgetText = ui->budgetInput->text();
    bool isNumber;
    double budget = budgetText.toDouble(&isNumber);

    if (!isNumber || budget <= 0) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a valid budget.");
        return;
    }

    // Check for cached data before making API requests
    QStringList stocks;
    QSqlQuery query;
    query.prepare("SELECT symbol, price FROM stock_cache WHERE price < :budget AND "
                  "last_updated > :last_day");
    query.bindValue(":budget", budget);
    query.bindValue(":last_day", QDateTime::currentSecsSinceEpoch() - 86400); // 24 hours ago

    if (query.exec()) {
        while (query.next()) {
            stocks << query.value(0).toString();
        }
    } else {
        std::cerr << "Database query error: " << query.lastError().text().toStdString() << std::endl;
    }

    if (!stocks.isEmpty()) {
        displayStocks(stocks);
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
    auto [fetchStatus, assets] = client.getAssets();

    if (!fetchStatus.ok()) {
        QMessageBox::critical(this, "API Error", QString::fromStdString(fetchStatus.getMessage()));
        return;
    }

    // Gather symbols
    std::vector<std::string> symbols;

    for (const auto& asset : assets) {
        symbols.push_back(asset.symbol);
    }

    auto [tradeStatus, lastTrades] = client.getLatestTrades(symbols);

    if (!tradeStatus.ok()) {
        std::cerr << "API Error fetching trades: " << tradeStatus.getMessage() << std::endl;
        QMessageBox::critical(this, "API Error", QString::fromStdString(tradeStatus.getMessage()));
        return;
    }

    // Log trades data to see if it's returning as expected
    for (const auto& [symbol, lastTrade] : lastTrades) {
        std::cout << "Trade for symbol: " << symbol << " - Price: " << lastTrade.price << std::endl;
    }

    QStringList stockInfoList; // Store formatted stock info to display in UI

    for (const auto& [symbol, lastTrade] : lastTrades) {
        double price = lastTrade.price;

        // Only include stocks within the user's budget
        if (price <= budget) {
            // Fetch historical data for the last 20 days
            int period = 20;
            std::vector<double> priceHistory;

            // Prepare the symbol in a vector for getBars
            std::vector<std::string> symbolVec = {symbol};

            // Calculate the date range (last 20 days)
            QDateTime endDate = QDateTime::currentDateTime();
            QDateTime startDate = endDate.addDays(-period);

            // Convert dates to ISO format strings (Alpaca expects "YYYY-MM-DD")
            std::string end = endDate.toString("yyyy-MM-dd").toStdString();
            std::string start = startDate.toString("yyyy-MM-dd").toStdString();

            // Fetch the bars data using the date range
            auto [barStatus, bars] = client.getBars(symbolVec, start, end, "", "", "day", period);

            if (barStatus.ok()) {
                // Check if the symbol exists in the bars map
                auto it = bars.bars.find(symbol);
                if (it != bars.bars.end()) {
                    // Iterate over the vector of Bar objects for this symbol
                    for (const auto& bar : it->second) {
                        priceHistory.push_back(bar.close_price); // Store closing prices
                    }
                } else {
                    std::cerr << "No bars found for symbol " << symbol << std::endl;
                    continue; // Skip this stock if no bars are found
                }
            } else {
                std::cerr << "API Error fetching bars for " << symbol << ": " << barStatus.getMessage() << std::endl;
                continue; // Skip this stock if historical data fails
            }

            if (priceHistory.size() < period) {
                std::cerr << "Not enough historical data for " << symbol << std::endl;
                continue; // Skip stocks without enough data
            }

            // Calculate total score
            double score = StockAnalysis::calculateTotalScore(price, priceHistory, period);

            // Format stock information with score
            QString stockInfo = QString("Ticker: %1 | Price: %2 | Score: %3")
                                    .arg(QString::fromStdString(symbol))
                                    .arg(price)
                                    .arg(score, 0, 'f', 2);

            stockInfoList << stockInfo;
        }

        // Insert or replace cache with new data
        QSqlQuery insertQuery;
        insertQuery.prepare("INSERT OR REPLACE INTO stock_cache (symbol, price, last_updated) "
                            "VALUES (:symbol, :price, :last_updated)");
        insertQuery.bindValue(":symbol", QString::fromStdString(symbol));
        insertQuery.bindValue(":price", price);
        insertQuery.bindValue(":last_updated", QDateTime::currentSecsSinceEpoch());

        if (!insertQuery.exec()) {
            std::cerr << "Database insert error: " << insertQuery.lastError().text().toStdString() << std::endl;
        }
    }

    // Display the list of stocks with scores
    displayStocks(stockInfoList);
}

// Update displayStocks to accept and show score information
void MainWindow::displayStocks(const QStringList& stockInfoList) {
    ui->stockList->clear();
    if (!stockInfoList.isEmpty()) {
        ui->stockList->setPlainText(stockInfoList.join("\n") +
                                    "\n\nTotal stocks found: " + QString::number(stockInfoList.size()));
    } else {
        ui->stockList->setPlainText("No stocks found within the specified budget.");
    }
}

MainWindow::~MainWindow() {
    delete ui;
}
