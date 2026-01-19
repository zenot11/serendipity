cmake_minimum_required(VERSION 3.16)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
project(core)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED CMAKE_TOOLCHAIN_FILE)
  message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

find_package(Crow CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(cpr REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)
pkg_check_modules(LIBPQ REQUIRED libpq)
include_directories(${LIBPQ_INCLUDE_DIRS})
link_directories(${LIBPQ_LIBRARY_DIRS})


add_executable(core
    src/main.cpp
    src/db/db_base.cpp
    src/db/db_courses.cpp
    src/db/db_tests.cpp
    src/db/db_questions.cpp
    src/db/db_attempts.cpp
    src/db/db_user.cpp
    src/db/db_notifications.cpp
    src/security/jwt.cpp
)

target_link_libraries(core
    ${PQXX_LIBRARIES}
    ${LIBPQ_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    Crow::Crow 
    cpr::cpr   
)
