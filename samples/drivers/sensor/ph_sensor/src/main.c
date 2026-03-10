/*
 * samples/ph_sensor_serial/src/main.c
 *
 * Serial sample — pH sensor in on-demand mode, output to UART.
 *
 * Config: CONFIG_PH_SENSOR=y
 *         CONFIG_PH_SENSOR_TRANSPORT_SERIAL=y
 *         CONFIG_PH_SENSOR_MODE_ON_DEMAND=y
 *         CONFIG_PH_SENSOR_I2C_READ_TIMEOUT_MS=2000
 *
 * No network. No threads. App drives the read loop.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include <drivers/sensor/ph_sensor.h>

LOG_MODULE_REGISTER(ph_serial_sample, LOG_LEVEL_INF);

#define READ_INTERVAL_SEC 10

static const struct device *ph_dev = DEVICE_DT_GET(DT_NODELABEL(ezo_ph));

BUILD_ASSERT(DT_NODE_EXISTS(DT_NODELABEL(ezo_ph)),
	     "ezo_ph DT node missing — check overlay");

int main(void)
{
	if (!device_is_ready(ph_dev)) {
		LOG_ERR("ezo_ph device not ready");
		return -ENODEV;
	}

	LOG_INF("pH serial sample started");

	while (true) {
		struct sensor_value val;

		int err = sensor_sample_fetch(ph_dev);
		if (err) {
			LOG_ERR("sample_fetch failed: %d", err);
			k_sleep(K_SECONDS(READ_INTERVAL_SEC));
			continue;
		}

		err = sensor_channel_get(ph_dev,
			(enum sensor_channel)PH_SENSOR_CHAN_PH, &val);
		if (err) {
			LOG_ERR("channel_get failed: %d", err);
			k_sleep(K_SECONDS(READ_INTERVAL_SEC));
			continue;
		}

		LOG_INF("pH: %d.%06d", val.val1, val.val2);

		k_sleep(K_SECONDS(READ_INTERVAL_SEC));
	}

	return 0;
}
