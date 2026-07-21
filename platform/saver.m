// saver.m — TEMPVS as a macOS screensaver.
//
// The same instrument the app runs, hosted in a ScreenSaverView on its
// own OpenGL context. No annunciator, no debug panel, no pointer: a
// screensaver is the instrument left alone, touring its stations.
//
// Screensavers run inside a sandboxed process on modern macOS, so the
// app's ~/.config file is unreachable. Settings live in
// ScreenSaverDefaults instead, edited through the Options sheet.

#import <ScreenSaver/ScreenSaver.h>
#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>

#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "../lib/sokol_gfx.h"
#include "../lib/sokol_log.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../lib/stb_image.h"

#include "../core/scene.h"
#include "../core/stage.h"
#include "../core/chrome.h"
#include "../core/globe.h"
#include "../shaders/tempus.glsl.h"

static Tempus  g_tempus;
static Scene   g_scene;
static DrawCtx g_draw;

// Assets ride inside the bundle; resolve against its Resources.
static const char *tempus_asset_path(const char *name) {
    static char buf[1024];
    NSBundle *b = [NSBundle bundleForClass:NSClassFromString(@"TempusSaverView")];
    NSString *p = [b pathForResource:[NSString stringWithUTF8String:name]
                              ofType:nil];
    if (!p) { snprintf(buf, sizeof buf, "assets/%s", name); return buf; }
    snprintf(buf, sizeof buf, "%s", [p UTF8String]);
    return buf;
}

#include "tempus_gfx.h"

// ---- Flying between stations ----
// The app's set_worldview, reduced to what a screensaver needs: the
// station's own blend rises, every other falls, and the base machine
// morph flies to the station's declared targets unless it parks.
static void saver__tween(double *v, double to, double dur) {
    tween_cancel_target(&g_scene.tweens, v);
    if (dur <= 0.0) { *v = to; return; }
    tween_start_delayed(&g_scene.tweens, v, *v, to, 0.0, dur,
                        EASE_IN_OUT_CUBIC);
}

static void saver_set_station(Scene *sc, int st, double dur) {
    double *blends[ST_COUNT] = {
        [ST_CAELVM]   = &sc->sky_blend,
        [ST_HORAE]    = &sc->horae_blend,
        [ST_ROTAE]    = &sc->rotae_blend,
        [ST_SAECVLVM] = &sc->saec_blend,
        [ST_OFFICIVM] = &sc->offic_blend,
        [ST_DRACO]    = &sc->draco_blend,
        [ST_ORBIS]    = &sc->orbis_blend,
        [ST_ASTROLAB] = &sc->astro_blend,
    };
    for (int i = 0; i < ST_COUNT; i++)
        if (blends[i]) saver__tween(blends[i], i == st ? 1.0 : 0.0, dur);
    saver__tween(&sc->orbis_wheel, st == ST_ORBIS ? 1.0 : 0.0, dur);
    if (!station_table[st].park_machine) {
        saver__tween(&sc->helio_blend, station_table[st].fly_helio, dur);
        saver__tween(&sc->system_blend, station_table[st].fly_system, dur);
        // The saver is ALL tour and has no user, so the wheel's zoom
        // is the station's to drive — the persistent-user-state rule
        // is the standalone's alone.
        sc->calendar_state.target_zoom = station_table[st].fly_zoom;
        saver__tween(&sc->calendar_state.zoom,
                     station_table[st].fly_zoom, dur);
    }
}

// ---- The tour: the instrument shows itself ----
// The app's idle attractor, here as the whole behaviour. Physical
// instruments first, then the dials — the annunciator's own order.
static const int g_tour[] = {
    ST_HOROLOGIVM, ST_ORBIS, ST_TELLVS, ST_MACHINA,
    ST_ASTROLAB, ST_CAELVM, ST_DRACO, ST_HORAE, ST_OFFICIVM,
};
#define TOUR_N ((int)(sizeof(g_tour) / sizeof(g_tour[0])))

// macOS builds a SECOND view for the full-size preview (and one per
// display when it runs for real). The instrument — core, scene, and
// the whole GPU layer — is process-global, so it boots exactly once
// and lives on ONE context; whichever view is animating owns the
// drawable. Booting per-view called sg_setup twice and drew nothing.
static NSOpenGLContext *g_ctx = nil;
static double g_clock = 0;      // the instrument's own elapsed seconds
static BOOL g_booted = NO;
static __weak id g_activeView = nil;

