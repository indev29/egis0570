# egis0570

This is the project aiming to provide Linux support for Egis Technology Inc. ID 1c7a:0570 fingerprint scanner (also known as LighTuning Technology Inc.)

**It's <a href="https://gitlab.freedesktop.org/libfprint/libfprint/-/issues/162#note_544560">not possible</a> to perform fingerprint recognition right now. Follow <a href=https://gitlab.freedesktop.org/libfprint/libfprint/-/issues/162>this</a> issue to keep up with development progress.**

Repository Content
------------------

* `libfprint/`: <a href="https://www.freedesktop.org/wiki/Software/fprint/libfprint/">libfprint</a> fork for driver intergration located <a href="https://gitlab.freedesktop.org/indev29/libfprint">here</a>
* `logs/`: usbpcap logs of Windows driver
* `pg/`: playground environment to test device communication

History
-------------

**2018-02**

Driver succesfully integrated in libfprint 0.7, all general functions are ready. Recognition _doesn't_ work due to the libfprint matching alogirthm and (as it turned out after some time) incorrect image size used.

**2020-06**

Found proper image size (thanks to @saeedark), adapted old code to libfprint 1.90. Recognition _doesn't_ work due to the libfprint matching algorithm (see <a href="https://gitlab.freedesktop.org/libfprint/libfprint/-/issues/162#note_544560">explanation</a>). Further work is not possible before proper matching algorithm is implemented in libfprint.
