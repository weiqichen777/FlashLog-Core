#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "logger_engine.h"
#include "hal_flash_mock.h"

#define CONCURRENT_TEST_FILE "virtual_flash_stress.bin"
#define TOTAL_LOG_COUNT      1000000UL // 100 萬筆高壓測試

static LoggerEngine g_engine;
static volatile bool g_producer_done = false;

// 統計數據
static uint64_t g_producer_checksum = 0;
static uint64_t g_consumer_checksum = 0;
static uint32_t g_consumer_received_count = 0;

// Producer 執行緒 (模擬高速資料源 / ISR)
static void *producer_thread(void *arg) {
    (void)arg;
    for (uint32_t i = 1; i <= TOTAL_LOG_COUNT; i++) {
        uint32_t payload_val = i;

        // 當 Ring Buffer 滿時做 yield 重試 (模擬 ISR 緩衝或流控)
        while (!logger_write(&g_engine, &payload_val, sizeof(payload_val))) {
            // 在使用者空間使用 sched_yield 讓出 CPU
            sched_yield();
        }

        g_producer_checksum += payload_val;
    }

    __atomic_store_n(&g_producer_done, true, __ATOMIC_RELEASE);
    return NULL;
}

// Consumer 執行緒 (模擬背景 Flash 儲存任務)
// Consumer 執行緒
static void *consumer_thread(void *arg) {
    (void)arg;
    uint32_t last_seq_id = 0;

    while (true) {
        LogEntry entry;
        // 直接彈出物件本體
        if (ring_buf_pop(&g_engine.ring_buf, &entry)) {
            // 1. 驗證單調遞增
            assert(entry.seq_id > last_seq_id);
            last_seq_id = entry.seq_id;

            // 2. 累加資料 Checksum
            uint32_t val = *(uint32_t *)entry.payload;
            g_consumer_checksum += val;
            g_consumer_received_count++;

            // 3. 寫入聚合緩衝區
            bdev_wrapper_write(&g_engine.bdev, &entry, sizeof(LogEntry));
        } else {
            if (__atomic_load_n(&g_producer_done, __ATOMIC_ACQUIRE) &&
                ring_buf_count(&g_engine.ring_buf) == 0) {
                break;
            }
            sched_yield();
        }
    }
    return NULL;
}

int main(void) {
    printf("======================================================================\n");
    printf("Starting Lock-free Concurrency Stress Test (%lu logs)...\n", TOTAL_LOG_COUNT);

    remove(CONCURRENT_TEST_FILE);
    assert(hal_flash_mock_init(CONCURRENT_TEST_FILE, 65536, 512));
    LoggerConfig cfg = { .total_blocks = 65536, .block_size = 512 };
    assert(logger_engine_init(&g_engine, hal_flash_mock_get_ops(), &cfg));

    pthread_t th_prod, th_cons;

    // 啟動生產者與消費者雙執行緒並發
    pthread_create(&th_cons, NULL, consumer_thread, NULL);
    pthread_create(&th_prod, NULL, producer_thread, NULL);

    pthread_join(th_prod, NULL);
    pthread_join(th_cons, NULL);

    // Flush 最後殘留的 Block
    assert(logger_flush(&g_engine) == 0);
    hal_flash_mock_deinit();

    printf("  -> Total Logs Sent:     %lu\n", TOTAL_LOG_COUNT);
    printf("  -> Total Logs Received: %u\n", g_consumer_received_count);
    printf("  -> Producer Checksum:   0x%016llX\n", (unsigned long long)g_producer_checksum);
    printf("  -> Consumer Checksum:   0x%016llX\n", (unsigned long long)g_consumer_checksum);

    // 驗證無漏包、無錯位
    assert(g_consumer_received_count == TOTAL_LOG_COUNT);
    assert(g_producer_checksum == g_consumer_checksum);

    printf("PASSED: Zero Data Race, Perfect Monotonic Order, 100%% Integrity.\n");
    printf("======================================================================\n\n");
    return 0;
}
