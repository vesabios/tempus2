# T E M P V S

An astronomical clock for time and celestial-body reasoning, built to feel
like a mix of **horological art piece**, **aircraft instrument panel**, and
**fine timepiece** — with a minimal, high-end graphic treatment. It ships as
a screensaver (eventually) and runs as a standalone app and in the browser.

This is the second incarnation. The first (`../tempus`) was a C++/Cinder
screensaver, shipped and notarized for macOS and Windows. Tempus2 is a
ground-up portable rewrite that keeps v1 as its feature spec.

## What it shows

Three composable views, layered into one instrument:

- **Calendar wheel** — the year as a ring: 365 day ticks, month names, the
  eight sabbats (cross-quarter days and solstices/equinoxes, with either
  traditional or alternate names), leap-year marks, and a pointer for today.
- **Clock** — analog face with hour/minute hands and a seconds hand that
  either sweeps continuously or ticks per-second (config).
- **Solar instrument** — the centerpiece. Earth rendered orthographically
  as seen **from space** directly above your configured lat/lon (north up,
  east right), lit by the true sun:
  - the **terminator** (day/night line) sweeps the disc as the planet turns;
    its tilt against the poles is the axial tilt made visible
  - a **30° graticule** makes the shaded disc read as a globe
  - a **center marker** = you are here
  - the **sun marker** is the orthographic projection of the subsolar point
    — it sits exactly on the globe's lit pole by construction
  - a **daylight arc** (sunrise→sunset on a 24h ring; noon at the bottom,
    where a northern-hemisphere sun culminates) surrounds the globe
  - at night you see the far side's daylight peeking around the limb —
    "night is currently over Asia," live

Beyond the geocentric dial and the heliocentric earth, a third worldview
station: the **full system**. The camera pulls back until the calendar
wheel reads as what it always was — Earth's orbit — and the rest of the
family appears: all eight planets and Pluto on horologically spaced rings
(true ecliptic-of-date longitudes, JPL Keplerian elements in
`core/planets.h`), a **zodiac dial** outside them all, **sight-lines**
that run from Earth through each body to where it lands in the zodiac
(the bend of each curve is the honest cost of a not-to-scale layout; the
endpoints are true, and retrogrades read right off the markers), and the
**aspect web** — chords between bodies whose geocentric separations hit
the sacred divisions (conjunction, sextile, square, trine, opposition,
plus the minors), brightening as the aspect tightens. A grand trine is an
equilateral triangle drawn across the whole instrument, live.

And the fourth station, CAELVM: the local sky itself, first person. An
all-sky chart — zenith at center, horizon at the rim, north up and east
LEFT (you are looking up, not down at a map) — with every body at its
true azimuth and altitude, sized by apparent magnitude, tracing its
diurnal arc from rising to setting. The ecliptic lies across the bowl
with the sign cusps ticked, the background breathes with the sun's own
altitude through the twilights, and the moon hangs as a phase-lit globe
with its bright limb aimed at the sun. Entering from MACHINA MVNDI is a
true morph, element to element: beads fly from their orbit rings to
their places in your sky (setting bodies exit through the rim), orbit
rings unfurl into diurnal arcs — the year becomes the day — the zodiac
dial bends into the ecliptic's lie across the bowl, and the calendar
wheel grows into the horizon: the year becomes the ground. The stations
are named on the instrument: HOROLOGIVM, TELLVS, MACHINA MVNDI, CAELVM.

The local horizon cuts through the dial as the chart's ascendant axis:
the below-horizon arc of the zodiac shades dark in the moat and wheels
around once a day. Each body's marker answers the season-scale "can I
see it from here at all" — filled if it reaches dark-enough sky tonight
(per-body thresholds: the moon shines through twilight, Mercury never
escapes it, the dim outer planets need real darkness), hollow while
it's lost for weeks behind the sun or pinned below the horizon. The
gold combust wedge around the sun's marker shows the glare zone that
swallows them. Sky math (equatorial conversion + sidereal time) is
validated against SPA to arcminutes.

Solar math is NREL's SPA (`core/spa.c`); sunrise/sunset via `core/sunset.h`.
The instrument is astronomically honest: the clock shows machine wall time,
the globe shows the truth for the configured coordinates at this absolute
moment. Point it at another city and they will (correctly) disagree.

## Design principles

1. **Durable core, disposable shells.** `core/` is dependency-free C11 —
   no rendering, no platform, no allocation. Platform shells (sokol today,
   `.saver`/`.scr` shims tomorrow) are thin adapters expected to be
   rewritten as platforms churn. The wasm build is treated as the long-term
   durable artifact: plain C compiles to wasm on the oldest, most stable
   toolchain there is.

2. **Never spin the fan.** A screensaver that heats the GPU is a failure.
   The scene reports the frame rate it actually needs
   (`scene_desired_fps()` in `core/scene.h`, driven by `PacePolicy`):
   full rate only during transitions/tweens/time-warps, ~20 fps for a
   sweeping second hand, ~4 fps ticking, ~1 fps ambient. Shells skip all
   update/rebuild work between due frames and boost briefly on input.

