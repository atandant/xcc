# SPDX-License-Identifier: MIT
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -g
CPPFLAGS = -Isrc -Ibuild
BISON   ?= bison
FLEX    ?= flex

BUILD = build
BIN   = xcc

# Hand-written sources
SRCS = src/main.c src/arena.c src/ast.c src/type.c src/diag.c src/sema.c src/codegen.c
OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

# Generated sources (bison/flex) - compiled with warnings off
GEN_OBJS = $(BUILD)/parser.o $(BUILD)/lexer.o

DEPS = $(OBJS:.o=.d) $(GEN_OBJS:.o=.d)

.PHONY: all clean test examples

all: $(BIN)

$(BIN): $(OBJS) $(GEN_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD):
	mkdir -p $(BUILD)

# bison generates parser.c and parser.h together
$(BUILD)/parser.c $(BUILD)/parser.h: src/parser.y | $(BUILD)
	$(BISON) -d -o $(BUILD)/parser.c src/parser.y

# flex needs the bison-generated token header
$(BUILD)/lexer.c: src/lexer.l $(BUILD)/parser.h | $(BUILD)
	$(FLEX) -o $(BUILD)/lexer.c src/lexer.l

# hand-written objects (with full warnings + auto deps)
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

# generated objects (warnings suppressed; flex/bison output is noisy).
# _POSIX_C_SOURCE exposes fileno(), which flex's scanner relies on.
GEN_CPPFLAGS = $(CPPFLAGS) -D_POSIX_C_SOURCE=200809L

$(BUILD)/parser.o: $(BUILD)/parser.c | $(BUILD)
	$(CC) $(CFLAGS) $(GEN_CPPFLAGS) -w -MMD -MP -c $< -o $@

$(BUILD)/lexer.o: $(BUILD)/lexer.c $(BUILD)/parser.h | $(BUILD)
	$(CC) $(CFLAGS) $(GEN_CPPFLAGS) -w -MMD -MP -c $< -o $@

test: $(BIN)
	./tests/run.sh

examples: $(BIN)
	./examples/build.sh

clean:
	rm -rf $(BUILD) $(BIN)

-include $(DEPS)
