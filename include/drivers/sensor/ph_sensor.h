#ifndef PH_SENSOR_H_
#define PH_SENSOR_H_

#include <zephyr/drivers/sensor.h>

enum ph_sensor_channel {
  PH_SENSOR_CHAN_PH = SENSOR_CHAN_PRIV_START,
};

enum ph_sensor_attribute {
  PH_SENSOR_ATTR_CALIBRATION = SENSOR_ATTR_PRIV_START,
};

#endif /* PH_SENSOR_H_ */
