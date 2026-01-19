#include "db.h"
#include <iostream>

// Начать попытку
int DB::startTestAttempt(int testId, std::string userId) {
    ensureConnection();

    const char* checkSql = "SELECT is_active, question_ids FROM tests WHERE id = $1::int AND is_deleted = false";
    std::string tId = std::to_string(testId);
    const char* tParams[] = { tId.c_str() };
    PGresult* testRes = PQexecParams((PGconn*)conn, checkSql, 1, nullptr, tParams, nullptr, nullptr, 0);

    if (PQresultStatus(testRes) != PGRES_TUPLES_OK || PQntuples(testRes) == 0) {
        PQclear(testRes);
        return -1;
    }
    bool isActive = (std::string(PQgetvalue(testRes, 0, 0)) == "t");
    PQclear(testRes);
    if (!isActive) return -2;

    const char* createSql = 
        "INSERT INTO test_attempts (user_id, test_id, questions_snapshot, user_answers, status, score) "
        "SELECT $1, $2::int, to_jsonb(question_ids), "
        "       (SELECT jsonb_object_agg(q_id, -1) FROM unnest(question_ids) AS q_id), "
        "       'in_progress', 0.0 "
        "FROM tests WHERE id = $2::int "
        "ON CONFLICT (user_id, test_id) DO NOTHING "
        "RETURNING id";

    const char* aParams[] = { userId.c_str(), tId.c_str() };
    PGresult* res = PQexecParams((PGconn*)conn, createSql, 2, nullptr, aParams, nullptr, nullptr, 0);
    
    int attemptId = -1;
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        attemptId = std::stoi(PQgetvalue(res, 0, 0));
    } else {
        if (PQntuples(res) == 0) attemptId = -3;
        else std::cerr << "Start attempt failed: " << PQerrorMessage((PGconn*)conn) << std::endl;
    }
    
    PQclear(res);
    return attemptId;
}

// Изменить значение ответа
bool DB::updateAttemptAnswer(int attemptId, int questionId, int answerIndex) {
    ensureConnection();
    
    std::string attIdStr = std::to_string(attemptId);
    std::string qIdStr = std::to_string(questionId);

    const char* existSql = "SELECT 1 FROM questions WHERE id = $1::int LIMIT 1";
    const char* existParams[] = { qIdStr.c_str() };
    PGresult* existRes = PQexecParams((PGconn*)conn, existSql, 1, nullptr, existParams, nullptr, nullptr, 0);
    
    if (PQresultStatus(existRes) != PGRES_TUPLES_OK || PQntuples(existRes) == 0) {
        std::cerr << "Validation failed: Question ID " << questionId << " does not exist." << std::endl;
        PQclear(existRes);
        return false;
    }
    PQclear(existRes);
    
    const char* checkSql = 
        "SELECT ta.user_answers->>$2::text, q.correct_option "
        "FROM test_attempts ta "
        "JOIN questions q ON q.id = $2::int "
        "WHERE ta.id = $1::int";
    
    const char* params[] = { attIdStr.c_str(), qIdStr.c_str() };
    PGresult* resCheck = PQexecParams((PGconn*)conn, checkSql, 2, nullptr, params, nullptr, nullptr, 0);

    double scoreDelta = 0.0;

    if (PQresultStatus(resCheck) == PGRES_TUPLES_OK && PQntuples(resCheck) > 0) {
        std::string oldAnswer = PQgetvalue(resCheck, 0, 0);
        int correctOption = std::stoi(PQgetvalue(resCheck, 0, 1));
        
        bool wasCorrect = (!oldAnswer.empty() && std::stoi(oldAnswer) == correctOption);
        bool isNowCorrect = (answerIndex == correctOption);

        if (!wasCorrect && isNowCorrect) scoreDelta = 1.0;
        if (wasCorrect && !isNowCorrect) scoreDelta = -1.0;
    }
    PQclear(resCheck);

    std::string updateSql = 
        "UPDATE test_attempts "
        "SET user_answers = user_answers || jsonb_build_object($2::text, $3::int), "
        "    score = score + $4::float "
        "WHERE id = $1::int AND status = 'in_progress'";

    std::string deltaStr = std::to_string(scoreDelta);
    std::string ansIdxStr = std::to_string(answerIndex);
    const char* updateParams[] = { attIdStr.c_str(), qIdStr.c_str(), ansIdxStr.c_str(), deltaStr.c_str() };

    PGresult* res = PQexecParams((PGconn*)conn, updateSql.c_str(), 4, nullptr, updateParams, nullptr, nullptr, 0);
    
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK && std::string(PQcmdTuples(res)) == "1");
    PQclear(res);
    return success;
}

// Список пользователей прошедших тест
std::vector<std::string> DB::getUsersWhoPassedTest(int testId) {
    ensureConnection();
    std::string tId = std::to_string(testId);
    const char* params[] = { tId.c_str() };

    const char* sql = 
        "SELECT DISTINCT user_id FROM test_attempts "
        "WHERE test_id = $1::int AND status = 'completed'";

    PGresult* res = PQexecParams((PGconn*)conn, sql, 1, nullptr, params, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Get passed users failed: " << PQerrorMessage((PGconn*)conn) << std::endl;
        PQclear(res);
        return {};
    }

    std::vector<std::string> userIds;
    int rows = PQntuples(res);
    userIds.reserve(rows);

    for (int i = 0; i < rows; i++) {
        userIds.push_back(PQgetvalue(res, i, 0));
    }

    PQclear(res);
    return userIds;
}

