#ifndef HAL_FLASH_MOCK_H
#define HAL_FLASH_MOCK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "bdev_wrapper.h"

// 提供預設值作為方便的 Fallback
#define DEFAULT_FLASH_TOTAL_BLOCKS 65536
#define DEFAULT_FLASH_BLOCK_SIZE   512

/**
 * @brief 初始化模擬 Flash 裝置（支援自訂尺寸）
 * @param image_path 模擬 Flash 檔案路徑
 * @param total_blocks 總 Block 數量
 * @param block_size 每個 Block 的位元組大小（必須為 2 的冪次方，如 512, 2048, 4096）
 */
bool hal_flash_mock_init(const char *image_path, uint32_t total_blocks, uint32_t block_size);

void hal_flash_mock_deinit(void);
const HalBlockOps *hal_flash_mock_get_ops(void);
void hal_flash_mock_inject_bad_block(uint32_t block_id);
uint32_t hal_flash_mock_get_total_blocks(void);
uint32_t hal_flash_mock_get_block_size(void);

#endif // HAL_FLASH_MOCK_H
