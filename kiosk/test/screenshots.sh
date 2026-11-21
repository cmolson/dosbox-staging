#!/usr/bin/env bash
# Launch the first game, let it idle until kicked, then test the F12 hotkey.
set -u
rm -rf shots && mkdir shots

Xvfb :99 -screen 0 1920x1080x24 > /dev/null 2>&1 &
export DISPLAY=:99
sleep 2

/dosbox/dosbox --noprimaryconf \
    --set fullscreen=false --set window_size=1920x1080 --set window_position=0,0 \
    --set kiosk_idle_timeout=30 \
    > shots/dosbox.log 2>&1 &

shot() { import -window root "shots/$1.png"; }

sleep 8;  shot 01-menu
xdotool key Return
sleep 10; shot 02-game
sleep 22; shot 03-warning
sleep 14; shot 04-menu-after-kick
xdotool key Return
sleep 10; xdotool key F12
sleep 8;  shot 05-menu-after-f12

pkill dosbox
grep KIOSK shots/dosbox.log
