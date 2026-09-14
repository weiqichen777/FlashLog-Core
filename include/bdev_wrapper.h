#ifndef BDEV_WRAPPER_H
#define BDEV_WRAPPER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define BDEV_BLOCK_SIZE 512

typedef struct {
    int (*raw_write)(uint32_t block_id, const uint8_t *buffer);
    int (*raw_read)(uint32_t block_id, uint8_t *buffer);
} HalBlockOps;

typedef struct {
    const HalBlockOps *ops;
    uint8_t buffer[BDEV_BLOCK_SIZE];
    uint32_t current_block;
    size_t cursor;
    bool dirty;
} BdevWrapper;

bool bdev_wrapper_init(BdevWrapper *bdev, const HalBlockOps *ops, uint32_t start_block);

/**
 * @brief 將累積快取強制寫入實體 Block，剩餘部分填入 0xFF
 */
int bdev_wrapper_flush(BdevWrapper *bdev);

/**
 * @brief 寫入任意長度資料，自動聚合至 512B 區塊
 */
int bdev_wrapper_write(BdevWrapper *bdev, const void *data, size_t len);

#endif // BDEV_WRAPPER_H
