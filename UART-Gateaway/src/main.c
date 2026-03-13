/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/dk_prov.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/bluetooth/mesh/shell.h>
#include "model_handler.h"
#include "init.h"
#include "uart_cmd.h"
#include "log_capture.h"
//#include "filtering.h"
#include "smp_bt.h"

/*ADDED INCLUDES*/
#include <zephyr/settings/settings.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/ring_buffer.h>
#include <string.h>
#include <stdio.h>

#if defined(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>
#endif



// ...existing code...
// If you need to define a log backend, use the new log_backend_capture_init as the .init callback
// Example:
// static const struct log_backend_api log_capture_api = {
//   .process = log_capture_process,
//   .dropped = log_capture_dropped,
//   .panic = log_capture_panic,
//   .init = log_backend_capture_init,
// };
// LOG_BACKEND_DEFINE(log_capture_backend, log_capture_api, true, NULL);




int main(void)
{
    int err;
    printk("Initializing...\n");
    err = bt_enable(bt_ready);
    if (err && err != -EALREADY) {
        printk("Bluetooth init failed (err %d)\n", err);
    }
    err = uart_cmd_init();
    if (err) {
        printk("UART command init failed (err %d)\n", err);
    }
    log_capture_init();
    printk("Press the <Tab> button for supported commands.\n");
    printk("Before any Mesh commands you must run \"mesh init\"\n");
    printk("Ready to receive commands via UART...\n");
    while (1) {
        k_sleep(K_FOREVER);
    }
    return 0;
}
