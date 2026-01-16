## Telegram Bot Module

Модуль Telegram-бота предназначен для взаимодействия с пользователями через Telegram API
и интеграции с основной логикой проекта.

### Структура модуля

telegram_module/
- TelegramClient.{h,cpp} — HTTP-клиент для работы с Telegram Bot API
- CommandParser.{h,cpp} — разбор и фильтрация входящих команд
- BotLogicClient.{h,cpp} — связка Telegram-бота с бизнес-логикой проекта
- Message.h — структура входящих сообщений
- main.cpp — точка входа для запуска Telegram-бота
- json.hpp — библиотека для работы с JSON

### Поддерживаемые команды

- `/start` — запуск бота
- `/help` — список доступных команд

### Сборка и запуск

```bash
g++ -std=c++17 \
telegram_module/main.cpp \
telegram_module/TelegramClient.cpp \
telegram_module/CommandParser.cpp \
telegram_module/BotLogicClient.cpp \
-o telegram_bot -lcurl
Перед запуском необходимо задать токен Telegram-бота:
export TELEGRAM_BOT_TOKEN=ВАШ_ТОКЕН
./telegram_bot

Примечания:
Токен бота хранится в переменной окружения и не добавляется в репозиторий
Модуль разрабатывается в отдельных ветках telegram-module/telegram-module-v2

Telegram-модуль находится в активной разработке.
Часть функциональности будет добавлена в следующих коммитах.
