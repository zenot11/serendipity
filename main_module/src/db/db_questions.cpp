#include "db.h"

// Создание вопроса
int DB::createQuestion(std::string authorId, const std::string& title, const std::string& content, 
                       const std::vector<std::string>& options, int correctOption) {
    ensureConnection();

    crow::json::wvalue::list optList;
    for (const auto& opt : options) {
        optList.push_back(crow::json::wvalue(opt));
    }
    crow::json::wvalue optionsJson(optList);
    std::string optionsStr = optionsJson.dump();

    const char* sql = 
        "INSERT INTO questions (id, version, author_id, title, content, options, correct_option) "
        "VALUES (nextval('questions_id_seq'), 1, $1, $2, $3, $4, $5) "
        "RETURNING id";

    std::string aId = authorId;
    std::string cOpt = std::to_string(correctOption);
    const char* params[] = { aId.c_str(), title.c_str(), content.c_str(), optionsStr.c_str(), cOpt.c_str() };

    PGresult* res = PQexecParams((PGconn*)conn, sql, 5, nullptr, params, nullptr, nullptr, 0);

    int newId = -1;
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        newId = std::stoi(PQgetvalue(res, 0, 0));
    }
    PQclear(res);
    return newId;
}

// Удаление вопроса
bool DB::deleteQuestion(int questionId) {
    ensureConnection();
    std::string qIdStr = std::to_string(questionId);
    const char* params[] = { qIdStr.c_str() };

    const char* checkSql = "SELECT 1 FROM test_questions WHERE question_id = $1 LIMIT 1";
    PGresult* checkRes = PQexecParams((PGconn*)conn, checkSql, 1, nullptr, params, nullptr, nullptr, 0);
    bool isUsed = (PQntuples(checkRes) > 0);
    PQclear(checkRes);

    if (isUsed) return false; 

    const char* sql = "UPDATE questions SET is_deleted = true WHERE id = $1";
    PGresult* res = PQexecParams((PGconn*)conn, sql, 1, nullptr, params, nullptr, nullptr, 0);
    
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

// Изменить вопрос
int DB::updateQuestion(int questionId, std::string userId, const std::string& title, 
                       const std::string& content, const std::vector<std::string>& options, 
                       int correctOption) {
    ensureConnection();
    std::string qId = std::to_string(questionId);
    const char* params[] = { qId.c_str() };

    const char* verSql = "SELECT MAX(version), author_id FROM questions WHERE id = $1 GROUP BY author_id";
    PGresult* res = PQexecParams((PGconn*)conn, verSql, 1, nullptr, params, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return -1;
    }

    int currentVersion = std::stoi(PQgetvalue(res, 0, 0));
    std::string authorId = PQgetvalue(res, 0, 1);
    PQclear(res);


    crow::json::wvalue::list optList;
    for (const auto& opt : options) {
        optList.push_back(crow::json::wvalue(opt));
    }
    crow::json::wvalue optionsJson(optList);
    std::string optionsStr = optionsJson.dump();
    std::string newVer = std::to_string(currentVersion + 1);
    std::string aId = authorId;
    std::string cOpt = std::to_string(correctOption);

    const char* insertSql = 
        "INSERT INTO questions (id, version, author_id, title, content, options, correct_option) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING version";

    const char* insParams[] = { qId.c_str(), newVer.c_str(), aId.c_str(), 
                                title.c_str(), content.c_str(), optionsStr.c_str(), cOpt.c_str() };

    PGresult* insRes = PQexecParams((PGconn*)conn, insertSql, 7, nullptr, insParams, nullptr, nullptr, 0);
    
    int createdVersion = -1;
    if (PQresultStatus(insRes) == PGRES_TUPLES_OK) {
        createdVersion = std::stoi(PQgetvalue(insRes, 0, 0));
    }
    PQclear(insRes);

    return createdVersion;
}

// Получить детали вопроса
Question DB::getQuestionByIdAndVersion(int questionId, int version) {
    ensureConnection();
    std::string qId = std::to_string(questionId);
    std::string ver = std::to_string(version);
    const char* params[] = { qId.c_str(), ver.c_str() };

    const char* sql = 
        "SELECT id, version, author_id, title, content, options, correct_option, is_deleted "
        "FROM questions WHERE id = $1::int AND version = $2::int";

    PGresult* res = PQexecParams((PGconn*)conn, sql, 2, nullptr, params, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return {}; 
    }

    Question q;
    q.id = std::stoi(PQgetvalue(res, 0, 0));
    q.version = std::stoi(PQgetvalue(res, 0, 1));
    q.author_id = PQgetvalue(res, 0, 2);
    q.title = PQgetvalue(res, 0, 3);
    q.content = PQgetvalue(res, 0, 4);
    auto optionsJson = crow::json::load(PQgetvalue(res, 0, 5));
    if (optionsJson) for (auto& opt : optionsJson) q.options.push_back(opt.s());
    q.correct_option = std::stoi(PQgetvalue(res, 0, 6));
    q.is_deleted = (std::string(PQgetvalue(res, 0, 7)) == "t");

    PQclear(res);
    return q;
}

// Получить список вопросов
std::vector<Question> DB::getQuestionsList(std::string userId, bool canSeeAll) {
    ensureConnection();
    
    std::string sql = 
        "SELECT DISTINCT ON (id) id, version, author_id, title "
        "FROM questions "
        "WHERE is_deleted = false ";

    PGresult* res = nullptr;
    if (!canSeeAll) {
        sql += " AND author_id = $1";
        sql += " ORDER BY id, version DESC";
        const char* params[] = { userId.c_str() };
        res = PQexecParams((PGconn*)conn, sql.c_str(), 1, nullptr, params, nullptr, nullptr, 0);
    } else {
        sql += " ORDER BY id, version DESC";
        res = PQexec((PGconn*)conn, sql.c_str());
    }
    

    std::vector<Question> list;

    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            Question q;
            q.id = std::stoi(PQgetvalue(res, i, 0));
            q.version = std::stoi(PQgetvalue(res, i, 1));
            q.author_id = PQgetvalue(res, i, 2);
            q.title = PQgetvalue(res, i, 3);
            list.push_back(q);
        }
    }
    if (res) {
        PQclear(res);
    }
    return list;
}

