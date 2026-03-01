#!/bin/bash
# update_pd.sh - Update Pure Data sources in the ESPdLib Arduino library
#
# Usage: ./update_pd.sh /path/to/new/pd-x.xx-x
#
# This copies .c and .h files from a Pd source tree into the library,
# renaming .c → .inc so Arduino IDE won't compile them individually.
# Each .inc file has a corresponding pd_*.c wrapper in pure_data/ that
# #includes pd_build_defines.h and then the .inc file.
#
# After running, check if the libpd Makefile in the new version has
# added or removed source files, and update the wrapper list accordingly.

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 /path/to/pd-source-directory"
    echo "Example: $0 ../../pd-0.57-0"
    exit 1
fi

PD_SRC="$1/src"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEST="$SCRIPT_DIR/../src/pure_data/src"

if [ ! -d "$PD_SRC" ]; then
    echo "Error: $PD_SRC not found"
    exit 1
fi

echo "Updating Pd sources from: $PD_SRC"
echo "Destination: $DEST"

# Remove old source files
rm -f "$DEST"/*.inc "$DEST"/*.h

# Copy headers as-is
cp "$PD_SRC"/*.h "$DEST/"
NHEADERS=$(ls "$DEST"/*.h 2>/dev/null | wc -l)
echo "Copied $NHEADERS header files"

# Copy .c files as .inc
for f in "$PD_SRC"/*.c; do
    base=$(basename "$f" .c)
    cp "$f" "$DEST/${base}.inc"
done
NSOURCES=$(ls "$DEST"/*.inc 2>/dev/null | wc -l)
echo "Copied $NSOURCES source files (as .inc)"

echo ""
echo "Done! Check the libpd Makefile in the new Pd version for any"
echo "added/removed source files and update the pd_*.c wrappers in"
echo "src/pure_data/ accordingly."
