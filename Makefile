# RogueC build. `make help` lists targets.

CC    ?= cc
MODE  ?= release
OUT   := build/$(MODE)

WARN  := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes \
         -Wmissing-prototypes -Wformat=2 -Werror

ifeq ($(MODE),debug)
  OPT   := -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer
  LDOPT := -fsanitize=address,undefined
else
  OPT   := -O2
  LDOPT :=
endif

ifeq ($(shell uname -s),Darwin)
  CURSES := -lncurses
else
  CURSES := -lncursesw
endif

CFLAGS := -std=c17 $(WARN) $(OPT) -MMD -MP -Isrc

# core: game logic, no I/O. term: ncurses frontend. tests link the core only.
CORE_OBJ := $(patsubst %.c,$(OUT)/%.o,$(wildcard src/core/*.c))
TERM_OBJ := $(patsubst %.c,$(OUT)/%.o,$(wildcard src/platform/term/*.c))
TEST_OBJ := $(patsubst %.c,$(OUT)/%.o,$(wildcard tests/*.c))

.PHONY: all run debug test leaks format clean help

all: $(OUT)/rogue                     ## build the game (release)

$(OUT)/rogue: $(CORE_OBJ) $(TERM_OBJ)
	$(CC) $(LDOPT) -o $@ $^ $(CURSES)

$(OUT)/test_runner: $(CORE_OBJ) $(TEST_OBJ)
	$(CC) $(LDOPT) -o $@ $^

$(OUT)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: all                              ## build and play
	./$(OUT)/rogue

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

clean:                                ## remove build output
	rm -rf build

help:
	@grep -hE '^[a-z]+:.*##' Makefile | awk -F':.*## ' '{ printf "  make %-8s %s\n", $$1, $$2 }'

-include $(CORE_OBJ:.o=.d) $(TERM_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
