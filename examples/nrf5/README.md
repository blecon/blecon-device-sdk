# nRF5 SDK Example

This example demonstrates how to use Blecon with the legacy nRF5 SDK.

## Compilation

The example requires CMake and the Arm GNU toolchain to be compiled.

Configure CMake from the SDK's root directory:
```bash
$ mkdir build && cd build
$ cmake --fresh -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBLECON_NRF5_PORT=ON -DBLECON_NRF5_BOARD=NRF52840_DK ..
```

The example uses the S140 Softdevice which needs to be flashed as well. It is downloaded as part of the build process and located in `examples/nrf5/nrf5-sdk/nrf5-sdk/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex` within your build directory.

## nRF5 SDK

During the compilation process, the nRF5 SDK (v17.1.0) is downloaded to `examples/nrf5/nrf5-sdk/nrf5-sdk/` within your build directory and automatically patched.

If you use your own version of the SDK, you must patch it with `nrf5-sdk/nrf5-sdk_v17.1.0.patch` to fix a bug in the Oberon crypto backend.

## Integration within your own app

A couple of notes when integrating the Blecon SDK with your nRF5 SDK app:
* The `.blecon_config` section must be located in a flash sector within your linker script
* The nRF5 event loop port uses the `app_scheduler` and `app_timer` modules under the hood
* Various configuration parameters must be enabled in your sdk_config.h, see below

### Minimal SDK config
```c
#define NRF_BLE_CONN_PARAMS_ENABLED 1
#define NRF_CRYPTO_BACKEND_NRF_HW_RNG_ENABLED 1
#define NRF_CRYPTO_BACKEND_NRF_HW_RNG_MBEDTLS_CTR_DRBG_ENABLED 0
#define NRF_CRYPTO_RNG_STATIC_MEMORY_BUFFERS_ENABLED 1
#define NRF_CRYPTO_RNG_AUTO_INIT_ENABLED 1
#define NRF_CRYPTO_BACKEND_OBERON_ENABLED 1
#define NRF_CRYPTO_BACKEND_OBERON_CHACHA_POLY_ENABLED 1
#define NRF_CRYPTO_BACKEND_OBERON_ECC_CURVE25519_ENABLED 1
#define NRF_CRYPTO_BACKEND_OBERON_HASH_SHA256_ENABLED 1
#define NRF_CRYPTO_BACKEND_OBERON_HMAC_SHA256_ENABLED 1
#define APP_SCHEDULER_ENABLED 1
#define APP_TIMER_ENABLED 1
#define APP_TIMER_CONFIG_USE_SCHEDULER 1
```

Additionally, if using NFC:
```c
#define NFC_NDEF_MSG_ENABLED 1
#define NFC_NDEF_RECORD_ENABLED 1
#define NFC_NDEF_URI_MSG_ENABLED 1
#define NFC_NDEF_URI_REC_ENABLED 1
#define NFC_PLATFORM_ENABLED 1
```
