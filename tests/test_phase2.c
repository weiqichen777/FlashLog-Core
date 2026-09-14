#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "logger_engine.h"
#include "hal_flash_mock.h"

#define TEST_IMAGE "virtual_flash.bin"
#define TEST_PACKET_COUNT 100

int main(void) {
    printf("======================================================================\n");
    printf("[Phase 2] Starting End-to-End Pipeline Integration Test...\n");

    // 移除舊的模擬映像檔
    remove(TEST_IMAGE);

    // 1. 初始化 Mock HAL 與 Logger Engine
    assert(hal_flash_mock_init(TEST_IMAGE));
    LoggerEngine engine;
    assert(logger_engine_init(&engine, hal_flash_mock_get_ops()));

    // 2. 模擬寫入 100 筆遙測資料
    printf("  -> Enqueueing %d telemetry logs...\n", TEST_PACKET_COUNT);
    for (uint32_t i = 0; i < TEST_PACKET_COUNT; i++) {
        char dummy_msg[LOG_MAX_PAYLOAD];
        snprintf(dummy_msg, sizeof(dummy_msg), "DATA_%04u", i + 1);

        bool ok = logger_write(&engine, dummy_msg, (uint16_t)strlen(dummy_msg) + 1);
        assert(ok);

        // 每隔幾筆排程一次背景消費，模擬真實時序
        if (i % 8 == 0) {
            logger_process(&engine, 16);
        }
    }

    // 3. 排空剩餘資料並 Flush 到虛擬 Flash
    logger_process(&engine, 200);
    assert(logger_flush(&engine) == 0);
    assert(engine.dropped_logs == 0);
    printf("  -> All logs processed. Flush complete.\n");

    hal_flash_mock_deinit();

    // 4. 二進位回讀檢驗 (Offline Verification)
    printf("  -> Verifying virtual_flash.bin integrity...\n");
    FILE *fp = fopen(TEST_IMAGE, "rb");
    assert(fp != NULL);

    uint32_t expected_seq = 1;
    LogEntry read_entry;

    while (fread(&read_entry, 1, sizeof(LogEntry), fp) == sizeof(LogEntry)) {
        // 如果讀到 0xFF 代表到達未寫滿的 Flash 擦除區
        if (read_entry.magic == 0xFFFF) {
            continue;
        }

        assert(read_entry.magic == LOG_MAGIC);
        assert(read_entry.seq_id == expected_seq);

        char expected_str[LOG_MAX_PAYLOAD];
        snprintf(expected_str, sizeof(expected_str), "DATA_%04u", expected_seq);
        assert(strcmp((char *)read_entry.payload, expected_str) == 0);

        expected_seq++;
        if (expected_seq > TEST_PACKET_COUNT) {
            break;
        }
    }

    fclose(fp);
    assert(expected_seq == TEST_PACKET_COUNT + 1);

    printf("[Phase 2] Verification PASSED: 100%% Monotonic & Payload Accurate.\n");
    printf("======================================================================\n\n");
    return 0;
}
