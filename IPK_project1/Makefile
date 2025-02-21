CC = g++
CFLAGS = -Wall -Wextra -std=c++17
TARGET = ipk-l4-scan
SRC = ipk-l4-scan.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) -lpcap

clean:
	rm -f $(TARGET)
