#include "logger_engine.h"
#include <string.h>
#include <stdlib.h>

#define RING_BUF_CAPACITY 256
#define MAX_SUPPORTED_BLOCKS 65536
#define MAX_SUPPORTED_BLOCK_SIZE 4096

static LogEntry ring_storage[RING_BUF_CAPACITY];
static uint32_t bitmap_storage[MAX_SUPPORTED_BLOCKS / 32];
static uint8_t bdev_cache[MAX_SUPPORTED_BLOCK_SIZE]; // 最大可支援到 4KB Sector

bool logger_engine_init(LoggerEngine *engine, const HalBlockOps *hal_ops, const LoggerConfig *config) {
    if (!engine || !hal_ops) return false;

    uint32_t total_blocks = (config && config->total_blocks > 0) ? config->total_blocks : MAX_SUPPORTED_BLOCKS;
    uint32_t block_size = (config && config->block_size > 0) ? config->block_size : 512;

    if (total_blocks > MAX_SUPPORTED_BLOCKS || block_size > MAX_SUPPORTED_BLOCK_SIZE) {
        return false;
    }

    memset(engine, 0, sizeof(LoggerEngine));

    if (!ring_buf_init(&engine->ring_buf, ring_storage, sizeof(LogEntry), RING_BUF_CAPACITY)) {
        return false;
    }
    if (!bitmap_init(&engine->block_map, bitmap_storage, total_blocks)) {
        return false;
    }

    int32_t first_block = bitmap_find_and_alloc(&engine->block_map);
    if (first_block < 0) return false;

    if (!bdev_wrapper_init(&engine->bdev, hal_ops, bdev_cache, block_size, (uint32_t)first_block)) {
        return false;
    }

    engine->next_seq_id = 1;
    engine->dropped_logs = 0;
    return true;
}

bool logger_write(LoggerEngine *engine, const void *data, uint16_t len) {
    if (!engine || !data || len > LOG_MAX_PAYLOAD) {
        return false;
    }

    uint32_t head = engine->ring_buf.head;
    uint32_t tail = __atomic_load_n(&engine->ring_buf.tail, __ATOMIC_ACQUIRE);
    if ((head - tail) >= engine->ring_buf.capacity) {
        engine->dropped_logs++;
        return false;
    }

    LogEntry entry;
    entry.magic = LOG_MAGIC;
    entry.length = len;
    entry.seq_id = engine->next_seq_id++;
    memcpy(entry.payload, data, len);

    if (!ring_buf_push(&engine->ring_buf, &entry)) {
        engine->next_seq_id--;
        engine->dropped_logs++;
        return false;
    }

    return true;
}

size_t logger_process(LoggerEngine *engine, size_t max_entries) {
    if (!engine) return 0;

    size_t processed = 0;
    LogEntry entry;

    while (processed < max_entries && ring_buf_pop(&engine->ring_buf, &entry)) {
        int rc = bdev_wrapper_write(&engine->bdev, &entry, sizeof(LogEntry));
        
        // 若寫入失敗（例如碰到壞塊觸發 flush 失敗）
        if (rc != 0) {
            bool recovered = false;
            // 嘗試容錯重試：尋找下一個乾淨的區塊
            for (int retry = 0; retry < 3; retry++) {
                int32_t new_block = bitmap_find_and_alloc(&engine->block_map);
                if (new_block < 0) {
                    break; // 空間真正耗盡
                }

                // 重新綁定至新區塊
                engine->bdev.current_block = (uint32_t)new_block;
                // 重試 flush 剛剛尚未成功寫入的緩衝區
                if (bdev_wrapper_flush(&engine->bdev) == 0) {
                    recovered = true;
                    break;
                }
            }

            if (!recovered) {
                // 無法恢復，計入丟包
                engine->dropped_logs++;
                break;
            }
        }
        processed++;
    }

    return processed;
}

int logger_flush(LoggerEngine *engine) {
    if (!engine) return -1;
    
    int rc = bdev_wrapper_flush(&engine->bdev);
    if (rc != 0) {
        // Flush 失敗時（當前區塊是壞塊），自動換塊重試
        for (int retry = 0; retry < 3; retry++) {
            int32_t new_block = bitmap_find_and_alloc(&engine->block_map);
            if (new_block < 0) {
                return -1;
            }
            engine->bdev.current_block = (uint32_t)new_block;
            if (bdev_wrapper_flush(&engine->bdev) == 0) {
                return 0;
            }
        }
        return rc;
    }
    return 0;
}
