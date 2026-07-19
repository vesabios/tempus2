// saver_probe.m — run the real .saver bundle in a window and dump a frame.
//
// A screensaver can't be screenshotted (macOS protects that layer), so
// this loads the installed bundle exactly as the system does, drives
// animateOneFrame for a while, and reads the framebuffer back to PNG.
// Verification for a host that is otherwise invisible.
//
//   cc -framework Cocoa -framework OpenGL -o /tmp/saver_probe \
//      tools/saver_probe.m && /tmp/saver_probe out.png [seconds]

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../lib/stb_image_write.h"

int main(int argc, char **argv) { @autoreleasepool {
    const char *out = argc > 1 ? argv[1] : "/tmp/saver.png";
    double secs = argc > 2 ? atof(argv[2]) : 3.0;

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

    NSString *p = [NSHomeDirectory() stringByAppendingPathComponent:
                   @"Library/Screen Savers/Tempus.saver"];
    NSBundle *b = [NSBundle bundleWithPath:p];
    NSError *err = nil;
    if (![b loadAndReturnError:&err]) {
        fprintf(stderr, "load failed: %s\n", [[err description] UTF8String]);
        return 1;
    }
    Class cls = [b principalClass];

    NSRect frame = NSMakeRect(0, 0, 1280, 800);
    NSWindow *win = [[NSWindow alloc]
        initWithContentRect:frame
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered defer:NO];
    [win setLevel:NSNormalWindowLevel];
    [win setBackgroundColor:[NSColor blackColor]];

    id view = [[cls alloc] initWithFrame:frame isPreview:NO];
    [win setContentView:view];
    [win makeKeyAndOrderFront:nil];
    [view startAnimation];

    // Drive it: the tour needs real seconds to fly between stations
    NSDate *until = [NSDate dateWithTimeIntervalSinceNow:secs];
    while ([until timeIntervalSinceNow] > 0) {
        @autoreleasepool {
            [view animateOneFrame];
            [[NSRunLoop currentRunLoop]
                runMode:NSDefaultRunLoopMode
             beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.008]];
        }
    }

    // Read back whatever the saver just presented
    NSOpenGLContext *ctx = [NSOpenGLContext currentContext];
    if (!ctx) { fprintf(stderr, "no GL context — saver never drew\n"); return 2; }
    [ctx makeCurrentContext];
    GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
    int w = vp[2] ? vp[2] : 1280, h = vp[3] ? vp[3] : 800;
    unsigned char *px = malloc((size_t)w * h * 4);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    // GL origin is bottom-left
    unsigned char *flip = malloc((size_t)w * h * 4);
    for (int y = 0; y < h; y++)
        memcpy(flip + (size_t)y * w * 4, px + (size_t)(h - 1 - y) * w * 4,
               (size_t)w * 4);
    for (size_t i = 3; i < (size_t)w * h * 4; i += 4) flip[i] = 255;
    stbi_write_png(out, w, h, 4, flip, w * 4);
    fprintf(stderr, "wrote %s (%dx%d)\n", out, w, h);
    [view stopAnimation];
    return 0;
} }
