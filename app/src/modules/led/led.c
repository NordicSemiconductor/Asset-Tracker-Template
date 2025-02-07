/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "message_channel.h"
#include "led.h"
#include "location.h"
#include "network.h"


#define PWM_LED0	DT_ALIAS(pwm_led0)
#define PWM_LED1	DT_ALIAS(pwm_led1)
#define PWM_LED2	DT_ALIAS(pwm_led2)

#if DT_NODE_HAS_STATUS(PWM_LED0, okay)
static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(PWM_LED0);
#else
#error "Unsupported board: pwm-led 0 devicetree alias is not defined"
#endif
#if DT_NODE_HAS_STATUS(PWM_LED1, okay)
static const struct pwm_dt_spec pwm_led1 = PWM_DT_SPEC_GET(PWM_LED1);
#else
#error "Unsupported board: pwm-led 1 devicetree alias is not defined"
#endif
#if DT_NODE_HAS_STATUS(PWM_LED2, okay)
static const struct pwm_dt_spec pwm_led2 = PWM_DT_SPEC_GET(PWM_LED2);
#else
#error "Unsupported board: pwm-led 2 devicetree alias is not defined"
#endif

/* Register log module */
LOG_MODULE_REGISTER(led, CONFIG_APP_LED_LOG_LEVEL);

void led_callback(const struct zbus_channel *chan);

/* Register listener - led_callback will be called everytime a channel that the module listens on
 * receives a new message.
 */
ZBUS_LISTENER_DEFINE(led, led_callback);

ZBUS_CHAN_DEFINE(LED_CHAN,
		 struct led_msg,
		 NULL,
		 NULL,
		 ZBUS_OBSERVERS_EMPTY,
		 ZBUS_MSG_INIT(0)
);

/* Observe channels */
ZBUS_CHAN_ADD_OBS(LED_CHAN, led, 0);

static int pwm_out(const struct led_msg *led_msg)
{
	int err;
	
	#define PWM_PERIOD PWM_USEC(255)
	
	if (!pwm_is_ready_dt(&pwm_led0)) {
		LOG_ERR("Error: PWM device %s is not ready\n", pwm_led0.dev->name);
		return -ENODEV;
	}

	/* RED */
	err = pwm_set_dt(&pwm_led0, PWM_PERIOD, PWM_USEC(led_msg->red));
	if (err) {
		LOG_ERR("pwm_set_dt, error:%d", err);
		return err;
	}

	/* GREEN */
	err = pwm_set_dt(&pwm_led1, PWM_PERIOD, PWM_USEC(led_msg->green));
	if (err) {
		LOG_ERR("pwm_set_dt, error:%d", err);
		return err;
	}

	/* BLUE */
	err = pwm_set_dt(&pwm_led2, PWM_PERIOD, PWM_USEC(led_msg->blue));
	if (err) {
		LOG_ERR("pwm_set_dt, error:%d", err);
		return err;
	}

	return 0;
}


/* Function called when there is a message received on a channel that the module listens to */
void led_callback(const struct zbus_channel *chan)
{
	if (&LED_CHAN == chan) {
		const struct led_msg *led_msg = zbus_chan_const_msg(chan);
		pwm_out(led_msg);
	}
}
