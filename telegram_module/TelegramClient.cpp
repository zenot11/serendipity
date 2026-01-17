#include "TelegramClient.h"

#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

#include "json.hpp"

using json = nlohmann::json;

// -----------------------------
// write callback для getUpdates
// -----------------------------
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// -----------------------------
// ctor
// -----------------------------
TelegramClient::TelegramClient(const std::string& token)
    : token(token), last_update_id(0) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

// -----------------------------
// dtor
// -----------------------------
TelegramClient::~TelegramClient() {
    curl_global_cleanup();
}

// -----------------------------
// основной polling
// -----------------------------
void TelegramClient::poll() {
    while (true) {
        std::string response;
        CURL* curl = curl_easy_init();

        if (!curl) {
            std::cerr << "[poll ERROR] curl_easy_init failed\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        std::ostringstream url;
        url << "https://api.telegram.org/bot"
            << token
            << "/getUpdates?timeout=30&offset="
            << last_update_id;

        curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 35L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cerr << "[poll ERROR] "
                      << curl_easy_strerror(res) << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        if (response.empty()) {
            continue;
        }

        try {
            auto data = json::parse(response);

            if (!data.contains("result")) {
                continue;
            }

            for (const auto& update : data["result"]) {
                last_update_id = update["update_id"].get<long>() + 1;

                if (!update.contains("message")) continue;
                if (!update["message"].contains("text")) continue;

                const auto& msg = update["message"];
                long chatId = msg["chat"]["id"].get<long>();
                std::string text = msg["text"].get<std::string>();

                std::cout << "[recv] " << text << std::endl;

                std::string reply = logic.handleCommand(text);
                if (reply.empty()) {
                    reply = "Я получил: " + text;
                }

                sendMessage(chatId, reply);
            }

        } catch (const json::parse_error& e) {
            std::cerr << "[json parse error] " << e.what() << std::endl;
            std::cerr << "Response was:\n" << response << std::endl;
        }
    }
}

// -----------------------------
// sendMessage (БЕЗ парсинга!)
// -----------------------------
void TelegramClient::sendMessage(long chatId, const std::string& text) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    char* escaped = curl_easy_escape(curl, text.c_str(), text.length());

    std::string url =
        "https://api.telegram.org/bot" + token +
        "/sendMessage?chat_id=" + std::to_string(chatId) +
        "&text=" + (escaped ? escaped : "");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    // ❗ ВАЖНО: ответ нам не нужен
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullptr);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cerr << "[sendMessage ERROR] "
                  << curl_easy_strerror(res) << std::endl;
    }

    if (escaped) curl_free(escaped);
    curl_easy_cleanup(curl);
}
