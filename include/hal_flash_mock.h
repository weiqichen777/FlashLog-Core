#ifndef HAL_FLASH_MOCK_H
#define HAL_FLASH_MOCK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "bdev_wrapper.h"

#define MOCK_FLASH_TOTAL_BLOCKS 65536
#define MOCK_FLASH_BLOCK_SIZE   512

/**
 * @brief 初始化模擬 Flash 裝置（若檔案不存在則自動建立並填滿 0xFF）
 * @param image_path 模擬 Flash 存放路徑（如 "virtual_flash.bin"）
 */
bool hal_flash_mock_init(const char *image_path);

/**
 * @brief 關閉模擬 Flash 裝置並同步檔案
 */
void hal_flash_mock_deinit(void);

/**
 * @brief 取得符合 BdevWrapper 要求的 HAL 操作結構體
 */
const HalBlockOps *hal_flash_mock_get_ops(void);

/**
 * @brief 故障注入：強制標記某個 Block 為壞塊（寫入時將回傳 -1）
 */
void hal_flash_mock_inject_bad_block(uint32_t block_id);

#endif // HAL_FLASH_MOCK_H
