#include "TelegramClient.h"
#include <curl/curl.h>
#include <iostream>
#include "json.hpp"

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    output->append((char*)contents, size * nmemb);
    return size * nmemb;
}

TelegramClient::TelegramClient(const std::string& token)
    : token(token) {}

std::vector<TelegramClient::IncomingMessage> TelegramClient::getUpdates() {
    std::vector<IncomingMessage> messages;
    std::string response;

    CURL* curl = curl_easy_init();
    if (!curl) return messages;

    std::string url =
        "https://api.telegram.org/bot" + token + "/getUpdates";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[telegram] bad response\n";
        return messages;
    }

    auto data = json::parse(response, nullptr, false);
    if (!data.is_object()) return messages;

    for (auto& update : data["result"]) {
        if (!update.contains("message")) continue;
        if (!update["message"].contains("text")) continue;

        IncomingMessage msg;
        msg.chatId = update["message"]["chat"]["id"];
        msg.text = update["message"]["text"];

        messages.push_back(msg);
    }

    return messages;
}

void TelegramClient::sendMessage(long chatId, const std::string& text) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    std::string escaped = curl_easy_escape(curl, text.c_str(), 0);

    std::string url =
        "https://api.telegram.org/bot" + token +
        "/sendMessage?chat_id=" + std::to_string(chatId) +
        "&text=" + escaped;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
}


