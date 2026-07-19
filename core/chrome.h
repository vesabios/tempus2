// chrome.h — the instrument's engraved register.
//
// Location, date, and the hour, upper-left in chrome space: plain
// 1280-tall units with no camera pull, so the readout holds still
// while the instrument flies. Shared by the app and the screensaver;
// the app adds its own time controls beneath it.

#ifndef TEMPUS_CHROME_H
#define TEMPUS_CHROME_H

static float chrome_text_tracked(DrawCtx *d, int weight, float size,
                                 float x, float y, float track_em,
                                 const char *s) {
    char c[2] = { 0, 0 };
    float cur = x;
    for (; *s; s++) {
        c[0] = *s;
        cur += draw_text_ex(d, weight, size, cur, y, c) + track_em * size;
    }
    return cur - track_em * size - x;
}

static void tempus_chrome_readout(DrawCtx *d, const Tempus *t,
                                  float w, float h) {
        // Civil-time readout, upper-left: the modern register in chrome
        // space, opposite the annunciator. Location + GMT offset over
        // the date and 12-hour time.
            float half_w = (w / h) * 640.0f;
            float x0 = -half_w + 42.0f;

            // Nearest charted city to the configured home. Rescanned
            // only when the location moves (ORBIS drags it live).
            static double loc_la = 999.0, loc_lo = 999.0;
            static const char *loc_city = NULL;
            static double loc_km = 0.0;
            if (t->config.latitude != loc_la ||
                t->config.longitude != loc_lo) {
                loc_la = t->config.latitude;
                loc_lo = t->config.longitude;
                double la = loc_la * M_PI / 180.0;
                double lo = loc_lo * M_PI / 180.0;
                double o0 = cos(la) * cos(lo), o1 = cos(la) * sin(lo);
                double o2 = sin(la);
                int best = 0;
                double bd = -2.0;
                for (int i = 0; i < CITY_NUM; i++) {
                    double cla = city_pts[i].lat * 0.01 * M_PI / 180.0;
                    double clo = city_pts[i].lon * 0.01 * M_PI / 180.0;
                    double dd = cos(cla) * cos(clo) * o0
                              + cos(cla) * sin(clo) * o1 + sin(cla) * o2;
                    if (dd > bd) { bd = dd; best = i; }
                }
                if (bd > 1.0) bd = 1.0;
                loc_city = city_pts[best].name;
                loc_km = acos(bd) * 6371.0;
            }

            char tzs[16];
            double tz = t->config.timezone;
            int tzh = (int)fabs(tz);
            int tzm = (int)((fabs(tz) - tzh) * 60.0 + 0.5);
            if (tzm)
                snprintf(tzs, sizeof(tzs), "GMT%c%d:%02d",
                         tz < 0 ? '-' : '+', tzh, tzm);
            else
                snprintf(tzs, sizeof(tzs), "GMT%c%d",
                         tz < 0 ? '-' : '+', tzh);

            char loc_label[64];
            if (loc_city && loc_km < 150.0)
                snprintf(loc_label, sizeof(loc_label), "%s", loc_city);
            else
                snprintf(loc_label, sizeof(loc_label), "%.2f%s %.2f%s",
                         fabs(loc_la), loc_la >= 0 ? "N" : "S",
                         fabs(loc_lo), loc_lo >= 0 ? "E" : "W");

            char mon[16];
            snprintf(mon, sizeof(mon), "%s",
                     cal_month_names[t->month - 1]);
            for (char *p = mon + 1; *p; p++)
                if (*p >= 'A' && *p <= 'Z') *p += 'a' - 'A';
            char dateline[40];
            snprintf(dateline, sizeof(dateline), "%s %d, %d",
                     mon, t->day, t->year);

            int hh = t->hours % 12;
            if (hh == 0) hh = 12;
            char timeline[16];
            snprintf(timeline, sizeof(timeline), "%d:%02d",
                     hh, t->mins);
            const char *meridiem = t->hours < 12 ? "AM" : "PM";

            // Engraved caps up top: place tracked wide, offset dimmer.
            // Below, the date in light figures; then the hour as the
            // hero line, meridiem in small caps sharing its baseline.
            float lx = x0;
            draw_set_color(d, dca(0.62f, 0.60f, 0.55f, 0.62f));
            lx += chrome_text_tracked(d, SDF_WEIGHT_MEDIUM, 15.0f,
                                      lx, -604.0f, 0.38f, loc_label);
            draw_set_color(d, dca(0.50f, 0.49f, 0.46f, 0.40f));
            chrome_text_tracked(d, SDF_WEIGHT_MEDIUM, 15.0f,
                                lx + 22.0f, -604.0f, 0.38f, tzs);

            draw_set_color(d, dca(0.78f, 0.75f, 0.68f, 0.78f));
            draw_text_ex(d, SDF_WEIGHT_LIGHT, 40.0f,
                         x0, -578.0f, dateline);

            draw_set_color(d, dca(0.78f, 0.75f, 0.68f, 0.95f));
            float tw = draw_text_ex(d, SDF_WEIGHT_LIGHT, 68.0f,
                                    x0, -528.0f, timeline);
            float mer_y = -528.0f + sdf_nascent[SDF_WEIGHT_LIGHT] * 68.0f
                        - sdf_nascent[SDF_WEIGHT_MEDIUM] * 20.0f;
            draw_set_color(d, dca(0.62f, 0.60f, 0.55f, 0.62f));
            chrome_text_tracked(d, SDF_WEIGHT_MEDIUM, 20.0f,
                                x0 + tw + 12.0f, mer_y, 0.20f, meridiem);

}

#endif // TEMPUS_CHROME_H
