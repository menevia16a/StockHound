#ifndef PRICEVALIDATOR_H
#define PRICEVALIDATOR_H

#include <QString>
#include <QSqlDatabase>

class PriceValidator {
public:
    // Constructor
    explicit PriceValidator(QSqlDatabase& database);

    void validateAndCorrectPrices() const;
    void revalidateSuspiciousScores() const;

private:
    QSqlDatabase& db;

    double getLatestClosingPrice(const QString& symbol) const;

    void updateTradePrice(const QString& symbol, double correctedPrice) const;
};

#endif // PRICEVALIDATOR_H
