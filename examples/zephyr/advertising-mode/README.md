<!--
Copyright (c) Blecon Ltd
SPDX-License-Identifier: Apache-2.0
-->

# Blecon Advertising Mode Example

This example demonstrates how to use the `blecon_set_advertising_mode()` API to control the advertising behavior and power consumption of a Blecon device.

## Overview

The Blecon SDK provides three advertising modes that allow you to balance power consumption against connection latency:

### High Performance Mode
- **Advertising Period**: 100ms constant
- **Power Consumption**: Highest
- **Use Case**: Applications requiring low-latency connectivity, powered devices

### Balanced Mode (Default)
- **Advertising Period**: Defaults to 1s; when requesting a connection advertises every 100ms (limited to 30s per 3 minute window - reverts to 1s period during the rest of the window)
- **Power Consumption**: Moderate
- **Use Case**: General-purpose applications, good compromise between power and performance

### Ultra Low Power Mode
- **Advertising Period**: Same as balanced, except that the device does not advertise when not initiating a connection
- **Power Consumption**: Lowest
- **Use Case**: Battery-powered devices, sensors that connect infrequently

## Building

Build the example for your board:

```bash
west build -b <board> examples/zephyr/advertising-mode
```

For example, for nRF52840 DK:
```bash
west build -b nrf52840dk_nrf52840 examples/zephyr/advertising-mode
```

## Flashing

```bash
west flash
```

## Usage

Once the device is running, you can interact with it using the shell:

### View Current Mode
```
blecon get_mode
```

### Change Advertising Mode
```
blecon set_mode high_performance
blecon set_mode balanced
blecon set_mode ultra_low_power
```

### Initiate Connection
```
blecon connection_initiate
```
