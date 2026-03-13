#ifndef LOG_CAPTURE_H
#define LOG_CAPTURE_H

#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log_backend.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void log_capture_init(void);
void log_capture_enable(bool enable);
size_t strip_ansi_escapes(const char *src, size_t src_len, char *dst, size_t dst_size);
void log_backend_capture_init(const struct log_backend *const backend);

#ifdef __cplusplus
}
#endif

#endif // LOG_CAPTURE_H
