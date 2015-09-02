# ip-notify
A simple C application that executes a script every time an ip is added to the system or deleted.

This uses netlink to wait for changes.
Since netlink is linux specific this will only work on linux bases systems.
