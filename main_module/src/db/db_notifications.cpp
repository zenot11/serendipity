#include "db.h"


// Сохранить уведомление в базе
void DB::pushNotification(
    const std::string& userId, 
    const std::string& type, 
    const std::string& title, 
    const std::string& message, 
    const crow::json::wvalue& payload
) {
    ensureConnection();

    std::string payloadStr = payload.dump();
    if (payloadStr == "null") payloadStr = "{}";

    const char* paramValues[5];
    paramValues[0] = userId.c_str();
    paramValues[1] = type.c_str();
    paramValues[2] = title.c_str();
    paramValues[3] = message.c_str();
    paramValues[4] = payloadStr.c_str();

    const char* query = 
        "INSERT INTO notifications (user_id, type, title, message, payload) "
        "VALUES ($1, $2, $3, $4, $5::jsonb)";

    PGresult* res = PQexecParams(
        (PGconn*)conn,
        query,
        5,
        NULL,
        paramValues,
        NULL,
        NULL,
        0 
    );

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Error add notification: " << PQerrorMessage((PGconn*)conn) << std::endl;
    }

    PQclear(res);
}
// Удалить уведомления (мягкое удаление)
void DB::markNotificationsAsSent(const std::vector<int>& ids, std::string userId) {
    if (ids.empty()) return;

    std::string pg_array = "{";
    for (size_t i = 0; i < ids.size(); ++i) {
        pg_array += std::to_string(ids[i]);
        if (i < ids.size() - 1) pg_array += ",";
    }
    pg_array += "}";

    const char* paramValues[2];
    
    paramValues[0] = pg_array.c_str();
    paramValues[1] = userId.c_str();

    const char* sql = "UPDATE notifications SET is_sent_tg = TRUE WHERE id = ANY($1::int[]) AND user_id = $2";

    PGresult* res = PQexecParams((PGconn*)conn, sql, 2, nullptr, paramValues, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        CROW_LOG_ERROR << "DB Error (markNotificationsAsSent): " << PQerrorMessage((PGconn*)conn);
    }

    PQclear(res);
}
// Получить список уведомлений
std::vector<crow::json::wvalue> DB::getUnsentNotifications(std::string userId) {
    std::vector<crow::json::wvalue> notifications;
    
    const char* paramValues[1];
    paramValues[0] = userId.c_str();

    const char* sql = "SELECT id, type, title, message, payload FROM notifications "
                      "WHERE user_id = $1 AND is_sent_tg = FALSE";

    PGresult* res = PQexecParams((PGconn*)conn, sql, 1, nullptr, paramValues, nullptr, nullptr, 0);

    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; i++) {
            crow::json::wvalue n;
            n["id"] = std::stoi(PQgetvalue(res, i, 0));
            n["type"] = PQgetvalue(res, i, 1);
            n["title"] = PQgetvalue(res, i, 2);
            n["message"] = PQgetvalue(res, i, 3);
            
            std::string payloadStr = PQgetvalue(res, i, 4);
            n["payload"] = crow::json::load(payloadStr);
            
            notifications.push_back(std::move(n));
        }
    }
    PQclear(res);
    return notifications;
}