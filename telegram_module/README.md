# Telegram Module

C++ Telegram bot module using libcurl.

Implements:
- long polling via Telegram Bot API
- command parsing
- basic bot logic

## Build

```bash
make

Run:
export TELEGRAM_BOT_TOKEN=your_token_here
./telegram_bot

Structure:

main.cpp — entry point

TelegramClient.* — Telegram API interaction

BotLogicClient.* — bot logic

CommandParser.* — command parsing