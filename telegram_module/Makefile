# ==============================
# Telegram Module Makefile
# ==============================

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# macOS Homebrew paths (на Intel обычно /usr/local, на Apple Silicon /opt/homebrew)
BREW_PREFIX ?= $(shell (brew --prefix 2>/dev/null) || echo /opt/homebrew)

INCLUDES = -I. -I$(BREW_PREFIX)/include
LIBPATH  = -L$(BREW_PREFIX)/lib

LIBS_TELEGRAM = -lcurl -pthread
LIBS_BOTLOGIC = -lhiredis -pthread

TARGET_TG  = telegram_bot
TARGET_BL  = bot_logic

SRC_TG = \
    main.cpp \
    TelegramClient.cpp \
    BotLogicClient.cpp \
    CommandParser.cpp

OBJ_TG = $(SRC_TG:.cpp=.o)

SRC_BL = BotLogicServer.cpp
OBJ_BL = $(SRC_BL:.cpp=.o)

all: $(TARGET_TG) $(TARGET_BL)

$(TARGET_TG): $(OBJ_TG)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS_TELEGRAM)

$(TARGET_BL): $(OBJ_BL)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBPATH) $(LIBS_BOTLOGIC)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

auth_mock: AuthMockServer.o
	$(CXX) $(CXXFLAGS) -o auth_mock AuthMockServer.o -pthread

AuthMockServer.o: AuthMockServer.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c AuthMockServer.cpp -o AuthMockServer.o

clean:
	rm -f $(OBJ_TG) $(OBJ_BL) $(TARGET_TG) $(TARGET_BL)
	rm -f AuthMockServer.o auth_mock


rebuild: clean all
