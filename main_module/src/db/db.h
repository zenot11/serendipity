#pragma once
#include "crow.h"
#include <vector>
#include <string>
#include <libpq-fe.h>
#include <stdexcept>
#include <iostream>
#include "../domain/test.h"
#include "../domain/course.h"
#include "../domain/question.h"

// Структура оценки пользователя
struct UserScore {
    std::string user_id;
    double score;
};

// Структура информации о пользователе
struct UserProfileData {
    std::vector<Course> courses;
    std::vector<Test> tests;
    std::vector<UserScore> grades;
};

class DB {
public:
    DB(const std::string& conninfo);
    ~DB();

    // Тетсты
    Test getTestById(int testId);
    bool updateTestStatus(int testId, bool isActive);
    void finalizeAllTestAttempts(int testId);
    std::vector<Test> getTestsByCourseId(int courseId);
    int createTest(int courseId, const std::string& title, std::string authorId);
    bool deleteTest(int testId);
    void setTestActivity(int testId, bool active);

    bool removeQuestionFromTest(int testId, int questionId);
    bool addQuestionToTest(int testId, int questionId);
    bool reorderQuestionsInTest(int testId, const std::vector<int>& questionIds);
    std::vector<std::string> getUsersWhoPassedTest(int testId);
    std::vector<UserScore> getTestScores(int testId, std::string userIdFilter, bool isAuthor);
    std::vector<AttemptDetails> getTestAttemptDetails(int testId, std::string userIdFilter, bool isAuthor);

    std::vector<int> getQuestionIdsByTestId(int testId);
    bool canAccessCourse(std::string userId, int courseId);
    int getCourseIdByTestId(int testId);

    // Попытки
    int startTestAttempt(int testId, std::string userId);
    bool updateAttemptAnswer(int attemptId, int questionId, int answerIndex);
    bool isAttemptOwnedBy(int attemptId, std::string userId);
    bool completeAttempt(int attemptId);
    crow::json::wvalue getAttemptData(int testId, std::string userId);
    crow::json::wvalue getAttemptAnswers(int testId, std::string userId);

    // Курсы
    std::vector<Course> getCourses();
    Course getCourseById(int courseId);
    int createCourse(const std::string& title, const std::string& description, std::string teacherId);
    void deleteCourse(int courseId);
    bool updateCourse(int courseId, std::string title, std::string description);
    bool addStudentToCourse(int courseId, std::string userId);
    bool removeStudentFromCourse(int courseId, std::string userId);
    std::vector<std::string> getStudentIdsByCourseId(int courseId);
    bool isUserEnrolled(int courseId, std::string userId);

    // Вопросы
    int createQuestion(std::string authorId, const std::string& title, const std::string& content, const std::vector<std::string>& options, int correctOption);
    bool deleteQuestion(int questionId);
    int updateQuestion(int questionId, std::string userId, const std::string& title, const std::string& content, const std::vector<std::string>& options, int correctOption);
    std::vector<Question> getQuestionsList(std::string userId, bool canSeeAll);
    Question getQuestionById(int questionId);
    Question getQuestionByIdAndVersion(int questionId, int version);
    bool hasUserAttemptForQuestion(std::string userId, int questionId);

    // Пользователь
    crow::json::wvalue getUserDataProfile(std::string userId, bool includeCourses, bool includeTests, bool includeGrades);

    // Уведомления
    void markNotificationsAsSent(const std::vector<int>& ids, std::string userId);
    std::vector<crow::json::wvalue> getUnsentNotifications(std::string userId);
    void pushNotification(
        const std::string& userId, 
        const std::string& type, 
        const std::string& title, 
        const std::string& message, 
        const crow::json::wvalue& payload = {}
    );
private:
    void ensureConnection();
    void* conn;
    std::string conninfo;   
};
