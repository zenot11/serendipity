#pragma once
#include "crow.h"
#include "../db/db.h"
#include "../security/jwt.h"
#include "../security/access.h"
#include "../security/auth_guard.h"

inline void registerTestRoutes(crow::SimpleApp& app, DB& db) {
    // Получение тестов по курсу
    CROW_ROUTE(app, "/courses/<int>/tests").methods("GET"_method)
    ([&db](const crow::request& req, int courseId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto course = db.getCourseById(courseId);
        if (course.id == 0 || course.is_deleted) {
            return crow::response(404, "Course not found");
        }

        bool isAuthor = (ctx.userId == course.author_id);
        bool isEnrolled = db.isUserEnrolled(courseId, ctx.userId);

        PermissionRule adminRule{
            "course:testList", 
            false, 
            nullptr
        };
        bool isAdmin = (checkAccess(ctx, adminRule, "").code == 200);

        if (!isAuthor && !isEnrolled && !isAdmin) {
            return crow::response(403, "Access denied. You must be enrolled or be the author.");
        }

        auto tests = db.getTestsByCourseId(courseId);
        
        std::vector<crow::json::wvalue> test_list;
        for (auto& t : tests) {
            crow::json::wvalue item;
            item["id"] = t.id;
            item["title"] = t.title;
            test_list.push_back(std::move(item));
        }
        crow::json::wvalue res;
        res["tests"] = std::move(test_list);
        return crow::response(200, res);
    });
    // Создание теста по курсу
    CROW_ROUTE(app, "/courses/<int>/tests").methods("POST"_method)
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
                "course:test:add", 
                false, 
                nullptr
            };
            if (checkAccess(ctx, rule, "").code != 200) {
                return crow::response(403, "Forbidden");
            }
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("title")) {
            return crow::response(400, "Missing test title");
        }
        int testId = db.createTest(
            courseId, 
            body["title"].s(), 
            ctx.userId
        );

        if (testId == -1) return crow::response(500, "Database error");

        crow::json::wvalue res;
        res["id"] = testId;
        return crow::response(201, res);
    });


    // Удаление теста по id
    CROW_ROUTE(app, "/tests/<int>").methods("DELETE"_method)
    ([&db](const crow::request& req, int testId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto test = db.getTestById(testId);
        if (test.id == 0 || test.is_deleted) {
            return crow::response(404, "Test not found or already deleted");
        }

        auto course = db.getCourseById(test.course_id);

        if (ctx.userId != course.author_id) {
            PermissionRule rule{
                "course:test:del", 
                false, 
                nullptr
            };
            if (checkAccess(ctx, rule, "").code != 200) {
                return crow::response(403, "Forbidden: Only course author can delete tests");
            }
        }

        if (db.deleteTest(testId)) {
            auto studentIds = db.getStudentIdsByCourseId(test.course_id);
            for (auto sId : studentIds) {
                db.pushNotification(
                    sId,
                    "academic",
                    "Тест удален",
                    "Тест '" + test.title + "' был удален из курса '" + course.title + "'."
                );
            }
            return crow::response(204);
        } else {
            return crow::response(500, "Database error during deletion");
        }
    });
    // Посмотреть информацию от тесте(Активный тест или нет) ccc
    CROW_ROUTE(app, "/courses/<int>/tests/<int>/status").methods("GET"_method)
    ([&db](const crow::request& req, int courseId, int testId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto course = db.getCourseById(courseId);
        auto test = db.getTestById(testId);

        if (course.id == 0 || course.is_deleted || test.id == 0 || test.is_deleted || test.course_id != courseId) {
            return crow::response(404, "Course or Test not found");
        }

        bool isAuthor = (ctx.userId == course.author_id);
        bool isEnrolled = db.isUserEnrolled(courseId, ctx.userId);

        PermissionRule adminRule{
            "course:test:read", 
            false, 
            nullptr
        };
        bool isAdmin = (checkAccess(ctx, adminRule, "").code == 200);

        if (!isAuthor && !isEnrolled && !isAdmin) {
            return crow::response(403, "Forbidden: You are not enrolled in this course");
        }

        crow::json::wvalue res;
        res["is_active"] = test.is_active;

        return crow::response(200, res);
    });
    // Активация/деактивация теста
    CROW_ROUTE(app, "/courses/<int>/tests/<int>/activation").methods("PATCH"_method)
    ([&db](const crow::request& req, int courseId, int testId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto course = db.getCourseById(courseId);
        auto test = db.getTestById(testId);
        if (course.id == 0 || course.is_deleted || test.id == 0 || test.is_deleted || test.course_id != courseId) {
            return crow::response(404, "Course or Test not found");
        }

        if (ctx.userId != course.author_id) {
            PermissionRule rule{
                "course:test:write", 
                false, 
                nullptr
            };
            if (checkAccess(ctx, rule, "").code != 200) {
                return crow::response(403, "Only course author can toggle test status");
            }
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("is_active")) {
            return crow::response(400, "Missing 'is_active' field");
        }
        bool newStatus = body["is_active"].b();

        if (!db.updateTestStatus(testId, newStatus)) {
            return crow::response(500, "Failed to update test status");
        }

        if (newStatus) {
            auto studentIds = db.getStudentIdsByCourseId(courseId);
            for (auto sId : studentIds) {
                db.pushNotification(
                    sId,
                    "academic",
                    "Доступен новый тест",
                    "В курсе '" + course.title + "' открыт тест: " + test.title,
                    {{"test_id", testId}, {"course_id", courseId}}
                );
            }
        } else {
            auto usersInProcess = db.getUsersWhoPassedTest(testId);

            db.finalizeAllTestAttempts(testId);

            for (auto uId : usersInProcess) {
                db.pushNotification(
                    uId,
                    "academic",
                    "Тест завершен",
                    "Преподаватель закрыл тест '" + test.title + "'. Ваша попытка сохранена автоматически.",
                    {{"test_id", testId}}
                );
            }
        }

        return crow::response(204);
    });
    // Удаление вопроса из теста
    CROW_ROUTE(app, "/tests/<int>/questions/<int>").methods("DELETE"_method)
    ([&db](const crow::request& req, int testId, int questionId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto test = db.getTestById(testId);
        if (test.id == 0) return crow::response(404, "Test not found");

        auto course = db.getCourseById(test.course_id);

        if (ctx.userId != course.author_id) {
            PermissionRule rule{
                "test:quest:del", 
                false, 
                nullptr
            };
            if (checkAccess(ctx, rule, "").code != 200) {
                return crow::response(403, "Forbidden: You must be a course teacher or have test:quest:del permission");
            }
        }

        if (db.removeQuestionFromTest(testId, questionId)) {
            return crow::response(204);
        } else {
            return crow::response(400, "Cannot remove question: test already has attempts");
        }
    });
    // Добавление вопроса в тест
    CROW_ROUTE(app, "/tests/<int>/questions/<int>").methods("POST"_method)
    ([&db](const crow::request& req, int testId, int questionId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto test = db.getTestById(testId);
        auto question = db.getQuestionById(questionId);
        if (test.id == 0 || question.id == 0) return crow::response(404, "Test or Question not found");

        auto course = db.getCourseById(test.course_id);

        if (ctx.userId != course.author_id || ctx.userId != question.author_id) {
            PermissionRule rule{
                "test:quest:add", 
                false, 
                nullptr
            };
            if (checkAccess(ctx, rule, "").code != 200) {
                return crow::response(403, "Forbidden: You must be the course teacher AND the question author");
            }
        }

        if (db.addQuestionToTest(testId, questionId)) {
            return crow::response(201, "Question added to test");
        } else {
            return crow::response(400, "Cannot add question: test already has attempts");
        }
    });
    // Изменение порядка вопросов в тесте
    CROW_ROUTE(app, "/tests/<int>/questions/reorder").methods("PATCH"_method)
    ([&db](const crow::request& req, int testId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto test = db.getTestById(testId);
        if (test.id == 0) return crow::response(404, "Test not found");
        auto course = db.getCourseById(test.course_id);

        if (ctx.userId != course.author_id) {
            PermissionRule rule{
                "test:quest:update", 
                false, 
                nullptr
            };
            if (checkAccess(ctx, rule, "").code != 200) {
                return crow::response(403, "Forbidden: Only course teacher can reorder questions");
            }
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("question_ids")) {
            return crow::response(400, "Missing 'question_ids' array");
        }

        std::vector<int> qIds;
        for (auto& item : body["question_ids"]) {
            qIds.push_back(item.i());
        }

        if (db.reorderQuestionsInTest(testId, qIds)) {
            return crow::response(204);
        } else {
            return crow::response(400, "Cannot reorder: test has attempts or database error");
        }
        return crow::response(500, "Unexpected error"); 
    });

    // Получить вопросы в тесте
    CROW_ROUTE(app, "/tests/<int>/question-ids").methods("GET"_method)
    ([&db](const crow::request& req, int testId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");
        
        int courseId = db.getCourseIdByTestId(testId);
        if (courseId == -1) {
            return crow::response(404, "Test not found");
        }

        if (!db.canAccessCourse(ctx.userId, courseId)) {
            return crow::response(403, "No access to this course");
        }
        
        auto questionIds = db.getQuestionIdsByTestId(testId);
        
        crow::json::wvalue response;
        for (size_t i = 0; i < questionIds.size(); i++) {
            response[i] = questionIds[i];
        }
        
        crow::response resp(200, response.dump());
        resp.set_header("Content-Type", "application/json");
        return resp;
    });
}
