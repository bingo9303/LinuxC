#!/bin/sh

gcc v4l2_usr.c v4l2_uvc_rk3399.c sdl2_usr.c libjpeg_decode.c -L/usr/local/lib -I/usr/local/include -l:libjpeg.so -l:libSDL2.so -lpthread -o uvcCamera
