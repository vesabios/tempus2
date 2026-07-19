CC = cc
CFLAGS = -std=c11 -O2 -Wall -Wno-unused-function -I core -I lib -I assets
LDFLAGS = -lm

# Platform detection
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
    # sokol_app must be compiled as Objective-C on macOS
    PLATFORM_CFLAGS = -x objective-c -fobjc-arc -Wno-deprecated-declarations
    LDFLAGS += -framework Cocoa -framework OpenGL -framework QuartzCore
else
    PLATFORM_CFLAGS =
    LDFLAGS += -lGL -lX11 -lXi -lXcursor -ldl -lpthread
endif

TARGET = tempus
OBJS = build/standalone.o build/spa.o

.PHONY: all clean atlas web serve

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

HEADERS = $(wildcard core/*.h core/views/*.h shaders/*.h lib/*.h) assets/font_atlas.h

build/standalone.o: platform/standalone.c $(HEADERS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(PLATFORM_CFLAGS) -c platform/standalone.c -o $@

build/spa.o: core/spa.c core/spa.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c core/spa.c -o $@

# Web/wasm build (requires emcc; the durable artifact + browser dev harness)
WEB_DIR = web

web: $(WEB_DIR)/index.html

# The runtime is a single self-contained .js (wasm + assets embedded)
# loaded by a static shell page — BOTH work when the html is opened
# straight from disk (no server): script tags load from file://, and
# the binary lives inside a .js file where emcc's byte-string escaping
# is safe. (Inlining it into the html corrupts the wasm: the HTML
# tokenizer normalizes raw CR bytes to LF inside inline scripts.)
$(WEB_DIR)/index.html: platform/standalone.c platform/shell.html core/spa.c $(HEADERS) assets/font_atlas.png
	@mkdir -p $(WEB_DIR)
	emcc -std=gnu11 -O2 -Wall -Wno-unused-function -I core -I lib -I assets \
	    platform/standalone.c core/spa.c -o $(WEB_DIR)/index.js \
	    -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1 \
	    -sSINGLE_FILE=1 \
	    --embed-file assets/font_atlas.png \
	    --embed-file assets/land_mask.png \
	    --embed-file assets/moon_mask.png
	sed 's|{{{ SCRIPT }}}|<script src="index.js"></script>|' \
	    platform/shell.html > $@

serve: web
	cd $(WEB_DIR) && python3 -m http.server 8123

# Ephemeris validation harness
tools/test_planets: tools/test_planets.c core/planets.h core/spa.c core/spa.h
	$(CC) -std=c11 -O2 -Wall -I core tools/test_planets.c core/spa.c -o $@ -lm

.PHONY: test_planets
test_planets: tools/test_planets
	./tools/test_planets

# Font atlas generation
tools/bake_font: tools/bake_font.c
	$(CC) -std=c11 -O2 -I lib tools/bake_font.c -o $@ -lm

atlas: tools/bake_font
	./tools/bake_font assets

assets/font_atlas.h assets/font_atlas.png: tools/bake_font
	./tools/bake_font assets

clean:
	rm -f $(TARGET) tools/bake_font
	rm -rf build

# ---- macOS screensaver bundle ----
# The instrument as a .saver: same core, same GPU layer, hosted in a
# ScreenSaverView. Ad-hoc signed for local use; shipping it to other
# Macs needs a Developer ID and Apple notarization.
SAVER = Tempus.saver

.PHONY: saver saver-install

saver:
	@rm -rf $(SAVER)
	@mkdir -p $(SAVER)/Contents/MacOS $(SAVER)/Contents/Resources
	$(CC) $(CFLAGS) $(PLATFORM_CFLAGS) -bundle \
	    -framework ScreenSaver -framework Cocoa -framework OpenGL \
	    -framework Foundation \
	    platform/saver.m core/spa.c -o $(SAVER)/Contents/MacOS/Tempus -lm
	@cp platform/saver-Info.plist $(SAVER)/Contents/Info.plist
	@cp assets/font_atlas.png assets/land_mask.png assets/moon_mask.png \
	    $(SAVER)/Contents/Resources/
	@codesign --force --deep --sign - $(SAVER) 2>/dev/null || true
	@echo "built $(SAVER)"

saver-install: saver
	@mkdir -p ~/Library/Screen\ Savers
	@rm -rf ~/Library/Screen\ Savers/$(SAVER)
	@cp -R $(SAVER) ~/Library/Screen\ Savers/
	@echo "installed to ~/Library/Screen Savers/$(SAVER)"
