#!/bin/sh

gcc SDL2_jpeg_file_test.c -l:libjpeg.so -l:libSDL2.so -o SDL2_jpeg_file_test
gcc SDL2_jpeg_file_test_multiple.c -l:libjpeg.so -l:libSDL2.so -o SDL2_jpeg_file_test_multiple




