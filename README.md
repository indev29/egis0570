# egis0570

This is the project focusing on creating Linux driver for Egis Technology Inc. ID 1c7a:0570 fingerprint scanner (also known as LighTuning Technology Inc.)

**Note, that this project is far from over and there is no way to setup fingerprint recognition right now.**

Repository Content
------------------

* `libfprint/`: <a href="https://www.freedesktop.org/wiki/Software/fprint/libfprint/">libfprint</a> fork for driver intergration
* `logs/`: usbpcap logs of Windows driver
* `pg/`: playground environment to test device communication

Current state
-------------

All general functions were ready, driver was succesfully integrated in libfprint 0.7, although there were some problems with matching algorythm due to the incorrect image size used.

Right now the proper image size and overall device logic are disclosed. Hopefully, it will be possible to adapt old code to newer version of libfprint and add device support to further fprint releases.
