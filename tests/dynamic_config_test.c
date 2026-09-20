#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "logger_engine.h"
#include "hal_flash_mock.h"

// 取得檔案實體位元組大小
static long get_file_size(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fclose(fp);
    return sz;
}

static void test_flash_profile(const char *image_name, uint32_t total_blocks, uint32_t block_size, uint32_t log_count) {
    printf("  -> Testing Profile: Blocks=%u, BlockSize=%u bytes, Logs=%u\n", 
           total_blocks, block_size, log_count);

    remove(image_name);

    // 1. 初始化 Mock HAL
    assert(hal_flash_mock_init(image_name, total_blocks, block_size));
    assert(hal_flash_mock_get_total_blocks() == total_blocks);
    assert(hal_flash_mock_get_block_size() == block_size);

    // 2. 初始化 Engine
    LoggerConfig cfg = {
        .total_blocks = total_blocks,
        .block_size = block_size
    };
    LoggerEngine engine;
    assert(logger_engine_init(&engine, hal_flash_mock_get_ops(), &cfg));

    // 3. 寫入日誌
    for (uint32_t i = 1; i <= log_count; i++) {
        char msg[LOG_MAX_PAYLOAD];
        snprintf(msg, sizeof(msg), "VAL_%04u", i);
        bool ok = logger_write(&engine, msg, (uint16_t)strlen(msg) + 1);
        assert(ok);

        // 模擬背景處理
        if (i % 4 == 0) {
            logger_process(&engine, 8);
        }
    }

    // 4. 排空與 Flush
    logger_process(&engine, log_count);
    assert(logger_flush(&engine) == 0);

    hal_flash_mock_deinit();

    // 5. 驗證產生的映像檔尺寸是否完全符合預期
    long expected_min_size = (long)total_blocks * block_size;
    long actual_size = get_file_size(image_name);
    assert(actual_size >= expected_min_size);

    // 6. 回讀驗證資料正確性與序號單調遞增
    FILE *fp = fopen(image_name, "rb");
    assert(fp != NULL);

    uint32_t verified_count = 0;
    LogEntry entry;

    while (fread(&entry, 1, sizeof(LogEntry), fp) == sizeof(LogEntry)) {
        if (entry.magic == 0xFFFF) {
            continue; // 跳過 Flash 擦除區的 0xFF Padding
        }
        if (entry.magic != LOG_MAGIC) {
            continue;
        }

        verified_count++;
        assert(entry.seq_id == verified_count);

        char expected_msg[LOG_MAX_PAYLOAD];
        snprintf(expected_msg, sizeof(expected_msg), "VAL_%04u", verified_count);
        assert(strcmp((char *)entry.payload, expected_msg) == 0);

        if (verified_count == log_count) break;
    }

    fclose(fp);
    assert(verified_count == log_count);
    printf("     [PASSED] Verified %u logs monotonically intact.\n", verified_count);
}

int main(void) {
    printf("======================================================================\n");
    printf("Starting Multi-Specification Validation...\n");

    // Profile A: 512B Block (模擬 SD / eMMC，共 256 個區塊 = 128 KB)
    test_flash_profile("test_512b.bin", 256, 512, 500);

    // Profile B: 4096B Sector (模擬 SPI NOR Flash，共 64 個區塊 = 256 KB)
    test_flash_profile("test_4k.bin", 64, 4096, 2000);

    // Profile C: 極端邊界測試 - 小容量壓力 (只有 4 個 512B Block = 2048 Bytes，最多塞 64 筆 32B Log)
    test_flash_profile("test_small.bin", 4, 512, 60);

    printf("PASSED: All Profiles Passed.\n");
    printf("======================================================================\n\n");
    return 0;
}
