

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>
#include <drivers/sensor/ph_sensor.h>

// Declare the static functions from ph_sensor.c as extern.
extern int send_command(const struct device *dev, const char *cmd);
extern int read_response(const struct device *dev, uint8_t *buf, size_t len);

LOG_MODULE_REGISTER(main);

// List of common debug commands from datasheet (expand as needed)
static const char *command_list[] = {
    "R",              // Take single pH reading
    "i",              // Device information
    "Status",         // Reading device status (voltage, restart reason)
    "Cal,?",          // Calibration status
    "Cal,mid,7.00",   // Mid-point calibration (adjust value as needed)
    "Cal,low,4.00",   // Low-point calibration
    "Cal,high,10.00", // High-point calibration
    "Cal,clear",      // Clear calibration
    "Slope,?",        // Probe slope info
    "T,?",            // Temperature compensation value
    "L,?",            // LED state
    "Find",           // Blink LED to find device
    "Plock,?",        // Protocol lock status
    NULL              // End of list
};

int main(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ezo_ph));
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

    if (!device_is_ready(dev)) {
        printk("pH sensor not ready\n");
        return -ENODEV;
    }

    if (!device_is_ready(console)) {
        printk("Console UART not ready\n");
        return -ENODEV;
    }

    printk("pH Sensor Debug Menu (UART/I2C compatible)\n");
    printk("Select a command by number (or enter custom command directly).\n");
    printk("Commands from datasheet; wait times handled by driver.\n\n");

    // Print the menu
    for (int i = 0; command_list[i] != NULL; i++) {
        printk("%d: %s\n", i + 1, command_list[i]);
    }
    printk("0: Enter custom command\n\n");

    char input_buf[64];
    size_t input_idx = 0;
    uint8_t response_buf[64];

    while (1) {
        printk("Enter choice (0-%d) or custom: ", (int)(sizeof(command_list)/sizeof(command_list[0]) - 1));

        // Read input line
        input_idx = 0;
        while (input_idx < sizeof(input_buf) - 1) {
            unsigned char c;
            if (uart_poll_in(console, &c) == 0) {
                if (c == '\r' || c == '\n') {
                    input_buf[input_idx] = '\0';
                    break;
                }
                input_buf[input_idx++] = c;
            }
            k_sleep(K_MSEC(10));
        }

        if (input_idx == 0) continue;

        const char *cmd = NULL;
        int choice = atoi(input_buf);

        if (choice == 0) {
            // Custom command
            printk("Enter custom command: ");
            input_idx = 0;
            while (input_idx < sizeof(input_buf) - 1) {
                unsigned char c;
                if (uart_poll_in(console, &c) == 0) {
                    if (c == '\r' || c == '\n') {
                        input_buf[input_idx] = '\0';
                        break;
                    }
                    input_buf[input_idx++] = c;
                }
                k_sleep(K_MSEC(10));
            }
            cmd = input_buf;
        } else if (choice > 0 && choice <= (int)(sizeof(command_list)/sizeof(command_list[0]) - 1)) {
            cmd = command_list[choice - 1];
        } else {
            printk("Invalid choice.\n");
            continue;
        }

        if (cmd && strlen(cmd) > 0) {
            printk("Sending: %s\n", cmd);
            int ret = send_command(dev, cmd);
            if (ret < 0) {
                printk("Send failed: %d\n", ret);
                continue;
            }

            // Wait for processing (datasheet recommends up to 900ms; adjust if needed)
            k_sleep(K_MSEC(900));

            ret = read_response(dev, response_buf, sizeof(response_buf));
            if (ret < 0) {
                printk("Read failed: %d\n", ret);
            } else {
                printk("Response: %s\n", response_buf);
            }
        }
    }

    return 0;
}
