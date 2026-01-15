#include "BotLogicClient.h"

std::string BotLogicClient::handleCommand(Command cmd) {
    switch (cmd) {
        case Command::START:
            return "Бот запущен. Используйте /help";
        case Command::STATUS:
            return "Система работает нормально";
        case Command::HELP:
            return "/start - запуск\n/status - статус\n/help - помощь";
        default:
            return "Неизвестная команда";
    }
}

