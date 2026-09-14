#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint8_t *storage;
    size_t item_size;      // 每個元素的大小 (bytes)
    uint32_t capacity;     // 元素總數 (必須為 2 的冪次方)
    uint32_t mask;
    volatile uint32_t head;
    volatile uint32_t tail;
} RingBuffer;

bool ring_buf_init(RingBuffer *rb, void *storage, size_t item_size, uint32_t capacity);
bool ring_buf_push(RingBuffer *rb, const void *item);
bool ring_buf_pop(RingBuffer *rb, void *out_item);

static inline uint32_t ring_buf_count(const RingBuffer *rb) {
    return rb ? (rb->head - rb->tail) : 0;
}

#endif // RING_BUFFER_H
