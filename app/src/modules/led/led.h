/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**@file
 *
 * @brief   LED module.
 *
 * Module that handles LED behaviour.
 */

#ifndef LED_H__
#define LED_H__

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/* momo */
ZBUS_CHAN_DECLARE(
	LED_CHAN
);

enum led_msg_type {
	LED_RGB_SET,
};

struct led_msg {
	enum led_msg_type type;

	/** RGB values (0 to 255) */
	uint8_t red;
	uint8_t green;
	uint8_t blue;

	/** Duration of the RGB on/off cycle */
	uint32_t duration_on_msec;
	uint32_t duration_off_msec;

	/** Number of on/off cycles (-1 indicates forever) */
	int repetitions;
};

#define MSG_TO_LED_MSG(_msg) ((const struct led_msg *)_msg)





#define LED_1 1
#define LED_2 2
#define LED_3 3
#define LED_4 4

#define LED_ON(x)		(x)
#define LED_BLINK(x)		((x) << 8)
#define LED_GET_ON(x)		((x)&0xFF)
#define LED_GET_BLINK(x)	(((x) >> 8) & 0xFF)

#define LED_ON_PERIOD_NORMAL	500
#define LED_OFF_PERIOD_NORMAL	2000
#define LED_ON_PERIOD_ERROR	200
#define LED_OFF_PERIOD_ERROR	200
#define LED_ON_PERIOD_SHORT	350
#define LED_OFF_PERIOD_SHORT	350
#define LED_ON_PERIOD_STROBE	50
#define LED_OFF_PERIOD_STROBE	50
#define LED_OFF_PERIOD_LONG	4000

#define LED_MAX UINT8_MAX

#define LED_COLOR_OFF		LED_COLOR(0, 0, 0)
#define LED_COLOR_RED		LED_COLOR(LED_MAX, 0, 0)
#define LED_COLOR_GREEN		LED_COLOR(0, LED_MAX, 0)
#define LED_COLOR_BLUE		LED_COLOR(0, 0, LED_MAX)
#define LED_COLOR_YELLOW	LED_COLOR(LED_MAX, LED_MAX, 0)
#define LED_COLOR_CYAN		LED_COLOR(0, LED_MAX, LED_MAX)
#define LED_COLOR_PURPLE	LED_COLOR(LED_MAX, 0, LED_MAX)
#define LED_COLOR_WHITE		LED_COLOR(LED_MAX, LED_MAX, LED_MAX)

#define LED_LTE_CONNECTING_COLOR	LED_COLOR_YELLOW
#define LED_LOCATION_SEARCHING_COLOR	LED_COLOR_GREEN
#define LED_POLL_MODE_COLOR		LED_COLOR_BLUE
#define LED_ERROR_SYSTEM_FAULT_COLOR	LED_COLOR_RED
#define LED_OFF_COLOR			LED_COLOR_OFF

#ifdef __cplusplus
}
#endif

#endif /* LED_H__ */
