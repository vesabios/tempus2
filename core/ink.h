// ink.h — the staging table (Stage 4 of docs/TRANSITIONS.md).
//
// Every element's entrance window on its driving blend, declared in
// ONE place. A window {a, b} means: the element is fully absent at
// blend a, fully present at blend b, smoothstepped between. The old
// choreography lived as ~20 scattered smoothstep calls; the constants
// here are those exact values — pixel parity by construction — and
// tuning the instrument's entrance choreography is now editing one
// table instead of hunting call sites.
//
// Windows on the SYSTEM stage (ss): the machine's furniture staggers
// in as the full system arrives — rings first, then beads, zodiac,
// sight-lines, and the aspect web last (each element waits for its
// anchors). Windows on a CHART blend (sky/draco): what that chart
// fades as it takes or leaves the stage. Windows on an OVERLAY blend:
// how fast the machine bows out beneath it.

#ifndef INK_H
#define INK_H

typedef enum {
    // -- machine furniture, on the system stage --
    INK_SYS_DIAL,      // the geo dial dissolving under the system
    INK_RING,          // orbit rings
    INK_PLANET,        // planet beads (the machine's bead presence —
                       //   also the beads' claim in the member rows)
    INK_ZODIAC,        // zodiac band + sigils
    INK_SIGHT,         // geocentric sight-lines
    INK_WEB,           // aspect web
    INK_MOON_SHRINK,   // the moon's shrink toward a bead at system
    INK_BEAD_CLAIM,    // the beads' machine claim in the member rows:
                       //   full-width, so a geo-parked flight to the
                       //   system spreads the transit over the WHOLE
                       //   flight (was INK_PLANET's window — the
                       //   travel compressed and read as a dash)

    // -- the machine beneath a rising surface --
    INK_MACHINE_EXIT,  // machine fade under ANY overlay or chart
    INK_ORBIS_CHROME,  // clock chrome under the ORBIS closeup

    // -- chart staging, on the chart's own blend --
    INK_BORN,          // born-in-place fade (bodies with no seat)
    INK_CHART_LATE,    // late chart furniture (labels, compass)
    INK_HORIZON,       // the horizon rim
    INK_LUM_SKYVIS,    // the luminaries' sky claim
    INK_DRACO_VIS,     // the luminaries' draco claim
    INK_DRACO_STAGE,   // eclipse fire: only at the fully-arrived chart

    INK_COUNT
} Ink;

static const double ink_table[INK_COUNT][2] = {
    [INK_SYS_DIAL]     = { 0.15, 0.60 },
    [INK_RING]         = { 0.05, 0.45 },
    [INK_PLANET]       = { 0.20, 0.60 },
    [INK_ZODIAC]       = { 0.40, 0.80 },
    [INK_SIGHT]        = { 0.60, 0.95 },
    [INK_WEB]          = { 0.70, 1.00 },
    [INK_MOON_SHRINK]  = { 0.20, 0.70 },
    [INK_BEAD_CLAIM]   = { 0.00, 1.00 },
    [INK_MACHINE_EXIT] = { 0.00, 0.55 },
    [INK_ORBIS_CHROME] = { 0.00, 0.20 },
    [INK_BORN]         = { 0.10, 0.75 },
    [INK_CHART_LATE]   = { 0.25, 0.95 },
    [INK_HORIZON]      = { 0.05, 0.50 },
    [INK_LUM_SKYVIS]   = { 0.00, 0.15 },
    [INK_DRACO_VIS]    = { 0.00, 0.10 },
    [INK_DRACO_STAGE]  = { 0.90, 0.995 },
};

// Presence of an element at blend x (0 absent .. 1 present)
static inline float ink_in(Ink e, double x) {
    return (float)tempus_smoothstep(ink_table[e][0], ink_table[e][1], x);
}

// Complement: what remains of an element x deep into a departure
static inline float ink_out(Ink e, double x) {
    return 1.0f - ink_in(e, x);
}

#endif // INK_H
