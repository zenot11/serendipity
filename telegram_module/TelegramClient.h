#pragma once

#include <string>
#include <mutex>

#include "BotLogicClient.h"
#include "CommandParser.h"

class TelegramClient {
public:
    explicit TelegramClient(const std::string& token);
    ~TelegramClient();

    void poll();

private:
    std::string token;
    long long last_update_id = 0;

    BotLogicClient logic;
    CommandParser parser;
    std::once_flag cron_once;

    void startCronThreads();
    void sendMessage(long long chatId, const std::string& text);
};
