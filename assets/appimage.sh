#!/bin/bash

export XDG_DATA_DIRS="$APPDIR/usr/share:${XDG_DATA_DIRS:-/usr/share}"
export GTK_EXE_PREFIX="$APPDIR/usr"
export GTK_PATH="$APPDIR/usr/lib/gtk-4.0"

if [ $# -eq 0 ]; then
    exec "$APPDIR/usr/bin/libresplit"
else
    exec "$APPDIR/usr/bin/libresplit-ctl" "$@"
fi
