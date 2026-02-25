#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <drivers/sensor/ph_sensor.h>
void main(void) {
  const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ezo_ph));

  if (!device_is_ready(dev)) {
    printk("pH sensor not ready\n");
    return;
  }

  while (1) {
    struct sensor_value ph;

    sensor_sample_fetch(dev);
    sensor_channel_get(dev, PH_SENSOR_CHAN_PH, &ph);

    printk("pH: %d.%06d\n", ph.val1, ph.val2);

    k_sleep(K_SECONDS(5));
  }
}
