#include "PolygonAPI.h"

#include <curl/curl.h>
#include <sstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

PolygonAPI::PolygonAPI(const std::string& apiKey) : apiKey(apiKey), baseURL("https://api.polygon.io/v2/") {}

std::string PolygonAPI::getStockData(const std::string& tickerSymbol) {
    std::string url = baseURL + "aggs/ticker/" + tickerSymbol + "/prev?adjusted=true&apiKey=" + apiKey;
    std::string response = makeRequest(url);

    try {
        auto jsonResponse = json::parse(response);
        if (jsonResponse.contains("error")) {
            std::cerr << "API Error: " << jsonResponse["error"] << std::endl;
            return "Error: API request failed.";
        }

        if (jsonResponse["status"] == "OK" && !jsonResponse["results"].empty()) {
            // Process stock data here...
        } else {
            return "Error: No data available for " + tickerSymbol;
        }
    } catch (const std::exception& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << std::endl;
        return "Error: Failed to parse API response.";
    }

    return response;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);

    return size * nmemb;
}

std::string PolygonAPI::makeRequest(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string readBuffer;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "cURL Error: " << curl_easy_strerror(res) << std::endl;
            readBuffer = "{\"error\":\"cURL request failed\"}";
        }

        curl_easy_cleanup(curl);
    } else {
        readBuffer = "{\"error\":\"Failed to initialize cURL\"}";
    }

    return readBuffer;
}
