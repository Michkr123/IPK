CC = g++
CFLAGS = -Wall -Wextra -std=c++17
TARGET = ipk25-chat
SRC = ipk25-chat.cpp ipk25-chat-udp.cpp ipk25-chat-tcp.cpp

OBJ = $(SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ) 
