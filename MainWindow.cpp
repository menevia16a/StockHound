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
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::onSearchButtonClicked() {
    // Get the budget from the user input
    bool ok;
    double budget = ui->budgetInput->text().toDouble(&ok);

    if (!ok || budget <= 0) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a valid budget.");
        return;
    }

    ui->stockList->setPlainText("Loading...");
    ui->stockList->clear();
}
