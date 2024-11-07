#ifndef POLYGON_API_H
#define POLYGON_API_H

#include <string>
#include <vector>

class PolygonAPI {
public:
    PolygonAPI(const std::string& apiKey);
    std::string getStockData(const std::string& tickerSymbol);
    std::vector<double> getHistoricalPrices(const std::string& tickerSymbol, int days);

private:
    std::string apiKey;
    std::string baseURL;

    std::string makeRequest(const std::string& url);
};

#endif // POLYGON_API_H
