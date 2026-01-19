#include "db.h"

// Посмотреть информацию о пользователе (курсы, оценки, попытки)
crow::json::wvalue DB::getUserDataProfile(std::string userId, bool includeCourses, bool includeTests, bool includeGrades) {
    ensureConnection();
    crow::json::wvalue result;
    std::string idStr = userId;
    const char* paramValues[1] = { idStr.c_str() };

    if (includeCourses) {
        const char* sql = 
            "SELECT c.id, c.title, c.description "
            "FROM courses c "
            "JOIN course_students cs ON c.id = cs.course_id "
            "WHERE cs.user_id = $1 AND c.is_deleted = false";
        
        PGresult* res = PQexecParams((PGconn*)conn, sql, 1, nullptr, paramValues, nullptr, nullptr, 0);
        if (PQresultStatus(res) == PGRES_TUPLES_OK) {
            for (int i = 0; i < PQntuples(res); i++) {
                result["courses"][i]["id"] = std::stoi(PQgetvalue(res, i, 0));
                result["courses"][i]["title"] = PQgetvalue(res, i, 1);
                result["courses"][i]["description"] = PQgetvalue(res, i, 2);
            }
        } else {
            std::cerr << "Get user courses failed: " << PQerrorMessage((PGconn*)conn) << std::endl;
        }
        PQclear(res);
    }

    if (includeTests) {
        const char* sql = 
            "SELECT t.id, t.title, t.course_id "
            "FROM tests t "
            "JOIN course_students cs ON t.course_id = cs.course_id "
            "WHERE cs.user_id = $1 AND t.is_deleted = false AND t.is_active = true";

        PGresult* res = PQexecParams((PGconn*)conn, sql, 1, nullptr, paramValues, nullptr, nullptr, 0);
        if (PQresultStatus(res) == PGRES_TUPLES_OK) {
            for (int i = 0; i < PQntuples(res); i++) {
                result["tests"][i]["id"] = std::stoi(PQgetvalue(res, i, 0));
                result["tests"][i]["title"] = PQgetvalue(res, i, 1);
                result["tests"][i]["course_id"] = std::stoi(PQgetvalue(res, i, 2));
            }
        }
        PQclear(res);
    }

    if (includeGrades) {
        const char* sql = 
            "SELECT t.title, ta.score, ta.status, ta.created_at "
            "FROM test_attempts ta "
            "JOIN tests t ON ta.test_id = t.id "
            "WHERE ta.user_id = $1";

        PGresult* res = PQexecParams((PGconn*)conn, sql, 1, nullptr, paramValues, nullptr, nullptr, 0);
        if (PQresultStatus(res) == PGRES_TUPLES_OK) {
            for (int i = 0; i < PQntuples(res); i++) {
                result["grades"][i]["test_title"] = PQgetvalue(res, i, 0);
                result["grades"][i]["score"] = std::stod(PQgetvalue(res, i, 1));
                result["grades"][i]["status"] = PQgetvalue(res, i, 2);
                result["grades"][i]["date"] = PQgetvalue(res, i, 3);
            }
        }
        PQclear(res);
    }

    return result;
}
