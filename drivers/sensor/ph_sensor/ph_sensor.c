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

/*
 * SIGNAL_DATA_READY — signals app semaphore after each successful acquisition.
 * ph_data_ready_sem is declared weak; if the app does not define it the
 * address resolves to 0 and the macro is a no-op.
 */
extern struct k_sem ph_data_ready_sem __attribute__((weak));
#define SIGNAL_DATA_READY() \
do { \
if ((uintptr_t)&ph_data_ready_sem != 0U) { \
k_sem_give(&ph_data_ready_sem); \
} \
} while (0)

/* EZO I2C response codes (first byte of every I2C read) */
#define EZO_RESP_SUCCESS 1
#define EZO_RESP_SYNTAX_ERR 2
#define EZO_RESP_PROCESSING 254
#define EZO_RESP_NO_DATA 255

/*
 * Maximum EZO I2C response frame:
 * 1 status byte + up to 31 data bytes = 32
 */
#define EZO_I2C_FRAME_SIZE 32
#define EZO_READ_RETRIES CONFIG_PH_SENSOR_RETRY_COUNT
#define EZO_RETRY_DELAY_MS CONFIG_PH_SENSOR_RETRY_DELAY_MS

/* Forward declarations */
int send_command(const struct device *dev, const char *cmd);
int read_response(const struct device *dev, uint8_t *buf, size_t len);
static int ph_sensor_sample_fetch(const struct device *dev, enum sensor_channel chan);
static int ph_sensor_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val);
static int ph_sensor_attr_set(const struct device *dev, enum sensor_channel chan, enum sensor_attribute attr, const struct sensor_value *val);
static int ph_sensor_init(const struct device *dev);

/*
 * send_command - send an ASCII command to the EZO sensor.
 */
int send_command(const struct device *dev, const char *cmd)
{
    const struct ph_sensor_config *config = dev->config;
    char buf[32];
    int len;

    if (config->is_i2c) {
        len = snprintk(buf, sizeof(buf), "%s", cmd);
        if (len < 0 || len >= (int)sizeof(buf)) {
            LOG_ERR("Command too long: %s", cmd);
            return -ENOMEM;
        }
        return i2c_write(config->i2c.bus, (uint8_t *)buf, len, config->i2c.addr);
    } else {
        len = snprintk(buf, sizeof(buf), "%s\r", cmd);
        if (len < 0 || len >= (int)sizeof(buf)) {
            LOG_ERR("Command too long: %s", cmd);
            return -ENOMEM;
        }
        for (int i = 0; i < len; i++) {
            uart_poll_out(config->uart, buf[i]);
        }
        return 0;
    }
}

/*
 * read_response - read ASCII response from the EZO sensor into buf.
 */
