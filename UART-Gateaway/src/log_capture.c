#include "log_capture.h"
#include <zephyr/sys/ring_buffer.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define LOG_CAPTURE_BUF_SIZE 256
static uint8_t log_capture_buffer[LOG_CAPTURE_BUF_SIZE];
static struct ring_buf log_ringbuf;
static bool log_capture_enabled = false;


void log_capture_init(void) {
    ring_buf_init(&log_ringbuf, LOG_CAPTURE_BUF_SIZE, log_capture_buffer);
    log_capture_enabled = false;
}

void log_capture_enable(bool enable) {
    log_capture_enabled = enable;
}

// Log backend API callback for Zephyr logging system
void log_backend_capture_init(const struct log_backend *const backend) {
    ARG_UNUSED(backend);
    log_capture_init();
}

size_t strip_ansi_escapes(const char *src, size_t src_len, char *dst, size_t dst_size) {
    size_t dst_pos = 0;
    size_t i = 0;
    while (i < src_len && dst_pos < dst_size - 1) {
        if (src[i] == 0x1b) {
            i++;
            if (i < src_len && src[i] == '[') {
                i++;
                while (i < src_len && ((src[i] < '@' || src[i] > '~'))) {
                    i++;
                }
                if (i < src_len) i++;
            }
        } else if (src[i] == '~' && i + 2 < src_len && src[i+1] == '$' && src[i+2] == ' ') {
            i += 3;
        } else if (src[i] == '\r') {
            i++;
        } else {
            dst[dst_pos++] = src[i++];
        }
    }
    dst[dst_pos] = '\0';
    return dst_pos;
}
