#pragma once
#include "user_context.h"
#include <string>

// Проверка прав

struct PermissionRule {
    std::string permission;
    bool defaultAllowed = false;
    std::function<bool(const UserContext&, std::string)> checkDefault;
};

inline crow::response checkAccess(
    const UserContext& ctx,
    const PermissionRule& rule,
    std::string resourceOwnerId
) {
    if (ctx.blocked) {
        return crow::response(403);
    }
    if (ctx.permissions.find(rule.permission) != ctx.permissions.end()) {
        return crow::response(200);
    }
    if (rule.checkDefault && rule.checkDefault(ctx, resourceOwnerId)) {
        return crow::response(200);
    }
    if (rule.defaultAllowed) {
        if (!rule.checkDefault) {
            return crow::response(200);
        }
        return rule.checkDefault(ctx, resourceOwnerId) ? crow::response(200) : crow::response(403);
    }
    return crow::response(403);
}

inline bool hasPermission(
    const UserContext& ctx,
    const std::string& permission,
    std::string resourceOwnerId
) {
    PermissionRule rule;
    rule.permission = permission;
    rule.defaultAllowed = false;
    return checkAccess(ctx, rule, resourceOwnerId).code == 200;
}
