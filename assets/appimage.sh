#!/bin/sh

set -e

export PATH="$APPDIR/bin:$PATH"

# Keep quick-sharun's runtime fixes when dispatching to either executable.
. "$APPDIR/AppRun.lib"
for hook in "$APPDIR"/bin/*.hook; do
    [ -f "$hook" ] || continue
    . "$hook"
done

if [ $# -eq 0 ]; then
    exec "$APPDIR/bin/libresplit"
else
    exec "$APPDIR/bin/libresplit-ctl" "$@"
fi
