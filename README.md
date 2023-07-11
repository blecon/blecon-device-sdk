#   Blecon Device SDK (Preview)

> **Note**
> This is a preview release of the Blecon SDK. Please give us feedback! You can use GitHub issues and discussions within this repository, or email us.

##  Getting started

Please follow [the getting started guide in our docs](https://blecon.net//device-sdk-getting-started).

##  Board & Platform support

This inital release supports the NRF52840 DK board along with the NCS v2.4.0 SDK. Our `west.yml` manifest pulls in a Zephyr fork with two extra patches. If you're using the NCS-provided version of Zephyr, please cherry-pick the following patches:
* https://github.com/blecon/zephyr/commit/04d59f3554d35290d2d6eac0e1d2e4e3564b525f
* https://github.com/blecon/zephyr/commit/518a558f67e55afca551e90a69d4fddf6a7b1afb

| Board / Platform  | NCS       |
|-                  |-          |
| NRF52840 DK       | ✅        |

### Limitations
* NFC is not supported yet by the NCS port

##  License
* Source code included within this repository is licensed under the [Apache 2.0 license](LICENSE.md) (SPDX: Apache-2.0)
* Pre-compiled libraries (`lib/`) included within this repository are licensed under the [Blecon Library Binary License](lib/LICENSE.md)