/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

static int sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	return 0;
}

static int channel_get(const struct device *dev, enum sensor_channel chan,
		      struct sensor_value *val)
{
	return 0;
}

static const struct sensor_driver_api dummy_api = {
	.sample_fetch = &sample_fetch,
	.channel_get = &channel_get,
};

struct device_state state = {
	.initialized = true,
	.init_res = 0U,
};

struct device mock_sensor_device = {
	.name = "mock_sensor",
	.state = &state,
	.api = &dummy_api
};
