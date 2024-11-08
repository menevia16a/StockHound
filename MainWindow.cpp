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

    // Filter stocks within budget based on last trade price
    QStringList stocks;
    for (const auto& asset : assets) {
        // Use getLastTrade to retrieve the latest trade price for the asset
        auto [tradeStatus, lastTrade] = client.getLastTrade(asset.symbol);
        if (!tradeStatus.ok()) {
            continue;  // Skip if we can't retrieve the trade data
        }

        // Access the price from the trade member of LastTrade
        if (lastTrade.trade.price < budget) {
            stocks << QString::fromStdString(asset.symbol);
        }
    }

    displayStocks(stocks);
}

void MainWindow::displayStocks(const QStringList& stocks) {
    ui->stockList->clear();
    if (stocks.isEmpty()) {
        ui->stockList->setPlainText("No stocks found within the specified budget.");
        return;
    }

    ui->stockList->setPlainText(stocks.join("\n"));
}

MainWindow::~MainWindow()
{
    delete ui;
}