// The tour belongs to the INSTRUMENT, not to a view. As per-view
// ivars these were only ever set by the first view to boot, so the
// full-size view ran with tour off and a zero dwell — no touring at
// all (Seren). Same for the observer: read live, so the Options sheet
// reaches a running saver instead of only the next launch.
static BOOL   g_tourOn = YES;
static double g_dwell = 12.0;
static int    g_tourIdx = 0;
static double g_tourTimer = 0;
static double g_prefsTimer = 1e9;   // force a read on the first frame

@interface TempusSaverView : ScreenSaverView {
    double _last;
    NSWindow *_sheet;
    NSPopUpButton *_cityPop;
    NSTextField *_latField, *_lonField;
    NSButton *_tourCheck;
    NSSlider *_dwellSlider;
}
@end

@implementation TempusSaverView

+ (ScreenSaverDefaults *)prefs {
    return [ScreenSaverDefaults defaultsForModuleWithName:@"com.vesabios.tempus"];
}

- (instancetype)initWithFrame:(NSRect)frame isPreview:(BOOL)isPreview {
    self = [super initWithFrame:frame isPreview:isPreview];
    if (!self) return nil;
    [self setAnimationTimeInterval:1.0 / 60.0];
    [self setWantsBestResolutionOpenGLSurface:YES];
    return self;
}

- (void)ensureContext {
    if (g_ctx) return;
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFAAccelerated,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        0
    };
    NSOpenGLPixelFormat *pf =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    g_ctx = [[NSOpenGLContext alloc] initWithFormat:pf shareContext:nil];
    GLint swap = 1;
    [g_ctx setValues:&swap
        forParameter:NSOpenGLContextParameterSwapInterval];
}

// Take the drawable: the newest animating view wins it.
- (void)claimContext {
    [self ensureContext];
    if ([g_ctx view] != self) {
        [g_ctx clearDrawable];
        [g_ctx setView:self];
    }
    [g_ctx update];
}

// The system time zone names a city — "Europe/Berlin",
// "Australia/Sydney" — and the instrument already carries 1251 of
// them. Match the two and the saver knows where it stands, with no
// permission prompt (CoreLocation can't meaningfully ask from inside
// a screensaver). Falls back to the zone's offset for longitude,
// which at least puts the sun in the right quarter of the sky.
static BOOL tempus_auto_place(double *lat, double *lon) {
    NSString *zone = [[NSTimeZone systemTimeZone] name];
    NSArray *parts = [zone componentsSeparatedByString:@"/"];
    NSString *city = [[parts lastObject]
                      stringByReplacingOccurrencesOfString:@"_"
                                               withString:@" "];
    const char *want = [city UTF8String];
    if (want) {
        for (int i = 0; i < CITY_NUM; i++) {
            if (strcasecmp(city_pts[i].name, want) == 0) {
                *lat = city_pts[i].lat * 0.01;
                *lon = city_pts[i].lon * 0.01;
                return YES;
            }
        }
    }
    double off = [[NSTimeZone systemTimeZone] secondsFromGMT] / 3600.0;
    *lon = off * 15.0;
    *lat = 0.0;
    return NO;
}

+ (void)registerDefaults {
    double la = 51.4779, lo = -0.0015;
    tempus_auto_place(&la, &lo);
    [[TempusSaverView prefs] registerDefaults:@{
        @"latitude":  @(la),
        @"longitude": @(lo),
        @"auto":      @YES,
        @"tour":      @YES,
        @"dwell":     @12.0,
    }];
}

// Read the Options sheet's settings and apply them to a LIVE
// instrument. The sheet runs in another process, so the running saver
// has to re-read rather than be told.
- (void)applyDefaults {
    [TempusSaverView registerDefaults];
    ScreenSaverDefaults *d = [TempusSaverView prefs];
    [d synchronize];
    g_tourOn = [d boolForKey:@"tour"];
    g_dwell  = [d doubleForKey:@"dwell"];
    if (g_dwell < 3.0) g_dwell = 3.0;
    double la = [d doubleForKey:@"latitude"];
    double lo = [d doubleForKey:@"longitude"];
    if ([d boolForKey:@"auto"]) tempus_auto_place(&la, &lo);
    if (g_booted && (la != g_tempus.config.latitude
                     || lo != g_tempus.config.longitude))
        tempus_set_location(&g_tempus, la, lo);
}

