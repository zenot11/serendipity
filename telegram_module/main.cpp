#include "TelegramClient.h"
#include <cstdlib>
#include <iostream>

int main() {
    const char* token = std::getenv("TELEGRAM_BOT_TOKEN");
    if (!token) {
        std::cerr << "ERROR: TELEGRAM_BOT_TOKEN not set\n";
        return 1;
    }

    TelegramClient bot(token);
    bot.poll();

    return 0;
}


