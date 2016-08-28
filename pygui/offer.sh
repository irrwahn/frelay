#!/bin/sh

CMD_PIPE=/tmp/frelayctl
if [ -p $CMD_PIPE ] && [ $# -ge 2 ]; then
    DEST=$1
    shift
    echo offer $DEST '"'$PWD/$@'"' > $CMD_PIPE
else
    echo PEBKAC caused GURU MEDITATION
fi

