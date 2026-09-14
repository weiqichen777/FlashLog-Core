#ifndef LOGGER_ENGINE_H
#define LOGGER_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "mem_pool.h"
#include "ring_buffer.h"
#include "bitmap.h"
#include "bdev_wrapper.h"

#define LOG_MAX_PAYLOAD 24
#define LOG_MAGIC       0xAA55

// 封包對齊格式 (共 32 Bytes，天然對齊)
typedef struct __attribute__((packed)) {
    uint16_t magic;                     // 0xAA55 同步字
    uint16_t length;                    // payload 實際長度
    uint32_t seq_id;                    // 單調遞增序號
    uint8_t  payload[LOG_MAX_PAYLOAD];  // 資料內容
} LogEntry;

typedef struct {
    MemPool pool;
    RingBuffer ring_buf;
    Bitmap block_map;
    BdevWrapper bdev;
    uint32_t next_seq_id;
    uint32_t dropped_logs;
} LoggerEngine;

/**
 * @brief 初始化 Logger 引擎
 */
bool logger_engine_init(LoggerEngine *engine, const HalBlockOps *hal_ops);

/**
 * @brief 產生端（ISR / 高頻任務）寫入日誌：保證 O(1) 與無鎖
 */
bool logger_write(LoggerEngine *engine, const void *data, uint16_t len);

/**
 * @brief 消費端（背景 Task）推進管線：將 RingBuffer 資料轉移至 Flash
 * @param max_entries 單次最大處理筆數，避免長時間霸佔 CPU
 * @return 實際處理成功的日誌筆數
 */
size_t logger_process(LoggerEngine *engine, size_t max_entries);

/**
 * @brief 強制將暫存區剩餘資料寫入 Flash
 */
int logger_flush(LoggerEngine *engine);

#endif // LOGGER_ENGINE_H
