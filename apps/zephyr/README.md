# Zephyr Examples

These examples demonstrates how to use Blecon with Zephyr / nRF Connect SDK.

## Legacy UUIDv4 identities

To generate a legacy UUIDv4 on first boot with the internal modem, add this
setting to your application's `prj.conf`.

```conf
CONFIG_BLECON_IDENTITY_GENERATE_LEGACY_UUID_V4_ID=y
```

With empty identity storage, the modem uses the manufacturer-assigned identity
from OTP when present. Otherwise it generates and stores a UUIDv4 when this
option is enabled, or starts unprovisioned by default. Once an identity is
stored, changing the option preserves it.

## Compilation

Please refer to the Zephyr and NCS documentation. 
For instance, the hello world example can be compiled with:
```bash
west build -s blecon-device-sdk/apps/zephyr/hello-world/ -b nrf52840dk/nrf52840 -d build/zephyr/nrf52840dk/nrf52840/hello-world --pristine 
```
