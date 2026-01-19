#pragma ince
#include "user_context.h"
#include <crow.h>

UserContext parseAndVerifyJWT(const crow::request& req);