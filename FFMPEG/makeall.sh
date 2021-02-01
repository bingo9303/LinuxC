#!/bin/sh

gcc sdl2_display.c decode_mp4_rgb_sdl2.c -l:libjpeg.so -l:libSDL2.so -l:libavcodec.so -l:libavformat.so -l:libswscale.so -o decode_mp4_rgb_sdl2