3. **Instrument legibility, not explanation.** Like an attitude indicator,
   the display should explain itself through reference marks (graticule,
   ownship marker, noon-up arcs), not require a manual.

4. **Engraved, not rasterized.** All 2D strokes are feathered by one screen
   pixel with energy conservation (sub-pixel lines render dimmer, not
   aliased) — no MSAA needed, no moiré in the 365-tick day ring. Rendering
   is high-DPI (device pixels). Text is SDF with fwidth antialiasing.

## Architecture

```
core/            Pure C11, no dependencies — the durable artifact
  tempus.h       Master state: date/time, timezone (auto from system clock),
                 julian dates, solar position, config
  scene.h        Scene manager: views, layers, transitions, auto-cycle,
                 tween pool, RenderStyle, PacePolicy + scene_desired_fps()
  view.h         View vtable + shared RenderStyle (all colors/metrics)
  views/         view_clock.h, view_calendar.h, view_solar.h
  draw.h         Batched 2D primitives -> vertex/index arrays (feathered AA);
                 also carries the GlobeCmd slot for the 3D globe
  globe.h        Globe mesh + orientation math (earth->view rotation,
                 sun direction in view frame)
  tween.h        Tween pool, easing curves
  timeview.h     Per-view time state (views can warp time independently)
  timewarp.h     Virtual time acceleration (e.g. spin the sun through a day)
  calendar.h     Julian dates, sabbats, month/event names
  spa.c/.h       NREL Solar Position Algorithm
  sunset.h       Sunrise/sunset calculator

shaders/         Inline GLSL (desktop GL 3.3 + GLES3/WebGL2 via one #ifdef)
                 2D batch shader (SDF text) + globe shader (lambert
                 day/night, analytic graticule + terminator, silhouette AA)

platform/        Shells — thin, replaceable
  standalone.c   sokol_app shell: macOS (ObjC/GL), Linux (GL), wasm (GLES3);
                 nuklear debug panel, config file persistence, pacing loop
  shell.html     Fullscreen-canvas Emscripten shell

lib/             Vendored single-header libs: sokol, nuklear, stb
tools/           bake_font.c — SDF font atlas generator (run: make atlas)
assets/          Font atlas (generated), Avenir sources, misc art
```

The 2D pipeline is one vertex/index buffer built CPU-side each update and
drawn in a single call. The globe is the only 3D element: a view requests it
mid-batch via `draw_globe()` and the shell splits the 2D draw around it
(under-globe geometry, globe, over-globe geometry).

## Building

```sh
make            # native binary ./tempus (macOS: compiles shell as ObjC)
make web        # wasm build in web/ (requires emscripten)
make serve      # make web + http server on :8123
make atlas      # regenerate SDF font atlas from assets/fonts
```

Runs on macOS (OpenGL via sokol; Metal planned for the .saver shim),
Linux (GL/X11), and any WebGL2 browser.

## Running / dev harness

Keys: `Q`/`ESC` quit · `D` toggle debug panel · `A` toggle name set ·
`S` solar day-warp demo.

The nuklear debug panel has manual time sliders (scrub the sun around the
globe), location fields, pacing readout, and Save Config.

Env vars (dev):

| var | effect |
|-----|--------|
| `TEMPUS_SHOT=out.png` | render ~30 frames, dump framebuffer PNG, exit |
| `TEMPUS_DAYPCT=0.29` | pin time-of-day (0..1); `TEMPUS_YEARPCT` likewise |
| | (manual/pinned time is interpreted as **local solar time** at the configured longitude — slider midnight = sun opposite you) |
| `TEMPUS_PACE_LOG=1` | log desired fps + updates/sec once per second |
| `TEMPUS_TICK=1` | ticking second hand (lowest idle power) |
| `TEMPUS_CONFIG=path` | config file override |

Config persists to `~/.config/tempus.conf` (key=value: latitude, longitude,
elevation, timezone, timezone_auto, alternate_names, sweep_seconds,
timezone_name). Defaults to Berlin, DE. Timezone is auto-derived from the
system clock (DST-aware) unless `timezone_auto = 0`.

**Traveling / remote locations:** set `timezone_name = Europe/Berlin`
(any IANA zone) and the entire instrument — clock hands included — runs in
that location's time via the system tz database. Without it, the clock
shows machine time while the globe shows the configured location's true
solar state; they will honestly disagree when those places differ.

## Status / roadmap

Done: portable core, macOS/Linux/wasm builds, pacing system, globe
instrument, feathered AA + high-DPI, config persistence.

Next, roughly in order:

1. **v1 feature parity** — month-arc detailing, logo treatment, view
   auto-cycle polish (v1's `TempusApp.cpp` is the spec).
2. **macOS `.saver` shim** — ObjC `ScreenSaverView` driving sokol_gfx
   (Metal) directly, no sokol_app; timer-driven presentation completes the
   power story (no per-vsync work at all). v1's notarization notes apply.
3. **Windows `.scr` shim** — sokol_app D3D11 shell + `/s` `/c` `/p`
   argument handling, exit-on-input, multi-monitor.
4. **Web persistence** — localStorage-backed config for the wasm build.
