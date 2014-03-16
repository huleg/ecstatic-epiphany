#!/bin/sh

ffmpeg -f image2 -i output/frame-%04d.jpeg -i output/frame-%04d-memory.png -filter_complex "
    nullsrc=size=1440x480 [background];
    [0:v] setpts=PTS-STARTPTS, format=rgb24, scale=720x480 [left];
    [1:v] setpts=PTS-STARTPTS, format=rgb24, scale=720x480 [right];
    [background][left]       overlay=shortest=1       [background+left];
    [background+left][right] overlay=shortest=1:x=720
" output/imprint-debug.mp4
