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

# Maximum static build achievable from Ubuntu -dev packages (no source builds):
# statically links every dependency that ships a .a archive -- GLEW and the whole
# X11/xcb chain -- plus libgcc.
#
# What stays dynamic, and why it MUST:
#   * libglfw   - Ubuntu ships GLFW only as a shared lib (no libglfw3.a).
#   * libGL/GLX/GLdispatch - GLVND: the actual GPU driver (dri/*_dri.so) is
#                 dlopen'd at runtime, so there is no static libGL to link.
#   * libc      - kept dynamic (glibc static linking is discouraged / dlopen).
# Note: because libglfw stays dynamic it re-introduces libX11/libxcb at runtime,
# so `ldd` still lists them; GLEW is the dep that actually leaves our binary.
STATIC_TARGET := $(TARGET)-static
STATIC_LDFLAGS := -static-libgcc \
  -Wl,-Bstatic -lGLEW -lXcursor -lXinerama -lXrandr -lXi -lXxf86vm \
                -lXrender -lXext -lXfixes -lX11 -lxcb -lXau -lXdmcp -Wl,-Bdynamic \
  -lglfw -lGL -lpthread -ldl -lm

static: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -DGLEW_STATIC -o $(STATIC_TARGET) $(SRC) $(STATIC_LDFLAGS)

clean:
	rm -f $(TARGET) $(STATIC_TARGET) $(OBJ)

.PHONY: all run static clean