- (TempusConfig)configFromDefaults {
    [TempusSaverView registerDefaults];
    ScreenSaverDefaults *d = [TempusSaverView prefs];
    TempusConfig cfg = tempus_default_config();
    cfg.latitude  = [d doubleForKey:@"latitude"];
    cfg.longitude = [d doubleForKey:@"longitude"];
    if ([d boolForKey:@"auto"])
        tempus_auto_place(&cfg.latitude, &cfg.longitude);
    cfg.timezone_auto = true;
    return cfg;
}

- (void)bootIfNeeded {
    if (g_booted) return;
    sg_setup(&(sg_desc){ .logger.func = slog_func });

    TempusConfig cfg = [self configFromDefaults];
    tempus_init(&g_tempus, cfg);
    g_tempus.override_year = g_tempus.year;
    g_tempus.override_day_pct = g_tempus.percent_of_day;

    scene_init(&g_scene, &g_tempus);
    scene_register_view(&g_scene, VIEW_CLOCK,     &clock_vtable);
    scene_register_view(&g_scene, VIEW_CALENDAR,  &calendar_vtable);
    scene_register_view(&g_scene, VIEW_SOLAR,     &solar_vtable);
    scene_register_view(&g_scene, VIEW_ORRERY,    &orrery_vtable);
    scene_register_view(&g_scene, VIEW_SKY,       &sky_vtable);
    scene_register_view(&g_scene, VIEW_HORAE,     &horae_vtable);
    scene_register_view(&g_scene, VIEW_ROTAE,     &rotae_vtable);
    scene_register_view(&g_scene, VIEW_SAEC,      &saec_vtable);
    scene_register_view(&g_scene, VIEW_ORBIS,     &orbis_vtable);
    scene_register_view(&g_scene, VIEW_LVMEN,     &lumen_vtable);
    scene_register_view(&g_scene, VIEW_OFFIC,     &offic_vtable);
    scene_register_view(&g_scene, VIEW_DRACO,     &draco_vtable);
    scene_register_view(&g_scene, VIEW_ASTRO,     &astro_vtable);
    scene_register_view(&g_scene, VIEW_CLOCKBACK, &clockback_vtable);
    scene_register_view(&g_scene, VIEW_CALBACK,   &calback_vtable);
    scene_init_views(&g_scene, &g_tempus);

    scene_add_layer(&g_scene, VIEW_CALBACK);
    scene_add_layer(&g_scene, VIEW_SOLAR);
    scene_add_layer(&g_scene, VIEW_CLOCKBACK);
    scene_add_layer(&g_scene, VIEW_ORRERY);
    scene_add_layer(&g_scene, VIEW_SKY);
    scene_add_layer(&g_scene, VIEW_DRACO);
    scene_add_layer(&g_scene, VIEW_ASTRO);
    scene_add_layer(&g_scene, VIEW_LVMEN);
    scene_add_layer(&g_scene, VIEW_CLOCK);
    scene_add_layer(&g_scene, VIEW_HORAE);
    scene_add_layer(&g_scene, VIEW_ROTAE);
    scene_add_layer(&g_scene, VIEW_SAEC);
    scene_add_layer(&g_scene, VIEW_ORBIS);
    scene_add_layer(&g_scene, VIEW_OFFIC);
    // The wheel is the instrument's frame: above everything
    scene_add_layer(&g_scene, VIEW_CALENDAR);

    draw_init(&g_draw);
    tempus_gfx_init();

    saver_set_station(&g_scene, g_tour[0], 0.0);
    g_tourIdx = 0;
    g_tourTimer = 0;
    g_booted = YES;
}

- (void)startAnimation {
    [super startAnimation];
    g_activeView = self;
    [self claimContext];
    [self applyDefaults];
    _last = 0;
}

- (void)stopAnimation {
    [super stopAnimation];
    if (g_activeView == self) g_activeView = nil;
}

