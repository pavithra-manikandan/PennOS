#####
# SRC_DIR = src
# BIN_DIR = bin
# DOC_DIR = doc
# LOG_DIR = log

# TESTS_DIR = tests
# UTIL_DIR = src/util

# .PHONY: all tests clean info run-tests format

# CC = clang-15
# CFLAGS = -g3 -gdwarf-4 -pthread -Wall -Werror -Wno-gnu -O0 -g -D_GNU_SOURCE --std=gnu2x
# CPPFLAGS = -I $(SRC_DIR) -I $(UTIL_DIR)

# # Main source files
# MAIN_SRCS = $(SRC_DIR)/pennos.c $(SRC_DIR)/pennfat/pennfat.c
# MAIN_OBJS = $(BIN_DIR)/pennos.o $(BIN_DIR)/pennfat.o
# MAIN_EXECS = $(BIN_DIR)/pennos

# # Non-main sources (excluding main.c and pennfat.c)
# NON_MAIN_SRCS = $(shell find $(SRC_DIR) -type f -name '*.c' | grep -v "pennos.c" | grep -v "pennfat/pennfat.c")
# NON_MAIN_OBJS = $(patsubst %.c, %.o, $(NON_MAIN_SRCS))

# # Test sources
# TEST_SRCS = $(wildcard $(TESTS_DIR)/*.c)
# TEST_EXECS = $(patsubst $(TESTS_DIR)/%.c, $(BIN_DIR)/%, $(TEST_SRCS))

# # ===================== Build Rules =====================

# all: $(BIN_DIR) $(MAIN_EXECS)

# tests: $(BIN_DIR) $(TEST_EXECS)

# $(BIN_DIR):
# 	mkdir -p $(BIN_DIR)

# # ======= Build main object files =======
# $(BIN_DIR)/pennos.o: $(SRC_DIR)/pennos.c
# 	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

# $(BIN_DIR)/pennfat.o: $(SRC_DIR)/pennfat/pennfat.c
# 	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

# # ======= Link main programs =======
# $(BIN_DIR)/pennos: $(BIN_DIR)/pennos.o $(BIN_DIR)/pennfat.o $(NON_MAIN_OBJS)
# 	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(NON_MAIN_OBJS) $(BIN_DIR)/pennfat.o $(BIN_DIR)/pennos.o

# $(BIN_DIR)/pennfat: $(BIN_DIR)/pennfat.o $(NON_MAIN_OBJS)
# 	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(NON_MAIN_OBJS) $(BIN_DIR)/pennfat.o

# # ======= Build utility object files =======
# %.o: %.c
# 	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

# # ======= Build test programs =======
# $(BIN_DIR)/%: $(TESTS_DIR)/%.c $(NON_MAIN_OBJS)
# 	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $< $(NON_MAIN_OBJS) -lpthread

# # ===================== Utilities =====================



# info:
# 	@echo "Main Executables: $(MAIN_EXECS)"
# 	@echo "Main Objects: $(MAIN_OBJS)"
# 	@echo "Utility Objects: $(NON_MAIN_OBJS)"
# 	@echo "Test Executables: $(TEST_EXECS)"

# clean:
# 	rm -f $(NON_MAIN_OBJS) $(MAIN_OBJS) $(MAIN_EXECS) $(TEST_EXECS)

# run-tests: tests
# 	@echo "\nRunning Tests:\n"
# 	@for test_exec in $(TEST_EXECS); do \
# 		echo ">>> Running $$test_exec"; \
# 		$$test_exec || echo "Test $$test_exec failed"; \
# 		echo ""; \
# 	done

# format:
# 	clang-format -i --verbose --style=Chromium $(shell find $(SRC_DIR) $(TESTS_DIR) -type f \( -name '*.c' -o -name '*.h' \))

SRC_DIR = src
BIN_DIR = bin
DOC_DIR = doc
LOG_DIR = log

TESTS_DIR = tests
UTIL_DIR = src/util

.PHONY: all tests clean info run-tests format

CC = clang-15
CFLAGS = -g3 -gdwarf-4 -pthread -Wall -Werror -Wno-gnu -O0 -g -D_GNU_SOURCE --std=gnu2x
CPPFLAGS = -I $(SRC_DIR) -I $(UTIL_DIR)

# Main source files
MAIN_SRCS = $(SRC_DIR)/pennos.c $(SRC_DIR)/pennfat/pennfat.c
MAIN_OBJS = $(BIN_DIR)/pennos.o $(BIN_DIR)/pennfat.o
MAIN_EXECS = $(BIN_DIR)/pennos $(BIN_DIR)/pennfat

# Non-main sources (excluding main.c and pennfat.c)
NON_MAIN_SRCS = $(shell find $(SRC_DIR) -type f -name '*.c' | grep -v "pennos.c" | grep -v "pennfat/pennfat.c")
NON_MAIN_OBJS = $(patsubst %.c, %.o, $(NON_MAIN_SRCS))

# Test sources
TEST_SRCS = $(wildcard $(TESTS_DIR)/*.c)
TEST_EXECS = $(patsubst $(TESTS_DIR)/%.c, $(BIN_DIR)/%, $(TEST_SRCS))

# ===================== Build Rules =====================

all: $(BIN_DIR) $(MAIN_EXECS)

tests: $(BIN_DIR) $(TEST_EXECS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# ======= Build pennos.o =======
$(BIN_DIR)/pennos.o: $(SRC_DIR)/pennos.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

# ======= Build pennfat.o WITHOUT main (for linking into pennos) =======
$(BIN_DIR)/pennfat.o: $(SRC_DIR)/pennfat/pennfat.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

# ======= Build pennfat-standalone.o WITH main() =======
$(BIN_DIR)/pennfat-standalone.o: $(SRC_DIR)/pennfat/pennfat.c
	$(CC) $(CFLAGS) -DSTANDALONE_FAT $(CPPFLAGS) -c -o $@ $<

# ======= Link main programs =======
$(BIN_DIR)/pennos: $(BIN_DIR)/pennos.o $(BIN_DIR)/pennfat.o $(NON_MAIN_OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(NON_MAIN_OBJS) $(BIN_DIR)/pennfat.o $(BIN_DIR)/pennos.o

$(BIN_DIR)/pennfat: $(BIN_DIR)/pennfat-standalone.o $(NON_MAIN_OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(NON_MAIN_OBJS) $(BIN_DIR)/pennfat-standalone.o

# ======= Build utility object files =======
%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

# ======= Build test programs =======
$(BIN_DIR)/%: $(TESTS_DIR)/%.c $(NON_MAIN_OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $< $(NON_MAIN_OBJS) -lpthread

# ===================== Utilities =====================

info:
	@echo "Main Executables: $(MAIN_EXECS)"
	@echo "Main Objects: $(MAIN_OBJS)"
	@echo "Utility Objects: $(NON_MAIN_OBJS)"
	@echo "Test Executables: $(TEST_EXECS)"

clean:
	rm -f $(NON_MAIN_OBJS) $(MAIN_OBJS) $(BIN_DIR)/*.o $(MAIN_EXECS) $(TEST_EXECS)

run-tests: tests
	@echo "\nRunning Tests:\n"
	@for test_exec in $(TEST_EXECS); do \
		echo ">>> Running $$test_exec"; \
		$$test_exec || echo "Test $$test_exec failed"; \
		echo ""; \
	done

format:
	clang-format -i --verbose --style=Chromium $(shell find $(SRC_DIR) $(TESTS_DIR) -type f \( -name '*.c' -o -name '*.h' \))


