CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11
LDFLAGS := $(shell pkg-config --libs glfw3 gl glew) -lm

TARGET  := kos3d
SRC     := main.c dungeon.c mesh.c shader.c entities.c particles.c
OBJ     := $(SRC:.c=.o)
HDR     := math3d.h dungeon.h mesh.h shader.h entities.h particles.h

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all run clean
