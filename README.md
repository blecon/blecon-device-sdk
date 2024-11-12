#   Blecon Device SDK

> **Note**
> This version of the SDK works with nRF Connect SDK v2.8.0-rc1.

##  Getting started

Please follow [the getting started guide in our docs](https://developer.blecon.net/getting-started/blecon-modem-mcu).

##  Board & Platform support

This release supports the following boards along with the NCS v2.8.0-rc1 SDK and Zephyr 3.8.0 (with Nordic patches). Our `west.yml` manifest pulls in the NCS v2.8.0-rc1 SDK.
We also support the legacy nRF5 SDK. We do recommend using NCS/Zephyr as the nRF5 SDK is deprecated.

| Board / Platform  | NCS/Zephyr    | nRF5 SDK    |
|-                  |-             |-             |
| nRF54L15 DK       | ✅            |              |
| nRF52840 DK       | ✅            | ✅            |
| nRF52833 DK       | ✅            | ✅            |
| nRF52840 Dongle   | ✅            |              |
| Nucleo L433RC-P   | ✅            |              |

##  Examples

Examples are available for supported platforms and boards in the [examples/](examples/) directory.

> **Note**
> The Memfault example is only available on NCS/Zephyr and the nRF54L15 DK, nRF52840 DK and nRF52840 Dongle boards.


##  License
* Source code included within this repository is licensed under the [Apache 2.0 license](LICENSE.md) (SPDX: Apache-2.0)
* Pre-compiled libraries (`lib/`) included within this repository are licensed under the [Blecon Library Binary License](lib/LICENSE.md)
