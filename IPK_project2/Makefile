CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

SOURCES = ChatClient.cpp UDPChatClient.cpp TCPChatClient.cpp main.cpp Utils.cpp

OBJECTS = $(SOURCES:.cpp=.o)

TARGET = ipk25chat-client

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)