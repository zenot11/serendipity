#pragma once

#include <string>

// Telegram Client обязан отвечать "Нет такой команды" на неизвестные сообщения
// ДО пересылки в Bot Logic (по task_flow). Поэтому здесь определяем,
// какие команды считаем предусмотренными.

enum class Command {
    START,
    STATUS,
    HELP,
    PING,
    LOGIN,
    LOGOUT,
    NOTIFICATION,
    CMD,
    UNKNOWN
};

class CommandParser {
public:
    Command parse(const std::string& text) const;

    static bool isSupported(Command cmd) {
        return cmd != Command::UNKNOWN;
    }
};
