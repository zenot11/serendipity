#pragma once

#include <string>

struct Message {
    long long chat_id{};
    std::string text;
};
