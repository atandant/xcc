# SPDX-License-Identifier: MIT
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -g
CPPFLAGS = -Isrc -Ibuild
BISON   ?= bison

BUILD = build
BIN   = xcc

# Hand-written sources
SRCS = src/main.c src/arena.c src/source.c src/cpp/cpp.c src/cpp/macro.c \
       src/cpp/expr.c src/lexer.c \
       src/ast.c src/type.c src/diag.c src/sema.c \
       src/sema_scope.c src/sema_functab.c src/sema_typedef.c src/sema_struct.c \
       src/sema_enum.c src/abi_sysv_amd64.c \
       src/intconst.c src/ast_const_fold.c src/ast_const_prop.c src/ast_opt.c \
       src/lir_opt.c src/lir_dom.c src/lir_mem2reg.c src/lir_copy_prop.c \
       src/lir_dce.c src/lir_licm.c \
       src/lir_algebraic_simplify.c src/lir_strength_reduce.c \
       src/lir_simplify_conv.c src/codegen.c \
       src/lir.c src/lir_cfg.c src/lower.c src/liveness.c src/regalloc.c src/emit_x86.c
OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

# Generated parser source is compiled with warnings off
GEN_OBJS = $(BUILD)/parser.o

DEPS = $(OBJS:.o=.d) $(GEN_OBJS:.o=.d)

.PHONY: all clean test examples

LIR_TEST_SRCS = tests/lir/main.c tests/lir/test.c \
                tests/lir/algebraic.c tests/lir/strength_reduce.c \
                tests/lir/simplify_conv.c tests/lir/dce.c \
                tests/lir/licm.c tests/lir/mem2reg.c tests/lir/copy_prop.c \
                tests/lir/cfg.c \
                tests/lir/x87.c
LIR_TEST_OBJS = $(BUILD)/arena.o $(BUILD)/source.o $(BUILD)/diag.o $(BUILD)/lir.o \
                $(BUILD)/lir_cfg.o $(BUILD)/lir_dom.o $(BUILD)/lir_mem2reg.o \
                $(BUILD)/lir_copy_prop.o $(BUILD)/lir_dce.o $(BUILD)/lir_licm.o \
                $(BUILD)/lir_algebraic_simplify.o $(BUILD)/lir_strength_reduce.o \
                $(BUILD)/lir_simplify_conv.o $(BUILD)/type.o \
                $(BUILD)/abi_sysv_amd64.o $(BUILD)/liveness.o \
                $(BUILD)/regalloc.o $(BUILD)/emit_x86.o

all: $(BIN)

$(BIN): $(OBJS) $(GEN_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD):
	mkdir -p $(BUILD)

# bison generates parser.c and parser.h together; grouped targets prevent
# parallel make from running the same recipe once for each output.
$(BUILD)/parser.c $(BUILD)/parser.h &: src/parser.y | $(BUILD)
	$(BISON) -d -o $(BUILD)/parser.c src/parser.y

# hand-written objects (with full warnings + auto deps)
$(BUILD)/%.o: src/%.c | $(BUILD)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/lexer.o: $(BUILD)/parser.h

# generated object (warnings suppressed; bison output is noisy).
GEN_CPPFLAGS = $(CPPFLAGS)

$(BUILD)/parser.o: $(BUILD)/parser.c | $(BUILD)
	$(CC) $(CFLAGS) $(GEN_CPPFLAGS) -w -MMD -MP -c $< -o $@

$(BUILD)/lir-tests: $(LIR_TEST_SRCS) $(LIR_TEST_OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LIR_TEST_SRCS) $(LIR_TEST_OBJS) -o $@

test: $(BIN) $(BUILD)/lir-tests
	./$(BUILD)/lir-tests
	./tests/run.sh
	./tests/run-cpp.sh
	./tests/run-cpp-driver.sh

examples: $(BIN)
	./examples/build.sh

clean:
	rm -rf $(BUILD) $(BIN)

-include $(DEPS)
