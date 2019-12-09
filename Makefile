TARGET = ims
CC = g++
CFLAGS = -g -D_DEBUG -pedantic -std=c++11 -Wall
LDFLAGS = ./simlib/src/simlib.so ./simlib/src/simlib.h

.PHONY: all

all: $(TARGET) run experiment1 experiment2 experiment3

build: $(TARGET)

$(TARGET): $(TARGET).o
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $(TARGET)

$(TARGET).o: $(TARGET).cpp
	$(CC) -c $(CFLAGS) $^ -o $(TARGET).o

clean:
	rm -f $(TARGET).o $(TARGET)

run:
	./$(TARGET)

experiment1:
	echo "Running experiment 1"
	./$(TARGET) --experiment1

experiment2:
	echo "Running experiment 2"
	./$(TARGET) --experiment2

experiment3:
	echo "Running experiment 3"
	./$(TARGET) --experiment3
