#pragma once
#include "crow.h"
#include "cpr/cpr.h"
#include "course_handler.h"
#include "test_handler.h"
#include "question_handler.h"
#include "attempt_handler.h"
#include "user_handler.h"
#include "notification_handler.h"
#include "../db/db.h"
#include "../security/jwt.h"
#include "../security/access.h"
#include "../security/auth_guard.h"


inline void registerRoutes(crow::SimpleApp& app, DB& db) {
    registerCourseRoutes(app, db);
    registerTestRoutes(app, db);
    registerQuestionRoutes(app, db);
    registerAttemptRoutes(app, db);
    registerUserRoutes(app, db);
    registerNotificationRoutes(app, db);
}