#include "PolygonAPI.h"
#include <curl/curl.h>
#include <sstream>
#include <iostream>

PolygonAPI::PolygonAPI(const std::string& apiKey) : apiKey(apiKey), baseURL("https://api.polygon.io/v2/") {}

std::string PolygonAPI::getStockData(const std::string& tickerSymbol) {
    std::string url = baseURL + "aggs/ticker/" + tickerSymbol + "/prev?adjusted=true&apiKey=" + apiKey;
    return makeRequest(url);
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string PolygonAPI::makeRequest(const std::string& url) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
            std::cerr << "cURL Error: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
    }

    return readBuffer;
}
