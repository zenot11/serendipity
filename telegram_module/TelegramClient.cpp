#include "TelegramClient.h"

#include <curl/curl.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <thread>

#include "json.hpp"

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

static int getenv_int(const char* key, int def) {
    if (const char* v = std::getenv(key)) {
        try { return std::stoi(v); } catch (...) { return def; }
    }
    return def;
}

TelegramClient::TelegramClient(const std::string& token)
    : token(token), last_update_id(0) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

TelegramClient::~TelegramClient() {
    curl_global_cleanup();
}

void TelegramClient::startCronThreads() {
    std::call_once(cron_once, [&]() {
        const int loginInterval = getenv_int("LOGIN_CHECK_INTERVAL", 10);
        const int notifInterval = getenv_int("NOTIFY_CHECK_INTERVAL", 30);

        // Cron: login_check
        std::thread([this, loginInterval]() {
            while (true) {
                try {
                    auto msgs = logic.cronLoginCheck();
                    for (const auto& m : msgs) {
                        sendMessage(m.chat_id, m.text);
                    }
                } catch (...) {
                }
                std::this_thread::sleep_for(std::chrono::seconds(loginInterval));
            }
        }).detach();

        // Cron: notifications_check
        std::thread([this, notifInterval]() {
            while (true) {
                try {
                    auto msgs = logic.cronNotificationsCheck();
                    for (const auto& m : msgs) {
                        sendMessage(m.chat_id, m.text);
                    }
                } catch (...) {
                }
                std::this_thread::sleep_for(std::chrono::seconds(notifInterval));
            }
        }).detach();
    });
}

void TelegramClient::poll() {
    startCronThreads();

    while (true) {
        std::string response;
        CURL* curl = curl_easy_init();

        if (!curl) {
            std::cerr << "[poll ERROR] curl_easy_init failed\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        std::ostringstream url;
        url << "https://api.telegram.org/bot" << token
            << "/getUpdates?timeout=30&offset=" << last_update_id;

        curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 35L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cerr << "[poll ERROR] " << curl_easy_strerror(res) << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        if (response.empty()) continue;

        try {
            auto data = json::parse(response);
            if (!data.contains("result")) continue;

            for (const auto& update : data["result"]) {
                last_update_id = update["update_id"].get<long long>() + 1;

                if (!update.contains("message")) continue;
                const auto& msg = update["message"];
                if (!msg.contains("text")) continue;

                long long chatId = msg["chat"]["id"].get<long long>();
                std::string text = msg["text"].get<std::string>();

                // ВАЖНО по task_flow:
                // Telegram Client обязан сам отфильтровать неизвестные команды
                // и ответить: "Нет такой команды".
                Command cmd = parser.parse(text);
                if (!CommandParser::isSupported(cmd)) {
                    sendMessage(chatId, "Нет такой команды");
                    continue;
                }

                // Известная команда -> пересылаем в Bot Logic
                auto replies = logic.handleCommand(chatId, text);
                if (replies.empty()) {
                    sendMessage(chatId, "Команда принята.");
                    continue;
                }

                for (const auto& r : replies) {
                    sendMessage(r.chat_id, r.text);
                }
            }

        } catch (const json::parse_error& e) {
            std::cerr << "[json parse error] " << e.what() << std::endl;
            std::cerr << "Response was:\n" << response << std::endl;
        }
    }
}

void TelegramClient::sendMessage(long long chatId, const std::string& text) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    char* escaped = curl_easy_escape(curl, text.c_str(), (int)text.length());

    std::string url =
        "https://api.telegram.org/bot" + token +
        "/sendMessage?chat_id=" + std::to_string(chatId) +
        "&text=" + (escaped ? escaped : "");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullptr);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[sendMessage ERROR] " << curl_easy_strerror(res) << std::endl;
    }

    if (escaped) curl_free(escaped);
    curl_easy_cleanup(curl);
}
