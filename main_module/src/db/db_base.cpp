#include "db.h"

// Конструктор
DB::DB(const std::string& conninfo) : conninfo(conninfo), conn(nullptr) {
    ensureConnection();
}

// Деструктор
DB::~DB() {
    if (conn) {
        PQfinish((PGconn*)conn);
    }
}

// Проверка подключения к базе данных
void DB::ensureConnection() {
    if (!conn || PQstatus((PGconn*)conn) != CONNECTION_OK) {
        if (conn) PQfinish((PGconn*)conn);
        
        conn = PQconnectdb(conninfo.c_str());
        
        if (PQstatus((PGconn*)conn) != CONNECTION_OK) {
            std::cerr << "DB Connection Error: " << PQerrorMessage((PGconn*)conn) << std::endl;
        } else {
            std::cout << "Successfully connected to DB" << std::endl;
        }
    }
}