CC ?= gcc
BASE_CFLAGS := -Wall -Wextra -Werror -pedantic -std=c11 -Iinclude -pthread

ASAN_CFLAGS  := $(BASE_CFLAGS) -fsanitize=address,undefined -O1 -g
TSAN_CFLAGS  := $(BASE_CFLAGS) -fsanitize=thread -O2 -g
BENCH_CFLAGS := $(BASE_CFLAGS) -O3 -DNDEBUG

SRCS := src/mem_pool.c \
        src/ring_buffer.c \
        src/bitmap.c \
        src/bdev_wrapper.c \
        src/hal_flash_mock.c \
        src/logger_engine.c

TEST_PHASE1  := tests/unit_test.c
TEST_PHASE2  := tests/integration_test.c
TEST_PHASE3  := tests/concurrency_test.c
TEST_PHASE4  := tests/dynamic_config_test.c
TEST_PHASE5  := tests/fault_tolerance_test.c
TEST_PHASE6  := benchmark/perf_benchmark.c

ifeq (test,$(firstword $(MAKECMDGOALS)))
  SUB_CMD := $(word 2,$(MAKECMDGOALS))
  ifneq ($(SUB_CMD),)
    $(eval $(SUB_CMD): ;@:)
  endif
endif

.PHONY: all test unit integration concurrency-asan concurrency-tsan tsan dynamic-config benchmark clean \
        run-unit run-integration run-concurrency-asan run-concurrency-tsan run-dynamic-config run-benchmark

all: test

# -----------------------------------------------------------------------------
# 測試分流入口
# -----------------------------------------------------------------------------
test:
ifeq ($(SUB_CMD),unit)
	@$(MAKE) --no-print-directory run-unit
else ifeq ($(SUB_CMD),integration)
	@$(MAKE) --no-print-directory run-integration
else ifeq ($(SUB_CMD),concurrency-asan)
	@$(MAKE) --no-print-directory run-concurrency-asan
else ifeq ($(SUB_CMD),concurrency-tsan)
	@$(MAKE) --no-print-directory run-concurrency-tsan
else ifeq ($(SUB_CMD),dynamic-config)
	@$(MAKE) --no-print-directory run-dynamic-config
else ifeq ($(SUB_CMD),fault-tolerance)
	@$(MAKE) --no-print-directory run-fault-tolerance
else ifeq ($(SUB_CMD),benchmark)
	@$(MAKE) --no-print-directory run-benchmark
else ifneq ($(strip $(SUB_CMD)),)
	@echo "Error: Unknown test subtarget '$(SUB_CMD)'"
	@echo "Supported targets:"
	@echo "  make test unit"
	@echo "  make test integration"
	@echo "  make test concurrency-asan"
	@echo "  make test concurrency-tsan"
	@echo "  make test dynamic-config"
	@echo "  make test fault-tolerance"
	@echo "  make test benchmark"
	@exit 1
else
	@echo ">>> Running All Functional Test Suites <<<"
	@$(MAKE) --no-print-directory run-unit
	@$(MAKE) --no-print-directory run-integration
	@$(MAKE) --no-print-directory run-concurrency-asan
	@$(MAKE) --no-print-directory run-concurrency-tsan
	@$(MAKE) --no-print-directory run-dynamic-config
	@$(MAKE) --no-print-directory run-fault-tolerance
	@$(MAKE) --no-print-directory run-benchmark
endif

# -----------------------------------------------------------------------------
# 內部執行目標（與外部命令解耦，避免遞迴重複呼叫）
# -----------------------------------------------------------------------------
run-unit: unit-test
	@echo "Running Phase 1 unit tests (ASan)..."
	@./unit-test

run-integration: integration-test
	@echo "Running Phase 2 integration tests (ASan)..."
	@./integration-test

run-concurrency-asan: concurrency-asan-test
	@echo "Running Phase 3 concurrency tests (ASan)..."
	@./concurrency-asan-test

run-concurrency-tsan: concurrency-tsan-test
	@echo "Running Phase 3 concurrency tests (TSan)..."
	@./concurrency-tsan-test

run-dynamic-config: dynamic-config-test
	@echo "Running Phase 4 dynamic flash configuration tests (ASan)..."
	@./dynamic-config-test

run-fault-tolerance: fault-tolerance-test
	@echo "Running Phase 5 fault tolerance and boundary tests (ASan)..."
	@./fault-tolerance-test

run-benchmark: perf-bench-test
	@echo "Running Phase 6 performance benchmark tests..."
	@./perf-bench-test

# -----------------------------------------------------------------------------
# 快捷入口（支援直接下 make unit、make benchmark 等呼叫）
# -----------------------------------------------------------------------------
unit: run-unit
integration: run-integration
concurrency-asan: run-concurrency-asan
concurrency-tsan: run-concurrency-tsan
dynamic-config: run-dynamic-config
fault-tolerance: run-fault-tolerance
benchmark: run-benchmark

# -----------------------------------------------------------------------------
# 編譯規則
# -----------------------------------------------------------------------------
unit-test: $(SRCS) $(TEST_PHASE1)
	@$(CC) $(ASAN_CFLAGS) $^ -o $@

integration-test: $(SRCS) $(TEST_PHASE2)
	@$(CC) $(ASAN_CFLAGS) $^ -o $@

concurrency-asan-test: $(SRCS) $(TEST_PHASE3)
	@$(CC) $(ASAN_CFLAGS) $^ -o $@

concurrency-tsan-test: $(SRCS) $(TEST_PHASE3)
	@$(CC) $(TSAN_CFLAGS) $^ -o $@

dynamic-config-test: $(SRCS) $(TEST_PHASE4)
	@$(CC) $(ASAN_CFLAGS) $^ -o $@

fault-tolerance-test: $(SRCS) $(TEST_PHASE5)
	@$(CC) $(ASAN_CFLAGS) $^ -o $@

perf-bench-test: $(SRCS) $(TEST_PHASE6)
	@$(CC) $(BENCH_CFLAGS) $^ -o $@

clean:
	@rm -rf unit-test integration-test concurrency-asan-test concurrency-tsan-test dynamic-config-test fault-tolerance-test perf-bench-test virtual_flash*.bin test_*.bin *.dSYM
