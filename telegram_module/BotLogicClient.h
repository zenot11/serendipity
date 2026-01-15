#pragma once
#include <string>
#include "CommandParser.h"

class BotLogicClient {
public:
    std::string handleCommand(Command cmd);
};
