#include "hal_flash_mock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *flash_fp = NULL;
static uint32_t g_total_blocks = 0;
static uint32_t g_block_size = 0;
static bool *g_bad_block_map = NULL;

static int mock_block_write(uint32_t block_id, const uint8_t *buffer) {
    if (!flash_fp || block_id >= g_total_blocks || !buffer) {
        return -1;
    }
    if (g_bad_block_map && g_bad_block_map[block_id]) {
        return -2; // 模擬硬體壞塊
    }

    if (fseek(flash_fp, (long)(block_id * g_block_size), SEEK_SET) != 0) {
        return -1;
    }

    size_t written = fwrite(buffer, 1, g_block_size, flash_fp);
    if (written != g_block_size) {
        return -1;
    }
    fflush(flash_fp);
    return 0;
}

static int mock_block_read(uint32_t block_id, uint8_t *buffer) {
    if (!flash_fp || block_id >= g_total_blocks || !buffer) {
        return -1;
    }

    if (fseek(flash_fp, (long)(block_id * g_block_size), SEEK_SET) != 0) {
        return -1;
    }

    size_t read_bytes = fread(buffer, 1, g_block_size, flash_fp);
    if (read_bytes != g_block_size) {
        return -1;
    }
    return 0;
}

static HalBlockOps mock_ops = {
    .raw_write = mock_block_write,
    .raw_read = mock_block_read
};

bool hal_flash_mock_init(const char *image_path, uint32_t total_blocks, uint32_t block_size) {
    if (!image_path || total_blocks == 0 || block_size == 0) {
        return false;
    }

    g_total_blocks = total_blocks;
    g_block_size = block_size;

    g_bad_block_map = (bool *)calloc(total_blocks, sizeof(bool));
    if (!g_bad_block_map) return false;

    flash_fp = fopen(image_path, "r+b");
    if (!flash_fp) {
        flash_fp = fopen(image_path, "w+b");
        if (!flash_fp) {
            free(g_bad_block_map);
            g_bad_block_map = NULL;
            return false;
        }

        // 以 Sparse 方式擴展檔案尺寸，並在尾端寫入 0xFF
        uint64_t total_bytes = (uint64_t)total_blocks * block_size;
        if (fseek(flash_fp, (long)(total_bytes - 1), SEEK_SET) == 0) {
            fputc(0xFF, flash_fp);
            fflush(flash_fp);
        }
    }
    return true;
}

void hal_flash_mock_deinit(void) {
    if (flash_fp) {
        fclose(flash_fp);
        flash_fp = NULL;
    }
    if (g_bad_block_map) {
        free(g_bad_block_map);
        g_bad_block_map = NULL;
    }
    g_total_blocks = 0;
    g_block_size = 0;
}

const HalBlockOps *hal_flash_mock_get_ops(void) {
    return &mock_ops;
}

void hal_flash_mock_inject_bad_block(uint32_t block_id) {
    if (g_bad_block_map && block_id < g_total_blocks) {
        g_bad_block_map[block_id] = true;
    }
}

uint32_t hal_flash_mock_get_total_blocks(void) {
    return g_total_blocks;
}

uint32_t hal_flash_mock_get_block_size(void) {
    return g_block_size;
}
