#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ThirdParty/alpaca-trade-api-cpp/alpaca/client.h"
#include "ThirdParty/alpaca-trade-api-cpp/alpaca/config.h"

#include <nlohmann/json.hpp>
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
}

void MainWindow::onSearchButtonClicked() {
    QString budgetText = ui->budgetInput->text();
    bool isNumber;
    double budget = budgetText.toDouble(&isNumber);

    if (!isNumber || budget <= 0) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a valid budget.");
        return;
    }

    // Initialize Alpaca client
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

    // Filter for tradable US equities
    std::vector<alpaca::Asset> tradableAssets;
    for (const auto& asset : assets) {
        if (asset.tradable && asset.asset_class == "us_equity") {
            tradableAssets.push_back(asset);
        }
    }

    // Collect symbols of tradable assets
    std::vector<std::string> symbols;
    for (const auto& asset : tradableAssets) {
        symbols.push_back(asset.symbol);
    }

    // Fetch last trade prices for all symbols
    auto [tradeStatus, lastTrades] = client.getLastTrades(symbols);
    if (!fetchStatus.ok()) {
        std::cerr << "API Error fetching assets: " << fetchStatus.getMessage() << std::endl;
        QMessageBox::critical(this, "API Error", QString::fromStdString(fetchStatus.getMessage()));
        return;
    }

    // Filter stocks within budget
    QStringList stocks;
    for (const auto& [symbol, lastTrade] : lastTrades) {
        if (lastTrade.trade.price < budget) {
            stocks << QString::fromStdString(symbol);
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

    ui->stockList->setPlainText(stocks.join("\n"));
}

MainWindow::~MainWindow()
{
    delete ui;
}
