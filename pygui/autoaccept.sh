#!/bin/sh
if [ "$2" == "badboy" ]; then
    echo "remove $1" > /tmp/frelayctl
    notify-send "Auto rejected offer" "$2: $3 ($4)"
else
    echo "accept $1" > /tmp/frelayctl
    notify-send "Auto accepted offer" "$2: $3 ($4)"
fi
