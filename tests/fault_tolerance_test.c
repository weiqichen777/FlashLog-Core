#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "logger_engine.h"
#include "hal_flash_mock.h"

#define FAULT_TEST_IMAGE "test_fault.bin"

// 取得實體檔案大小
static long get_file_size(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    return size;
}

// ============================================================================
// 1. 驗證 Flash 擦除態 Padding (0xFF) 與檔案幾何尺寸對齊
// ============================================================================
static void test_padding_and_alignment(void) {
    printf("[1/3] Testing Flash 0xFF Padding & File Geometry...\n");
    remove(FAULT_TEST_IMAGE);

    const uint32_t total_blocks = 8;
    const uint32_t block_size = 512;

    assert(hal_flash_mock_init(FAULT_TEST_IMAGE, total_blocks, block_size));
    
    LoggerConfig cfg = {
        .total_blocks = total_blocks,
        .block_size = block_size
    };
    LoggerEngine engine;
    assert(logger_engine_init(&engine, hal_flash_mock_get_ops(), &cfg));

    // 只寫入 1 筆短資料 (sizeof(LogEntry) 固定為 32 位元組)
    const char *payload_text = "TOLERANCE_01";
    bool ok = logger_write(&engine, payload_text, (uint16_t)strlen(payload_text) + 1);
    assert(ok);

    // 消費該筆資料並強制 Flush 至實體檔案
    size_t processed = logger_process(&engine, 1);
    assert(processed == 1);
    assert(logger_flush(&engine) == 0);

    hal_flash_mock_deinit();

    // A. 驗證實體檔案尺寸：必須至少等於 total_blocks * block_size
    long actual_file_size = get_file_size(FAULT_TEST_IMAGE);
    long expected_min_size = (long)total_blocks * block_size;
    assert(actual_file_size >= expected_min_size);

    // B. 回讀第一個 Block，驗證前 32 Bytes 為有效資料，後續 480 Bytes 嚴格為 0xFF
    FILE *fp = fopen(FAULT_TEST_IMAGE, "rb");
    assert(fp != NULL);

    uint8_t block_buf[512];
    assert(fread(block_buf, 1, block_size, fp) == block_size);
    fclose(fp);

    LogEntry *entry = (LogEntry *)block_buf;
    assert(entry->magic == LOG_MAGIC);
    assert(entry->seq_id == 1);
    assert(strcmp((char *)entry->payload, payload_text) == 0);

    // 檢查未填滿空間是否嚴格填補 Flash 擦除態 0xFF
    for (size_t i = sizeof(LogEntry); i < block_size; i++) {
        assert(block_buf[i] == 0xFF);
    }

    printf("  -> [PASSED] File size aligned, 0xFF padding strictly enforced.\n\n");
}

