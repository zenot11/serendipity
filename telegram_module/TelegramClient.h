#pragma once

#include <string>
#include <vector>

class TelegramClient {
public:
    struct IncomingMessage {
        long chatId;
        std::string text;
    };

    TelegramClient(const std::string& token);

    std::vector<IncomingMessage> getUpdates();
    void sendMessage(long chatId, const std::string& text);

private:
    std::string token;
};
