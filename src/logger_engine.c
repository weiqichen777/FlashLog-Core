#include "logger_engine.h"
#include <string.h>

#define RING_BUF_CAPACITY 256
#define TOTAL_FLASH_BLOCKS 65536

// 直接配置儲存實體 LogEntry 的陣列
static LogEntry ring_storage[RING_BUF_CAPACITY];
static uint32_t bitmap_storage[TOTAL_FLASH_BLOCKS / 32];

bool logger_engine_init(LoggerEngine *engine, const HalBlockOps *hal_ops) {
    if (!engine || !hal_ops) return false;

    memset(engine, 0, sizeof(LoggerEngine));

    // 初始化直接容納 LogEntry 的 RingBuffer
    if (!ring_buf_init(&engine->ring_buf, ring_storage, sizeof(LogEntry), RING_BUF_CAPACITY)) {
        return false;
    }
    if (!bitmap_init(&engine->block_map, bitmap_storage, TOTAL_FLASH_BLOCKS)) {
        return false;
    }

    int32_t first_block = bitmap_find_and_alloc(&engine->block_map);
    if (first_block < 0) return false;

    if (!bdev_wrapper_init(&engine->bdev, hal_ops, (uint32_t)first_block)) {
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

    // 檢查是否有空間
    uint32_t head = engine->ring_buf.head;
    uint32_t tail = __atomic_load_n(&engine->ring_buf.tail, __ATOMIC_ACQUIRE);
    if ((head - tail) >= engine->ring_buf.capacity) {
        engine->dropped_logs++;
        return false;
    }

    // 在 Stack 上填寫，不需鎖也不需全域 Pool
    LogEntry entry;
    entry.magic = LOG_MAGIC;
    entry.length = len;
    entry.seq_id = engine->next_seq_id++;
    memcpy(entry.payload, data, len);

    if (!ring_buf_push(&engine->ring_buf, &entry)) {
        engine->next_seq_id--; // 還原
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
        if (rc != 0) {
            int32_t new_block = bitmap_find_and_alloc(&engine->block_map);
            if (new_block >= 0) {
                engine->bdev.current_block = (uint32_t)new_block;
                engine->bdev.cursor = 0;
                engine->bdev.dirty = false;
            }
            break;
        }
        processed++;
    }

    return processed;
}

int logger_flush(LoggerEngine *engine) {
    if (!engine) return -1;
    return bdev_wrapper_flush(&engine->bdev);
}
