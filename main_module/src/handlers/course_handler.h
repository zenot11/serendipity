#pragma once
#include "crow.h"
#include "../db/db.h"
#include "../security/jwt.h"
#include "../security/access.h"
#include "../security/auth_guard.h"

inline void registerCourseRoutes(crow::SimpleApp& app, DB& db) {
    // Получение всех курсов (название, описание)
    CROW_ROUTE(app, "/courses").methods("GET"_method)
    ([&db](const crow::request& req){
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto courses = db.getCourses();
        std::vector<crow::json::wvalue> course_list;
        for (auto& c : courses) {
            if (c.id != 0 && !c.is_deleted) {
                crow::json::wvalue item;
                item["id"] = c.id;
                item["title"] = c.title;
                item["description"] = c.description;
                course_list.push_back(std::move(item));
            }
        }
        crow::json::wvalue final_res;
        final_res["courses"] = std::move(course_list);
        return crow::response(final_res);
    });
    // Получение курса по айди
    CROW_ROUTE(app, "/courses/<int>").methods("GET"_method)
    ([&db](const crow::request& req, int courseId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto course = db.getCourseById(courseId);
        if (course.id == 0 || course.is_deleted) {
            return crow::response(404, "Course not found");
        }
        crow::json::wvalue json_data;
        json_data["id"] = course.id;
        json_data["title"] = course.title;
        json_data["description"] = course.description;
        json_data["author_id"] = course.author_id;
        return crow::response(json_data);
    });
    // Создание курса
    CROW_ROUTE(app, "/courses").methods("POST"_method)
    ([&db](const crow::request& req) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        PermissionRule rule {
            "course:add",
            false,
            nullptr
        };
        auto check = checkAccess(ctx, rule, "").code;
        if (check != 200) {
            return crow::response(check);
        } 
        auto body = crow::json::load(req.body);
        if (!body) {
            return crow::response(400, "Invalid JSON");
        }
        int id = db.createCourse(
            body["title"].s(),
            body["description"].s(),
            ctx.userId
        );
        if (id == -1) {
            return crow::response(500, "Database error: check if title is unique or database is connected");
        }
        crow::json::wvalue res;
        res["id"] = id;
        return crow::response(201, res);
    });
    // Изменить курс
    CROW_ROUTE(app, "/courses/<int>").methods("PATCH"_method)
    ([&db](const crow::request& req, int courseId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto course = db.getCourseById(courseId);
        if (course.id == 0 || course.is_deleted) {
            return crow::response(404, "Course not found");
        }
        
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "Invalid JSON");


        PermissionRule rule {
            "course:info:write",
            false,
            [](const UserContext& ctx, std::string ownerId) {
                return ctx.userId == ownerId;
            }
        };
        if (checkAccess(ctx, rule, course.author_id).code != 200) {
            return crow::response(403, "Forbidden");
        } 

        std::string title = body.has("title") ? body["title"].s() : course.title;
        std::string description = body.has("description") ? body["description"].s() : course.description;

        if (db.updateCourse(courseId, title, description)) {
            return crow::response(204);
        } else {
            return crow::response(404, "Failed to update: Course not found or already deleted");
        }
    });
    // Удаление курса
    CROW_ROUTE(app, "/courses/<int>").methods("DELETE"_method)
    ([&db](const crow::request& req, int courseId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto course = db.getCourseById(courseId);
        if (course.id == 0 || course.is_deleted) {
            return crow::response(404, "Course not found");
        }

        PermissionRule rule {
            "course:del",
            false,
            [](const UserContext& ctx, std::string ownerId) {
                return ctx.userId == ownerId;
            }
        };
        if (checkAccess(ctx, rule, course.author_id).code != 200) {
            return crow::response(403);
        } 

        std::vector<std::string> studentIds = db.getStudentIdsByCourseId(courseId);
        std::string title = course.title;

        db.deleteCourse(courseId);

        for (auto sId : studentIds) {
            db.pushNotification(
                sId, 
                "academic", 
                "Курс удален", 
                "Дисциплина '" + title + "' была удалена автором."
            );
        }
        return crow::response(204);
    });
    // Добавить пользователя на курс
    CROW_ROUTE(app, "/courses/<int>/join").methods("POST"_method)
    ([&db](const crow::request& req, int courseId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto json_data = crow::json::load(req.body);
        if (!json_data || !json_data.has("user_id")) {
            return crow::response(400, "Missing user_id in request body");
        }

        std::string target_user_id = json_data["user_id"].s();

        if (ctx.userId != target_user_id) {
            PermissionRule rule{
                "course:user:add",
                false,
                nullptr
            };
            if (checkAccess(ctx, rule, "").code != 200) {
                return crow::response(403);
            }
        }
        if (!db.addStudentToCourse(courseId, target_user_id)) {
            return crow::response(404, "Course not found, is deleted, or user already joined");
        }

        if (ctx.userId != target_user_id) {
            auto course = db.getCourseById(courseId);
            db.pushNotification(
                target_user_id,
                "academic",
                "Зачисление",
                "Вы были зачислены на курс: " + course.title
            );
        }

        return crow::response(204);
    });
    // Удалить пользователя из курса
    CROW_ROUTE(app, "/courses/<int>/leave").methods("DELETE"_method)
    ([&db](const crow::request& req, int courseId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto json_data = crow::json::load(req.body);
        if (!json_data || !json_data.has("user_id")) {
            return crow::response(400, "Missing user_id in request body");
        }

        std::string target_user_id = json_data["user_id"].s();

        if (ctx.userId != target_user_id) {
            PermissionRule rule{
                "course:user:del",
                false,
                nullptr
            };
            if (checkAccess(ctx, rule, "").code != 200) {
                return crow::response(403);
            }
        }

        auto course = db.getCourseById(courseId);

        db.removeStudentFromCourse(courseId, target_user_id);

        if (ctx.userId != target_user_id) {
            db.pushNotification(
                target_user_id,
                "academic",
                "Исключение",
                "Вы были удалены из курса: " + course.title
            );
        }
        return crow::response(204);
    });
    // Список студентов на курсе
    CROW_ROUTE(app, "/courses/<int>/students").methods("GET"_method)
    ([&db](const crow::request& req, int courseId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto course = db.getCourseById(courseId);
        if (course.id == 0 || course.is_deleted) {
            return crow::response(404, "Course not found");
        }

        if (ctx.userId != course.author_id) {
            PermissionRule rule{
                "course:userList",
                false,
                nullptr
            };
            if (checkAccess(ctx, rule, "").code != 200) {
                return crow::response(403, "Forbidden");
            }
        }
        std::vector<std::string> studentIds = db.getStudentIdsByCourseId(courseId);
        crow::json::wvalue response;
        response["student_ids"] = std::move(studentIds); 

        return crow::response(200, response);
    });
}
