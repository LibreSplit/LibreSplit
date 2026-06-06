#!/bin/bash

export XDG_DATA_DIRS="$APPDIR/usr/share:${XDG_DATA_DIRS:-/usr/share}"

if [ $# -eq 0 ]; then
    exec "$APPDIR/usr/bin/libresplit"
else
    exec "$APPDIR/usr/bin/libresplit-ctl" "$@"
fi
