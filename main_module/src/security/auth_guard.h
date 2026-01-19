#pragma once
#include <crow.h>
#include "jwt.h"

// Проверка JWT

inline int authGuard(
    const crow::request& req,
    UserContext& ctx
) {
    try {
        ctx = parseAndVerifyJWT(req);

        if (ctx.blocked) {
            return 418;
        }

        return 200;
    } catch (...) {
        return 401;
    }
}