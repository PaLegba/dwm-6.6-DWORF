#!/bin/sh

THEME="leda"

Xephyr :1 -screen 1280x1024 &

sleep 1

DISPLAY=:1 xset s 120 120 +dpms

# DISPLAY=:1 xrdb -merge ~/.Xresources
# DISPLAY=:1 xrdb -merge ~/.Xresources.xterm

DISPLAY=:1 feh --bg-tile /usr/share/dwm/bg/bge.jpg &

# DISPLAY=:1 st -S ${THEME} -R on -e /bin/bash & 
# DISPLAY=:1 st -S ${THEME} -R on -e htop & 
# DISPLAY=:1 st -S ${THEME} -R on -e asciiclock & 


DISPLAY=:1 ./dwm-6.6 -S ${THEME} &

DISPLAY=:1 st -S ${THEME} -R on
