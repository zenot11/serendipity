#include "crow.h"
#include "db/db.h"
#include "handlers/base_handler.h"
#include <cstdlib>

int main() {
    crow::SimpleApp app;
    const char* env_conn = std::getenv("DB_CONNINFO");

    if (!env_conn) {
        std::cerr << "CRITICAL ERROR: DB_CONNINFO not found in environment!" << std::endl;
        return 1; 
    }

    DB db(env_conn);

    // Проверка активации
    CROW_ROUTE(app, "/health")([] {
        return "OK";
    });

    registerRoutes(app, db);

    app.port(18080).multithreaded().run();
}
