#!/bin/sh
echo "accept $1" > /tmp/frelayctl
notify-send "Auto accepted offer" "$2: $3 ($4)"
