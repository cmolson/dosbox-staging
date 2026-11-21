#!/usr/bin/env bash
# Record the screenshots.sh flow as demo/kiosk-demo.mp4.
set -u
rm -rf demo && mkdir demo

Xvfb :99 -screen 0 1920x1080x24 > /dev/null 2>&1 &
export DISPLAY=:99
sleep 2

ffmpeg -loglevel error -y -f x11grab -framerate 15 -video_size 1920x1080 -i :99 \
    -vf scale=1280:-2 -c:v libx264 -crf 23 -pix_fmt yuv420p demo/kiosk-demo.mp4 &
recorder=$!

/dosbox/dosbox --noprimaryconf \
    --set fullscreen=false --set window_size=1920x1080 --set window_position=0,0 \
    --set kiosk_idle_timeout=30 \
    > demo/dosbox.log 2>&1 &

sleep 8;  xdotool key Return
sleep 50
xdotool key Return
sleep 9;  xdotool key F12
sleep 7

kill -INT $recorder
sleep 2
pkill dosbox