- (void)animateOneFrame {
    // Only the view that owns the drawable draws; the others would
    // fight over it every frame
    if (g_activeView && g_activeView != self) return;
    if (!g_activeView) g_activeView = self;
    [self claimContext];
    [g_ctx makeCurrentContext];
    [self bootIfNeeded];

    NSRect bounds = [self bounds];
    NSRect back = [self convertRectToBacking:bounds];
    float w = (float)back.size.width, h = (float)back.size.height;

    double now = [NSDate timeIntervalSinceReferenceDate];
    double dt = _last > 0 ? now - _last : 1.0 / 60.0;
    if (dt > 0.5) dt = 0.5;
    _last = now;
    g_clock += dt;

    // Options may have changed in another process; look now and then
    g_prefsTimer += dt;
    if (g_prefsTimer > 2.0) { g_prefsTimer = 0; [self applyDefaults]; }

    // The tour: fly, dwell, fly on
    if (g_tourOn) {
        g_tourTimer += dt;
        if (g_tourTimer >= g_dwell) {
            g_tourTimer = 0;
            g_tourIdx = (g_tourIdx + 1) % TOUR_N;
            saver_set_station(&g_scene, g_tour[g_tourIdx], 3.5);
        }
    }

    tempus_update(&g_tempus, g_clock);
    scene_update(&g_scene, &g_tempus, dt);
    tempus_stage_views(&g_scene, g_tour[g_tourIdx]);

    // The system stage pulls the camera back; CAELVM eases it home
    float scale = h / 1280.0f * tempus_camera_scale(&g_scene);
    draw_begin(&g_draw, w, h);
    g_draw.sx = scale;
    g_draw.sy = scale;
    scene_render(&g_scene, &g_draw, &g_tempus);

    // The register rides chrome space: no camera pull, so it holds
    // still while the instrument flies
    g_draw.sx = g_draw.sy = h / 1280.0f;
    g_draw.tx = g_draw.ty = 0;
    tempus_chrome_readout(&g_draw, &g_tempus, w, h);

    if (g_draw.num_verts > 0) {
        sg_update_buffer(g_vbuf, &(sg_range){
            .ptr = g_draw.verts,
            .size = (size_t)g_draw.num_verts * sizeof(DrawVertex) });
        sg_update_buffer(g_ibuf, &(sg_range){
            .ptr = g_draw.indices,
            .size = (size_t)g_draw.num_indices * sizeof(uint32_t) });
    }

    sg_begin_pass(&(sg_pass){
        .action = g_pass_action,
        .swapchain = {
            .width = (int)w, .height = (int)h,
            .sample_count = 1,
            .color_format = SG_PIXELFORMAT_RGBA8,
            .depth_format = SG_PIXELFORMAT_DEPTH_STENCIL,
            .gl.framebuffer = 0,
        },
    });
    tempus_gfx_draw(&g_draw, w, h);
    sg_end_pass();
    sg_commit();

    [g_ctx flushBuffer];
}

- (void)drawRect:(NSRect)rect {
    [[NSColor blackColor] set];
    NSRectFill(rect);
}

- (BOOL)hasConfigureSheet { return YES; }

