#include "bitmap.h"
#include <string.h>

bool bitmap_init(Bitmap *bm, uint32_t *storage, size_t bit_count) {
    if (!bm || !storage || bit_count == 0) {
        return false;
    }

    bm->bit_count = bit_count;
    bm->word_count = (bit_count + (BITMAP_WORD_BITS - 1)) / BITMAP_WORD_BITS;
    bm->map = storage;

    memset(bm->map, 0, bm->word_count * sizeof(uint32_t));
    return true;
}

void bitmap_set(Bitmap *bm, size_t bit) {
    if (bit < bm->bit_count) {
        bm->map[bit / BITMAP_WORD_BITS] |= (1U << (bit % BITMAP_WORD_BITS));
    }
}

void bitmap_clear(Bitmap *bm, size_t bit) {
    if (bit < bm->bit_count) {
        bm->map[bit / BITMAP_WORD_BITS] &= ~(1U << (bit % BITMAP_WORD_BITS));
    }
}

bool bitmap_test(const Bitmap *bm, size_t bit) {
    if (bit >= bm->bit_count) {
        return false;
    }
    return (bm->map[bit / BITMAP_WORD_BITS] & (1U << (bit % BITMAP_WORD_BITS))) != 0;
}

int32_t bitmap_find_and_alloc(Bitmap *bm) {
    for (size_t i = 0; i < bm->word_count; i++) {
        if (bm->map[i] != 0xFFFFFFFFU) {
            uint32_t inverted = ~bm->map[i];
            int bit_pos = __builtin_ctz(inverted); // 計算末尾連續 0 的個數 (單指令)
            size_t actual_bit = (i * BITMAP_WORD_BITS) + (size_t)bit_pos;

            if (actual_bit < bm->bit_count) {
                bm->map[i] |= (1U << bit_pos);
                return (int32_t)actual_bit;
            }
        }
    }
    return -1;
}
