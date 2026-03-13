/*UART INIT*/
#include "init.h"
#include <zephyr/drivers/uart.h>
#include <zephyr/bluetooth/mesh/shell.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/printk.h>
#include "model_handler.h"

void bt_ready(int err)
{
    if (err && err != -EALREADY) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }
    printk("Bluetooth initialized\n");
    int mesh_err = bt_mesh_init(&bt_mesh_shell_prov, model_handler_init());
    if (mesh_err) {
        printk("Initializing mesh failed (err %d)\n", mesh_err);
        return;
    }
    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }
    if (bt_mesh_is_provisioned()) {
        printk("Mesh network restored from flash\n");
    } else {
        printk("Use \"prov pb-adv on\" or \"prov pb-gatt on\" to enable advertising\n");
    }
}