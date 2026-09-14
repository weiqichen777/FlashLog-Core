#ifndef MEM_POOL_H
#define MEM_POOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct BlockNode {
    struct BlockNode *next;
} BlockNode;

typedef struct {
    uint8_t *raw_buffer;
    BlockNode *free_list;
    size_t block_size;
    size_t block_count;
    size_t free_count;
} MemPool;

/**
 * @brief 初始化固定區塊記憶體池
 * @param pool 物件指標
 * @param buffer 外部注入的記憶體區域（由呼叫者提供，保證零動態配置）
 * @param block_size 每個區塊的大小 (Bytes，至少 >= sizeof(void*))
 * @param block_count 區塊總數
 */
bool mem_pool_init(MemPool *pool, void *buffer, size_t block_size, size_t block_count);

/**
 * @brief 分配一個區塊 (O(1))
 */
void *mem_pool_alloc(MemPool *pool);

/**
 * @brief 釋放區塊回記憶體池 (O(1))
 */
void mem_pool_free(MemPool *pool, void *ptr);

/**
 * @brief 取得目前剩餘閒置區塊數
 */
static inline size_t mem_pool_available(const MemPool *pool) {
    return pool ? pool->free_count : 0;
}

#endif // MEM_POOL_H
