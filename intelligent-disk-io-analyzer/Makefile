CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -pedantic
TARGET = disk_analyzer
SRC = src/server.c

.PHONY: all run clean
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) disk_analyzer.exe
