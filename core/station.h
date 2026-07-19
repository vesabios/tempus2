// station.h — The stations, and what each one declares.
//
// Stage 1 of the transition framework (docs/TRANSITIONS.md): every
// per-station scalar and policy that used to live as a special case
// in the shell or a view becomes a COLUMN here. The station weight
// vector — normalized, barycentric — is the manager's blend input;
// Stage 2 adds per-object members (seats) on top of this spine.

#ifndef STATION_H
#define STATION_H

#include <stdbool.h>

typedef enum {
    ST_HOROLOGIVM = 0,   // geocentric dial + clock
    ST_HORAE,            // the planetary hours
    ST_ROTAE,            // the nested cycle wheels
    ST_SAECVLVM,         // the years of a life
    ST_TELLVS,           // heliocentric earth
    ST_MACHINA,          // full system + zodiac + aspect web
    ST_CAELVM,           // the local sky, first person
    ST_ORBIS,            // the world chart: choose your place
    ST_OFFICIVM,         // the book of hours
    ST_DRACO,            // the eclipse dragon
    ST_ASTROLAB,         // the planispheric astrolabe
    ST_COUNT
} Station;

typedef struct {
    const char *name;        // annunciator Latin

    // Flight targets for the base machine morph (helio, calendar
    // zoom, system); park_machine = the station leaves the machine
    // wherever it stands (CAELVM's law)
    float fly_helio, fly_zoom, fly_system;
    bool  park_machine;

    // The calendar wheel's declared members
    float wheel_r;           // resting radius
    float wheel_furniture;   // furniture scale (marks/text/band)

    // Band input policy
    bool  band_keep_time;    // band scrubs whole days, time held
    bool  band_week_clicks;  // band steps whole weeks (HORAE)
} StationDesc;

static const StationDesc station_table[ST_COUNT] = {
    //                 name            helio zoom sys  park   wheel furn  keep  week
    [ST_HOROLOGIVM] = { "HOROLOGIVM",    0,   0,   0,  false,  450, 1.0f, true,  false },
    [ST_HORAE]      = { "HORAE",         0,   0,   0,  false,  450, 1.0f, true,  true  },
    [ST_ROTAE]      = { "ROTAE",         0,   0,   0,  false,  450, 1.0f, false, false },
    [ST_SAECVLVM]   = { "SAECVLVM",      0,   0,   0,  false,  450, 1.0f, false, false },
    [ST_TELLVS]     = { "TELLVS",        1,   1,   0,  false,  450, 1.0f, false, false },
    [ST_MACHINA]    = { "MACHINA MVNDI", 1,   0,   1,  false,  762, 1.0f, false, false },
    [ST_CAELVM]     = { "CAELVM",        0,   0,   0,  true,   610, 1.0f, true,  false },
    [ST_ORBIS]      = { "ORBIS",         0,   0,   0,  false,  505, 0.85f, true, false },
    [ST_OFFICIVM]   = { "OFFICIVM",      0,   0,   0,  false,  450, 1.0f, true,  false },
    [ST_DRACO]      = { "DRACO",         0,   0,   0,  false,  450, 1.0f, false, false },
    [ST_ASTROLAB]   = { "ASTROLABIVM",   0,   0,   0,  false,  450, 1.0f, false, false },
};

// The station weight vector: normalized, barycentric, derived from
// the scene's blends. HOROLOGIVM is the residual — the home the
// machine stands in when nothing else claims it. All mixing in the
// framework happens over these weights; two-station flights are the
// special case, crossings are automatically correct.
static inline void station_weights(double helio, double system,
                                   double sky, double horae,
                                   double rotae, double saec,
                                   double orbis, double offic,
                                   double draco, double astro,
                                   double out[ST_COUNT]) {
    out[ST_TELLVS]   = helio * (1.0 - system);
    out[ST_MACHINA]  = helio * system;
    out[ST_CAELVM]   = sky;
    out[ST_HORAE]    = horae;
    out[ST_ROTAE]    = rotae;
    out[ST_SAECVLVM] = saec;
    out[ST_ORBIS]    = orbis;
    out[ST_OFFICIVM] = offic;
    out[ST_DRACO]    = draco;
    out[ST_ASTROLAB] = astro;
    double sum = 0;
    for (int i = 1; i < ST_COUNT; i++) sum += out[i];
    out[ST_HOROLOGIVM] = (sum < 1.0) ? 1.0 - sum : 0.0;
    sum += out[ST_HOROLOGIVM];
    if (sum > 1.0e-9)
        for (int i = 0; i < ST_COUNT; i++) out[i] /= sum;
}

// The dominant station by weight — input policy reads this
static inline Station station_dominant(const double w[ST_COUNT]) {
    Station best = ST_HOROLOGIVM;
    for (int i = 1; i < ST_COUNT; i++)
        if (w[i] > w[best]) best = (Station)i;
    return best;
}

#endif // STATION_H
