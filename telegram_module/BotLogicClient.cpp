#include "BotLogicClient.h"

#include <curl/curl.h>
#include <cstdlib>
#include <string>

#include "json.hpp"

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    std::string* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), total);
    return total;
}

static std::string getenv_or(const char* key, const std::string& def) {
    if (const char* v = std::getenv(key)) return std::string(v);
    return def;
}

BotLogicClient::BotLogicClient()
    : baseUrl(getenv_or("BOT_LOGIC_URL", "http://localhost:8080")) {}

std::vector<Message> BotLogicClient::handleCommand(long long chat_id, const std::string& text) {
    json body = {
        {"chat_id", chat_id},
        {"text", text}
    };
    return postJson("/bot/handle", chat_id, body.dump());
}

std::vector<Message> BotLogicClient::cronLoginCheck() {
    json body = json::object();
    return postJson("/bot/cron/login_check", 0, body.dump());
}

std::vector<Message> BotLogicClient::cronNotificationsCheck() {
    json body = json::object();
    return postJson("/bot/cron/notifications_check", 0, body.dump());
}

std::vector<Message> BotLogicClient::postJson(
    const std::string& path,
    long long fallback_chat_id,
    const std::string& json_body
) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        if (fallback_chat_id != 0) {
            return { Message{fallback_chat_id, "Ошибка: curl_easy_init()"} };
        }
        return {};
    }

    std::string response;
    std::string url = baseUrl + path;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)json_body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        if (fallback_chat_id != 0) {
            std::string msg = "Ошибка: Bot Logic недоступен (";
            msg += curl_easy_strerror(res);
            msg += ")";
            return { Message{fallback_chat_id, msg} };
        }
        return {};
    }

    if (response.empty()) {
        if (fallback_chat_id != 0) {
            return { Message{fallback_chat_id, "Пустой ответ от Bot Logic"} };
        }
        return {};
    }

    // Ожидаемый формат:
    // { "messages": [ {"chat_id":..., "text":"..."}, ...] }
    try {
        auto j = json::parse(response);

        if (j.contains("messages") && j["messages"].is_array()) {
            std::vector<Message> out;
            for (const auto& m : j["messages"]) {
                Message msg;
                msg.chat_id = m.value("chat_id", 0LL);
                msg.text = m.value("text", "");
                if (msg.chat_id != 0 && !msg.text.empty()) {
                    out.push_back(std::move(msg));
                }
            }
            return out;
        }

        // запасной формат: {"text":"..."}
        if (j.contains("text") && j["text"].is_string() && fallback_chat_id != 0) {
            return { Message{fallback_chat_id, j["text"].get<std::string>()} };
        }
    } catch (...) {
        // fallback ниже
    }

    // fallback: трактуем как обычный текст
    if (fallback_chat_id != 0) {
        return { Message{fallback_chat_id, response} };
    }
    return {};
}

