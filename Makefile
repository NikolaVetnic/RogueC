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

CFLAGS := -std=c17 $(WARN) $(OPT) -MMD -MP -I.

GAME_SRC := $(filter-out main.c,$(wildcard *.c))
GAME_OBJ := $(GAME_SRC:%.c=$(OUT)/%.o)
MAIN_OBJ := $(OUT)/main.o
TEST_OBJ := $(patsubst %.c,$(OUT)/%.o,$(wildcard tests/*.c))

.PHONY: all run debug test format clean help

all: $(OUT)/rogue                     ## build the game (release)

$(OUT)/rogue: $(GAME_OBJ) $(MAIN_OBJ)
	$(CC) $(LDOPT) -o $@ $^ $(CURSES)

$(OUT)/test_runner: $(GAME_OBJ) $(TEST_OBJ)
	$(CC) $(LDOPT) -o $@ $^ $(CURSES)

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

format:                               ## format all sources with clang-format
	clang-format -i $(wildcard *.c *.h tests/*.c tests/*.h)

clean:                                ## remove build output
	rm -rf build

help:
	@grep -E '^[a-z]+:.*##' $(MAKEFILE_LIST) | sed 's/:.*##/:/'

-include $(GAME_OBJ:.o=.d) $(MAIN_OBJ:.o=.d) $(TEST_OBJ:.o=.d)
