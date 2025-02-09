/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <date_time.h>
#include <zephyr/task_wdt/task_wdt.h>
#include <zephyr/smf.h>

#include "modules_common.h"
#include "message_channel.h"
#include "button.h"
#include "network.h"
#include "environmental.h"
#include "cloud_module.h"
#include "fota.h"
#include "location.h"
#include "led.h"

#if defined(CONFIG_APP_BATTERY)
#include "battery.h"
#endif /* CONFIG_APP_BATTERY */

/* Register log module */
LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

/* Define a ZBUS listener for this module */
static void app_callback(const struct zbus_channel *chan);
ZBUS_LISTENER_DEFINE(app_listener, app_callback);

/* Observe channels */
ZBUS_CHAN_ADD_OBS(CONFIG_CHAN, app_listener, 0);
ZBUS_CHAN_ADD_OBS(CLOUD_CHAN, app_listener, 0);
ZBUS_CHAN_ADD_OBS(BUTTON_CHAN, app_listener, 0);
ZBUS_CHAN_ADD_OBS(TIME_CHAN, app_listener, 0);

/* Forward declarations */
static void trigger_work_fn(struct k_work *work);
static void date_time_handler(const struct date_time_evt *evt);
static const struct smf_state states[];

/* Delayable work used to schedule triggers */
static K_WORK_DELAYABLE_DEFINE(trigger_work, trigger_work_fn);

/* State machine definitions */
static const struct smf_state states[];

/* Forward declarations of state handlers */
static void init_entry(void *o);
static void init_run(void *o);

static void cloud_connected_entry(void *o);
static void cloud_connected_run(void *o);

static void cloud_disconnected_entry(void *o);
static void cloud_disconnected_run(void *o);

/* Defining the hierarchical trigger module states:
 *
 *   STATE_INIT: Initial state where the module waits for time to be available.
 *   STATE_CLOUD_DISCONNECTED: Cloud connection is not established or paused
 *   STATE_CLOUD_CONNECTED: Cloud connection is established and ready to send data
 */
enum state {
	STATE_INIT,
	STATE_CLOUD_CONNECTED,
	STATE_CLOUD_DISCONNECTED,
};

/* Construct state table */
static const struct smf_state states[] = {
	[STATE_INIT] = SMF_CREATE_STATE(
		init_entry,
		init_run,
		NULL,
		NULL,
		NULL
	),
	[STATE_CLOUD_CONNECTED] = SMF_CREATE_STATE(
		cloud_connected_entry,
		cloud_connected_run,
		NULL,
		NULL,
		NULL
	),
	[STATE_CLOUD_DISCONNECTED] = SMF_CREATE_STATE(
		cloud_disconnected_entry,
		cloud_disconnected_run,
		NULL,
		NULL,
		NULL
	)
};

/* State object for the app module.
 * Used to transfer data between state changes.
 */
static struct state_object {
	/* This must be first */
	struct smf_ctx ctx;

	/* Last channel type that a message was received on */
	const struct zbus_channel *chan;

	/* Trigger interval */
	uint64_t interval_sec;

	/* Button number */
	uint8_t button_number;

	/* Time available */
	enum time_status time_status;

	/* Cloud status */
	enum cloud_msg_type status;
} app_state;

