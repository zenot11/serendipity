#pragma once

#include <string>
#include "BotLogicClient.h"

class TelegramClient {
public:
    explicit TelegramClient(const std::string& token);
    ~TelegramClient();                 // ← ВАЖНО: объявил деструктор

    void poll();

private:
    std::string token;
    long last_update_id = 0;
    BotLogicClient logic;

    void sendMessage(long chatId, const std::string& text);
};
