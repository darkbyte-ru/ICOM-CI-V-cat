# ICOM CI-V

Icom Communication Interface - V. This is a system, which provide advanced remote control LAN for multiply transceivers. More details can be found in attached [icom-civ.pdf](icom-civ.pdf) document. Also a bit info in russian can be found at my [blog post](https://darkbyte.ru/2016/92/arduino-cat-ci-v-interface-for-icom-ic-820/).

# IC-820

ICOM IC-820 transceiver has a poor support of remote control. In general there is only ability to get current frequency and modulation type of main band and change that.

# icom-ic820-cat.ino

In current example code we assume that only one transceiver connected to line via 10+11 pins of arduino board. At the begin code try to detect radio baudrate and get it bus address. On succesfull, it start to monitor bus for current frequency and operation mode, and print it to Serial.
