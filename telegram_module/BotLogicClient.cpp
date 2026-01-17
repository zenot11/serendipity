#include "BotLogicClient.h"

std::string BotLogicClient::handleCommand(const std::string& text) {
    if (text == "/start") {
        return "Привет пидор! Я Telegram бот на C++ 🚀";
    }

    if (text == "/help") {
        return
            "/start — начать\n"
            "/help — помощь\n"
            "/ping — проверить связь\n"
            "/status — статус бота";
    }

    if (text == "/ping") {
        return "pong 🏓";
    }

    if (text == "/status") {
        return "Бот работает ✅";
    }

    return "Неизвестная команда 🤔";
}

