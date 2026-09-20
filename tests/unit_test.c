#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "mem_pool.h"
#include "ring_buffer.h"
#include "bitmap.h"
#include "bdev_wrapper.h"

// 模擬 Mock Block Driver
static uint8_t mock_flash[4][BDEV_BLOCK_SIZE];
static uint32_t mock_write_count = 0;

static int mock_write(uint32_t block_id, const uint8_t *buf) {
    if (block_id >= 4) return -1;
    memcpy(mock_flash[block_id], buf, BDEV_BLOCK_SIZE);
    mock_write_count++;
    return 0;
}

static HalBlockOps mock_ops = { .raw_write = mock_write, .raw_read = NULL };

int main(void) {
    printf("======================================================================\n");
    printf("Starting Core Unit Tests...\n");

    // 1. MemPool 測試
    uint8_t pool_mem[4 * 32];
    MemPool pool;
    assert(mem_pool_init(&pool, pool_mem, 32, 4));
    void *b1 = mem_pool_alloc(&pool);
    void *b2 = mem_pool_alloc(&pool);
    assert(b1 && b2 && b1 != b2);
    assert(mem_pool_available(&pool) == 2);
    mem_pool_free(&pool, b1);
    assert(mem_pool_available(&pool) == 3);

    // 2. RingBuffer 自然溢位邊界測試
    uint32_t rb_storage[4];
    RingBuffer rb;
    assert(ring_buf_init(&rb, rb_storage, sizeof(uint32_t), 4));
    rb.head = 0xFFFFFFFEU;
    rb.tail = 0xFFFFFFFEU; // 人為設置在 32-bit 邊界
    uint32_t val1 = 0x11, val2 = 0x22;
    assert(ring_buf_push(&rb, &val1));
    assert(ring_buf_push(&rb, &val2));
    uint32_t out = 0;
    assert(ring_buf_pop(&rb, &out) && out == 0x11);
    assert(ring_buf_pop(&rb, &out) && out == 0x22);

    // 3. Bitmap 測試
    uint32_t bm_mem[2];
    Bitmap bm;
    assert(bitmap_init(&bm, bm_mem, 64));
    int32_t id0 = bitmap_find_and_alloc(&bm);
    int32_t id1 = bitmap_find_and_alloc(&bm);
    assert(id0 == 0 && id1 == 1);
    assert(bitmap_test(&bm, 0) == true);
    bitmap_clear(&bm, 0);
    assert(bitmap_test(&bm, 0) == false);
    assert(bitmap_find_and_alloc(&bm) == 0); // 重新分配 0

    // 4. BdevWrapper 聚合寫入測試
    uint8_t bdev_buf[BDEV_BLOCK_SIZE];
    BdevWrapper bdev;
    assert(bdev_wrapper_init(&bdev, &mock_ops, bdev_buf, BDEV_BLOCK_SIZE, 0));

    uint8_t payload[300];
    memset(payload, 0xAB, sizeof(payload));

    // 寫入 300 Bytes，不應觸發 Flash Write
    assert(bdev_wrapper_write(&bdev, payload, 300) == 0);
    assert(mock_write_count == 0);

    // 再寫入 300 Bytes (累計 600 Bytes)，應觸發一次 512B Block Write
    assert(bdev_wrapper_write(&bdev, payload, 300) == 0);
    assert(mock_write_count == 1);
    assert(bdev.cursor == (600 - 512));

    // 手動 Flush 剩餘的 88 Bytes
    assert(bdev_wrapper_flush(&bdev) == 0);
    assert(mock_write_count == 2);
    assert(mock_flash[1][87] == 0xAB);
    assert(mock_flash[1][88] == 0xFF); // 驗證 Padding 填滿 0xFF

    printf("PASSED: All Core Component Tests Passed.\n");
    printf("======================================================================\n\n");
    return 0;
}