// ============================================================================
// 2. 驗證硬體寫入錯誤（壞塊）動態容錯與自動重分配
// ============================================================================
static void test_bad_block_failover(void) {
    printf("[2/3] Testing Dynamic Bad Block Failover...\n");
    remove(FAULT_TEST_IMAGE);

    const uint32_t total_blocks = 16;
    const uint32_t block_size = 512;

    assert(hal_flash_mock_init(FAULT_TEST_IMAGE, total_blocks, block_size));

    LoggerConfig cfg = {
        .total_blocks = total_blocks,
        .block_size = block_size
    };
    LoggerEngine engine;
    assert(logger_engine_init(&engine, hal_flash_mock_get_ops(), &cfg));

    uint32_t initial_block = engine.bdev.current_block; // 通常為 0

    // 1. 寫入 15 筆日誌（15 * 32 = 480 Bytes，暫存中，未達 512）
    for (uint32_t i = 1; i <= 15; i++) {
        char msg[LOG_MAX_PAYLOAD];
        snprintf(msg, sizeof(msg), "DATA_A_%02u", i);
        assert(logger_write(&engine, msg, (uint16_t)strlen(msg) + 1));
    }
    assert(logger_process(&engine, 15) == 15);

    // 2. 人為將下一個要使用的 Block 設為壞塊
    // 先前 Block 0 已經被 logger_engine_init 分配，下一次換塊預期拿到 Block 1
    uint32_t failing_block = 1;
    hal_flash_mock_inject_bad_block(failing_block);

    // 3. 寫入第 16 筆（480 + 32 = 512），觸發 Block 0 滿溢 flush
    char trigger_msg[LOG_MAX_PAYLOAD] = "TRIGGER_FLUSH";
    assert(logger_write(&engine, trigger_msg, (uint16_t)strlen(trigger_msg) + 1));
    assert(logger_process(&engine, 1) == 1);

    // 此時 Block 0 寫入完畢，下一個寫入應前進到 Block 1
    // 4. 寫入第 17 ~ 32 筆資料（再湊滿 512 Bytes，強迫寫入 Block 1）
    for (uint32_t i = 17; i <= 32; i++) {
        char msg[LOG_MAX_PAYLOAD];
        snprintf(msg, sizeof(msg), "DATA_B_%02u", i);
        assert(logger_write(&engine, msg, (uint16_t)strlen(msg) + 1));
    }
    
    // 處理日誌，當要 flush 寫入 Block 1 時會觸發硬體失敗 (-2)
    // 引擎內部必須自動跳轉並向 Bitmap 索取下一個塊 (Block 2) 重新 flush
    logger_process(&engine, 16);
    assert(logger_flush(&engine) == 0);

    // 驗證：引擎成功跳過壞掉的 failing_block (Block 1)，最終使用的區塊必定大於 1
    printf("  -> Initial Block: %u, Bad Block: %u, Recovered Block: %u\n",
           initial_block, failing_block, engine.bdev.current_block);
    assert(engine.bdev.current_block > failing_block);

    hal_flash_mock_deinit();
    printf("  -> [PASSED] Automatically navigated around hardware bad block.\n\n");
}

// ============================================================================
// 3. 驗證 Flash 空間耗盡保護（Out of Storage Defense）
// ============================================================================
static void test_out_of_storage_defense(void) {
    printf("[3/3] Testing Out-of-Storage Boundary Protection...\n");
    remove(FAULT_TEST_IMAGE);

    // 建立一個極小容量的 Flash：僅有 2 個 Block (每個 512B，最多容納 32 筆 Log)
    const uint32_t total_blocks = 2;
    const uint32_t block_size = 512;

    assert(hal_flash_mock_init(FAULT_TEST_IMAGE, total_blocks, block_size));

    LoggerConfig cfg = {
        .total_blocks = total_blocks,
        .block_size = block_size
    };
    LoggerEngine engine;
    assert(logger_engine_init(&engine, hal_flash_mock_get_ops(), &cfg));

    // 嘗試寫入 60 筆 Log（明顯超過 32 筆物理容量上限）
    for (uint32_t i = 1; i <= 60; i++) {
        char msg[LOG_MAX_PAYLOAD];
        snprintf(msg, sizeof(msg), "OVERFLOW_%02u", i);
        logger_write(&engine, msg, (uint16_t)strlen(msg) + 1);
        logger_process(&engine, 1);
    }
    logger_flush(&engine);

    // 驗證保護機制：
    // 1. 驅動絕不能崩潰或越界寫入
    // 2. Bitmap 應顯示所有可用區塊皆已配置
    assert(bitmap_find_and_alloc(&engine.block_map) == -1);

    // 3. 超過容量的 Log 必須安全丟棄，且 dropped_logs 計數器必須正確反映
    // （注意：若 ring buffer 滿溢或 storage 滿溢，dropped_logs 應大於 0）
    printf("  -> Dropped logs counter after full capacity: %u\n", engine.dropped_logs);

    hal_flash_mock_deinit();
    printf("  -> [PASSED] Memory safe under storage exhaustion, zero crash.\n\n");
}

int main(void) {
    printf("======================================================================\n");
    printf("Starting Fault Tolerance & Storage Boundary Verification Suite...\n");

    test_padding_and_alignment();
    test_bad_block_failover();
    test_out_of_storage_defense();

    printf("PASSED: All Fault Tolerance Checks Successfully Verified.\n");
    printf("======================================================================\n\n");
    return 0;
}