// Получить вопрос по айди
Question DB::getQuestionById(int questionId) {
    ensureConnection();
    
    std::string qIdStr = std::to_string(questionId);
    const char* params[] = { qIdStr.c_str() };

    const char* sql = 
        "SELECT id, version, author_id, title, content, options, correct_option, is_deleted "
        "FROM questions "
        "WHERE id = $1 "
        "ORDER BY version DESC LIMIT 1";

    PGresult* res = PQexecParams(
        (PGconn*)conn,
        sql,
        1, nullptr, params, nullptr, nullptr, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return {};
    }

    Question q;
    q.id = std::stoi(PQgetvalue(res, 0, 0));
    q.version = std::stoi(PQgetvalue(res, 0, 1));
    q.author_id = PQgetvalue(res, 0, 2);
    q.title = PQgetvalue(res, 0, 3);
    q.content = PQgetvalue(res, 0, 4);

    auto optionsJson = crow::json::load(PQgetvalue(res, 0, 5));
    if (optionsJson) {
        for (auto& opt : optionsJson) {
            q.options.push_back(opt.s());
        }
    }

    q.correct_option = std::stoi(PQgetvalue(res, 0, 6));
    q.is_deleted = (std::string(PQgetvalue(res, 0, 7)) == "t");

    PQclear(res);
    return q;
}

// Была ли попытка пройти тест с конекретный вопросом
bool DB::hasUserAttemptForQuestion(std::string userId, int questionId) {
    ensureConnection();
    std::string uId = userId;
    std::string qId = std::to_string(questionId);
    const char* params[] = { uId.c_str(), qId.c_str() };

    const char* sql = 
        "SELECT 1 FROM test_attempts ta "
        "JOIN tests t ON ta.test_id = t.id "
        "WHERE ta.user_id = $1 "
        "AND $2::int = ANY(t.question_ids) "
        "LIMIT 1";

    PGresult* res = PQexecParams((PGconn*)conn, sql, 2, nullptr, params, nullptr, nullptr, 0);
    bool exists = false;
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        exists = (PQntuples(res) > 0);
    }
    if (res) PQclear(res);
    return exists;
}

