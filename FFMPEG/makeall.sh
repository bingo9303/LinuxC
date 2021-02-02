#!/bin/sh

gcc sdl2_display.c decode_mp4_rgb_sdl2.c -l:libjpeg.so -l:libSDL2.so -l:libavcodec.so -l:libavformat.so -l:libswscale.so -l:libavutil.so -o decode_mp4_rgb_sdl2


gcc sdl2_display.c decode_mp4_rgb_sdl2_multiple.c -l:libjpeg.so -l:libSDL2.so -l:libavcodec.so -l:libavformat.so -l:libswscale.so -l:libavutil.so -lpthread -o decode_mp4_rgb_sdl2_multiple

gcc sdl2_display.c decode_mp4_rgb_sdl2_pthread.c -l:libjpeg.so -l:libSDL2.so -l:libavcodec.so -l:libavformat.so -l:libswscale.so -l:libavutil.so -o decode_mp4_rgb_sdl2_pthread

