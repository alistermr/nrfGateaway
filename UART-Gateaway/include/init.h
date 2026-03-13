/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Model handler
 */

#ifndef INIT_H
#define INIT_H

#include <zephyr/drivers/uart.h>
#include <zephyr/bluetooth/bluetooth.h>

#ifdef __cplusplus
extern "C" {
#endif

int uart_cmd_init(void);
void bt_ready(int err);


#ifdef __cplusplus
}
#endif

#endif /* INIT_H__ */