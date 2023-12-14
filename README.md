# blues-bilge-alarm

Bilge Alarm using Blues Notecard Cell + WiFi. Check out the write up on [Hackster.io](https://www.hackster.io/bucknalla/cellular-bilge-alarm-monitoring-with-blues-wireless-fd1093).

## Setup

You'll need a working [Zephyr environment](https://docs.zephyrproject.org/latest/develop/getting_started/index.html). This project uses the [west](https://docs.zephyrproject.org/latest/guides/west/index.html) tool to setup the environment.

```bash
# initialize my-workspace for the example-application (main branch)
west init -m https://github.com/bucknalla/blues-bilge-alarm --mr main my-workspace
# update Zephyr modules
cd my-workspace
west update
```

To build and flash the project:

```bash
# build the project
west build -b swan_r5 blues-bilge-alarm/app
# flash the project
west flash
```

> Note: In order to flash the Swan board using `west flash`, you'll need to use an external programmer. For example the [ST-LINK/V3](https://shop.blues.com/collections/accessories) programmer.
