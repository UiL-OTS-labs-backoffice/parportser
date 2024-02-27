#!/bin/sh
module="parportser"
device="parportser"
mode="664"

# if the target tty isn't /dev/ttyS0, pass it as the first argument to this script
param=""
if [ -n "$1" ]; then
   param="default_tty=$1"
fi

insmod ./$module.ko $param || exit 1

# retrieve major number
major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)

rm -f /dev/parport0
mknod /dev/parport0 c $major 0
chmod 777 /dev/parport0