// Получить оценку пользователей (или себя)
std::vector<UserScore> DB::getTestScores(int testId, std::string userIdFilter, bool isAuthor) {
    ensureConnection();
    std::string tId = std::to_string(testId);
    
    std::string sql = "SELECT user_id, score FROM test_attempts WHERE test_id = $1::int AND status = 'completed'";
    std::vector<const char*> params = { tId.c_str() };

    if (!isAuthor) {
        sql += " AND user_id = $2";
        params.push_back(userIdFilter.c_str());
    }

    PGresult* res = PQexecParams((PGconn*)conn, sql.c_str(), (int)params.size(), nullptr, params.data(), nullptr, nullptr, 0);
    std::vector<UserScore> scores;

    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            scores.push_back({
                PQgetvalue(res, i, 0),
                std::stod(PQgetvalue(res, i, 1))
            });
        }
    }
    PQclear(res);
    return scores;
}

// Посмотреть ответы пользователей (пользователя)
std::vector<AttemptDetails> DB::getTestAttemptDetails(int testId, std::string userIdFilter, bool isAuthor) {
    ensureConnection();
    std::string tId = std::to_string(testId);
    std::string sql = "SELECT user_id, user_answers FROM test_attempts WHERE test_id = $1::int";
    std::vector<const char*> params = { tId.c_str() };

    if (!isAuthor) {
        sql += " AND user_id = $2";
        params.push_back(userIdFilter.c_str());
    }

    PGresult* res = PQexecParams((PGconn*)conn, sql.c_str(), (int)params.size(), nullptr, params.data(), nullptr, nullptr, 0);
    std::vector<AttemptDetails> result;

    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            AttemptDetails details;
            details.user_id = PQgetvalue(res, i, 0);
            
            auto userAnswers = crow::json::load(PQgetvalue(res, i, 1));
            if (userAnswers && userAnswers.t() == crow::json::type::Object) {
                for (auto& key : userAnswers.keys()) {
                    std::string aVal;
                    auto& node = userAnswers[key];
                    if (node.t() == crow::json::type::String) aVal = node.s();
                    else if (node.t() == crow::json::type::Number) aVal = std::to_string(node.i());
                    else aVal = "[other type]";
                    
                    details.answers.push_back({ "Question ID: " + key, aVal });
                }
            }
            result.push_back(details);
        }
    }
    PQclear(res);
    return result;
}

// Проверка на владение попыткой
bool DB::isAttemptOwnedBy(int attemptId, std::string userId) {
    ensureConnection();
    const char* sql = "SELECT 1 FROM test_attempts WHERE id = $1::int AND user_id = $2";
    std::string attId = std::to_string(attemptId);
    const char* params[] = { attId.c_str(), userId.c_str() };
    PGresult* res = PQexecParams((PGconn*)conn, sql, 2, nullptr, params, nullptr, nullptr, 0);
    bool owned = (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0);
    PQclear(res);
    return owned;
}

// Завершить попытку
bool DB::completeAttempt(int attemptId) {
    ensureConnection();
    std::string attId = std::to_string(attemptId);
    const char* params[] = { attId.c_str() };
    const char* sql = "UPDATE test_attempts SET status = 'completed' WHERE id = $1::int AND status = 'in_progress'";
    PGresult* res = PQexecParams((PGconn*)conn, sql, 1, nullptr, params, nullptr, nullptr, 0);
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK && std::string(PQcmdTuples(res)) == "1");
    PQclear(res);
    return success;
}

// Посмотреть попытку
crow::json::wvalue DB::getAttemptData(int testId, std::string userId) {
    ensureConnection();
    std::string tId = std::to_string(testId);
    const char* params[] = { userId.c_str(), tId.c_str() };
    const char* sql = "SELECT status, user_answers, questions_snapshot FROM test_attempts WHERE user_id = $1 AND test_id = $2::int";
    PGresult* res = PQexecParams((PGconn*)conn, sql, 2, nullptr, params, nullptr, nullptr, 0);

    crow::json::wvalue result;
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        result["status"] = PQgetvalue(res, 0, 0);
        result["user_answers"] = crow::json::load(PQgetvalue(res, 0, 1));
        result["questions_snapshot"] = crow::json::load(PQgetvalue(res, 0, 2));
    } else {
        result = nullptr;
    }
    PQclear(res);
    return result;
}

// Состояние ответов
crow::json::wvalue DB::getAttemptAnswers(int testId, std::string userId) {
    ensureConnection();
    
    std::string tId = std::to_string(testId);
    const char* params[] = { userId.c_str(), tId.c_str() };

    const char* sql = 
        "SELECT status, user_answers "
        "FROM test_attempts WHERE user_id = $1 AND test_id = $2::int";

    PGresult* res = PQexecParams((PGconn*)conn, sql, 2, nullptr, params, nullptr, nullptr, 0);

    crow::json::wvalue result;
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        result["status"] = PQgetvalue(res, 0, 0);
        result["answers"] = crow::json::load(PQgetvalue(res, 0, 1));
    } else {
        result = nullptr; 
    }

    PQclear(res);
    return result;
}