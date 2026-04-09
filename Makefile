CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wno-unused-function -I core -I lib -I assets
LDFLAGS = -lm

# Platform detection
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
    LDFLAGS += -framework Cocoa -framework OpenGL -framework QuartzCore
else
    LDFLAGS += -lGL -lX11 -lXi -lXcursor -ldl -lpthread
endif

SRCS = platform/standalone.c core/spa.c
TARGET = tempus

.PHONY: all clean atlas serve

all: $(TARGET)

$(TARGET): $(SRCS) assets/font_atlas.h assets/font_atlas.png
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)

# Font atlas generation
tools/bake_font: tools/bake_font.c
	$(CC) -std=c11 -O2 -I lib tools/bake_font.c -o $@ -lm

atlas: tools/bake_font
	./tools/bake_font assets

assets/font_atlas.h assets/font_atlas.png: tools/bake_font
	./tools/bake_font assets

clean:
	rm -f $(TARGET) tools/bake_font
