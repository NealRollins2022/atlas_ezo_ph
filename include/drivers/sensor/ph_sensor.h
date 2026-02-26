#ifndef PH_SENSOR_H_
#define PH_SENSOR_H_

#include <zephyr/drivers/sensor.h>

enum ph_sensor_channel {
	PH_SENSOR_CHAN_PH = SENSOR_CHAN_PRIV_START,
};

enum ph_sensor_attribute {
	/*
	 * PH_SENSOR_ATTR_CALIBRATION
	 *
	 * val->val1 = calibration point:
	 *   0 = low  (e.g. pH 4.0 buffer)
	 *   1 = mid  (e.g. pH 7.0 buffer)
	 *   2 = high (e.g. pH 10.0 buffer)
	 *
	 * val->val2 = actual buffer pH in micro-units.
	 *   e.g. pH 6.86 -> val->val2 = 6860000
	 *        pH 4.0  -> val->val2 = 4000000
	 */
	PH_SENSOR_ATTR_CALIBRATION = SENSOR_ATTR_PRIV_START,

	/*
	 * PH_SENSOR_ATTR_MODE_SWITCH (UART mode only)
	 *
	 * Switches the sensor from UART to I2C mode and locks the protocol.
	 * val->val1 = target I2C address (e.g. 99 for default 0x63).
	 *
	 * The driver sends "I2C,<addr>\r" followed by "Plock,1\r".
	 * After this the sensor must be power-cycled before I2C will work.
	 * Returns -ENOTSUP if the current instance is already on I2C.
	 */
	PH_SENSOR_ATTR_MODE_SWITCH,
};

#endif /* PH_SENSOR_H_ */
