#include "bdev_wrapper.h"
#include <string.h>

bool bdev_wrapper_init(BdevWrapper *bdev, const HalBlockOps *ops, uint8_t *buffer, uint32_t block_size, uint32_t start_block) {
    if (!bdev || !ops || !ops->raw_write || !buffer || block_size == 0) {
        return false;
    }

    bdev->ops = ops;
    bdev->buffer = buffer;
    bdev->block_size = block_size;
    bdev->current_block = start_block;
    bdev->cursor = 0;
    bdev->dirty = false;
    return true;
}

int bdev_wrapper_flush(BdevWrapper *bdev) {
    if (!bdev || !bdev->dirty || bdev->cursor == 0) {
        return 0;
    }

    if (bdev->cursor < bdev->block_size) {
        memset(bdev->buffer + bdev->cursor, 0xFF, bdev->block_size - bdev->cursor);
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
        size_t space = bdev->block_size - bdev->cursor;
        size_t chunk = (remaining < space) ? remaining : space;

        memcpy(bdev->buffer + bdev->cursor, src, chunk);
        bdev->cursor += chunk;
        src += chunk;
        remaining -= chunk;
        bdev->dirty = true;

        if (bdev->cursor == bdev->block_size) {
            int rc = bdev_wrapper_flush(bdev);
            if (rc != 0) return rc;
        }
    }
    return 0;
}
