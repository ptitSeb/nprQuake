#!/bin/bash

LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH LIBGL_FB=1 ./glquake.sdl -width 800 -height 480 $@ 
