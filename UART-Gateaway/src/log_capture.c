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

bool log_capture_has_logs(void) {
    return ring_buf_size_get(&log_ringbuf) > 0;
}

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

