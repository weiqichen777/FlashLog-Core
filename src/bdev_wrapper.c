#include "bdev_wrapper.h"
#include <string.h>

bool bdev_wrapper_init(BdevWrapper *bdev, const HalBlockOps *ops, uint32_t start_block) {
    if (!bdev || !ops || !ops->raw_write) {
        return false;
    }

    bdev->ops = ops;
    bdev->current_block = start_block;
    bdev->cursor = 0;
    bdev->dirty = false;
    return true;
}

int bdev_wrapper_flush(BdevWrapper *bdev) {
    if (!bdev || !bdev->dirty || bdev->cursor == 0) {
        return 0;
    }

    // Flash 未滿區塊以 0xFF 補齊
    if (bdev->cursor < BDEV_BLOCK_SIZE) {
        memset(bdev->buffer + bdev->cursor, 0xFF, BDEV_BLOCK_SIZE - bdev->cursor);
    }

    int rc = bdev->ops->raw_write(bdev->current_block, bdev->buffer);
    if (rc == 0) {
        bdev->current_block++;
        bdev->cursor = 0;
        bdev->dirty = false;
    }
    return rc;
}

int bdev_wrapper_write(BdevWrapper *bdev, const void *data, size_t len) {
    if (!bdev || !data) return -1;

    const uint8_t *src = (const uint8_t *)data;
    size_t remaining = len;

    while (remaining > 0) {
        size_t space = BDEV_BLOCK_SIZE - bdev->cursor;
        size_t chunk = (remaining < space) ? remaining : space;

        memcpy(bdev->buffer + bdev->cursor, src, chunk);
        bdev->cursor += chunk;
        src += chunk;
        remaining -= chunk;
        bdev->dirty = true;

        if (bdev->cursor == BDEV_BLOCK_SIZE) {
            int rc = bdev_wrapper_flush(bdev);
            if (rc != 0) return rc;
        }
    }
    return 0;
}
