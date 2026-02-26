# Atlas EZO-pH Zephyr Module for Nordic Boards

> A Zephyr/NCS off-tree module providing full support for the Atlas Scientific EZO-pH sensor on Nordic boards (nRF5340 DK, nRF52840, etc.), with UART/I2C interfaces, calibration, and Zephyr sensor API integration.

---

## Table of Contents

- [Features](#features)
- [Hardware](#hardware)
- [Driver Overview](#driver-overview)
- [Setup Instructions](#setup-instructions)
- [Device Tree Configuration](#device-tree-configuration)
- [Related Repositories](#related-repositories)
- [Extra Documentation](#extra-documentation)

---

## Features

- **Dual interface support** — UART (default) or I2C, selected via Device Tree property
- **Zephyr sensor API integration** — standard `sensor_sample_fetch` / `sensor_channel_get` polling
- **Calibration controls** — low, mid, and high-point calibration via `sensor_attr_set`
- **Minimal read demo** — sends `R` command, polls ASCII float response, logs parsed pH value
- **Mode switching** — UART to I2C switchable via attribute
- **Wi-Fi + BLE ready** — extendable for streaming pH data via nRF7002 shield
- **Flexible pin configuration** — compatible with off-tree NCS integration

---

## Hardware

| Image | Description |
|-------|-------------|
| *(see `/images`)* | nRF7002 EK Shield next to its box |
| *(see `/images`)* | nRF5340 DK beside its box |
| *(see `/images`)* | Physical scale reference with ruler |
| *(see `/images`)* | Backside showing custom header for access |
| *(see `/images`)* | Wiring with shield mounted on top |

> Replace `images/...` entries with your actual image file paths.

---

## Driver Overview

The included pH sensor driver demonstrates real-time pH readings from the Atlas EZO-pH via Zephyr's sensor subsystem.

### Device Initialization

- Detects interface from DT property (`"uart"` or `"i2c"`)
- Configures I2C spec or UART device pointer accordingly

### Sensor Control

Calibration and mode switching are handled via `sensor_attr_set`:

| Attribute | Example Command |
|-----------|----------------|
| Calibration (mid) | `Cal,mid,7.00` |
| Calibration (low) | `Cal,low,4.00` |
| Calibration (high) | `Cal,high,10.00` |
| Mode switch to I2C | `I2C,` + `Plock,1` |

### Reading & Fetch

- `sample_fetch` — sends `R`, waits 600 ms, reads and parses ASCII response to float
- `channel_get` — converts parsed float to `struct sensor_value`

### Command Handling

- Abstracted for both interfaces: UART (`poll_out` / `poll_in`) and I2C (`write` / `read`)
- 1-second read timeout

### Performance Monitoring

- Debug and info logging via `LOG_INF` / `LOG_DBG`

### BLE / Wi-Fi Integration

- Extendable to send pH values in BLE notifications or Wi-Fi packets via the nRF7002 shield

---

## Setup Instructions

### 1. Add the module to your `west.yml`

```yaml
- name: atlas_ezo_ph
  remote: atlas_ezo_ph
  revision: main
  path: modules/ph_sensor
```

### 2. Update your workspace

```bash
west update
```

### 3. Build the sample

```bash
west build -b nrf5340dk_nrf5340_cpuapp_ns modules/ph_sensor/sample
```

### 4. Flash to the board

```bash
west flash
```

---

## Device Tree Configuration

### UART (default)

```dts
&uart1 {
    status = "okay";
    current-speed = <9600>;
    pinctrl-0 = <&uart1_default>;
    pinctrl-1 = <&uart1_sleep>;
    pinctrl-names = "default", "sleep";

    ezo_ph: ezo-ph {
        compatible = "atlas,ezo-ph";
        interface = "uart";
        label = "EZO_PH";
    };
};
```

### I2C

```dts
&i2c1 {
    status = "okay";
    pinctrl-0 = <&i2c1_default>;
    pinctrl-1 = <&i2c1_sleep>;
    pinctrl-names = "default", "sleep";
    clock-frequency = <I2C_BITRATE_STANDARD>;

    ezo_ph: ezo-ph@63 {
        compatible = "atlas,ezo-ph";
        interface = "i2c";
        reg = <0x63>;
        label = "EZO_PH";
    };
};
```

### Pin Control

```dts
&pinctrl {
    uart1_default: uart1_default {
        group1 {
            psels = <NRF_PSEL(UART_TX, 0, XX)>,
                    <NRF_PSEL(UART_RX, 0, XX)>;
        };
    };

    uart1_sleep: uart1_sleep {
        group1 {
            psels = <NRF_PSEL(UART_TX, 0, XX)>,
                    <NRF_PSEL(UART_RX, 0, XX)>;
            low-power-enable;
        };
    };

    i2c1_default: i2c1_default {
        group1 {
            psels = <NRF_PSEL(TWIM_SCL, 0, XX)>,
                    <NRF_PSEL(TWIM_SDA, 0, XX)>;
        };
    };

    i2c1_sleep: i2c1_sleep {
        group1 {
            psels = <NRF_PSEL(TWIM_SCL, 0, XX)>,
                    <NRF_PSEL(TWIM_SDA, 0, XX)>;
            low-power-enable;
        };
    };
};
```

> **Note:** Replace `XX` with your actual GPIO pin numbers.

---

## Related Repositories

| Repository | Description |
|------------|-------------|
| [devacademy-ncsinter](https://github.com/) | Course exercises and solutions |
| [zephyrproject-rtos/zephyr](https://github.com/zephyrproject-rtos/zephyr) | Zephyr RTOS with sensor API |
| [nrfconnect/sdk-nrf](https://github.com/nrfconnect/sdk-nrf) | Official Nordic SDK repository |

---

## Extra Documentation

- **[Installation Guide (Off-Tree)](docs/installation.md)** — Step-by-step, beginner-friendly instructions for integrating this module off-tree
- **[Porting Notes & Pitfalls](docs/porting_notes.md)** — Key logic decisions and gotchas encountered during off-tree porting

---

## License

*Add your license information here.*

