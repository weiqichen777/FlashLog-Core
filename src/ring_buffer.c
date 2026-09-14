#include "ring_buffer.h"
#include <string.h>

static inline bool is_power_of_two(uint32_t x) {
    return (x != 0) && ((x & (x - 1)) == 0);
}

bool ring_buf_init(RingBuffer *rb, void *storage, size_t item_size, uint32_t capacity) {
    if (!rb || !storage || item_size == 0 || !is_power_of_two(capacity)) {
        return false;
    }

    rb->storage = (uint8_t *)storage;
    rb->item_size = item_size;
    rb->capacity = capacity;
    rb->mask = capacity - 1;
    rb->head = 0;
    rb->tail = 0;

    return true;
}

bool ring_buf_push(RingBuffer *rb, const void *item) {
    uint32_t head = rb->head;
    uint32_t tail = __atomic_load_n(&rb->tail, __ATOMIC_ACQUIRE);

    if ((head - tail) >= rb->capacity) {
        return false; // 滿了
    }

    // 將資料複製進專屬 slot
    uint8_t *dest = rb->storage + ((head & rb->mask) * rb->item_size);
    memcpy(dest, item, rb->item_size);

    // Release 語意：確保資料完全複製完，才更新 head
    __atomic_store_n(&rb->head, head + 1, __ATOMIC_RELEASE);
    return true;
}

bool ring_buf_pop(RingBuffer *rb, void *out_item) {
    uint32_t tail = rb->tail;
    uint32_t head = __atomic_load_n(&rb->head, __ATOMIC_ACQUIRE);

    if (head == tail) {
        return false; // 空的
    }

    // 從 slot 拷貝出資料
    const uint8_t *src = rb->storage + ((tail & rb->mask) * rb->item_size);
    memcpy(out_item, src, rb->item_size);

    // Release 語意：確保資料完全讀出，才更新 tail
    __atomic_store_n(&rb->tail, tail + 1, __ATOMIC_RELEASE);
    return true;
}
