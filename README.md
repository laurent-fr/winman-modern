Winman, a minimal Window Manager from O'Reilly Xlib Programming Manual (Vol 1.)

see chap. 16, p520-571 or http://www.sbin.org/doc/Xlib/chapt_16.html

Compile on modern Linux system.
Unusable as-is. Intended to understand how a window manager actually works.

Compile :
---------

apt-get install libbsd-dev libx11-dev

make

Run :
-----

apt-get install xnest xterm

Xnest :1

export DISPLAY=:1

./winman


TODO :
------

* Some bugs in the base code.
* No ghost window during move or resize
* Iconify hangs winman with some apps

