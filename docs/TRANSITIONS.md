# The Transition Framework

The design for the transitions-refactor branch, agreed 2026-07-19.
The instrument is an homage to precision timepieces; transitions are
part of the aesthetic and must be flawless. This document is the
architecture that makes flawlessness structural instead of patched.

## The model (the camera-manager pattern)

Each **station** declares, per object, its full state — its
**members**: position, orientation, scale, light, reference frame,
and flags. A single **manager** interpolates between *state
properties*, never between station pairs. Transitions are therefore
authored generically; a new station is a column of members, not a row
of pairwise special cases.

## Weights, not blends

One normalized **station weight vector** `w[ST_COUNT]` (sums to 1)
replaces the nine independent blends. All mixing is barycentric over
weights. Two-station flights are the special case; three-way
crossings are automatically correct (the machine's weight pins to
zero when two chart stations cross — the class of "object dips
through an unrelated seat mid-flight" becomes unrepresentable).

## Typed members

The member's type carries its interpolation rule:

- position in a shared polar frame → circular mean (never cartesian
  lerp across a polar space)
- scale → log-lerp
- orientation → continuity-corrected slerp (pq state per object)
- light → composed from live frames each frame; never baked vectors
  (never nlerp of frame snapshots against a slerping rotation)

## Frames: native vs private

**Manager-native frames** — the manager blends *inside* them and
evaluates them live every frame:

- `WORLD` — screen-centered instrument space
- `WHEEL` — the polar year/zodiac mapping (and its geo-flipped
  variant, DRACO's `+180`)
- `GLOBE_3D` — the live globe's center, radius, and rotation

These preserve 3D relationships THROUGH a tween: the moon's member at
TELLVS/MACHINA/ORBIS is `(frame: GLOBE_3D, bearing, reach, lat)` —
the tween blends `reach` while position is derived from the live
globe each frame, so the globe can fly, grow, and spin mid-transition
and the moon rides it.

**Station-private projections** — evaluate-only, opaque: CAELVM's
azimuthal chart, the sky dome, any nonlinear lookup. The station
evaluates privately and publishes results as coordinates in a native
frame. Exotic coordinate systems are never tweened between — and
never need to be.

## The rule stack

1. **Parents before children.** Frames form a small scene graph
   (wheel → globe → moon; chart frames terminal). Children hold
   coordinates, not results — staleness is unrepresentable (the
   render-time-read law, made structural).
2. **Same frame → blend local members**, evaluate against the live
   parent.
3. **Cross-frame → evaluate both sides live, then blend outputs**,
   with typed geometry (polar about a shared center where one
   exists). The aperture handoff is this rule.
4. **Absent member → fade in place.** A station that declares no seat
   for an object never inherits another station's; the object holds
   and fades (the ORBIS-globe-to-HORAE fix, generalized; CAELVM's
   machine-parking is the same rule).

## Staging (each lands only when the flight matrix diffs clean)

- **Stage 0 — done**: `TEMPUS_FLY` deterministic flight harness +
  `scripts/flight_matrix.sh` / `flight_diff.py`; baseline baked.
- **Stage 1**: station weight vector + station descriptor table
  (fly targets, wheel radius, furniture scale, input policy, layer
  dimming as table columns).
- **Stage 2 — done**: members + manager for the sun and moon; the
  orrery's moon composition dissolved into member rows
  (`seat_mix_pos` / `seat_mix_dir3`), and the aperture's claim moved
  onto the station weight vector (dial family vs globe family,
  normalized within the machine's seat) — the moon glides across the
  whole flight instead of welding at the first quarter. Acceptance:
  interleaved arbitration under the pinned observer, drift confined
  to the two intended buckets.
- **Stage 3**: planets onto the manager; `sky_owns` and coincidence
  handoffs deleted. Shape: the sky publishes per-body chart members
  (position, pip radius, alpha policy, style) exactly as it already
  publishes the luminaries' targets; the orrery renders every bead at
  every station, composing machine seat -> chart target by the weight
  vector; the sky's mover loop and the orrery's `sky_owns` abdication
  are both deleted. The seven planets only — the sun and moon are
  already VIEW_LVMEN's. Layer question to resolve: beads stay in the
  orrery's layer (under the charts' furniture) vs join the luminaries
  above; start with the orrery layer for pixel parity at rest.
  Parity vocabulary (view_sky.h mover, lines ~366-381 and ~597-727):
  `mb` = sky_blend (the chart's flight clock), `ms` = the LIVE
  machine's system stage, `fin` = smoothstep(0.10, 0.75, mb) the
  born-in-place fade, machine-seat weight `mw = ms*(1-mb)`, chart
  weight `sw = ms*mb + (1-ms)*fin`. Planet position = ring_seat*mw +
  chart*(1-mw); alpha = ba (0.78 subdued below horizon, stroked ring
  when not observable) * (ms + (1-ms)*fin). The orrery's machine-side
  stagger is a_planet = smoothstep(0.20, 0.60, ss), gated off by
  sky_owns today — that gate is what Stage 3 deletes. The sky has NO
  draco term: planets fade on DRACO flights purely through mb/fin
  (born-in-place in reverse) — carry that over unchanged. RENDERER
  DECISION: the orrery's opacity is floored at 0.002 at chart
  stations (it renders as the luminaries' composer only), so the
  beads cannot live in its pass — VIEW_LVMEN becomes the body
  renderer for all nine objects. The orrery composes each planet's
  member row (machine ring seat, published sky chart target, weights
  mw/sw from the vocabulary above) and publishes to LVMEN exactly as
  it does for the sun and moon. Stacking consequence, accepted:
  beads draw above the sky's compass/labels instead of under —
  review at-rest CAELVM diffs for real overlaps. Ring NAMES stay in
  the orrery (a_name tail), sky NAME labels stay in the sky (fb).
- **Stage 4 — done**: ink staging table (`core/ink.h`) — every
  flight-choreography smoothstep window (machine furniture staggers,
  machine-exit fades, chart born-in-place/late-furniture ramps, the
  luminaries' surface claims, the eclipse stage gate) is one
  declarative table, double-precision, same constants — verified
  bit-exact by spot shots. Deliberately NOT in the table: the clock
  hands' linear first-quarter exit (1 - 4*helio), the calendar's
  zoom-radius visibility windows, and DRACO's ephemeris-driven
  season/syzygy gates — those are driven by zoom or sky geometry,
  not flight blends.

Baseline protection: tag `pre-refactor`, bundle at
`~/claude/tempus2-pre-refactor.bundle`, private remote
`github.com/vesabios/tempus2`. Master does not move until parity.
