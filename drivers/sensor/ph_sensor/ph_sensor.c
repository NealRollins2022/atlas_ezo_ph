#define DT_DRV_COMPAT atlas_ezo_ph

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdlib.h>
#include <drivers/sensor/ph_sensor.h>

LOG_MODULE_REGISTER(ph_sensor, CONFIG_SENSOR_LOG_LEVEL);

/* EZO I2C response codes (first byte of every I2C read) */
#define EZO_RESP_SUCCESS     1
#define EZO_RESP_SYNTAX_ERR  2
#define EZO_RESP_PROCESSING  254
#define EZO_RESP_NO_DATA     255

/*
 * Maximum EZO I2C response frame:
 *   1 status byte + up to 31 data bytes (including sensor null terminator) = 32
 * The sensor pads any unused trailing bytes with 0x00, so always reading
 * the full frame in one transaction is safe and avoids truncation.
 */
#define EZO_I2C_FRAME_SIZE  32

struct ph_sensor_data {
	float ph_value;
};

struct ph_sensor_config {
	bool is_i2c;
	union {
		struct i2c_dt_spec i2c;
		const struct device *uart;
	};
};

/*
 * send_command - send an ASCII command to the EZO sensor.
 *
 * Appends \r as required by the Atlas EZO protocol on both I2C and UART.
 * All commands (R, Cal,xxx, I2C,xxx, Plock,1) require this terminator.
 */
static int send_command(const struct device *dev, const char *cmd)
{
	const struct ph_sensor_config *config = dev->config;
	char buf[32];
	int len;

	len = snprintk(buf, sizeof(buf), "%s\r", cmd);
	if (len < 0 || len >= (int)sizeof(buf)) {
		LOG_ERR("Command too long: %s", cmd);
		return -ENOMEM;
	}

	if (config->is_i2c) {
		return i2c_write(config->i2c.bus, (uint8_t *)buf, len,
				 config->i2c.addr);
	} else {
		for (int i = 0; i < len; i++) {
			uart_poll_out(config->uart, buf[i]);
		}
		return 0;
	}
}

/*
 * read_response - read ASCII response from the EZO sensor into buf.
 *
 * I2C: reads the full 32-byte frame in one transaction, validates the
 * EZO status byte in raw[0], strips it, and copies the payload into buf.
 * Reading the full frame is required — the EZO response length is variable
 * (e.g. "7.00\0" = 5 bytes, "10.00\0" = 6 bytes) and requesting less than
 * the frame size risks truncating the response or causing bus errors.
 *
 * UART: polls until \r, \n, or \0 terminator, or 1 s timeout.
 *
 * buf is always null-terminated on success.
 */
static int read_response(const struct device *dev, uint8_t *buf, size_t len)
{
	const struct ph_sensor_config *config = dev->config;

	if (config->is_i2c) {
		uint8_t raw[EZO_I2C_FRAME_SIZE];
		size_t payload_len;
		size_t copy_len;
		int ret;

		ret = i2c_read(config->i2c.bus, raw, sizeof(raw),
			       config->i2c.addr);
		if (ret < 0) {
			LOG_ERR("I2C read failed: %d", ret);
			return ret;
		}

		switch (raw[0]) {
		case EZO_RESP_SUCCESS:
			break;
		case EZO_RESP_PROCESSING:
			LOG_WRN("EZO still processing");
			return -EBUSY;
		case EZO_RESP_SYNTAX_ERR:
			LOG_ERR("EZO syntax error");
			return -EINVAL;
		case EZO_RESP_NO_DATA:
			LOG_ERR("EZO no data");
			return -ENODATA;
		default:
			LOG_ERR("EZO unknown response code: %d", raw[0]);
			return -EIO;
		}

		/* raw[1..31] is the null-terminated ASCII payload */
		payload_len = strnlen((char *)&raw[1], sizeof(raw) - 1);
		copy_len = MIN(payload_len, len - 1);
		memcpy(buf, &raw[1], copy_len);
		buf[copy_len] = '\0';
		return 0;

	} else {
		size_t idx = 0;
		uint32_t start = k_uptime_get_32();

		while (idx < (len - 1) &&
		       (k_uptime_get_32() - start) < 1000U) {
			unsigned char c;

			if (uart_poll_in(config->uart, &c) == 0) {
				if (c == '\r' || c == '\n' || c == '\0') {
					break;
				}
				buf[idx++] = (uint8_t)c;
			}
		}
		buf[idx] = '\0';
		return (idx > 0) ? 0 : -ETIMEDOUT;
	}
}

/*
 * ph_sensor_sample_fetch - trigger a measurement and store the result.
 *
 * Sends "R\r" and waits 600 ms for the EZO to complete its measurement
 * before reading back the ASCII pH string and parsing it to float.
 * Accepts SENSOR_CHAN_ALL or PH_SENSOR_CHAN_PH.
 */
static int ph_sensor_sample_fetch(const struct device *dev,
				  enum sensor_channel chan)
{
	struct ph_sensor_data *data = dev->data;
	uint8_t buf[20];
	int ret;

	if (chan != SENSOR_CHAN_ALL &&
	    chan != (enum sensor_channel)PH_SENSOR_CHAN_PH) {
		return -ENOTSUP;
	}

