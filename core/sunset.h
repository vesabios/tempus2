// sunset.h — Sunrise/sunset calculations
// Ported from SunSet.cpp (MIT License, Peter Buelow 2015)

#ifndef SUNSET_H
#define SUNSET_H

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    double latitude;
    double longitude;
    double julian_date;
    int    tz_offset;       // hours from GMT
    int    year, month, day;
} SunsetCalc;

static inline void sunset_init(SunsetCalc *s, double lat, double lon, int tz) {
    s->latitude = lat;
    s->longitude = lon;
    s->tz_offset = tz;
    s->julian_date = 0;
}

static inline void sunset_set_position(SunsetCalc *s, double lat, double lon, int tz) {
    s->latitude = lat;
    s->longitude = lon;
    s->tz_offset = tz;
}

static inline double sunset_deg2rad(double deg) { return M_PI * deg / 180.0; }
static inline double sunset_rad2deg(double rad) { return 180.0 * rad / M_PI; }

static inline double sunset_calc_jd(int y, int m, int d) {
    if (m <= 2) { y -= 1; m += 12; }
    int A = (int)floor(y / 100.0);
    int B = 2 - A + (int)floor(A / 4.0);
    return floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;
}

static inline double sunset_set_date(SunsetCalc *s, int y, int m, int d) {
    s->year = y;
    s->month = m;
    s->day = d;
    s->julian_date = sunset_calc_jd(y, m, d);
    return s->julian_date;
}

// --- internal helpers ---

static inline double sunset__time_julian_cent(double jd) {
    return (jd - 2451545.0) / 36525.0;
}

static inline double sunset__jd_from_julian_cent(double t) {
    return t * 36525.0 + 2451545.0;
}

static inline double sunset__mean_obliquity(double t) {
    double seconds = 21.448 - t * (46.8150 + t * (0.00059 - t * 0.001813));
    return 23.0 + (26.0 + seconds / 60.0) / 60.0;
}

static inline double sunset__geom_mean_long_sun(double t) {
    double L = 280.46646 + t * (36000.76983 + 0.0003032 * t);
    while (L > 360.0) L -= 360.0;
    while (L < 0.0) L += 360.0;
    return L;
}

static inline double sunset__obliquity_correction(double t) {
    double e0 = sunset__mean_obliquity(t);
    double omega = 125.04 - 1934.136 * t;
    return e0 + 0.00256 * cos(sunset_deg2rad(omega));
}

static inline double sunset__eccentricity(double t) {
    return 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
}

static inline double sunset__geom_mean_anomaly(double t) {
    return 357.52911 + t * (35999.05029 - 0.0001537 * t);
}

static inline double sunset__sun_eq_of_center(double t) {
    double m = sunset_deg2rad(sunset__geom_mean_anomaly(t));
    return sin(m) * (1.914602 - t * (0.004817 + 0.000014 * t))
         + sin(2.0 * m) * (0.019993 - 0.000101 * t)
         + sin(3.0 * m) * 0.000289;
}

static inline double sunset__sun_true_long(double t) {
    return sunset__geom_mean_long_sun(t) + sunset__sun_eq_of_center(t);
}

static inline double sunset__sun_apparent_long(double t) {
    double omega = 125.04 - 1934.136 * t;
    return sunset__sun_true_long(t) - 0.00569 - 0.00478 * sin(sunset_deg2rad(omega));
}

static inline double sunset__sun_declination(double t) {
    double e = sunset__obliquity_correction(t);
    double lambda = sunset__sun_apparent_long(t);
    return sunset_rad2deg(asin(sin(sunset_deg2rad(e)) * sin(sunset_deg2rad(lambda))));
}

static inline double sunset__equation_of_time(double t) {
    double eps = sunset__obliquity_correction(t);
    double l0 = sunset__geom_mean_long_sun(t);
    double e = sunset__eccentricity(t);
    double m = sunset__geom_mean_anomaly(t);
    double y = tan(sunset_deg2rad(eps) / 2.0);
    y *= y;

    double sin2l0 = sin(2.0 * sunset_deg2rad(l0));
    double sinm = sin(sunset_deg2rad(m));
    double cos2l0 = cos(2.0 * sunset_deg2rad(l0));
    double sin4l0 = sin(4.0 * sunset_deg2rad(l0));
    double sin2m = sin(2.0 * sunset_deg2rad(m));

    double Etime = y * sin2l0 - 2.0 * e * sinm + 4.0 * e * y * sinm * cos2l0
                 - 0.5 * y * y * sin4l0 - 1.25 * e * e * sin2m;
    return sunset_rad2deg(Etime) * 4.0;
}

static inline double sunset__hour_angle_sunrise(double lat, double dec) {
    double lr = sunset_deg2rad(lat);
    double dr = sunset_deg2rad(dec);
    return acos(cos(sunset_deg2rad(90.833)) / (cos(lr) * cos(dr)) - tan(lr) * tan(dr));
}

// --- public API ---

// Returns sunrise in minutes from midnight, local time
static inline double sunset_calc_sunrise(const SunsetCalc *s) {
    double t = sunset__time_julian_cent(s->julian_date);
    double eqTime = sunset__equation_of_time(t);
    double dec = sunset__sun_declination(t);
    double ha = sunset__hour_angle_sunrise(s->latitude, dec);
    double delta = s->longitude + sunset_rad2deg(ha);
    double timeUTC = 720.0 - 4.0 * delta - eqTime;

    // second pass for accuracy
    double newt = sunset__time_julian_cent(sunset__jd_from_julian_cent(t) + timeUTC / 1440.0);
    eqTime = sunset__equation_of_time(newt);
    dec = sunset__sun_declination(newt);
    ha = sunset__hour_angle_sunrise(s->latitude, dec);
    delta = s->longitude + sunset_rad2deg(ha);
    timeUTC = 720.0 - 4.0 * delta - eqTime;

    return timeUTC + 60.0 * s->tz_offset;
}

// Returns sunset in minutes from midnight, local time
static inline double sunset_calc_sunset(const SunsetCalc *s) {
    double t = sunset__time_julian_cent(s->julian_date);
    double eqTime = sunset__equation_of_time(t);
    double dec = sunset__sun_declination(t);
    double ha = -sunset__hour_angle_sunrise(s->latitude, dec); // negative for sunset
    double delta = s->longitude + sunset_rad2deg(ha);
    double timeUTC = 720.0 - 4.0 * delta - eqTime;

    double newt = sunset__time_julian_cent(sunset__jd_from_julian_cent(t) + timeUTC / 1440.0);
    eqTime = sunset__equation_of_time(newt);
    dec = sunset__sun_declination(newt);
    ha = -sunset__hour_angle_sunrise(s->latitude, dec);
    delta = s->longitude + sunset_rad2deg(ha);
    timeUTC = 720.0 - 4.0 * delta - eqTime;

    return timeUTC + 60.0 * s->tz_offset;
}

static inline double sunset_sun_declination(const SunsetCalc *s) {
    return sunset__sun_declination(sunset__time_julian_cent(s->julian_date));
}

static inline double sunset_sun_longitude(const SunsetCalc *s) {
    return sunset__sun_true_long(sunset__time_julian_cent(s->julian_date));
}

// Moon phase: returns 0-29 day of lunar cycle from a unix epoch timestamp
static inline int sunset_moon_phase(int from_epoch) {
    int phase = (from_epoch - 614100) % 2551443;
    int res = (int)floor(phase / (24.0 * 3600.0)) + 1;
    return (res == 30) ? 0 : res;
}

#endif // SUNSET_H