static void triggers_send(void)
{
	int err;
	struct network_msg network_msg = {
		.type = NETWORK_QUALITY_SAMPLE_REQUEST,
	};

	err = zbus_chan_pub(&NETWORK_CHAN, &network_msg, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

#if defined(CONFIG_APP_BATTERY)
	struct battery_msg battery_msg = {
		.type = BATTERY_PERCENTAGE_SAMPLE_REQUEST,
	};

	err = zbus_chan_pub(&BATTERY_CHAN, &battery_msg, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
#endif /* CONFIG_APP_BATTERY */

#if defined(CONFIG_APP_ENVIRONMENTAL)
	struct battery_msg environmental_msg = {
		.type = ENVIRONMENTAL_SENSOR_SAMPLE_REQUEST,
	};

	err = zbus_chan_pub(&ENVIRONMENTAL_CHAN, &environmental_msg, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
#endif /* CONFIG_APP_ENVIRONMENTAL */

	/* Send FOTA poll trigger */
	enum fota_msg_type fota_msg = FOTA_POLL;

	err = zbus_chan_pub(&FOTA_CHAN, &fota_msg, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub FOTA trigger, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	/* Send trigger for shadow polling */
	struct cloud_msg cloud_msg = {
		.type = CLOUD_POLL_SHADOW
	};

	err = zbus_chan_pub(&CLOUD_CHAN, &cloud_msg, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub shadow trigger, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	/* Trigger location search and environmental data sample */
	enum location_msg_type location_msg = LOCATION_SEARCH_TRIGGER;

	err = zbus_chan_pub(&LOCATION_CHAN, &location_msg, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub data sample trigger, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

/* Zephyr State Machine framework handlers */

/* STATE_INIT */

static void init_entry(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("%s", __func__);

	/* Setup handler for date_time library */
	date_time_register_handler(date_time_handler);
}

static void init_run(void *o)
{
	struct state_object *user_object = o;

	LOG_DBG("%s", __func__);

	if (user_object->chan == &CLOUD_CHAN) {
		if (user_object->status == CLOUD_CONNECTED_READY_TO_SEND) {
			LOG_DBG("Cloud connected and ready, going into connected state");
			STATE_SET(app_state, STATE_CLOUD_CONNECTED);
			return;
		}

		if ((user_object->status == CLOUD_DISCONNECTED) ||
			(user_object->status == CLOUD_CONNECTED_PAUSED)) {
			LOG_DBG("Cloud disconnected/paused, going into disconnected state");
			STATE_SET(app_state, STATE_CLOUD_DISCONNECTED);
			return;
		}
	}
}

/* STATE_DISCONNECTED */

static void cloud_disconnected_entry(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("%s", __func__);

	int err;

	/* Blink Yellow */
	struct led_msg led_msg = {
		.type = LED_RGB_SET,
		.red = 255,
		.green = 255,
		.blue = 0,
		.duration_on_msec = 250,
		.duration_off_msec = 2000,
		.repetitions = 10,
	};
	err = zbus_chan_pub(&LED_CHAN, &led_msg, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	k_work_cancel_delayable(&trigger_work);
}

static void cloud_disconnected_run(void *o)
{
	struct state_object *user_object = o;

	LOG_DBG("%s", __func__);

	if ((user_object->chan == &CLOUD_CHAN) &&
		(user_object->status == CLOUD_CONNECTED_READY_TO_SEND)) {
		LOG_DBG("Cloud connected and ready, going into connected state");
		STATE_SET(app_state, STATE_CLOUD_CONNECTED);
		return;
	}
}

/* STATE_CONNECTED */

static void cloud_connected_entry(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("%s", __func__);

	int err;

	/* Blink Green */
	struct led_msg led_msg = {
		.type = LED_RGB_SET,
		.red = 0,
		.green = 255,
		.blue = 0,
		.duration_on_msec = 250,
		.duration_off_msec = 2000,
		.repetitions = 10,
	};

	err = zbus_chan_pub(&LED_CHAN, &led_msg, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	k_work_reschedule(&trigger_work, K_NO_WAIT);
}

static void cloud_connected_run(void *o)
{
	struct state_object *user_object = o;

	LOG_DBG("%s", __func__);

	if ((user_object->chan == &CLOUD_CHAN) &&
		((user_object->status == CLOUD_CONNECTED_PAUSED) ||
		(user_object->status == CLOUD_DISCONNECTED))) {
		LOG_DBG("Cloud disconnected/paused, going into disconnected state");
		STATE_SET(app_state, STATE_CLOUD_DISCONNECTED);
		return;
	}

	if (user_object->chan == &BUTTON_CHAN) {
		LOG_DBG("Button %d pressed!", user_object->button_number);
		k_work_reschedule(&trigger_work, K_NO_WAIT);
		return;
	}

	if (user_object->chan == &CONFIG_CHAN) {
		LOG_DBG("Configuration update, new interval: %lld", user_object->interval_sec);
		k_work_reschedule(&trigger_work, K_SECONDS(user_object->interval_sec));
		return;
	}
}

static void date_time_handler(const struct date_time_evt *evt) {
	if (evt->type != DATE_TIME_NOT_OBTAINED) {
		int err;
		enum time_status time_status = TIME_AVAILABLE;

		err = zbus_chan_pub(&TIME_CHAN, &time_status, K_SECONDS(1));
		if (err) {
			LOG_ERR("zbus_chan_pub, error: %d", err);
			SEND_FATAL_ERROR();
		}
	}
}

/* Delayable work used to send triggers to the rest of the system */
static void trigger_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	LOG_DBG("Sending data sample trigger");

	triggers_send();

	k_work_reschedule(&trigger_work, K_SECONDS(app_state.interval_sec));
}

/* Function called when there is a message received on a channel that the module listens to */
static void app_callback(const struct zbus_channel *chan)
{
	int err;

	if ((chan != &CONFIG_CHAN) &&
		(chan != &CLOUD_CHAN) &&
		(chan != &BUTTON_CHAN) &&
		(chan != &TIME_CHAN)) {
		LOG_ERR("Unknown channel");
		return;
	}

	LOG_DBG("Received message on channel %s", zbus_chan_name(chan));

	/* Update the state object with the channel that the message was received on */
	app_state.chan = chan;

	/* Copy corresponding data to the state object depending on the incoming channel */
	if (&CONFIG_CHAN == chan) {
		const struct configuration *config = zbus_chan_const_msg(chan);

		if (config->update_interval_present) {
			app_state.interval_sec = config->update_interval;
		}
	} else if (&CLOUD_CHAN == chan) {
		const enum cloud_msg_type *status = zbus_chan_const_msg(chan);

		app_state.status = *status;
	} else if (&BUTTON_CHAN == chan) {
		const int *button_number = zbus_chan_const_msg(chan);

		app_state.button_number = (uint8_t)*button_number;
	} else if (&TIME_CHAN == chan) {
		const enum time_status *time_status = zbus_chan_const_msg(chan);

		app_state.time_status = *time_status;
	}

	LOG_DBG("Running SMF");

	/* State object updated, run SMF */
	err = STATE_RUN(app_state);
	if (err) {
		LOG_ERR("smf_run_state, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

static int app_init(void)
{
	app_state.interval_sec = CONFIG_APP_MODULE_TRIGGER_TIMEOUT_SECONDS;

	STATE_SET_INITIAL(app_state, STATE_INIT);

	return 0;
}

SYS_INIT(app_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
