#include "CommandParser.h"

Command CommandParser::parse(const std::string& text) {
    if (text == "/start") return Command::START;
    if (text == "/status") return Command::STATUS;
    if (text == "/help") return Command::HELP;
    return Command::UNKNOWN;
}
