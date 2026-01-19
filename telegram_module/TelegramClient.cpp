#include "TelegramClient.h"

#include <chrono>
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <thread>

#include "json.hpp"

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    std::string* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), total);
    return total;
}

TelegramClient::TelegramClient(const std::string& botToken)
    : token(botToken),
      apiBase("https://api.telegram.org/bot" + botToken),
      last_update_id(0),
      running(false),
      cron_running(false) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

TelegramClient::~TelegramClient() {
    stop();
    curl_global_cleanup();
}

void TelegramClient::startCronThreadsOnce() {
    if (cron_running) return;
    cron_running = true;

    cronLoginThread = std::thread([this]() { cronLoginLoop(); });
    cronNotificationsThread = std::thread([this]() { cronNotificationsLoop(); });
}

void TelegramClient::cronLoginLoop() {
    while (cron_running) {
        try {
            auto msgs = logic.cronLoginCheck();
            for (auto& m : msgs) sendMessage(m.chat_id, m.text);
        } catch (...) {
            // ignore
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

void TelegramClient::cronNotificationsLoop() {
    while (cron_running) {
        try {
            auto msgs = logic.cronNotificationsCheck();
            for (auto& m : msgs) sendMessage(m.chat_id, m.text);
        } catch (...) {
            // ignore
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void TelegramClient::stop() {
    running = false;

    cron_running = false;
    if (cronLoginThread.joinable()) cronLoginThread.join();
    if (cronNotificationsThread.joinable()) cronNotificationsThread.join();
}

void TelegramClient::sendMessage(long long chatId, const std::string& text) {
    std::lock_guard<std::mutex> lk(send_m);

    CURL* curl = curl_easy_init();
    if (!curl) return;

    std::string url = apiBase + "/sendMessage";

    json body;
    body["chat_id"] = chatId;
    body["text"] = text;

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // FIX: "HTTP2 framing layer" -> форсим HTTP/1.1
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

#ifdef CURLOPT_SSL_ENABLE_ALPN
    // VPN/прокси иногда ломают ALPN/HTTP2 negotiation
    curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 0L);
#endif

    // чтобы libcurl не использовал сигналы (важно в многопоточке)
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    std::string payload = body.dump();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[sendMessage ERROR] " << curl_easy_strerror(res) << "\n";
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}

void TelegramClient::poll() {
    running = true;
    startCronThreadsOnce();
    std::cout << "[telegram_bot] Polling started. Waiting for updates..." << std::endl;

    while (running) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        std::ostringstream oss;
        oss << apiBase << "/getUpdates?timeout=25&allowed_updates=%5B%22message%22%5D";
        if (last_update_id > 0) {
            oss << "&offset=" << (last_update_id + 1);
        }
        std::string url = oss.str();

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // FIX: "HTTP2 framing layer" -> форсим HTTP/1.1
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

#ifdef CURLOPT_SSL_ENABLE_ALPN
        // VPN/прокси иногда ломают ALPN/HTTP2 negotiation
        curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 0L);
#endif

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        // long-poll может резаться VPN/прокси; держим общий таймаут чуть больше server-side timeout=25
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 70L);
        // чтобы libcurl не использовал сигналы (важно в многопоточке)
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OPERATION_TIMEDOUT) {
            // Для long-poll это нормальная ситуация (особенно с VPN): просто повторяем цикл без спама в лог.
            curl_easy_cleanup(curl);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }
        if (res != CURLE_OK) {
            std::cerr << "[poll ERROR] " << curl_easy_strerror(res) << "\n";
            curl_easy_cleanup(curl);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
        curl_easy_cleanup(curl);

        try {
            json j = json::parse(response);
            if (!j.value("ok", false) || !j.contains("result")) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                continue;
            }

            static int idleTicks = 0;
            if (j["result"].empty()) {
                // Чтобы было видно, что бот жив и крутит polling
                if (++idleTicks % 20 == 0) {
                    std::cout << "[telegram_bot] still running..." << std::endl;
                }
            } else {
                idleTicks = 0;
            }

            for (auto& upd : j["result"]) {
                long long update_id = upd.value("update_id", 0LL);
                if (update_id > last_update_id) last_update_id = update_id;

                if (!upd.contains("message")) continue;
                auto& msg = upd["message"];

                if (!msg.contains("chat") || !msg["chat"].contains("id")) continue;
                long long chatId = msg["chat"]["id"].get<long long>();
                std::string text = msg.value("text", "");
                if (text.empty()) continue;
                std::cout << "[telegram_bot] incoming from chat=" << chatId << ": " << text << std::endl;

                // ВАЖНО: CommandParser у тебя возвращает enum, а не объект.
                // Поэтому никакого cmd.raw / cmd.name.
                // Логику делаем так:
                // - если это команда (начинается с '/'), проверяем известность
                // - неизвестные -> "Нет такой команды"
                // - известные и обычный текст -> отправляем в bot_logic как есть

                // Не фильтруем неизвестные команды здесь.
                // Любой текст (включая /что-то) отправляем в bot_logic: там решат,
                // это локальная команда (/login, /logout, /notification) или надо форвардить в Main.

                auto answers = logic.handleCommand(chatId, text);
                for (auto& a : answers) {
                    sendMessage(a.chat_id, a.text);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[poll JSON ERROR] " << e.what() << "\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}