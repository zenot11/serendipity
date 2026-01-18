#pragma once

#include <string>
#include <vector>

#include "Message.h"

// BotLogicClient = HTTP-клиент к компоненту Bot Logic (за Nginx в проде).
// TelegramClient вызывает его для:
// - обработки команд пользователя
// - периодических cron (login_check / notifications_check)

class BotLogicClient {
public:
    BotLogicClient();

    std::vector<Message> handleCommand(long long chat_id, const std::string& text);
    std::vector<Message> cronLoginCheck();
    std::vector<Message> cronNotificationsCheck();

private:
    std::string baseUrl;

    std::vector<Message> postJson(
        const std::string& path,
        long long fallback_chat_id,
        const std::string& json_body
    );
};

