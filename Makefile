CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g
TARGET := fillet-o-neuman.exe
OBJS := main.o parser.o pipeline.o

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c architecture.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) program.txt

clean:
	rm -f $(TARGET) $(OBJS)
