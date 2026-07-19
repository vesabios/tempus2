// stage.h — the ink staging: blends become layer opacities.
//
// One pass, every frame, turning the scene's station blends into what
// each layer is allowed to draw — the machine's fade under a rising
// chart, the clock chrome bowing out, the luminaries' surface claim,
// the parked ORBIS globe snapping home unseen. Shared by every host:
// without it a scene renders all fourteen layers at once.

#ifndef TEMPUS_STAGE_H
#define TEMPUS_STAGE_H

static void tempus_stage_views(Scene *sc, int station) {
    double hb = sc->helio_blend;
    double sky = sc->sky_blend;
    // The machine dissolves EARLY under the sky morph — its beads and
    // rings hand off to the sky view's moving copies, which render at
    // full strength for the whole flight (the sky view alphas its own
    // elements internally).
    double horae = sc->horae_blend;
    double rotae = sc->rotae_blend;
    double saec = sc->saec_blend;
    double orbis = sc->orbis_blend;
    double offic = sc->offic_blend;
    double astro = sc->astro_blend;
    double draco = sc->draco_blend;
    double fade = (double)ink_out(INK_MACHINE_EXIT, sky)
                * (double)ink_out(INK_MACHINE_EXIT, horae)
                * (double)ink_out(INK_MACHINE_EXIT, rotae)
                * (double)ink_out(INK_MACHINE_EXIT, saec)
                * (double)ink_out(INK_MACHINE_EXIT, offic)
                * (double)ink_out(INK_MACHINE_EXIT, draco)
                * (double)ink_out(INK_MACHINE_EXIT, astro);
    // ORBIS keeps the orrery: the globe IS the station (it grows to the
    // closeup inside the orrery itself). Only the clock chrome bows out
    // — and it bows out FAST (gone by a fifth of the flight): the dial
    // furniture has no business hanging over the growing planet.
    double orbis_fade = (double)ink_out(INK_ORBIS_CHROME, orbis);
    // The calendar wheel survives into the sky as its bezel — the time
    // control rides along to every worldview
    sc->views[VIEW_CALENDAR].opacity = 1.0;
    sc->views[VIEW_SOLAR].opacity = 0.0;    // data only, never draws
    // The orrery must keep rendering even fully faded — it is the
    // COMPOSER of the luminaries (its render publishes their
    // parameters for VIEW_LVMEN). Floor just above the render cutoff;
    // its vertex-heavy passes skip themselves below visibility.
    sc->views[VIEW_ORRERY].opacity = fade > 0.002 ? fade : 0.002;
    // The parked ORBIS closeup snaps home UNSEEN once the machine is
    // fully hidden at an overlay station (the absent-seat rule's
    // second half: park, fade, then reset while nobody watches).
    // The CHART stations (CAELVM, DRACO) keep the LUMINARIES visible
    // through their flights, and the luminaries' base seats read
    // orbis_blend — so wait for the chart to own them completely
    // (blend at 1) before snapping, or the sun and moon jump
    // mid-flight (Seren caught it on ORBIS -> DRACO, then again on
    // ORBIS -> CAELVM).
    if (station != ST_ORBIS && sc->orbis_blend > 0.001
        && fade < 0.004
        && (draco <= 0.001 || draco >= 0.999)
        && (sky <= 0.001 || sky >= 0.999)) {
        tween_cancel_target(&sc->tweens, &sc->orbis_blend);
        sc->orbis_blend = 0.0;
    }
    // Clock face and hands exit in the first quarter of the transit (and
    // return in the last quarter coming home) — they're geocentric
    // furniture and have no business lingering over the flight
    double clock_vis = 1.0 - hb * 4.0;
    if (clock_vis < 0) clock_vis = 0.0;
    sc->views[VIEW_CLOCK].opacity = clock_vis * fade * orbis_fade;
    sc->views[VIEW_CLOCKBACK].opacity =
        sc->views[VIEW_CLOCK].opacity;
    sc->views[VIEW_SKY].opacity = sky > 0.001 ? 1.0 : 0.0;
    sc->views[VIEW_HORAE].opacity = horae;
    sc->views[VIEW_ROTAE].opacity = rotae;
    sc->views[VIEW_SAEC].opacity = saec;
    // The station's own chrome (reticle, readout, city pips) follows
    // the RELEASED voice, so a parked flight fades it instead of
    // holding it over the next station until the snap
    sc->views[VIEW_ORBIS].opacity =
        orbis < sc->orbis_wheel ? orbis : sc->orbis_wheel;
    sc->views[VIEW_OFFIC].opacity = offic;
    sc->views[VIEW_ASTRO].opacity = astro;
    sc->views[VIEW_DRACO].opacity = draco;
    // The luminaries ride whichever surface is up: the machine's fade
    // or the sky — never both gone unless an overlay dial owns the
    // stage. This is what makes the sun and moon SINGLE objects: one
    // renderer, always on top, parameters composed continuously.
    {
        double lum = (double)ink_out(INK_LUM_SKYVIS, sky);
        double skyvis = (double)ink_in(INK_LUM_SKYVIS, sky);
        lum = fade > skyvis ? fade : skyvis;
        // The astrolabe is a CHART: it keeps the luminaries the whole
        // way, riding its published plate targets
        {
            double avis = (double)ink_in(INK_LUM_SKYVIS, astro);
            if (avis > lum) lum = avis;
        }
        // DRACO flights keep the luminaries with VIEW_LVMEN the whole
        // way (the orrery composes toward draco's published targets);
        // at full blend the station takes over at exact coincidence —
        // its own furnace-under, umbra-over sandwich needs the beads
        // in its own layer
        if (draco < 0.999) {
            double dvis = (double)ink_in(INK_DRACO_VIS, draco);
            if (dvis > lum) lum = dvis;
        }
        sc->views[VIEW_LVMEN].opacity = lum;
    }
}

#endif // TEMPUS_STAGE_H
