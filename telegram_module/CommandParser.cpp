#include "CommandParser.h"

#include <cctype>

static std::string first_token(const std::string& text) {
    // Берем первое "слово" (до пробела)
    std::string t = text;
    auto pos = t.find(' ');
    if (pos != std::string::npos) t = t.substr(0, pos);

    // Убираем ведущий '/'
    if (!t.empty() && t[0] == '/') t = t.substr(1);

    // Приводим к lower (ascii команды)
    for (char& c : t) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return t;
}

Command CommandParser::parse(const std::string& text) const {
    const std::string cmd = first_token(text);

    if (cmd == "start") return Command::START;
    if (cmd == "status") return Command::STATUS;
    if (cmd == "help") return Command::HELP;
    if (cmd == "ping") return Command::PING;

    // требования task_flow (Telegram):
    if (cmd == "login") return Command::LOGIN;
    if (cmd == "logout") return Command::LOGOUT;

    // удобно для ручной проверки
    if (cmd == "notification" || cmd == "notifications") return Command::NOTIFICATION;
    if (cmd == "cmd") return Command::CMD;

    return Command::UNKNOWN;
}
