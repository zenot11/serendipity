#pragma once
#include "crow.h"
#include "../db/db.h"
#include "../security/jwt.h"
#include "../security/access.h"
#include "../security/auth_guard.h"

inline void registerQuestionRoutes(crow::SimpleApp& app, DB& db) {
    // Создание вопроса
    CROW_ROUTE(app, "/questions").methods("POST"_method)
    ([&db](const crow::request& req) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        PermissionRule rule{
            "quest:create", 
            false, 
            nullptr};
        if (checkAccess(ctx, rule, "").code != 200) {
            return crow::response(403, "Forbidden: Cannot create questions");
        }

        auto body = crow::json::load(req.body);
        if (!body || !body.has("title") || !body.has("content") || !body.has("options") || !body.has("correct_option")) {
            return crow::response(400, "Missing required fields");
        }

        std::vector<std::string> options;
        for (auto& opt : body["options"]) {
            options.push_back(opt.s());
        }

        int questionId = db.createQuestion(
            ctx.userId,
            body["title"].s(),
            body["content"].s(),
            options,
            body["correct_option"].i()
        );

        if (questionId == -1) return crow::response(500, "Database error");

        crow::json::wvalue res;
        res["id"] = questionId;
        return crow::response(201, res);
    });
    // Удаление вопроса
    CROW_ROUTE(app, "/questions/<int>").methods("DELETE"_method)
    ([&db](const crow::request& req, int questionId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        auto question = db.getQuestionById(questionId);
        if (question.id == 0) return crow::response(404, "Question not found");

        if (ctx.userId != question.author_id) {
            PermissionRule rule{
                "quest:del", 
                false, 
                nullptr
            };
            if (checkAccess(ctx, rule, "").code != 200) {
                return crow::response(403, "Forbidden: You can only delete your own questions");
            }
        }

        if (db.deleteQuestion(questionId)) {
            return crow::response(204);
        } else {
            return crow::response(400, "Cannot delete question: it is used in one or more tests");
        }
    });
    // Изменение вопроса
    CROW_ROUTE(app, "/questions/<int>").methods("PATCH"_method, "PUT"_method)
    ([&db](const crow::request& req, int questionId) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        Question q = db.getQuestionById(questionId); 
        if (q.id == 0) return crow::response(404, "Question not found");

        PermissionRule rule{
            "quest:update", 
            false, 
            nullptr
        };
        if (checkAccess(ctx, rule, "").code != 200 && q.author_id != ctx.userId) {
            return crow::response(403, "Forbidden: Missing quest:update permission");
        }

        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "Invalid JSON");


        if (req.method == "PUT"_method) {
            if (!body.has("title") || !body.has("content") || !body.has("options") || !body.has("correct_option")) {
                return crow::response(400, "PUT request requires all fields");
            }
        }

        std::string newTitle = body.has("title") ? body["title"].s() : q.title;
        std::string newContent = body.has("content") ? body["content"].s() : q.content;
        int newCorrectOption = body.has("correct_option") ? body["correct_option"].i() : q.correct_option;

        std::vector<std::string> newOptions;
        if (body.has("options")) {
            if (body["options"].t() != crow::json::type::List) return crow::response(400, "Options must be an array");
            for (auto& opt : body["options"]) newOptions.push_back(opt.s());
        } else {
            newOptions = q.options;
        }

        if (newOptions.empty()) {
            return crow::response(400, "Question must have options");
        }

        int result = db.updateQuestion(
            questionId, 
            ctx.userId,
            newTitle, 
            newContent,
            newOptions, 
            newCorrectOption
        );

        if (result == -1) return crow::response(404, "Question not found");
        if (result == -2) return crow::response(403, "Forbidden: Not your question");
        if (result < 0) return crow::response(500, "DB Error");

        crow::json::wvalue res;
        res["new_version"] = result;
        return crow::response(200, res);
    });
    // Получить детали вопроса
    CROW_ROUTE(app, "/questions/<int>/<int>").methods("GET"_method)
    ([&db](const crow::request& req, int questionId, int version) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        PermissionRule rule{
            "quest:read", 
            false, 
            nullptr
        };
        bool hasGlobalRead = (checkAccess(ctx, rule, "").code == 200);

        Question q = db.getQuestionByIdAndVersion(questionId, version);
        if (q.id == 0) return crow::response(404, "Question not found");

        if (hasGlobalRead) return crow::response(200, q.to_json());

        if (q.author_id == ctx.userId) return crow::response(200, q.to_json());

        if (!q.is_deleted && db.hasUserAttemptForQuestion(ctx.userId, questionId)) {
            return crow::response(200, q.to_json());
        }

        return crow::response(403, "Access denied");
    });
    // Посмотреть список вопросов (своих)
    CROW_ROUTE(app, "/questions").methods("GET"_method)
    ([&db](const crow::request& req) {
        UserContext ctx;
        auto auth = authGuard(req, ctx);
        if (auth == 418) return crow::response(403, "Blocked");
        if (auth == 401) return crow::response(401, "Unauthorized");

        PermissionRule rule{
            "quest:list:read", 
            false, 
            nullptr};
        bool canSeeAll = (checkAccess(ctx, rule, "").code == 200);

        auto questions = db.getQuestionsList(ctx.userId, canSeeAll);

        crow::json::wvalue::list questions_json;
        for (const auto& q : questions) {
            crow::json::wvalue item;
            item["id"] = q.id;
            item["version"] = q.version;
            item["author_id"] = q.author_id;
            item["title"] = q.title;
            questions_json.push_back(std::move(item));
        }

        crow::json::wvalue res;
        res["questions"] = std::move(questions_json);
        return crow::response(200, res);
    });
}