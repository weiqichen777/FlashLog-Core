#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BITMAP_WORD_BITS 32U

typedef struct {
    uint32_t *map;
    size_t bit_count;
    size_t word_count;
} Bitmap;

/**
 * @brief 初始化 Bitmap
 * @param bm 物件指標
 * @param storage uint32_t 陣列，長度至少需為 ALIGN_UP(bit_count, 32) / 32
 * @param bit_count 管理的資源總數
 */
bool bitmap_init(Bitmap *bm, uint32_t *storage, size_t bit_count);

void bitmap_set(Bitmap *bm, size_t bit);
void bitmap_clear(Bitmap *bm, size_t bit);
bool bitmap_test(const Bitmap *bm, size_t bit);

/**
 * @brief 尋找第一個閒置資源 (0) 並標記為佔用 (1)
 * @return 成功回傳資源索引 (>= 0)，若已無閒置空間回傳 -1
 */
int32_t bitmap_find_and_alloc(Bitmap *bm);

#endif // BITMAP_H
