#!/bin/bash
# Render the flight matrix: station pairs captured mid-flight at fixed
# moments, on a fixed 60Hz timestep and a pinned clock — the same
# flight always lands on the same pixels. The regression net for the
# transition refactor.
#
# THE DATE IS PINNED TOO, and must be. This used to fix only the time
# of day (TEMPUS_DAYPCT), which left the DATE floating at whatever day
# the run happened on: moon phase, planet seats, the wheel's pointer
# and the date text all moved, so two runs taken on different days came
# back 100% drift on every shot. That is indistinguishable from a real
# regression, which means the net silently stopped catching anything
# the moment a baseline aged a day. Pinning the year position makes a
# baseline comparable forever.
#
# Usage:
#   scripts/flight_matrix.sh [outdir]     # curated pairs (~5 min)
#   FULL=1 scripts/flight_matrix.sh       # every ordered pair (long)
#   T="1.0 2.0" scripts/flight_matrix.sh  # custom capture moments
#   YEARPCT=0.5 scripts/flight_matrix.sh  # a different pinned date
#
# Compare two runs with scripts/flight_diff.py. Baselines are only
# comparable when rendered with the same YEARPCT and DAYPCT.
set -e
cd "$(dirname "$0")/.."

OUT=${1:-flightmatrix/$(git rev-parse --short HEAD)}
mkdir -p "$OUT"
T=${T:-"0.9 1.75 2.6 3.8"}
# ~mid-May: clear of both the yule seam and the leap-day boundary, so
# the pinned date lands on the same calendar day in common and leap
# years alike.
YEARPCT=${YEARPCT:-0.37}
DAYPCT=${DAYPCT:-0.40}

# Curated: the tour route both directions, plus the frame-inversion
# crossings (sky/orbis/draco) that carry the hardest morphs.
PAIRS="HOROLOGIVM,HORAE HORAE,HOROLOGIVM HOROLOGIVM,ORBIS ORBIS,HOROLOGIVM
ORBIS,TELLVS TELLVS,ORBIS TELLVS,MACHINA MACHINA,TELLVS
MACHINA,CAELVM CAELVM,MACHINA CAELVM,DRACO DRACO,CAELVM
DRACO,HOROLOGIVM HOROLOGIVM,DRACO ORBIS,HORAE HORAE,ORBIS
ORBIS,CAELVM CAELVM,ORBIS ORBIS,DRACO DRACO,ORBIS
HOROLOGIVM,MACHINA MACHINA,HOROLOGIVM HOROLOGIVM,CAELVM CAELVM,HOROLOGIVM
HOROLOGIVM,OFFICIVM OFFICIVM,HOROLOGIVM MACHINA,DRACO DRACO,MACHINA"

if [ -n "$FULL" ]; then
    S="HOROLOGIVM HORAE ROTAE SAECVLVM TELLVS MACHINA CAELVM ORBIS OFFICIVM DRACO"
    PAIRS=""
    for a in $S; do for b in $S; do
        [ "$a" = "$b" ] || PAIRS="$PAIRS $a,$b"
    done; done
fi

n=0
for pair in $PAIRS; do
    for t in $T; do
        name="${pair/,/__}_t${t}"
        TEMPUS_DAYPCT=$DAYPCT TEMPUS_YEARPCT=$YEARPCT \
            TEMPUS_FLY="$pair" TEMPUS_FLYT="$t" \
            TEMPUS_SHOT="$OUT/$name.png" ./tempus >/dev/null 2>&1
        n=$((n+1))
    done
    echo "  $pair done"
done
echo "flight matrix: $n shots -> $OUT"
