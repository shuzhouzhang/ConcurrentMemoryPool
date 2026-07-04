CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -pedantic -O2 -pthread
ASAN_FLAGS ?= -std=c++11 -Wall -Wextra -pedantic -O1 -g -fsanitize=address -fno-omit-frame-pointer -pthread

BUILD_DIR := build
CORE_SRC := ThreadCache.cpp CentralCache.cpp PageCache.cpp
TEST_BIN := $(BUILD_DIR)/cmp_test
BENCH_BIN := $(BUILD_DIR)/cmp_bench
ASAN_TEST_BIN := $(BUILD_DIR)/cmp_test_asan

.PHONY: all clean test bench asan-test

all: $(TEST_BIN) $(BENCH_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_BIN): uniTest.cpp $(CORE_SRC) *.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) uniTest.cpp $(CORE_SRC) -o $@

$(BENCH_BIN): benchmark.cpp $(CORE_SRC) *.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) benchmark.cpp $(CORE_SRC) -o $@

$(ASAN_TEST_BIN): uniTest.cpp $(CORE_SRC) *.h | $(BUILD_DIR)
	$(CXX) $(ASAN_FLAGS) uniTest.cpp $(CORE_SRC) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

bench: $(BENCH_BIN)
	./$(BENCH_BIN)

asan-test: $(ASAN_TEST_BIN)
	./$(ASAN_TEST_BIN)

clean:
	rm -rf $(BUILD_DIR)
