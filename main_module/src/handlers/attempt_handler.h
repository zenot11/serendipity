#pragma once
#include "crow.h"
#include "../db/db.h"
#include "../security/jwt.h"
#include "../security/access.h"
#include "../security/auth_guard.h"


inline void registerAttemptRoutes(crow::SimpleApp& app, DB& db) {
    // Список пользователей прошедших тест
    CROW_ROUTE(app, "/tests/<int>/passed-users").methods("GET"_method)
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
                "test:answer:read", 
                false, 
                nullptr
            };
            if (checkAccess(ctx, rule, "").code != 200) {
                return crow::response(403, "Forbidden: Only course teacher can view results");
            }
        }

        std::vector<std::string> users = db.getUsersWhoPassedTest(testId);

        crow::json::wvalue res;
        res["user_ids"] = std::move(users);
        return crow::response(200, res);
    });
    // Оценки пользователей (свои оценки)
    CROW_ROUTE(app, "/tests/<int>/scores").methods("GET"_method)
    ([&db](const crow::request& req, int testId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto test = db.getTestById(testId);
        if (test.id == 0) return crow::response(404, "Test not found");
        auto course = db.getCourseById(test.course_id);

        PermissionRule rule{
            "test:answer:read", 
            false, 
            nullptr
        };

        bool hasGlobalRead = (checkAccess(ctx, rule, "").code == 200);
        bool isAuthor = (ctx.userId == course.author_id || hasGlobalRead);

        auto scores = db.getTestScores(testId, ctx.userId, isAuthor);

        if (!isAuthor && scores.empty()) {
            return crow::response(403, "Access denied or no completed attempts");
        }

        crow::json::wvalue::list scores_json;
        for (const auto& s : scores) {
            crow::json::wvalue item;
            item["user_id"] = s.user_id;
            item["score"] = s.score;
            scores_json.push_back(std::move(item));
        }

        crow::json::wvalue res;
        res["scores"] = std::move(scores_json);
        return crow::response(200, res);
    });
    // Посмотреть ответы пользователей (или свои ответы)
    CROW_ROUTE(app, "/tests/<int>/answers").methods("GET"_method)
    ([&db](const crow::request& req, int testId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto test = db.getTestById(testId);
        if (test.id == 0) return crow::response(404, "Test not found");
        auto course = db.getCourseById(test.course_id);

        PermissionRule rule{
            "test:answer:read", 
            false, 
            nullptr
        };
        bool hasGlobalRead = (checkAccess(ctx, rule, "").code == 200);
        bool isAuthor = (ctx.userId == course.author_id || hasGlobalRead);

        auto details = db.getTestAttemptDetails(testId, ctx.userId, isAuthor);

        if (!isAuthor && details.empty()) {
            return crow::response(403, "Access denied: You can only view your own answers");
        }

        crow::json::wvalue::list final_list;
        for (const auto& att : details) {
            crow::json::wvalue item;
            item["user_id"] = att.user_id;
            
            crow::json::wvalue::list ans_json;
            for (const auto& ans : att.answers) {
                crow::json::wvalue a;
                a["question"] = ans.question_text;
                a["answer"] = ans.user_answer_text;
                ans_json.push_back(std::move(a));
            }
            item["answers"] = std::move(ans_json);
            final_list.push_back(std::move(item));
        }

        crow::json::wvalue res;
        res["attempts"] = std::move(final_list);
        return crow::response(200, res);
    });
    // Создание попытки (Начало теста)
    CROW_ROUTE(app, "/tests/<int>/start").methods("POST"_method)
    ([&db](const crow::request& req, int testId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto test = db.getTestById(testId);
        if (test.id == 0) return crow::response(404, "Test not found");

        if (!db.isUserEnrolled(test.course_id, ctx.userId)) {
            return crow::response(403, "You are not enrolled in this course");
        }

        int attemptId = db.startTestAttempt(testId, ctx.userId);

        if (attemptId == -1) return crow::response(404, "Test not found");
        if (attemptId == -2) return crow::response(400, "Test is not active");
        if (attemptId == -3) return crow::response(400, "Attempt already exists");

        crow::json::wvalue res;
        res["attempt_id"] = attemptId;
        return crow::response(201, res);
    });
    // Отправка ответов внутри попытки
    CROW_ROUTE(app, "/attempts/<int>/answers").methods("POST"_method)
    ([&db](const crow::request& req, int attemptId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        if (!db.isAttemptOwnedBy(attemptId, ctx.userId)) {
            return crow::response(403, "Forbidden: You do not own this attempt");
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("question_id") || !body.has("answer_index")) {
            return crow::response(400, "Missing question_id or answer_index");
        }

        int questionId = body["question_id"].i();
        int answerIndex = body["answer_index"].i();

        if (db.updateAttemptAnswer(attemptId, questionId, answerIndex)) {
            return crow::response(204);
        } else {
            return crow::response(400, "Cannot change answer: Attempt completed or not found");
        }
    });
    // Отправка ответа на конкретный вопрос
    CROW_ROUTE(app, "/attempts/<int>/questions/<int>/answer").methods("PATCH"_method)
    ([&db](const crow::request& req, int attemptId, int questionId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        bool isOwner = db.isAttemptOwnedBy(attemptId, ctx.userId);
        PermissionRule updateRule{"answer:update", false, nullptr};
        bool hasPermission = (checkAccess(ctx, updateRule, "").code == 200);

        if (!isOwner && !hasPermission) {
            return crow::response(403, "Forbidden: Access denied");
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("answer_index")) {
            return crow::response(400, "Missing answer_index");
        }

        if (db.updateAttemptAnswer(attemptId, questionId, body["answer_index"].i())) {
            return crow::response(204);
        } else {
            return crow::response(400, "Update failed: Attempt completed or not found");
        }
    });
    // Удалить ответ
    CROW_ROUTE(app, "/attempts/<int>/questions/<int>/answer").methods("DELETE"_method)
    ([&db](const crow::request& req, int attemptId, int questionId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        bool isOwner = db.isAttemptOwnedBy(attemptId, ctx.userId);
        PermissionRule delRule{"answer:del", false, nullptr};
        bool hasPermission = (checkAccess(ctx, delRule, "").code == 200);

        if (!isOwner && !hasPermission) {
            return crow::response(403, "Forbidden: Access denied");
        }

        if (db.updateAttemptAnswer(attemptId, questionId, -1)) {
            return crow::response(204);
        } else {
            return crow::response(400, "Delete failed: Attempt completed or not found");
        }
    });
    // Завершение попытки студентом
    CROW_ROUTE(app, "/attempts/<int>/complete").methods("POST"_method)
    ([&db](const crow::request& req, int attemptId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        if (!db.isAttemptOwnedBy(attemptId, ctx.userId)) {
            return crow::response(403, "Forbidden: You do not own this attempt");
        }

        if (db.completeAttempt(attemptId)) {
            return crow::response(200, "Attempt completed successfully");
        } else {
            return crow::response(400, "Cannot complete: Attempt already finished or not found");
        }
    });
    // Посмотреть попытку
    CROW_ROUTE(app, "/tests/<int>/attempts/<string>").methods("GET"_method)
    ([&db](const crow::request& req, int testId, std::string targetUserId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto test = db.getTestById(testId);
        if (test.id == 0) return crow::response(404, "Test not found");
        auto course = db.getCourseById(test.course_id);

        bool isOwner = (ctx.userId == targetUserId);
        bool isAuthor = (ctx.userId == course.author_id);
        
        PermissionRule readRule{
            "test:answer:read", 
            false, 
            nullptr
        };
        bool hasPermission = (checkAccess(ctx, readRule, "").code == 200);

        if (!isOwner && !isAuthor && !hasPermission) {
            return crow::response(403, "Forbidden: You can only view your own attempts or must be a teacher");
        }

        auto attemptData = db.getAttemptData(testId, targetUserId);
        
        if (attemptData.t() == crow::json::type::Null) {
            return crow::response(404, "Attempt not found");
        }

        return crow::response(200, std::move(attemptData));
    });
    // Просмотр ответов пользователя в конкретном тесте
    CROW_ROUTE(app, "/tests/<int>/attempts/<string>/answers").methods("GET"_method)
    ([&db](const crow::request& req, int testId, std::string targetUserId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto test = db.getTestById(testId);
        if (test.id == 0) return crow::response(404, "Test not found");
        auto course = db.getCourseById(test.course_id);


        bool isOwner = (ctx.userId == targetUserId);
        bool isAuthor = (ctx.userId == course.author_id);
        
        PermissionRule readRule{
            "answer:read", 
            false, 
            nullptr
        };
        bool hasPermission = (checkAccess(ctx, readRule, "").code == 200);

        if (!isOwner && !isAuthor && !hasPermission) {
            return crow::response(403, "Forbidden: Access denied to view these answers");
        }

        auto data = db.getAttemptAnswers(testId, targetUserId);
        
        if (data.t() == crow::json::type::Null) {
            return crow::response(404, "Attempt not found for this user");
        }

        return crow::response(200, std::move(data));
    });

}
