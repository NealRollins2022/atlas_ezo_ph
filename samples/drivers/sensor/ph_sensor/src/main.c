
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <drivers/sensor/ph_sensor.h>

int main(void)
{
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ezo_ph));

	if (!device_is_ready(dev)) {
		printk("pH sensor not ready\n");
		return -ENODEV;
	}

	while (1) {
		struct sensor_value ph;
		int ret;

		ret = sensor_sample_fetch(dev);
		if (ret < 0) {
			printk("Failed to fetch sample: %d\n", ret);
			k_sleep(K_SECONDS(5));
			continue;
		}

		ret = sensor_channel_get(dev,
					 (enum sensor_channel)PH_SENSOR_CHAN_PH,
					 &ph);
		if (ret < 0) {
			printk("Failed to get channel: %d\n", ret);
			k_sleep(K_SECONDS(5));
			continue;
		}

		printk("pH: %d.%06d\n", ph.val1, ph.val2);

		k_sleep(K_SECONDS(5));
	}

	return 0;
}
