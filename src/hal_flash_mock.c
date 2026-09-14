#include "hal_flash_mock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *flash_fp = NULL;
static bool bad_block_map[MOCK_FLASH_TOTAL_BLOCKS];

static int mock_block_write(uint32_t block_id, const uint8_t *buffer) {
    if (!flash_fp || block_id >= MOCK_FLASH_TOTAL_BLOCKS || !buffer) {
        return -1;
    }
    // 檢查是否注入了硬體故障
    if (bad_block_map[block_id]) {
        return -2; // 模擬硬體 I/O 錯誤
    }

    if (fseek(flash_fp, (long)(block_id * MOCK_FLASH_BLOCK_SIZE), SEEK_SET) != 0) {
        return -1;
    }

    size_t written = fwrite(buffer, 1, MOCK_FLASH_BLOCK_SIZE, flash_fp);
    if (written != MOCK_FLASH_BLOCK_SIZE) {
        return -1;
    }
    fflush(flash_fp);
    return 0;
}

static int mock_block_read(uint32_t block_id, uint8_t *buffer) {
    if (!flash_fp || block_id >= MOCK_FLASH_TOTAL_BLOCKS || !buffer) {
        return -1;
    }

    if (fseek(flash_fp, (long)(block_id * MOCK_FLASH_BLOCK_SIZE), SEEK_SET) != 0) {
        return -1;
    }

    size_t read_bytes = fread(buffer, 1, MOCK_FLASH_BLOCK_SIZE, flash_fp);
    if (read_bytes != MOCK_FLASH_BLOCK_SIZE) {
        return -1;
    }
    return 0;
}

static HalBlockOps mock_ops = {
    .raw_write = mock_block_write,
    .raw_read = mock_block_read
};

bool hal_flash_mock_init(const char *image_path) {
    memset(bad_block_map, 0, sizeof(bad_block_map));

    // 先嘗試以讀寫模式開啟
    flash_fp = fopen(image_path, "r+b");
    if (!flash_fp) {
        // 檔案不存在，建立全新映像檔並全區填 0xFF (模擬 Flash 擦除狀態)
        flash_fp = fopen(image_path, "w+b");
        if (!flash_fp) return false;

        fseek(flash_fp, (long)(MOCK_FLASH_TOTAL_BLOCKS * MOCK_FLASH_BLOCK_SIZE - 1), SEEK_SET);
        fputc(0xFF, flash_fp);
        fflush(flash_fp);
    }
    return true;
}

void hal_flash_mock_deinit(void) {
    if (flash_fp) {
        fclose(flash_fp);
        flash_fp = NULL;
    }
}

const HalBlockOps *hal_flash_mock_get_ops(void) {
    return &mock_ops;
}

void hal_flash_mock_inject_bad_block(uint32_t block_id) {
    if (block_id < MOCK_FLASH_TOTAL_BLOCKS) {
        bad_block_map[block_id] = true;
    }
}
