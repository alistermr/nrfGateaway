#include "uart_cmd.h"
//#include <_mingw_stat64.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <string.h>
#include <stdio.h>
#include "log_capture.h"

//kanksje
#include <zephyr/bluetooth/mesh.h>
#include <zephyr/bluetooth/mesh/shell.h>


#define CMD_BUFFER_SIZE 512
#define CMD_QUEUE_LEN   32
//static char *cmd_buffer[CMD_BUFFER_SIZE];
//static int *cmd_buffer_pos = 0;
//static K_SEM_DEFINE(cmd_sem, 0, 1);
//static K_SEM_DEFINE(cmd_count_sem, 0, CMD_BUFFER_SIZE);
static const struct device *uart_dev;
K_MSGQ_DEFINE(cmd_msgq, CMD_BUFFER_SIZE, CMD_QUEUE_LEN, 4);
static void uart30_send(const char *data, size_t len);

static const char *commands[] = {
    "init",
    "scan",
    "prov",
    "bind",
    NULL
};


static void run_command(const char *command);
static void uart30_send(const char *data, size_t len);

static void uuid_to_str(const uint8_t uuid[16], char *out, size_t out_len)
{
    static const char hex[] = "0123456789abcdef";
    size_t p = 0;

    for (size_t i = 0; i < 16 && (p + 2) < out_len; i++) {
        out[p++] = hex[(uuid[i] >> 4) & 0x0F];
        out[p++] = hex[uuid[i] & 0x0F];
    }

    if (p < out_len) {
        out[p] = '\0';
    } else if (out_len > 0) {
        out[out_len - 1] = '\0';
    }
}

/* Note: Signature may vary slightly by Zephyr version. */
static void uart_unprov_beacon_cb(const uint8_t uuid[16],
                                  bt_mesh_prov_oob_info_t oob_info,
                                  uint32_t *uri_hash){
    char uuid_str[33];
    char line[128];

    uuid_to_str(uuid, uuid_str, sizeof(uuid_str));

    snprintk(line, sizeof(line),
             "uuid=%s\r\n",
             uuid_str);

    uart30_send(line, strlen(line));
}

static bool enqueue_command(const char *cmd)
{
    if (!cmd || cmd[0] == '\0') {
        return false;
    }

    char tmp[CMD_BUFFER_SIZE];
    snprintf(tmp, sizeof(tmp), "%s", cmd);

    if (k_msgq_put(&cmd_msgq, tmp, K_NO_WAIT) != 0) {
        printk("Command queue full, dropping: %s\n", tmp);
        return false;
    }
    return true;
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

//receives uart commands from uart and runs them
static void uart_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    static char uart_buffer[CMD_BUFFER_SIZE];
    static size_t uart_buffer_pos = 0;
    uint8_t c;
    if (!uart_irq_update(dev)) {
        return;
    }
    if (!uart_irq_rx_ready(dev)) {
        return;
    }
    while (uart_fifo_read(dev, &c, 1) == 1) {
        if (c == '\r' || c == '\n') {
            if (uart_buffer_pos > 0) {
                //check if the command is in the list of allowed commands
                run_command(uart_buffer);
                uart_buffer_pos = 0;
                memset(uart_buffer, 0, sizeof(uart_buffer));
            }
        } else if (uart_buffer_pos < CMD_BUFFER_SIZE - 1) {
            uart_buffer[uart_buffer_pos++] = (char)c;
        } else {
            printk("Command buffer overflow, resetting\n");
            uart_buffer_pos = 0;
        }
    }
}

static void run_command(const char *command)
{
    static bool scanning = false;

    if (strcmp(command, "init") == 0) {
        printk("Initializing device...\n");
        enqueue_command("mesh init");
        enqueue_command("mesh cdb create");
        enqueue_command("mesh prov local 0 0x0001");
    } else if (strcmp(command, "scan") == 0) {
        scanning = !scanning;
        if (scanning) {
            printk("Scanning for devices...\n");
            //enqueue_command("mesh prov beacon-listen on");
            bt_mesh_shell_prov.unprovisioned_beacon = uart_unprov_beacon_cb;
        } else {
            printk("Stopping scan...\n");
            //enqueue_command("mesh prov beacon-listen off");
            bt_mesh_shell_prov.unprovisioned_beacon = NULL;
        }
    } else if (strncmp(command, "prov", strlen("prov")) == 0) {
        const char *uuid = command + strlen("prov");
        while (*uuid == ' ') {
            uuid++;
        }
        if (*uuid == '\0') {
            printk("Provision requires UUID\n");
            return;
        }

        char prov_cmd[CMD_BUFFER_SIZE];
        snprintf(prov_cmd, sizeof(prov_cmd),
                 "mesh prov remote-adv %s 0 0x0010 5", uuid);
        enqueue_command(prov_cmd);
    } else if (strcmp(command, "bind") == 0) {
        enqueue_command("mesh target dst local");
        enqueue_command("mesh target net 0");
        enqueue_command("mesh models cfg appkey add 0 0");
        enqueue_command("mesh target dst 0x0010");
        enqueue_command("mesh models cfg appkey add 0 0");
        enqueue_command("mesh models cfg model app-bind 0x0010 0 0x1000");
        enqueue_command("mesh models cfg model app-bind 0x0011 0 0x1000");
        enqueue_command("mesh models cfg model app-bind 0x0012 0 0x1000");
        enqueue_command("mesh models cfg model app-bind 0x0013 0 0x1000");
    } else {
        enqueue_command(command);
    }
}

static void cmd_executor_thread(void)
{
    const struct shell *sh = shell_backend_dummy_get_ptr();
    char local_cmd[CMD_BUFFER_SIZE];
    while (1) {
        k_msgq_get(&cmd_msgq, local_cmd, K_FOREVER);
        printk("Running command: %s\n", local_cmd);

        //memset(cmd_buffer, 0, sizeof(cmd_buffer));
        //printk("Executing UART command: %s\n", local_cmd);
        if (sh) {
            shell_backend_dummy_clear_output(sh);
            int ret = shell_execute_cmd(sh, local_cmd);
            //k_sleep(K_MSEC(100));
            printk("shell_execute_cmd returned: %d\n", ret);
            size_t output_size;
            const char *output = shell_backend_dummy_get_output(sh, &output_size);
            if (output_size > 0) {
                //used for forwarding the mesh UUIDs from the shell output to the UART
                forward_mesh_uuid_from_output(output, output_size);
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
