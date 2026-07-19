#!/bin/bash
# Render the flight matrix: station pairs captured mid-flight at fixed
# moments, on a fixed 60Hz timestep and a pinned clock — the same
# flight always lands on the same pixels. The regression net for the
# transition refactor.
#
# Usage:
#   scripts/flight_matrix.sh [outdir]     # curated pairs (~5 min)
#   FULL=1 scripts/flight_matrix.sh       # every ordered pair (long)
#   T="1.0 2.0" scripts/flight_matrix.sh  # custom capture moments
#
# Compare two runs with scripts/flight_diff.py.
set -e
cd "$(dirname "$0")/.."

OUT=${1:-flightmatrix/$(git rev-parse --short HEAD)}
mkdir -p "$OUT"
T=${T:-"0.9 1.75 2.6 3.8"}

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
        TEMPUS_DAYPCT=0.40 TEMPUS_FLY="$pair" TEMPUS_FLYT="$t" \
            TEMPUS_SHOT="$OUT/$name.png" ./tempus >/dev/null 2>&1
        n=$((n+1))
    done
    echo "  $pair done"
done
echo "flight matrix: $n shots -> $OUT"
