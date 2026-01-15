#include "TelegramClient.h"
#include "CommandParser.h"
#include "BotLogicClient.h"

#include <unistd.h>
#include <cstdlib>
#include <iostream>

int main() {
    const char* token = getenv("TELEGRAM_BOT_TOKEN");
    if (!token) {
        std::cerr << "TELEGRAM_BOT_TOKEN not set\n";
        return 1;
    }

    TelegramClient telegram(token);
    CommandParser parser;
    BotLogicClient logic;

    while (true) {
        auto messages = telegram.getUpdates();

        for (auto& msg : messages) {
            Command cmd = parser.parse(msg.text);
            std::string reply = logic.handleCommand(cmd);
            telegram.sendMessage(msg.chatId, reply);
        }

        sleep(2);
    }
}

