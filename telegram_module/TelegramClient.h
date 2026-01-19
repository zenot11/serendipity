#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "BotLogicClient.h"

class TelegramClient {
public:
    explicit TelegramClient(const std::string& botToken);
    ~TelegramClient();

    void poll();   // основной цикл
    void stop();   // корректная остановка

private:
    void startCronThreadsOnce();
    void cronLoginLoop();
    void cronNotificationsLoop();

    void sendMessage(long long chatId, const std::string& text);

private:
    std::string token;
    std::string apiBase; // https://api.telegram.org/bot<TOKEN>

    long long last_update_id{0};

    std::atomic<bool> running{false};
    std::atomic<bool> cron_running{false};

    std::thread cronLoginThread;
    std::thread cronNotificationsThread;

    // на всякий случай сериализуем sendMessage (можно убрать, но так надёжнее)
    std::mutex send_m;

    BotLogicClient logic;
};