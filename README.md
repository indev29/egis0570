# egis0570

This is the project focusing on creating Linux driver for Egis Technology Inc. ID 1c7a:0570 fingerprint scanner (also known as LighTuning Technology Inc.)

Repository Content
------------------

* `libfprint/`: <a href="https://www.freedesktop.org/wiki/Software/fprint/libfprint/">libfprint</a> repository with driver integrated
* `logs/`: usbpcap logs of Windows driver
* `scans/`: fingerprint scans

Files in repository root contain some source code for playground purposes.

Work progress
-------------

1. (DONE) Catching Windows driver output
2. (DONE) Understanding exchange protocol. Writing test Linux driver using *libusb*
3. (DONE) Writing and integrating *libfprint* driver
4. (WIP)  General testing and verification check
5. Adjusting threshold and cleaning repository
6. Finish
