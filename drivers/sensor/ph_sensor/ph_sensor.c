#define DT_DRV_COMPAT atlas_ezo_ph

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <drivers/sensor/ph_sensor.h>

LOG_MODULE_REGISTER(ph_sensor, CONFIG_SENSOR_LOG_LEVEL);

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
static int send_command(const struct device *dev, const char *cmd) {
  const struct ph_sensor_config *config = dev->config;

  if (config->is_i2c) {
    return i2c_write(config->i2c.bus, (uint8_t *)cmd, strlen(cmd), config->i2c.addr);
  } else {
    for (size_t i = 0; i < strlen(cmd); i++) {
      uart_poll_out(config->uart, cmd[i]);
    }
    return 0;
  }
}

static int read_response(const struct device *dev, uint8_t *buf, size_t len) {
  const struct ph_sensor_config *config = dev->config;

  if (config->is_i2c) {
    return i2c_read(config->i2c.bus, buf, len, config->i2c.addr);
  } else {
    size_t idx = 0;
    uint32_t start = k_uptime_get_32();
    while (idx < len && (k_uptime_get_32() - start) < 1000) {  // Timeout 1s
      if (uart_poll_in(config->uart, &buf[idx]) == 0) {
        if (buf[idx] == '\0' || buf[idx] == '\r') break;  // Terminate on CR or NULL
        idx++;
      }
    }
    buf[idx] = '\0';
    return (idx > 0) ? 0 : -ETIMEDOUT;
  }
}

static int ph_sensor_sample_fetch(const struct device *dev) {
  struct ph_sensor_data *data = dev->data;
  uint8_t buf[20];

  send_command(dev, "R");
  k_sleep(K_MSEC(600));  // Standard read delay

  int ret = read_response(dev, buf, sizeof(buf));
  if (ret < 0) return ret;

  data->ph_value = strtof((char *)buf, NULL);
  return 0;
}

static int ph_sensor_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val) {
  struct ph_sensor_data *data = dev->data;
  if (chan != PH_SENSOR_CHAN_PH) return -ENOTSUP;
  val->val1 = (int32_t)data->ph_value;
  val->val2 = (int32_t)((data->ph_value - val->val1) * 1000000);
  return 0;
}

static int ph_sensor_attr_set(const struct device *dev, enum sensor_channel chan, enum sensor_attribute attr, const struct sensor_value *val) {
  if (attr == PH_SENSOR_ATTR_CALIBRATION) {
    char cmd[20];
    float cal_val = val->val1 + (val->val2 / 1000000.0f);
    switch (val->val1) {  // 0=low, 1=mid, 2=high
      case 0: snprintk(cmd, sizeof(cmd), "Cal,low,%.2f", cal_val); break;
      case 1: snprintk(cmd, sizeof(cmd), "Cal,mid,%.2f", cal_val); break;
      case 2: snprintk(cmd, sizeof(cmd), "Cal,high,%.2f", cal_val); break;
      default: return -EINVAL;
    }
    send_command(dev, cmd);
    k_sleep(K_MSEC(600));
    return 0;
  } else if (attr == PH_SENSOR_ATTR_MODE_SWITCH) {
    // Example: val->val1 = I2C address
    char cmd[20];
    snprintk(cmd, sizeof(cmd), "I2C,%d", val->val1);
    send_command(dev, cmd);
    k_sleep(K_MSEC(300));  // Switch delay
    send_command(dev, "Plock,1");
    return 0;
  }
  return -ENOTSUP;
}

static const struct sensor_driver_api ph_sensor_api = {
  .sample_fetch = ph_sensor_sample_fetch,
  .channel_get = ph_sensor_channel_get,
  .attr_set = ph_sensor_attr_set,
};

static int ph_sensor_init(const struct device *dev) {
  const struct ph_sensor_config *config = dev->config;

  if (config->is_i2c) {
    if (!device_is_ready(config->i2c.bus)) return -ENODEV;
  } else {
    if (!device_is_ready(config->uart)) return -ENODEV;
  }
  return 0;
}

#define PH_SENSOR_DEFINE(inst)                                               \
  static struct ph_sensor_data ph_sensor_data_##inst;                        \
  static const struct ph_sensor_config ph_sensor_config_##inst = {           \
    .is_i2c = strcmp(DT_INST_PROP(inst, interface), "i2c") == 0,             \
    {                                                                        \
      .i2c = I2C_DT_SPEC_INST_GET(inst),                                     \
    }                                                                        \
  };                                                                         \
  DEVICE_DT_INST_DEFINE(inst, ph_sensor_init, NULL,                          \
                        &ph_sensor_data_##inst, &ph_sensor_config_##inst,    \
                        POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,            \
                        &ph_sensor_api);

DT_INST_FOREACH_STATUS_OKAY(PH_SENSOR_DEFINE)