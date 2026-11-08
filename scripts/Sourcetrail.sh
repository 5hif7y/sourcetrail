#!/bin/sh

# Get the current directory in canonical form (resolve all the symlinks, if present)
SOURCETRAIL_PATH="$(dirname "$(readlink -f "$0")")"

# Libraries are linked statically so no need for LD_LIBRARY_PATH.
#export LD_LIBRARY_PATH="$SOURCETRAIL_PATH/lib:$LD_LIBRARY_PATH"

export QT_XKB_CONFIG_ROOT="/usr/share/X11/xkb:$QT_XKB_CONFIG_ROOT"

# Qt platform plugins are also statically linked, so no need for that either.
#export QT_QPA_PLATFORM_PLUGIN_PATH="$SOURCETRAIL_PATH/plugins/platforms"

export QT_QPA_FONTDIR="$SOURCETRAIL_PATH/data/fonts:$QT_QPA_FONTDIR"
export SOURCETRAIL_VIA_SCRIPT=1
export OPENSSL_CONF=/etc/ssl/

exec "$SOURCETRAIL_PATH/Sourcetrail" "$@"
