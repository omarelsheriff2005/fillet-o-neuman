CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g

TARGET  := fillet-o-neuman.exe
OBJS    := main.o parser.o pipeline.o

RAYLIB_DIR := raylib
RAYLIB_INC := $(RAYLIB_DIR)/include
RAYLIB_LIB := $(RAYLIB_DIR)/lib
RAYLIB_LINK := -L$(RAYLIB_LIB) -lraylib -lopengl32 -lgdi32 -lwinmm

GUI_TARGET := gui.exe
GUI_OBJS   := gui.o parser.o pipeline.o

.PHONY: all gui run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

gui: $(GUI_TARGET)

$(GUI_TARGET): $(GUI_OBJS)
	$(CC) $(CFLAGS) -o $@ $(GUI_OBJS) $(RAYLIB_LINK)

gui.o: gui.c architecture.h
	$(CC) $(CFLAGS) -I$(RAYLIB_INC) -c gui.c -o gui.o

%.o: %.c architecture.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) program.txt

clean:
	rm -f $(TARGET) $(OBJS) $(GUI_TARGET) $(GUI_OBJS)
