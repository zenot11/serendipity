#include "db.h"

// Получение всех курсов
std::vector<Course> DB::getCourses() {
    ensureConnection();
    std::vector<Course> courses;
    PGresult* res = PQexec((PGconn*)conn, "SELECT id, title, description FROM courses WHERE is_deleted = false");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "SELECT failed: " << PQerrorMessage((PGconn*)conn) << std::endl;
        PQclear(res);
        return courses;
    }

    for (int i = 0; i < PQntuples(res); i++) {
        courses.push_back({
            std::stoi(PQgetvalue(res, i, 0)),
            PQgetvalue(res, i, 1),
            PQgetvalue(res, i, 2)
        });
    }
    PQclear(res);
    return courses;
}

// Получение курса по айди
Course DB::getCourseById(int courseId) {
    ensureConnection();
    std::string idStr = std::to_string(courseId);
    const char* paramValues[1] = { idStr.c_str() };

    PGresult* res = PQexecParams(
        (PGconn*)conn,
        "SELECT id, title, description, author_id, is_deleted FROM courses WHERE id = $1",
        1, nullptr, paramValues, nullptr, nullptr, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return {0, "", "", 0, false};
    }

    Course c{
        std::stoi(PQgetvalue(res, 0, 0)),
        PQgetvalue(res, 0, 1),
        PQgetvalue(res, 0, 2),
        PQgetvalue(res, 0, 3),
        std::string(PQgetvalue(res, 0, 4)) == "t"
    };
    PQclear(res);
    return c;  
}

// Создание курса
int DB::createCourse(const std::string& title, const std::string& description, std::string authorId) {
    ensureConnection();
    const char* paramValues[3] = { 
        title.c_str(), 
        description.c_str(), 
        authorId.c_str() 
    };

    PGresult* res = PQexecParams(
        (PGconn*)conn,
        "INSERT INTO courses(title, description, author_id) VALUES ($1, $2, $3) RETURNING id",
        3, nullptr, paramValues, nullptr, nullptr, 0
    );

    int newId = -1;

    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        newId = std::stoi(PQgetvalue(res, 0, 0));
    } else {
        std::cerr << "Create course failed: " << PQerrorMessage((PGconn*)conn) << std::endl;
    }
    PQclear(res);
    return newId;
}

// Удаление курса (мягкое удаление)
void DB::deleteCourse(int courseId) {
    ensureConnection();
    
    std::string idStr = std::to_string(courseId);
    const char* paramValues[] = { idStr.c_str() };
    PGresult* res = PQexecParams(
        (PGconn*)conn,
        "UPDATE courses SET is_deleted = true WHERE id = $1",
        1, nullptr, paramValues, nullptr, nullptr, 0
    );

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Soft delete course failed: " << PQerrorMessage((PGconn*)conn) << std::endl;
    }

    PQclear(res);
}

// Изменение информации о курсе
bool DB::updateCourse(int courseId, std::string title, std::string description) {
    ensureConnection();
    
    std::string idStr = std::to_string(courseId);
    const char* paramValues[3] = { 
        title.c_str(), 
        description.c_str(), 
        idStr.c_str() 
    };

    PGresult* res = PQexecParams(
        (PGconn*)conn,
        "UPDATE courses SET title = $1, description = $2 WHERE id = $3 AND is_deleted = false",
        3, nullptr, paramValues, nullptr, nullptr, 0
    );

    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK && std::string(PQcmdTuples(res)) == "1");

    if (!success) {
        std::cerr << "Update course failed: " << PQerrorMessage((PGconn*)conn) << std::endl;
    }

    PQclear(res);
    return success;
}

// Добавление студента на курс
bool DB::addStudentToCourse(int courseId, std::string userId) {
    ensureConnection();

    std::string cIdStr = std::to_string(courseId);
    
    const char* paramValues[2] = { 
        cIdStr.c_str(), 
        userId.c_str() 
    };

    const char* sql = 
        "INSERT INTO course_students (course_id, user_id) "
        "SELECT $1::int, $2 "
        "WHERE EXISTS (SELECT 1 FROM courses WHERE id = $1::int AND is_deleted = false) "
        "ON CONFLICT (course_id, user_id) DO NOTHING";

    PGresult* res = PQexecParams(
        (PGconn*)conn,
        sql,
        2, nullptr, paramValues, nullptr, nullptr, 0
    );
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Add student failed: " << PQerrorMessage((PGconn*)conn) << std::endl;
        PQclear(res);
        return false;
    }

    bool success = (std::string(PQcmdTuples(res)) == "1");

    PQclear(res);
    return success;
}

// Удаление студента с курса
bool DB::removeStudentFromCourse(int courseId, std::string userId) {
    ensureConnection();

    std::string cIdStr = std::to_string(courseId);
    
    const char* paramValues[] = { 
        cIdStr.c_str(), 
        userId.c_str() 
    };

    const char* sql = 
        "DELETE FROM course_students "
        "WHERE course_id = $1 AND user_id = $2";

    PGresult* res = PQexecParams(
        (PGconn*)conn,
        sql,
        2, nullptr, paramValues, nullptr, nullptr, 0
    );

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Remove student failed: " << PQerrorMessage((PGconn*)conn) << std::endl;
        PQclear(res);
        return false;
    }

    bool wasRemoved = (std::string(PQcmdTuples(res)) == "1");

    PQclear(res);
    return wasRemoved;
}

// Список всех студентов курса
std::vector<std::string> DB::getStudentIdsByCourseId(int courseId) {
    ensureConnection();

    std::string cIdStr = std::to_string(courseId);
    std::vector<std::string> studentIds;
    const char* paramValues[] = { 
        cIdStr.c_str(), 
    };

    const char* sql = 
        "SELECT cs.user_id FROM course_students cs "
        "JOIN courses c ON cs.course_id = c.id "
        "WHERE c.id = $1 AND c.is_deleted = false";

    PGresult* res = PQexecParams(
        (PGconn*)conn,
        sql,
        1, nullptr, paramValues, nullptr, nullptr, 0
    );

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "Fetch students failed: " << PQerrorMessage((PGconn*)conn) << std::endl;
        PQclear(res);
        return studentIds;
    }

    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        const char* val = PQgetvalue(res, i, 0);
        if (val) {
            studentIds.push_back(val);
        }
    }

    PQclear(res);
    return studentIds;
}