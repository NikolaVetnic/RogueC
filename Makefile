# RogueC build. `make help` lists targets.
#
# MODE selects the build: release (default), debug (sanitizers) or web (Emscripten).

MODE  ?= release
OUT   := build/$(MODE)

WARN  := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes \
         -Wmissing-prototypes -Wformat=2 -Werror

ifeq ($(MODE),debug)
  CC    ?= cc
  OPT   := -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer
  LDOPT := -fsanitize=address,undefined
else ifeq ($(MODE),web)
  CC    := emcc
  OPT   := -Os
  LDOPT :=
else
  CC    ?= cc
  OPT   := -O2
  LDOPT :=
endif

ifeq ($(shell uname -s),Darwin)
  CURSES := -lncurses
else
  CURSES := -lncursesw
endif

CFLAGS := -std=c17 $(WARN) $(OPT) -MMD -MP -Isrc

# core: game logic, no I/O. term: ncurses frontend. gui: raylib frontend
# (desktop and web). common: shared by the frontends (high-score storage).
# Tests link the core only.
CORE_OBJ := $(patsubst %.c,$(OUT)/%.o,$(wildcard src/core/*.c))
TERM_OBJ := $(patsubst %.c,$(OUT)/%.o,$(wildcard src/platform/term/*.c))
GUI_OBJ  := $(patsubst %.c,$(OUT)/%.o,$(wildcard src/platform/raylib/*.c))
COMMON_OBJ := $(patsubst %.c,$(OUT)/%.o,$(wildcard src/platform/common/*.c))
TEST_OBJ := $(patsubst %.c,$(OUT)/%.o,$(wildcard tests/*.c))

# raylib: the installed one (pkg-config) for desktop; for the web, the same
# version built from source with Emscripten (`make deps`).
RAYLIB_VERSION := 6.2
RAYLIB_DIR     := third_party/raylib
RAYLIB_WEB_LIB := build/raylib-web/libraylib.web.a

ifeq ($(MODE),web)
  RAYLIB_INC  := -isystem $(RAYLIB_DIR)/src
  RAYLIB_LIBS := $(RAYLIB_WEB_LIB)
else
  # -isystem: raylib's header isn't held to this project's warning flags
  RAYLIB_INC  := $(patsubst -I%,-isystem %,$(shell pkg-config --cflags raylib 2>/dev/null))
  RAYLIB_LIBS := $(shell pkg-config --libs raylib 2>/dev/null) -lm
endif
$(GUI_OBJ): CFLAGS += $(RAYLIB_INC)

WEB_SITE  := build/www
WEB_FLAGS := -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 \
             --preload-file assets/fonts@assets/fonts --shell-file web/shell.html

.PHONY: all run gui run-gui web serve pages deps debug test leaks format clean distclean help

all: $(OUT)/rogue                     ## build the terminal game (release)

$(OUT)/rogue: $(CORE_OBJ) $(TERM_OBJ) $(COMMON_OBJ)
	$(CC) $(LDOPT) -o $@ $^ $(CURSES)

$(OUT)/rogue-gui: $(CORE_OBJ) $(GUI_OBJ) $(COMMON_OBJ)
	$(CC) $(LDOPT) -o $@ $^ $(RAYLIB_LIBS)

$(OUT)/test_runner: $(CORE_OBJ) $(TEST_OBJ)
	$(CC) $(LDOPT) -o $@ $^

$(WEB_SITE)/index.html: $(CORE_OBJ) $(GUI_OBJ) $(COMMON_OBJ) $(RAYLIB_WEB_LIB) web/shell.html $(wildcard assets/fonts/*)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(CORE_OBJ) $(GUI_OBJ) $(COMMON_OBJ) $(RAYLIB_WEB_LIB) $(OPT) $(WEB_FLAGS)

$(OUT)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(RAYLIB_DIR)/src/raylib.h:
	git clone --depth 1 --branch $(RAYLIB_VERSION) https://github.com/raysan5/raylib.git $(RAYLIB_DIR)

$(RAYLIB_WEB_LIB): | $(RAYLIB_DIR)/src/raylib.h
	@mkdir -p $(dir $@)
	$(MAKE) -C $(RAYLIB_DIR)/src PLATFORM=PLATFORM_WEB RAYLIB_RELEASE_PATH=$(abspath $(dir $@))

run: all                              ## build and play in the terminal
	./$(OUT)/rogue

gui: $(OUT)/rogue-gui                 ## build the desktop window version (needs raylib installed)

run-gui: gui                          ## build and play in a window
	./$(OUT)/rogue-gui

deps: $(RAYLIB_WEB_LIB)               ## fetch raylib source and build it for the web (needs emcc)

web:                                  ## build the browser version into build/www (needs emcc)
	$(MAKE) MODE=web $(WEB_SITE)/index.html

serve: web                            ## build the browser version and serve it on localhost:8000
	python3 -m http.server 8000 --directory $(WEB_SITE)

pages: web                            ## commit build/www to the gh-pages branch (push it to deploy)
	touch $(WEB_SITE)/.nojekyll
	rm -f build/pages.index
	GIT_INDEX_FILE=build/pages.index git --work-tree=$(WEB_SITE) add -A .
	tree=$$(GIT_INDEX_FILE=build/pages.index git write-tree) && \
	parent=$$(git rev-parse -q --verify gh-pages) ; \
	commit=$$(git commit-tree $$tree $${parent:+-p $$parent} -m "Browser build of $$(git rev-parse --short HEAD)") && \
	git update-ref refs/heads/gh-pages $$commit && \
	echo "gh-pages updated; deploy with: git push origin gh-pages"
	rm -f build/pages.index

debug:                                ## build with ASan + UBSan
	$(MAKE) MODE=debug

test:                                 ## build and run unit tests with sanitizers
	$(MAKE) MODE=debug build/debug/test_runner
	./build/debug/test_runner

leaks:                                ## macOS: run tests under `leaks` (ASan finds no leaks on macOS)
	$(MAKE) MODE=release build/release/test_runner
	leaks --atExit -- ./build/release/test_runner

format:                               ## format all sources with clang-format
	clang-format -i $(wildcard src/*/*.[ch] src/*/*/*.[ch] tests/*.[ch])

clean:                                ## remove build output (keeps fetched raylib)
	rm -rf build

distclean: clean                      ## also remove fetched dependencies
	rm -rf third_party

help:
	@grep -hE '^[a-z-]+:.*##' Makefile | awk -F':.*## ' '{ printf "  make %-10s %s\n", $$1, $$2 }'

-include $(CORE_OBJ:.o=.d) $(TERM_OBJ:.o=.d) $(GUI_OBJ:.o=.d) $(COMMON_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
