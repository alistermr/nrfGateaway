#include "uart_cmd.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/console/console.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <string.h>
#include "log_capture.h"


#define CMD_BUFFER_SIZE 512
static char cmd_buffer[CMD_BUFFER_SIZE];
static int cmd_buffer_pos = 0;
static K_SEM_DEFINE(cmd_sem, 0, 1);
static const struct device *uart_dev;

static void uart30_send(const char *data, size_t len)
{
    if (!uart_dev || !device_is_ready(uart_dev)) {
        return;
    }
    /* Use a static buffer for stripped output */
	static char clean_buf[512];
	size_t clean_len = strip_ansi_escapes(data, len, clean_buf, sizeof(clean_buf));

    for (size_t i = 0; i < clean_len; i++) {
        uart_poll_out(uart_dev, clean_buf[i]);
    }
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

static void uart_isr(const struct device *dev, void *user_data)
{
    uint8_t c;
    if (!uart_irq_update(dev)) {
        return;
    }
    if (!uart_irq_rx_ready(dev)) {
        return;
    }
    while (uart_fifo_read(dev, &c, 1) == 1) {
        if (c == '\r' || c == '\n') {
            if (cmd_buffer_pos > 0) {
                cmd_buffer[cmd_buffer_pos] = '\0';
                cmd_buffer_pos = 0;
                k_sem_give(&cmd_sem);
            }
        } else if (cmd_buffer_pos < CMD_BUFFER_SIZE - 1) {
            cmd_buffer[cmd_buffer_pos++] = c;
        } else {
            printk("Command buffer overflow, resetting\n");
            cmd_buffer_pos = 0;
        }
    }
}

// size_t mesh_filter(const char *src, size_t src_len, size_t dst_size){
//     //check if the command starts with "mesh " and drop if not
//     if (src_len < 5 || strncmp(src, "mesh ", 5) != 0) {
//         return 0;
//     }
//     return src_len;
// }

static void cmd_executor_thread(void)
{
    const struct shell *sh = shell_backend_dummy_get_ptr();
    char local_cmd[CMD_BUFFER_SIZE];
    while (1) {
        k_sem_take(&cmd_sem, K_FOREVER);
        strncpy(local_cmd, cmd_buffer, CMD_BUFFER_SIZE - 1);
        local_cmd[CMD_BUFFER_SIZE - 1] = '\0';
        memset(cmd_buffer, 0, CMD_BUFFER_SIZE);
        //printk("Executing UART command: %s\n", local_cmd);
        if (sh) {
            shell_backend_dummy_clear_output(sh);
                log_capture_init(); // Clear log buffer before execution
                int ret = shell_execute_cmd(sh, local_cmd);
                printk("shell_execute_cmd returned: %d\n", ret);
                size_t output_size;
                const char *output = shell_backend_dummy_get_output(sh, &output_size);
                if (output_size > 0) {
                    uart30_send(output, output_size);
                    uart30_send("\r\n", 2);
                } else {
                    char resp[64];
                    snprintf(resp, sizeof(resp), "ret=%d\r\n", ret);
                    uart30_send(resp, strlen(resp));
                }
        } else {
            printk("Shell backend not available\n");
            uart30_send("ERROR: Shell not available\r\n", 28);
        }
    }
}

K_THREAD_DEFINE(cmd_executor_tid, 2048, cmd_executor_thread, NULL, NULL, NULL, 5, 0, 0);

int uart_cmd_init(void)
{
    int err;
    uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart30));
    if (!device_is_ready(uart_dev)) {
        printk("UART command device not ready\n");
        return -ENODEV;
    }
    err = uart_irq_callback_user_data_set(uart_dev, uart_isr, NULL);
    if (err < 0) {
        if (err == -ENOTSUP) {
            printk("Interrupt-driven UART API not enabled\n");
        }
        return err;
    }
    uart_irq_rx_enable(uart_dev);
    printk("UART command reception initialized on UART30\n");
    printk("TX: P0.0, RX: P0.1, 115200 baud\n");
    return 0;
}
