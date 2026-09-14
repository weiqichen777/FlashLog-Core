#include "mem_pool.h"

#define POOL_ALIGNMENT (sizeof(void *))
#define ALIGN_UP(sz, align) (((sz) + ((align) - 1)) & ~((align) - 1))

bool mem_pool_init(MemPool *pool, void *buffer, size_t block_size, size_t block_count) {
    if (!pool || !buffer || block_count == 0) {
        return false;
    }

    // 區塊至少需要容納一個指標 (In-place Free List)
    if (block_size < sizeof(BlockNode)) {
        block_size = sizeof(BlockNode);
    }
    block_size = ALIGN_UP(block_size, POOL_ALIGNMENT);

    pool->raw_buffer = (uint8_t *)buffer;
    pool->block_size = block_size;
    pool->block_count = block_count;
    pool->free_count = block_count;
    pool->free_list = NULL;

    // 將所有區塊連結進 free_list
    for (size_t i = 0; i < block_count; i++) {
        BlockNode *node = (BlockNode *)(pool->raw_buffer + (i * block_size));
        node->next = pool->free_list;
        pool->free_list = node;
    }

    return true;
}

void *mem_pool_alloc(MemPool *pool) {
    if (!pool || !pool->free_list) {
        return NULL;
    }

    BlockNode *node = pool->free_list;
    pool->free_list = node->next;
    pool->free_count--;

    return (void *)node;
}

void mem_pool_free(MemPool *pool, void *ptr) {
    if (!pool || !ptr) {
        return;
    }

    // 指標合法性校驗：確認 ptr 落在 raw_buffer 範圍內且位移對齊
    uint8_t *p = (uint8_t *)ptr;
    ptrdiff_t offset = p - pool->raw_buffer;
    size_t total_size = pool->block_size * pool->block_count;

    if (offset < 0 || (size_t)offset >= total_size || (offset % pool->block_size) != 0) {
        return; // 非法指標直接拒絕
    }

    BlockNode *node = (BlockNode *)ptr;
    node->next = pool->free_list;
    pool->free_list = node;
    pool->free_count++;
}
