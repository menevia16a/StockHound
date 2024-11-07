#ifndef POLYGON_API_H
#define POLYGON_API_H

#include <string>

class PolygonAPI {
public:
    PolygonAPI(const std::string& apiKey);
    std::string getStockData(const std::string& tickerSymbol);

private:
    std::string apiKey;
    std::string baseURL;

    std::string makeRequest(const std::string& url);
};

#endif // POLYGON_API_H
