CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17
LDFLAGS = -lpcap

SRC = main.cpp PortScanner.cpp TCPScanner.cpp UDPScanner.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = ipk-l4-scan

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(OBJ) $(TARGET)