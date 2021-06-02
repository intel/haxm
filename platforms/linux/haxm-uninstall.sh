#!/usr/bin/env bash

DEVNAME=HAX
GRPNAME=haxm
MODNAME=haxm

# Remove udev rule
rm -f /lib/udev/rules.d/99-haxm.rules

# Remove group, if necessary
if [ $(getent group $GRPNAME) ]; then
    groupdel $GRPNAME
fi

# Remove from boot-time kernel module list
sed -i "/^${MODNAME}$/d" /etc/modules

# Unload kernel module
modprobe -r $MODNAME

echo 'HAXM successfully uninstalled'
