#!/usr/bin/env bash

DEVNAME=HAX
GRPNAME=haxm
MODNAME=haxm

# Create group, if necessary
groupadd -f $GRPNAME

# Create udev rule
echo "KERNEL==\"${DEVNAME}\", GROUP=\"${GRPNAME}\", MODE=\"0660\"" \
    > /lib/udev/rules.d/99-haxm.rules

# Load kernel module
depmod -a
modprobe $MODNAME

# Add to boot-time kernel module list, only once
sed -i "/^${MODNAME}$/d" /etc/modules
echo $MODNAME >> /etc/modules

echo 'HAXM successfully installed'
