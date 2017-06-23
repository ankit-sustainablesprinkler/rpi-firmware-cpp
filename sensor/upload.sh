#!/bin/bash

echo 12 > /sys/class/gpio/unexport
echo 11 > /sys/class/gpio/unexport
echo 10 > /sys/class/gpio/unexport
echo 9 > /sys/class/gpio/unexport

avrdude -p atmega328p -C /home/pi/avrdude_gpio.conf -c flow_sensor -v -U flash:w:flow_sensor.ino.hex:i
sleep 1
#sleep 30
rmmod spi_bcm2835
modprobe spi_bcm2835
