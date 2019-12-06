TARGET = main
CC = g++
CFLAGS = -g -D_DEBUG -pedantic -std=c++11 -Wall
LDFLAGS = ./simlib/src/simlib.so ./simlib/src/simlib.h

.PHONY: all

all: $(TARGET) run

$(TARGET): $(TARGET).o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $(TARGET)

$(TARGET).o: $(TARGET).cpp
	$(CC) -c $(CFLAGS) $^ -o $(TARGET).o

clean:
	rm -f $(TARGET).o $(TARGET)

run:
	./$(TARGET)
