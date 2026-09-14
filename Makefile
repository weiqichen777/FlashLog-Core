CC ?= gcc
BASE_CFLAGS := -Wall -Wextra -Werror -pedantic -std=c11 -Iinclude -pthread

ASAN_CFLAGS := $(BASE_CFLAGS) -fsanitize=address,undefined -O1 -g
TSAN_CFLAGS := $(BASE_CFLAGS) -fsanitize=thread -O2 -g
BENCH_CFLAGS := $(BASE_CFLAGS) -O3 -DNDEBUG

SRCS := src/mem_pool.c \
        src/ring_buffer.c \
        src/bitmap.c \
        src/bdev_wrapper.c \
        src/hal_flash_mock.c \
        src/logger_engine.c

TEST_PHASE1 := tests/test_phase1.c
TEST_PHASE2 := tests/test_phase2.c
TEST_PHASE3 := tests/test_phase3_concurrency.c
BENCH_SRC   := benchmark/perf_benchmark.c

.PHONY: all test tsan bench clean

all: test

test: test_phase1 test_phase2 test_phase3_asan
	@echo "Running Phase 1 tests (ASan)..."
	@./test_phase1
	@echo "Running Phase 2 integration tests (ASan)..."
	@./test_phase2
	@echo "Running Phase 3 concurrency tests (ASan)..."
	@./test_phase3_asan

test_phase1: $(SRCS) $(TEST_PHASE1)
	@$(CC) $(ASAN_CFLAGS) $^ -o $@

test_phase2: $(SRCS) $(TEST_PHASE2)
	@$(CC) $(ASAN_CFLAGS) $^ -o $@

test_phase3_asan: $(SRCS) $(TEST_PHASE3)
	@$(CC) $(ASAN_CFLAGS) $^ -o $@

tsan: test_phase3_tsan
	@echo "Running Phase 3 concurrency stress test with ThreadSanitizer..."
	@./test_phase3_tsan

test_phase3_tsan: $(SRCS) $(TEST_PHASE3)
	@$(CC) $(TSAN_CFLAGS) $^ -o $@

bench: $(SRCS) $(BENCH_SRC)
	@$(CC) $(BENCH_CFLAGS) $^ -o perf_bench
	@./perf_bench

clean:
	@rm -rf test_phase1 test_phase2 test_phase3_asan test_phase3_tsan perf_bench virtual_flash*.bin *.dSYM
	