int read_response(const struct device *dev, uint8_t *buf, size_t len)
{
    const struct ph_sensor_config *config = dev->config;

    if (config->is_i2c) {
        uint8_t raw[EZO_I2C_FRAME_SIZE];
        int ret;
        int retries = EZO_READ_RETRIES;

        do {
            ret = i2c_read(config->i2c.bus, raw, sizeof(raw), config->i2c.addr);
            if (ret < 0) {
                LOG_ERR("I2C read failed: %d", ret);
                return ret;
            }
            LOG_DBG("EZO status byte: %d", raw[0]);
            if (raw[0] != EZO_RESP_PROCESSING &&
                raw[0] != EZO_RESP_NO_DATA) {
                break;
            }
            LOG_WRN("EZO still processing, retrying in %d ms (%d retries left)",
                    EZO_RETRY_DELAY_MS, retries - 1);
            k_sleep(K_MSEC(EZO_RETRY_DELAY_MS));
        } while (--retries > 0);

        switch (raw[0]) {
        case EZO_RESP_SUCCESS:
            break;
        case EZO_RESP_PROCESSING:
            LOG_ERR("EZO timed out still processing after retries");
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

        size_t payload_len = strnlen((char *)&raw[1], sizeof(raw) - 1);
        size_t copy_len = MIN(payload_len, len - 1);
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
 */
static int ph_sensor_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct ph_sensor_data *data = dev->data;

    if (chan != SENSOR_CHAN_ALL && chan != (enum sensor_channel)PH_SENSOR_CHAN_PH) {
        return -ENOTSUP;
    }

#if CONFIG_PH_SENSOR_MODE_CONTINUOUS
    /* Value already updated by acquisition thread; return cached value */
    LOG_DBG("pH: %.2f (from continuous buffer)", (double)data->ph_value);
    return 0;
#elif CONFIG_PH_SENSOR_MODE_ON_DEMAND
    uint8_t buf[20];
    int ret;

    ret = send_command(dev, "R");
    if (ret < 0) {
        LOG_ERR("Failed to send R command: %d", ret);
        return ret;
    }
    k_msleep(1000);

    ret = read_response(dev, buf, sizeof(buf));
    if (ret < 0) {
        LOG_ERR("Failed to read response: %d", ret);
        return ret;
    }

    data->ph_value = strtof((char *)buf, NULL);
    LOG_DBG("pH: %.2f (on-demand)", (double)data->ph_value);
    return 0;
#endif
}

static int ph_sensor_channel_get(const struct device *dev, enum sensor_channel chan,
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
 */
static int ph_sensor_attr_set(const struct device *dev, enum sensor_channel chan,
                              enum sensor_attribute attr, const struct sensor_value *val)
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
        k_sleep(K_MSEC(config->is_i2c ? 900 : 800));
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
    .channel_get = ph_sensor_channel_get,
    .attr_set = ph_sensor_attr_set,
};

#if CONFIG_PH_SENSOR_MODE_CONTINUOUS
K_THREAD_STACK_DEFINE(ph_acq_stack, CONFIG_PH_SENSOR_ACQ_THREAD_STACK_SIZE);

static void ph_sensor_acq_thread(void *arg1, void *arg2, void *arg3)
{
    const struct device *dev = (const struct device *)arg1;
    struct ph_sensor_data *data = dev->data;
    uint8_t response_buf[20];
    int ret;
    /* FIX: Added retries per acquisition to handle transient failures */
    const int max_retries = 3;

    LOG_INF("pH acquisition thread started");

    while (1) {
        int retries = max_retries;

        do {
            ret = send_command(dev, "R");
            if (ret < 0) {
                LOG_WRN("Failed to send R command: %d (retry %d)", ret, retries - 1);
                k_msleep(500);  /* FIX: Short delay before retry */
                continue;
            }

            k_msleep(1000);  /* Processing time */

            ret = read_response(dev, response_buf, sizeof(response_buf));
            if (ret < 0) {
                LOG_WRN("Failed to read response: %d (retry %d)", ret, retries - 1);
                k_msleep(500);
                continue;
            }

            /* FIX: Log the raw response for debugging static values */
            LOG_INF("Raw response: %s", response_buf);

            /* FIX: Parse with endptr check for valid float conversion */
            char *endptr;
            data->ph_value = strtof((char *)response_buf, &endptr);
            if (endptr == (char *)response_buf || *endptr != '\0') {
                LOG_ERR("Invalid pH response (not a float): %s", response_buf);
                ret = -EINVAL;  /* Force retry */
                continue;
            }

            LOG_DBG("Parsed pH: %.2f", (double)data->ph_value);
            break;  /* Success */
        } while (ret < 0 && --retries > 0);

        if (ret < 0) {
            LOG_ERR("Acquisition failed after retries; skipping signal");
        } else {
            /* FIX: Signal only on valid update */
            SIGNAL_DATA_READY();
#if CONFIG_PH_SENSOR_TRANSPORT_SERIAL
            LOG_INF("pH: %.2f", data->ph_value);
#endif
        }

        k_msleep(CONFIG_PH_SENSOR_MEASUREMENT_INTERVAL_MS);
    }
}
#endif /* CONFIG_PH_SENSOR_MODE_CONTINUOUS */

/* NEW: Runtime start function */
int ph_sensor_runtime_start(const struct device *dev)
{
    struct ph_sensor_data *data = dev->data;

    if (data->initialized) {
        return 0;
    }

    /* FIX: Removed "C,0" — unnecessary for I2C (causes syntax err) and disables continuous for UART. 
     * If UART and want continuous streaming, add conditional: if (!config->is_i2c) send_command(dev, "C,1");
     */

    /* FIX: Optional initial calibration check for debugging */
    uint8_t cal_buf[20];
    int ret = send_command(dev, "Cal?");
    if (ret == 0) {
        k_msleep(300);
        read_response(dev, cal_buf, sizeof(cal_buf));
        LOG_INF("Calibration status: %s", cal_buf);
    }

#if CONFIG_PH_SENSOR_MODE_CONTINUOUS
    k_thread_create(&data->acq_thread,
                    ph_acq_stack, K_THREAD_STACK_SIZEOF(ph_acq_stack),
                    ph_sensor_acq_thread,
                    (void *)dev, NULL, NULL,
                    CONFIG_PH_SENSOR_ACQ_THREAD_PRIORITY,
                    0, K_NO_WAIT);
    k_thread_name_set(&data->acq_thread, "ph_acq");
    LOG_INF("Continuous mode: acquisition thread started");
#elif CONFIG_PH_SENSOR_MODE_ON_DEMAND
    LOG_INF("On-demand mode: single-read polling via app");
#endif

    data->initialized = true;
    LOG_INF("pH sensor runtime activated");
    return 0;
}

static int ph_sensor_init(const struct device *dev)
{
    const struct ph_sensor_config *config = dev->config;
    struct ph_sensor_data *data = dev->data;

    if (config->is_i2c) {
        if (!device_is_ready(config->i2c.bus)) {
            LOG_ERR("I2C bus not ready");
            return -ENODEV;
        }
        LOG_INF("EZO-pH on I2C addr 0x%02x", config->i2c.addr);
    } else {
        if (!device_is_ready(config->uart)) {
            LOG_ERR("UART device not ready");
            return -ENODEV;
        }
        LOG_INF("EZO-pH on UART");
    }

    /* Lightweight init only */
    data->initialized = false;
    data->ph_value = 0.0f;  /* FIX: Already good */

#if CONFIG_PH_SENSOR_TRANSPORT_MATTER
    LOG_INF("Transport: Matter/CHIP");
#elif CONFIG_PH_SENSOR_TRANSPORT_SERIAL
    LOG_INF("Transport: Serial (debug)");
#endif

    return 0;
}

#define PH_SENSOR_DEFINE(inst) \
static struct ph_sensor_data ph_sensor_data_##inst; \
static const struct ph_sensor_config ph_sensor_config_##inst = { \
    .is_i2c = DT_INST_ON_BUS(inst, i2c), \
    COND_CODE_1(DT_INST_ON_BUS(inst, i2c), \
                ({ .i2c = I2C_DT_SPEC_INST_GET(inst), }), \
                ({ .uart = DEVICE_DT_GET(DT_INST_BUS(inst)), }) \
    ) \
}; \
DEVICE_DT_INST_DEFINE(inst, ph_sensor_init, NULL, \
                      &ph_sensor_data_##inst, \
                      &ph_sensor_config_##inst, \
                      POST_KERNEL, CONFIG_PH_SENSOR_INIT_PRIORITY, \
                      &ph_sensor_api);

DT_INST_FOREACH_STATUS_OKAY(PH_SENSOR_DEFINE)