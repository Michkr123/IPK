# Makefile for the Chat Client project (UDP & TCP implementations)
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# List of source files (including Utils files)
SOURCES = ChatClient.cpp UDPChatClient.cpp TCPChatClient.cpp main.cpp Utils.cpp

# Object files generated from the sources.
OBJECTS = $(SOURCES:.cpp=.o)

# Target executable name.
TARGET = ipk25chat-client

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
	