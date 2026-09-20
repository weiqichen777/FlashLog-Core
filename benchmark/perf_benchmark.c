#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>
#include "mem_pool.h"
#include "ring_buffer.h"
#include "bdev_wrapper.h"

#define BENCH_OPS 10000000UL // 1000 萬次操作

// 統計硬體 Block 寫入次數
static uint32_t g_raw_block_writes = 0;

static int bench_mock_write(uint32_t block_id, const uint8_t *buffer) {
    (void)block_id;
    (void)buffer;
    g_raw_block_writes++;
    return 0;
}

static HalBlockOps bench_hal_ops = {
    .raw_write = bench_mock_write,
    .raw_read = NULL
};

// 取得高精度奈秒時間
static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void bench_memory_pool(void) {
    printf("[1/3] Benchmarking Fixed-Block Memory Pool (%lu ops)...\n", BENCH_OPS);
    
    #define POOL_SIZE 64
    uint8_t pool_mem[POOL_SIZE * 32];
    MemPool pool;
    bool ok = mem_pool_init(&pool, pool_mem, 32, POOL_SIZE);
    if (!ok) {
        fprintf(stderr, "MemPool init failed\n");
        return;
    }

    uint64_t start = get_time_ns();
    for (uint64_t i = 0; i < BENCH_OPS; i++) {
        void *p = mem_pool_alloc(&pool);
        mem_pool_free(&pool, p);
    }
    uint64_t end = get_time_ns();

    double total_ms = (double)(end - start) / 1e6;
    double ns_per_op = (double)(end - start) / (double)(BENCH_OPS * 2);
    printf("  -> Total Time:       %.2f ms\n", total_ms);
    printf("  -> Latency:          %.2f ns/op (Alloc + Free combined)\n\n", ns_per_op);
}

static void bench_ring_buffer(void) {
    printf("[2/3] Benchmarking Lock-Free SPSC Ring Buffer (%lu ops)...\n", BENCH_OPS);

    #define RB_DEPTH 256
    uint32_t storage[RB_DEPTH];
    RingBuffer rb;
    bool ok = ring_buf_init(&rb, storage, sizeof(uint32_t), RB_DEPTH);
    if (!ok) {
        fprintf(stderr, "RingBuffer init failed\n");
        return;
    }

    uint32_t dummy = 0x5A5A;
    uint32_t out = 0;

    uint64_t start = get_time_ns();
    for (uint64_t i = 0; i < BENCH_OPS; i++) {
        ring_buf_push(&rb, &dummy);
        ring_buf_pop(&rb, &out);
    }
    uint64_t end = get_time_ns();

    double total_sec = (double)(end - start) / 1e9;
    double mops = ((double)BENCH_OPS / total_sec) / 1e6;
    printf("  -> Throughput:       %.2f Mops/s (Push + Pop pairs)\n", mops);
    printf("  -> Avg Transfer:     %.2f ns/item\n\n", (double)(end - start) / (double)BENCH_OPS);
}

static void bench_bdev_coalescing(void) {
    printf("[3/3] Benchmarking Driver Wrapper (WAF & Write Reduction)...\n");

    #define BENCH_BLOCK_SIZE 512
    uint8_t bdev_cache_buf[BENCH_BLOCK_SIZE];
    BdevWrapper bdev;
    
    // 傳入完整的 5 個參數：&bdev, ops, buffer, block_size, start_block
    if (!bdev_wrapper_init(&bdev, &bench_hal_ops, bdev_cache_buf, BENCH_BLOCK_SIZE, 0)) {
        fprintf(stderr, "BdevWrapper init failed\n");
        return;
    }
    g_raw_block_writes = 0;

    // 模擬 1,000,000 次 32-Byte 日誌寫入
    #define LOG_WRITES 1000000UL
    uint8_t dummy_log[32] = {0};

    for (uint64_t i = 0; i < LOG_WRITES; i++) {
        bdev_wrapper_write(&bdev, dummy_log, sizeof(dummy_log));
    }
    bdev_wrapper_flush(&bdev);

    uint64_t total_payload_bytes = LOG_WRITES * sizeof(dummy_log);
    uint64_t total_flash_bytes_written = (uint64_t)g_raw_block_writes * BENCH_BLOCK_SIZE;
    double waf = (double)total_flash_bytes_written / (double)total_payload_bytes;
    double reduction_pct = (1.0 - ((double)g_raw_block_writes / (double)LOG_WRITES)) * 100.0;

    printf("  -> Telemetry Log Count:      %lu writes (32 bytes each)\n", LOG_WRITES);
    printf("  -> Flash Physical Blocks:    %u writes (%d bytes each)\n", g_raw_block_writes, BENCH_BLOCK_SIZE);
    printf("  -> Write Amplification (WAF): %.2f\n", waf);
    printf("  -> Flash Wear Reduction:     %.2f%%\n\n", reduction_pct);
}

int main(void) {
    printf("======================================================================\n");

    bench_memory_pool();
    bench_ring_buffer();
    bench_bdev_coalescing();

    printf("PASSED: Benchmark Completed.\n");
    printf("======================================================================\n");
    return 0;
}
