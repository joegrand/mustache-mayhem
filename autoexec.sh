#!/bin/sh

# Delay to make sure system is completely initialized
sleep 30

# Network setup (optional for development)
# Set a static MAC address for the BBB (default is to randomize)
ifconfig usb0 down
ifconfig usb0 hw ether 06:55:e9:d3:07:6e
ifconfig usb0 up

# Get IP address via local machine's shared internet connection 
# Timeout after trying 5 times for a lease
# (default is 192.168.7.2)
/sbin/udhcpc -i usb0 -t 5 -n
