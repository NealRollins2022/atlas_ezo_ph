#ifndef PH_SENSOR_H_
#define PH_SENSOR_H_

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>  // Required for struct i2c_dt_spec
#include <zephyr/drivers/uart.h>  // Required for const struct device *uart

struct ph_sensor_config {
	bool is_i2c;
	union {
		struct i2c_dt_spec i2c;
		const struct device *uart;
	};
};

struct ph_sensor_data {
	float ph_value;
	bool initialized;  /* Flag for runtime activation */
#if CONFIG_PH_SENSOR_MODE_CONTINUOUS
	struct k_thread acq_thread;
#endif
};

enum ph_sensor_channel {
	PH_SENSOR_CHAN_PH = SENSOR_CHAN_PRIV_START,
};

enum ph_sensor_attribute {
	PH_SENSOR_ATTR_CALIBRATION = SENSOR_ATTR_PRIV_START,
	PH_SENSOR_ATTR_MODE_SWITCH,
};
#ifdef __cplusplus
extern "C" {
#endif

int ph_sensor_runtime_start(const struct device *dev);

#ifdef __cplusplus
}
#endif
#endif /* PH_SENSOR_H_ */