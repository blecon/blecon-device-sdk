<!--
Copyright (c) Blecon Ltd
SPDX-License-Identifier: Apache-2.0
-->

# Bluetooth coexistence example

This example demonstrates how Blecon can be integrated into an application with existing Bluetooth device functionality.

## Strategy

As the Bluetooth resources are shared among Blecon and the application, the following strategy is used:
* Both Blecon and the application use their own Bluetooth identity
* Different advertising sets are created using the extended advertising API
* When a connection is made, the application and Blecon check which underlying Bluetooth identity was used to establish the connection, and ignore connections not targeting their own identity

This means that advertising and connection stages can be handled independently by Blecon and the application, and no explicit coordination is required.

The following restrictions apply:
* The extended advertising API must be used by the application
* The application must check the underlying Bluetooth identity used upon new connections, and ignore any connection not targeting the application's Bluetooth identity
* The Zephyr Bluetooth stack needs to be configured to support the number of connections, identities and advertising sets required by both the application and Blecon (see `coexistence.conf` for an example)

Notes:
* The default Bluetooth identity (`BT_ID_DEFAULT`) is not used by Blecon and is therefore available for the application to use
* Blecon doesn't leverage pairing/bonding or the filter access list; these features can be used by the application

## Example

This example combine the Blecon hello world with the Zephyr heartrate sensor examples. Minimal tweaks were made to accomodate the requirements listed above (configuration, use of the extended advertising API and checking the Bluetooth identity for new connections).
