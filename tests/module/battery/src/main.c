/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>

#include <zephyr/fff.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <zephyr/logging/log.h>
#include "message_channel.h"
#include "battery.h"

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, task_wdt_feed, int);
FAKE_VALUE_FUNC(int, task_wdt_add, uint32_t, task_wdt_callback_t, void *);
FAKE_VALUE_FUNC(float, nrf_fuel_gauge_process, float, float, float, float, bool, void *);
FAKE_VALUE_FUNC(int, charger_read_sensors, float *, float *, float *, int32_t *);
FAKE_VALUE_FUNC(int, nrf_fuel_gauge_init, const struct nrf_fuel_gauge_init_parameters *, void *);

ZBUS_MSG_SUBSCRIBER_DEFINE(battery_subscriber);
ZBUS_CHAN_ADD_OBS(BATTERY_CHAN, battery_subscriber, 0);

LOG_MODULE_REGISTER(battery_module_test, 4);

void check_battery_event(enum battery_msg_type expected_battery_type)
{
	int err;
	const struct zbus_channel *chan;
	struct battery_msg battery_msg;

	/* Allow the test thread to sleep so that the DUT's thread is allowed to run. */
	k_sleep(K_MSEC(100));

	err = zbus_sub_wait_msg(&battery_subscriber, &chan, &battery_msg, K_MSEC(1000));
	if (err == -ENOMSG) {
		LOG_ERR("No battery event received");
		TEST_FAIL();
	} else if (err) {
		LOG_ERR("zbus_sub_wait, error: %d", err);
		SEND_FATAL_ERROR();

		return;
	}

	if (chan != &BATTERY_CHAN) {
		LOG_ERR("Received message from wrong channel");
		TEST_FAIL();
	}

	TEST_ASSERT_EQUAL(expected_battery_type, battery_msg.type);
}

void check_no_battery_events(uint32_t time_in_seconds)
{
	int err;
	const struct zbus_channel *chan;
	struct battery_msg battery_msg;

	/* Allow the test thread to sleep so that the DUT's thread is allowed to run. */
	k_sleep(K_SECONDS(time_in_seconds));

	err = zbus_sub_wait_msg(&battery_subscriber, &chan, &battery_msg, K_MSEC(1000));
	if (err == 0) {
		LOG_ERR("Received trigger event with type %d", battery_msg.type);
		TEST_FAIL();
	}
}

static void send_battery_percentage_sample_request(void)
{
	struct battery_msg msg = {
		.type = BATTERY_PERCENTAGE_SAMPLE_REQUEST,
	};

	int err = zbus_chan_pub(&BATTERY_CHAN, &msg, K_SECONDS(1));

	TEST_ASSERT_EQUAL(0, err);
}

void test_battery_percentage_sample(void)
{
	for (int i = 0; i < 10; i++) {
		/* Given */
		send_battery_percentage_sample_request();

		/* When */
		k_sleep(K_SECONDS(1));

		/* Then */
		check_battery_event(BATTERY_PERCENTAGE_SAMPLE_REQUEST);
		check_battery_event(BATTERY_PERCENTAGE_SAMPLE_RESPONSE);
		check_no_battery_events(3600);
	}
}

/* This is required to be added to each test. That is because unity's
 * main may return nonzero, while zephyr's main currently must
 * return 0 in all cases (other values are reserved).
 */
extern int unity_main(void);

int main(void)
{
	(void)unity_main();

	k_sleep(K_FOREVER);

	return 0;
}
