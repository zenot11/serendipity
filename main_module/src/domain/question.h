#pragma once
#include <string>
#include <vector>

struct Question {
    int id = 0;
    int version = 1;
    std::string author_id;
    std::string title;
    std::string content;
    std::vector<std::string> options;
    int correct_option = 0;
    bool is_deleted = false;

    crow::json::wvalue to_json() const {
        crow::json::wvalue j;
        j["id"] = id;
        j["version"] = version;
        j["author_id"] = author_id;
        j["title"] = title;
        j["content"] = content;
        
        crow::json::wvalue::list optList;
        for (const auto& opt : options) {
            optList.push_back(crow::json::wvalue(opt));
        }
        j["options"] = std::move(optList);
        
        j["correct_option"] = correct_option;
        j["is_deleted"] = is_deleted;
        return j;
    }
};
