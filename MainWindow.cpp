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

    // Gather tradeable symbols
    std::vector<std::string> symbols;
    for (const auto& asset : assets) {
        if (asset.tradable && asset.asset_class == "us_equity") {
            symbols.push_back(asset.symbol);
        }
    }

    // Fetch last trade prices for all symbols
    auto [tradeStatus, lastTrades] = client.getLatestTrades(symbols);
    if (!tradeStatus.ok()) {
        std::cerr << "API Error fetching trades: " << tradeStatus.getMessage() << std::endl;
        QMessageBox::critical(this, "API Error", QString::fromStdString(tradeStatus.getMessage()));
        return;
    }

    // Update cache and filter stocks within budget
    QSqlQuery insertQuery;
    for (const auto& [symbol, lastTrade] : lastTrades) {
        double price = lastTrade.price;
        if (price < budget) {
            stocks << QString::fromStdString(symbol);
        }

        // Insert or replace cache with new data
        insertQuery.prepare("INSERT OR REPLACE INTO stock_cache (symbol, price, last_updated) "
                            "VALUES (:symbol, :price, :last_updated)");
        insertQuery.bindValue(":symbol", QString::fromStdString(symbol));
        insertQuery.bindValue(":price", price);
        insertQuery.bindValue(":last_updated", QDateTime::currentSecsSinceEpoch());

        if (!insertQuery.exec()) {
            std::cerr << "Database insert error: " << insertQuery.lastError().text().toStdString() << std::endl;
        }
    }

    displayStocks(stocks);
}

void MainWindow::displayStocks(const QStringList& stocks) {
    ui->stockList->clear();
    if (!stocks.isEmpty()) {
        ui->stockList->setPlainText(stocks.join("\n") + "\n\nTotal stocks found: " + QString::number(stocks.size()));
    } else {
        ui->stockList->setPlainText("No stocks found within the specified budget.");
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
