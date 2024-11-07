#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <QMessageBox>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>

using json = nlohmann::json;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , polygonAPI(new PolygonAPI("qDBekvAkp7R3_8zkuOKUHSS17f7wkjU6")) {
    ui->setupUi(this);

    // Connect the search button to the slot
    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::onSearchButtonClicked);
}

MainWindow::~MainWindow() {
    delete ui;
    delete polygonAPI;
}

void MainWindow::onSearchButtonClicked() {
    // Get the budget from the user input
    bool ok;
    double budget = ui->budgetInput->text().toDouble(&ok);

    if (!ok || budget <= 0) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a valid budget.");
        return;
    }

    // Show loading message
    ui->stockList->setPlainText("Loading...");

    // Placeholder for actual stock fetching and filtering
    std::vector<std::string> stockSymbols = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};
    std::vector<std::string> affordableStocks;

    for (const auto& symbol : stockSymbols) {
        std::string stockData = polygonAPI->getStockData(symbol);

        if (stockData.find("Error") == 0) {  // Check if there's an error in the response
            QMessageBox::warning(this, "API Error", QString::fromStdString(stockData));
            ui->stockList->clear();
            return;
        }

        try {
            auto jsonResponse = json::parse(stockData);
            if (jsonResponse["status"] == "OK") {
                auto results = jsonResponse["results"];

                if (!results.empty()) {
                    double closePrice = results[0]["c"];

                    if (closePrice <= budget) {
                        affordableStocks.push_back(symbol + " - Price: " + std::to_string(closePrice));
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "JSON Parsing Error: " << e.what() << std::endl;
        }
    }

    // Display the list of affordable stocks in the text area
    ui->stockList->clear();

    if (affordableStocks.empty()) {
        ui->stockList->setPlainText("No stocks found within your budget.");
    } else {
        for (const auto& stock : affordableStocks) {
            ui->stockList->appendPlainText(QString::fromStdString(stock));
        }
    }
}
