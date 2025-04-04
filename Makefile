# Need to use g++ because the sdk headers use classes
CC=g++
INCLUDES=-Isrc/include/sdk/common -Isrc/include/sdk/public -Isrc/include/sdk/pm_shared -Isrc/include/sdk/engine -Isrc/include
CFLAGS=-Wall -Wextra -Wno-write-strings -m32 -fPIC -fpermissive $(INCLUDES)
LDFLAGS=-lm -lGL

IMGUI_DIR=src/include/imgui
IMGUI_INCLUDES=-I$(IMGUI_DIR)
IMGUI_SRCS=$(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp $(IMGUI_DIR)/backends/imgui_impl_opengl2.cpp
IMGUI_OBJS=$(patsubst %.cpp,%.o,$(IMGUI_SRCS))

OBJS=obj/main.c.o obj/globals.c.o obj/settings.c.o obj/hooks.c.o obj/detour.c.o obj/util.c.o obj/features/movement.c.o obj/features/anti_aim.c.o obj/features/fov.c.o obj/features/namechanger.c.o obj/features/esp.c.o obj/features/chams.c.o obj/features/aim.c.o obj/features/misc.c.o obj/features/thirdperson.c.o obj/game_detection.c.o obj/menu.c.o $(IMGUI_OBJS)
BIN=libhlcheat.so

.PHONY: clean all inject

# -------------------------------------------

all: $(BIN)

clean:
	rm -f $(OBJS)
	rm -f $(BIN)

inject: $(BIN)
	bash ./inject.sh

# -------------------------------------------

# -fPIC (in CFLAGS) and -shared for creating a library (shared object)
# -m32 (in CFLAGS) because of the game's arch
$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -shared -o $@ $(OBJS) $(LDFLAGS)

obj/%.c.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $< $(LDFLAGS)

$(IMGUI_DIR)/backends/%.o: $(IMGUI_DIR)/backends/%.cpp
	$(CC) $(CFLAGS) $(IMGUI_INCLUDES) -c -o $@ $< $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $< $(LDFLAGS)
