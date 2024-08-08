#   Blecon Device SDK (Preview)

> **Note**
> This is a preview release of the Blecon SDK. Please give us feedback! You can use GitHub issues and discussions within this repository, or email us.

##  Getting started

Please follow [the getting started guide in our docs](https://developer.blecon.net/getting-started/blecon-modem-mcu).

##  Board & Platform support

This release supports the following boards along with the NCS v2.7.0 SDK and Zephyr 3.6.0 (with Nordic patches). Our `west.yml` manifest pulls in the NCS v2.7.0 SDK.
We also support the legacy nRF5 SDK. We do recommend using NCS/Zephyr as the nRF5 SDK is deprecated.

| Board / Platform  | NCS/Zephyr    | nRF5 SDK    |
|-                  |-             |-             |
| NRF52840 DK       | ✅            | ✅            |
| NRF52833 DK       | ✅            | ✅            |
| NRF52840 Dongle   | ✅            |              |
| Nucleo L433RC-P   | ✅            |              |

##  Examples

Examples are available for supported platforms and boards in the [examples/](examples/) directory.

##  License
* Source code included within this repository is licensed under the [Apache 2.0 license](LICENSE.md) (SPDX: Apache-2.0)
* Pre-compiled libraries (`lib/`) included within this repository are licensed under the [Blecon Library Binary License](lib/LICENSE.md)