- (NSWindow *)configureSheet {
    if (_sheet) return _sheet;
    ScreenSaverDefaults *d = [TempusSaverView prefs];

    NSRect r = NSMakeRect(0, 0, 460, 260);
    _sheet = [[NSWindow alloc] initWithContentRect:r
                styleMask:NSWindowStyleMaskTitled
                  backing:NSBackingStoreBuffered defer:NO];
    NSView *v = [_sheet contentView];

    NSTextField *title = [NSTextField labelWithString:@"T E M P V S"];
    [title setFrame:NSMakeRect(24, 210, 300, 24)];
    [title setFont:[NSFont systemFontOfSize:15 weight:NSFontWeightLight]];
    [v addSubview:title];

    NSTextField *hint = [NSTextField labelWithString:
        @"The instrument keeps time for a place. Choose yours."];
    [hint setFrame:NSMakeRect(24, 188, 420, 18)];
    [hint setFont:[NSFont systemFontOfSize:11]];
    [hint setTextColor:[NSColor secondaryLabelColor]];
    [v addSubview:hint];

    // City picker — the same charted cities the readout names
    NSTextField *cl = [NSTextField labelWithString:@"Place"];
    [cl setFrame:NSMakeRect(24, 150, 60, 20)];
    [v addSubview:cl];
    _cityPop = [[NSPopUpButton alloc]
                initWithFrame:NSMakeRect(90, 146, 250, 26) pullsDown:NO];
    [_cityPop addItemWithTitle:@"Automatic (from time zone)"];
    [_cityPop addItemWithTitle:@"Custom…"];
    for (int i = 0; i < CITY_NUM; i++)
        [_cityPop addItemWithTitle:
            [NSString stringWithUTF8String:city_pts[i].name]];
    [_cityPop setTarget:self];
    [_cityPop setAction:@selector(cityPicked:)];
    [v addSubview:_cityPop];

    NSTextField *ll = [NSTextField labelWithString:@"Latitude"];
    [ll setFrame:NSMakeRect(24, 112, 60, 20)];
    [v addSubview:ll];
    _latField = [[NSTextField alloc] initWithFrame:NSMakeRect(90, 108, 90, 22)];
    [_latField setDoubleValue:[d doubleForKey:@"latitude"]];
    [v addSubview:_latField];

    NSTextField *ol = [NSTextField labelWithString:@"Longitude"];
    [ol setFrame:NSMakeRect(196, 112, 70, 20)];
    [v addSubview:ol];
    _lonField = [[NSTextField alloc] initWithFrame:NSMakeRect(268, 108, 90, 22)];
    [_lonField setDoubleValue:[d doubleForKey:@"longitude"]];
    [v addSubview:_lonField];

    _tourCheck = [NSButton checkboxWithTitle:@"Tour the stations"
                                      target:nil action:nil];
    [_tourCheck setFrame:NSMakeRect(90, 74, 200, 20)];
    [_tourCheck setState:[d boolForKey:@"tour"] ? NSControlStateValueOn
                                                : NSControlStateValueOff];
    [v addSubview:_tourCheck];

    NSTextField *dl = [NSTextField labelWithString:@"Dwell"];
    [dl setFrame:NSMakeRect(24, 44, 60, 20)];
    [v addSubview:dl];
    _dwellSlider = [NSSlider sliderWithValue:[d doubleForKey:@"dwell"]
                                    minValue:5 maxValue:60
                                      target:nil action:nil];
    [_dwellSlider setFrame:NSMakeRect(90, 42, 250, 20)];
    [v addSubview:_dwellSlider];

    NSButton *ok = [NSButton buttonWithTitle:@"Done" target:self
                                      action:@selector(sheetDone:)];
    [ok setFrame:NSMakeRect(356, 8, 90, 30)];
    [ok setKeyEquivalent:@"\r"];
    [v addSubview:ok];

    NSButton *cancel = [NSButton buttonWithTitle:@"Cancel" target:self
                                          action:@selector(sheetCancel:)];
    [cancel setFrame:NSMakeRect(260, 8, 90, 30)];
    [v addSubview:cancel];

    return _sheet;
}

- (void)cityPicked:(id)sender {
    NSInteger sel = [_cityPop indexOfSelectedItem];
    if (sel == 0) {                     // Automatic
        double la, lo;
        tempus_auto_place(&la, &lo);
        [_latField setDoubleValue:la];
        [_lonField setDoubleValue:lo];
        return;
    }
    NSInteger i = sel - 2;              // 0 = Automatic, 1 = Custom
    if (i < 0 || i >= CITY_NUM) return;
    [_latField setDoubleValue:city_pts[i].lat * 0.01];
    [_lonField setDoubleValue:city_pts[i].lon * 0.01];
}

- (void)sheetDone:(id)sender {
    ScreenSaverDefaults *d = [TempusSaverView prefs];
    [d setBool:([_cityPop indexOfSelectedItem] == 0) forKey:@"auto"];
    [d setDouble:[_latField doubleValue] forKey:@"latitude"];
    [d setDouble:[_lonField doubleValue] forKey:@"longitude"];
    [d setBool:([_tourCheck state] == NSControlStateValueOn) forKey:@"tour"];
    [d setDouble:[_dwellSlider doubleValue] forKey:@"dwell"];
    [d synchronize];
    g_prefsTimer = 1e9;   // apply on the next frame
    [[NSApplication sharedApplication] endSheet:_sheet];
}

- (void)sheetCancel:(id)sender {
    [[NSApplication sharedApplication] endSheet:_sheet];
}

@end
