#pragma once
#include <string>
#include <vector>

struct Test {
    int id;
    int course_id;
    std::string title;
    bool is_active;
    bool is_deleted;
};

struct UserAnswer {
    std::string question_text;
    std::string user_answer_text;
};

struct AttemptDetails {
    std::string user_id;
    std::vector<UserAnswer> answers;
};