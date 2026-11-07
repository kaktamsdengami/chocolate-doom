#!/bin/sh
#
# Copyright(C) 2026 PLAYER TWO
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
#
# Regression test for f-kb1-real-1: a medikit picked up below 25 health
# must show GOTMEDINEED, at 25 or more it must show GOTMEDIKIT.
#
# Usage: qa/test_medikit_message.sh <chocolate-doom binary> <freedoom1.wad>
# Needs a display; run under xvfb-run on a headless machine.

set -e

DOOM=$1
IWAD=$2
NEED="Took a large health refill. Never give up!"
NORMAL="Took a large health refill."
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
FAIL=0

check() {
    health=$1
    expected=$2
    cat > "$TMP/script.txt" <<END
2 health $health
2 warp 60 480 180
3 move 10 0 0 6
20 quit
END
    SDL_AUDIODRIVER=dummy "$DOOM" -iwad "$IWAD" -warp 1 1 -skill 3 \
        -nosound -nomouse -window -nograbmouse \
        -qa-script "$TMP/script.txt" -qa-log "$TMP/log.jsonl" \
        > "$TMP/out.txt" 2>&1
    if grep '"type":"pickup"' "$TMP/log.jsonl" \
        | grep -q "\"sprite\":\"MEDI\",\"health_before\":$health,.*\"message\":\"$expected\""; then
        echo "PASS: health $health -> '$expected'"
    else
        echo "FAIL: health $health, expected '$expected', got:"
        grep '"type":"pickup"' "$TMP/log.jsonl" || echo "  (no pickup event)"
        FAIL=1
    fi
}

check 20 "$NEED"
check 24 "$NEED"
check 25 "$NORMAL"
check 60 "$NORMAL"

exit $FAIL
