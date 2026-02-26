
Atlas EZO-pH Zephyr Module for Nordic Boards
This Zephyr/NCS module provides support for the Atlas Scientific EZO-pH sensor on Nordic boards (e.g., nRF5340 DK, nRF52840, etc.), including UART/I2C interfaces, calibration, and integration with Zephyr's sensor API. It is designed to work with the nRF7002 Wi-Fi/BLE shield or standalone, for applications like environmental monitoring.

Features
Dual interface support: UART (default) or I2C (selected via DT property)
Zephyr sensor API integration for polling (sensor_sample_fetch / channel_get)
Calibration controls: Low/mid/high points via attributes
Minimal read demo that:
Sends "R" command for pH reading
Polls response (ASCII float)
Logs parsed pH value
Supports mode switching (UART to I2C via attribute)
Compatible with Wi-Fi + BLE telemetry (extendable for streaming pH data)
Flexible UART/I²C/pin configuration
Compatible with off-tree NCS integration
Hardware Images
Image	Description
Failed to load image

View link
nRF7002 EK Shield next to its box
Failed to load image

View link
nRF5340 DK beside its box
Failed to load image

View link
Physical scale reference with ruler
Failed to load image

View link
Backside showing custom header for access
Failed to load image

View link
Wiring with shield on top
(Replace images/... with your actual image file paths.)	
Sample Driver Summary
The included pH sensor driver demonstrates real-time pH readings from the Atlas EZO-pH via Zephyr's sensor subsystem:

Device Initialization
Detects interface from DT property ("uart" or "i2c")
Configures I2C spec or UART device pointer
Sensor Control
Attributes for calibration (sensor_attr_set): Commands like "Cal,mid,7.00"
Mode switch attribute: "I2C,<addr>" + "Plock,1"
Unified send_command/read_response for both interfaces
Reading & Fetch
sample_fetch: Sends "R", waits 600ms, reads/parses ASCII to float
channel_get: Converts to struct sensor_value
Command Handling
Abstracted for UART (poll_out/in) or I2C (write/read)
Timeout (1s) for reads
Performance Monitoring
Logs via LOG_INF / LOG_DBG
Integration
Extendable for BLE/Wi-Fi: Send pH in packets
This driver serves as a reference implementation for pH sensing over UART/I2C in Zephyr on Nordic boards.
Setup Instructions
Add the module to your workspace via west.yml:
- name: atlas_ezo_ph
  remote: atlas_ezo_ph
  revision: main
  path: modules/ph_sensor
Update your workspace:
west update
Build the sample:
west build -b nrf5340dk_nrf5340_cpuapp_ns modules/ph_sensor/sample
Flash to the board:
west flash
Device Tree / Peripheral Configuration
// For UART (default)
&uart1 { status = "okay"; current-speed = <9600>; pinctrl-0 = <&uart1_default>; pinctrl-1 = <&uart1_sleep>; pinctrl-names = "default", "sleep"; 
  ezo_ph: ezo-ph { compatible = "atlas,ezo-ph"; interface = "uart"; label = "EZO_PH"; };
};

// For I2C
&i2c1 { status = "okay"; pinctrl-0 = <&i2c1_default>; pinctrl-1 = <&i2c1_sleep>; pinctrl-names = "default", "sleep"; clock-frequency = <I2C_BITRATE_STANDARD>;
  ezo_ph: ezo-ph@63 { compatible = "atlas,ezo-ph"; interface = "i2c"; reg = <0x63>; label = "EZO_PH"; };
};

&pinctrl {
  uart1_default: uart1_default { group1 { psels = <NRF_PSEL(UART_TX,0,XX)>, <NRF_PSEL(UART_RX,0,XX)>; }; };
  uart1_sleep: uart1_sleep { group1 { psels = <NRF_PSEL(UART_TX,0,XX)>, <NRF_PSEL(UART_RX,0,XX)>; low-power-enable; }; };
  i2c1_default: i2c1_default { group1 { psels = <NRF_PSEL(TWIM_SCL,0,XX)>, <NRF_PSEL(TWIM_SDA,0,XX)>; }; };
  i2c1_sleep: i2c1_sleep { group1 { psels = <NRF_PSEL(TWIM_SCL,0,XX)>, <NRF_PSEL(TWIM_SDA,0,XX)>; low-power-enable; }; };
};
Related Repositories
devacademy-ncsinter – Course exercises and solutions
zephyrproject-rtos/zephyr – Zephyr RTOS with sensor API
nrfconnect/sdk-nrf – Official Nordic SDK repository
Extra Documentation
Installation Guide (Off-Tree) – Step-by-step, beginner-friendly instructions
Porting Notes & Pitfalls – Key logic decisions and gotchas during off-tree porting