	ret = send_command(dev, "R");
	if (ret < 0) {
		LOG_ERR("Failed to send R command: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(600));

	ret = read_response(dev, buf, sizeof(buf));
	if (ret < 0) {
		LOG_ERR("Failed to read response: %d", ret);
		return ret;
	}

	data->ph_value = strtof((char *)buf, NULL);
	LOG_DBG("pH raw: \"%s\"  parsed: %d.%06d", buf,
		(int)data->ph_value,
		(int)((data->ph_value - (int)data->ph_value) * 1000000));
	return 0;
}

static int ph_sensor_channel_get(const struct device *dev,
				 enum sensor_channel chan,
				 struct sensor_value *val)
{
	const struct ph_sensor_data *data = dev->data;

	if (chan != (enum sensor_channel)PH_SENSOR_CHAN_PH) {
		return -ENOTSUP;
	}

	val->val1 = (int32_t)data->ph_value;
	val->val2 = (int32_t)((data->ph_value - val->val1) * 1000000);
	return 0;
}

/*
 * ph_sensor_attr_set - calibration and mode-switch support.
 *
 * PH_SENSOR_ATTR_CALIBRATION:
 *   val->val1 = calibration point selector:
 *     0 = low  (typically pH 4.0)
 *     1 = mid  (typically pH 7.0)
 *     2 = high (typically pH 10.0)
 *   val->val2 = actual buffer pH value in micro-units.
 *     e.g. for pH 6.86: val->val2 = 6860000
 *
 * PH_SENSOR_ATTR_MODE_SWITCH (UART only):
 *   val->val1 = desired I2C address (e.g. 99 for 0x63).
 *   Sends "I2C,<addr>\r" then "Plock,1\r".
 *   Returns -ENOTSUP if called on an I2C-configured instance.
 */
static int ph_sensor_attr_set(const struct device *dev,
			      enum sensor_channel chan,
			      enum sensor_attribute attr,
			      const struct sensor_value *val)
{
	const struct ph_sensor_config *config = dev->config;
	int ret;

	ARG_UNUSED(chan);

	if (attr == (enum sensor_attribute)PH_SENSOR_ATTR_CALIBRATION) {
		char cmd[32];
		double cal_val = val->val2 / 1000000.0;

		switch (val->val1) {
		case 0:
			snprintk(cmd, sizeof(cmd), "Cal,low,%.2f", cal_val);
			break;
		case 1:
			snprintk(cmd, sizeof(cmd), "Cal,mid,%.2f", cal_val);
			break;
		case 2:
			snprintk(cmd, sizeof(cmd), "Cal,high,%.2f", cal_val);
			break;
		default:
			LOG_ERR("Invalid calibration point: %d", val->val1);
			return -EINVAL;
		}

		ret = send_command(dev, cmd);
		if (ret < 0) {
			return ret;
		}
		k_sleep(K_MSEC(600));
		return 0;

	} else if (attr == (enum sensor_attribute)PH_SENSOR_ATTR_MODE_SWITCH) {
		char cmd[20];

		if (config->is_i2c) {
			LOG_ERR("MODE_SWITCH only valid from UART mode");
			return -ENOTSUP;
		}

		snprintk(cmd, sizeof(cmd), "I2C,%d", val->val1);
		ret = send_command(dev, cmd);
		if (ret < 0) {
			return ret;
		}
		k_sleep(K_MSEC(300));

		ret = send_command(dev, "Plock,1");
		if (ret < 0) {
			return ret;
		}
		return 0;
	}

	return -ENOTSUP;
}

static const struct sensor_driver_api ph_sensor_api = {
	.sample_fetch = ph_sensor_sample_fetch,
	.channel_get  = ph_sensor_channel_get,
	.attr_set     = ph_sensor_attr_set,
};

static int ph_sensor_init(const struct device *dev)
{
	const struct ph_sensor_config *config = dev->config;

	if (config->is_i2c) {
		if (!device_is_ready(config->i2c.bus)) {
			LOG_ERR("I2C bus not ready");
			return -ENODEV;
		}
		LOG_INF("EZO-pH initialised on I2C addr 0x%02x",
			config->i2c.addr);
	} else {
		if (!device_is_ready(config->uart)) {
			LOG_ERR("UART device not ready");
			return -ENODEV;
		}
		LOG_INF("EZO-pH initialised on UART");
	}
	return 0;
}

/*
 * PH_SENSOR_DEFINE - per-instance instantiation macro.
 *
 * Uses COND_CODE_1 to populate either the i2c or uart union member
 * based on which bus the DT node sits on. The original code always
 * populated .i2c unconditionally, leaving .uart as an uninitialised
 * pointer for UART instances.
 */
#define PH_SENSOR_DEFINE(inst)                                               \
	static struct ph_sensor_data ph_sensor_data_##inst;                  \
	static const struct ph_sensor_config ph_sensor_config_##inst = {     \
		.is_i2c = DT_INST_ON_BUS(inst, i2c),                        \
		COND_CODE_1(DT_INST_ON_BUS(inst, i2c),                      \
			({ .i2c = I2C_DT_SPEC_INST_GET(inst), }),            \
			({ .uart = DEVICE_DT_GET(DT_INST_BUS(inst)), })      \
		)                                                            \
	};                                                                   \
	DEVICE_DT_INST_DEFINE(inst, ph_sensor_init, NULL,                    \
			      &ph_sensor_data_##inst,                        \
			      &ph_sensor_config_##inst,                      \
			      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,      \
			      &ph_sensor_api);

DT_INST_FOREACH_STATUS_OKAY(PH_SENSOR_DEFINE)

