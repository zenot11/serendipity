#pragma once
#include <string>

enum class Command {
    START,
    STATUS,
    HELP,
    PING,
    UNKNOWN
};

class CommandParser {
public:
    Command parse(const std::string& text);
};
