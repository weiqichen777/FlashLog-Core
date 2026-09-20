#ifndef BDEV_WRAPPER_H
#define BDEV_WRAPPER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define BDEV_BLOCK_SIZE 512  // 保留相容性預設值

// HAL 底層區塊操作介面定義
typedef struct {
    int (*raw_write)(uint32_t block_id, const uint8_t *buffer);
    int (*raw_read)(uint32_t block_id, uint8_t *buffer);
} HalBlockOps;

typedef struct {
    const HalBlockOps *ops;
    uint8_t *buffer;         // 指向實體緩衝區
    uint32_t block_size;     // 區塊大小
    uint32_t current_block;
    size_t cursor;
    bool dirty;
} BdevWrapper;

/**
 * @brief 初始化區塊聚合驅動
 */
bool bdev_wrapper_init(BdevWrapper *bdev, const HalBlockOps *ops, uint8_t *buffer, uint32_t block_size, uint32_t start_block);

/**
 * @brief 強制將暫存區資料寫入硬體 Block，未滿補 0xFF
 */
int bdev_wrapper_flush(BdevWrapper *bdev);

/**
 * @brief 寫入任意長度資料，自動聚合至 block_size
 */
int bdev_wrapper_write(BdevWrapper *bdev, const void *data, size_t len);

#endif // BDEV_WRAPPER_H
