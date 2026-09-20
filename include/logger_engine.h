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

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint16_t length;
    uint32_t seq_id;
    uint8_t  payload[LOG_MAX_PAYLOAD];
} LogEntry;

typedef struct {
    uint32_t total_blocks;
    uint32_t block_size;
} LoggerConfig;

typedef struct {
    RingBuffer ring_buf;
    Bitmap block_map;
    BdevWrapper bdev;
    uint32_t next_seq_id;
    uint32_t dropped_logs;
} LoggerEngine;

/**
 * @brief 初始化 Logger 引擎（支援傳入 config，若 config 為 NULL 則使用預設 65536 blocks / 512B）
 */
bool logger_engine_init(LoggerEngine *engine, const HalBlockOps *hal_ops, const LoggerConfig *config);

bool logger_write(LoggerEngine *engine, const void *data, uint16_t len);
size_t logger_process(LoggerEngine *engine, size_t max_entries);
int logger_flush(LoggerEngine *engine);

#endif // LOGGER_ENGINE_H
