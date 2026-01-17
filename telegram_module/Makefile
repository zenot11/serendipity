# ==============================
# Telegram Bot Client Makefile
# ==============================

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LIBS = -lcurl -pthread

TARGET = telegram_bot

SRC = \
    main.cpp \
    TelegramClient.cpp \
    BotLogicClient.cpp \
    CommandParser.cpp

OBJ = $(SRC:.cpp=.o)

# ---------- build ----------
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ---------- clean ----------
clean:
	rm -f $(OBJ) $(TARGET)

# ---------- rebuild ----------
rebuild: clean all
