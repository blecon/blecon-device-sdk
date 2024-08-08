# Zephyr Examples

These examples demonstrates how to use Blecon with Zephyr / nRF Connect SDK.

## Compilation

Please refer to the Zephyr and NCS documentation. 
For instance, the hello world example can be compiled with:
```bash
west build -s blecon-device-sdk/examples/zephyr/hello-world/ -b nrf52840dk/nrf52840 -d build/zephyr/nrf52840dk/nrf52840/hello-world --pristine 
```