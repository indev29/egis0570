# egis0570 (Closed)

This is the project focusing on creating Linux driver for Egis Technology Inc. ID 1c7a:0570 fingerprint scanner (also known as LighTuning Technology Inc.)

**All work is stopped, see `Current state` for more information.**

Repository Content
------------------

* `libfprint/`: <a href="https://www.freedesktop.org/wiki/Software/fprint/libfprint/">libfprint</a> repository with driver integrated
* `logs/`: usbpcap logs of Windows driver
* `scans/`: fingerprint scans

Files in repository root contain some source code for playground purposes.

Current state
-------------

All general functions are ready, driver is succesfully integrated in libfprint, but there are some problems with matching algorythm.
<br/>
Seems like matching algorythm used by libfprint can't work with data from this device. The scan area is too small to detect enough minutiae.
<br/>
It is possible to try to work with it as a swipe-type sensor, but there is no guarantee and I currentry unable to do this work.

If someone wants to try finishing my work, feel free to do it. Contact me if you have any questions: indev12@gmail.com
<br/>
This thread from libfprint mailing list may help you a little: https://lists.freedesktop.org/archives/fprint/2018-February/000983.html
