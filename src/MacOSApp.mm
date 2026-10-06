/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

// See MacOSApp.h. Self-contained on purpose: Objective-C++ cannot use the C++
// precompiled header, so this file includes no FPP headers.

#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

#include <atomic>
#include <map>
#include <memory>

#include "MacOSApp.h"

static std::atomic<bool> s_active{ false };

bool MacOSAppActive() {
    return s_active;
}

bool MacOSAppInit() {
    if (s_active) {
        return true;
    }
    if (![NSThread isMainThread]) {
        return false;
    }
    // Only in a GUI login session. Started over ssh, or with nobody logged in,
    // there is no window server to talk to, and AppKit is better left alone.
    CFDictionaryRef session = CGSessionCopyCurrentDictionary();
    if (!session) {
        return false;
    }
    CFRelease(session);

    @autoreleasepool {
        [NSApplication sharedApplication];
        // No Dock icon or menu bar: fppd is a daemon that happens to own a window.
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        [NSApp finishLaunching];
    }
    s_active = true;
    return true;
}

// ---- The main-thread wait -------------------------------------------------

static CFFileDescriptorRef s_waitFd = nullptr;
static CFRunLoopSourceRef s_waitSource = nullptr;
static int s_waitFdNum = -1;
static bool s_fdReady = false;

static void fdReadable(CFFileDescriptorRef, CFOptionFlags, void*) {
    s_fdReady = true;
    // nextEventMatchingMask only returns for an event (or its deadline), so
    // post one to end the wait now rather than at the timeout.
    NSEvent* wake = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                       location:NSZeroPoint
                                  modifierFlags:0
                                      timestamp:0
                                   windowNumber:0
                                        context:nil
                                        subtype:0
                                          data1:0
                                          data2:0];
    [NSApp postEvent:wake atStart:YES];
}

int MacOSWaitForFd(int fd, int timeoutMs) {
    if (!s_active || ![NSThread isMainThread]) {
        return -1;
    }
    if (fd != s_waitFdNum) {
        if (s_waitSource) {
            CFRunLoopRemoveSource(CFRunLoopGetMain(), s_waitSource, kCFRunLoopDefaultMode);
            CFRelease(s_waitSource);
            CFFileDescriptorInvalidate(s_waitFd);
            CFRelease(s_waitFd);
        }
        s_waitFd = CFFileDescriptorCreate(nullptr, fd, false, fdReadable, nullptr);
        s_waitSource = CFFileDescriptorCreateRunLoopSource(nullptr, s_waitFd, 0);
        CFRunLoopAddSource(CFRunLoopGetMain(), s_waitSource, kCFRunLoopDefaultMode);
        s_waitFdNum = fd;
    }

    @autoreleasepool {
        s_fdReady = false;
        // One-shot: re-armed for every wait.
        CFFileDescriptorEnableCallBacks(s_waitFd, kCFFileDescriptorReadCallBack);
        NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:timeoutMs / 1000.0];
        while (!s_fdReady) {
            NSEvent* ev = [NSApp nextEventMatchingMask:NSEventMaskAny
                                             untilDate:deadline
                                                inMode:NSDefaultRunLoopMode
                                               dequeue:YES];
            if (!ev) {
                break; // timed out
            }
            if ([ev type] != NSEventTypeApplicationDefined) {
                [NSApp sendEvent:ev];
            }
        }
    }
    return s_fdReady ? 1 : 0;
}

// ---- Video windows --------------------------------------------------------

static std::map<int, NSWindow*> s_videoWindows;
static std::map<int, int> s_videoWindowGeneration;

// Run on the main thread, which spends its idle time in MacOSWaitForFd() and
// so picks this up promptly. Bounded, because a caller that the main thread is
// itself waiting on would otherwise deadlock; on timeout the block still runs
// later, harmlessly, and the caller carries on without a window.
static bool runOnMainThread(void (^block)(void), int timeoutMs) {
    if ([NSThread isMainThread]) {
        block();
        return true;
    }
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    dispatch_async(dispatch_get_main_queue(), ^{
        block();
        dispatch_semaphore_signal(done);
    });
    return dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, (int64_t)timeoutMs * NSEC_PER_MSEC)) == 0;
}

void* MacOSShowVideoWindow(int slot) {
    if (!s_active) {
        return nullptr;
    }
    auto view = std::make_shared<std::atomic<void*>>(nullptr);
    bool ran = runOnMainThread(^{
        NSWindow* window = s_videoWindows[slot];
        if (!window) {
            window = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 960, 540)
                                                 styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
            // No close button: the sink renders into this view for as long as
            // the pipeline runs, so it must not go away underneath it.
            [window setReleasedWhenClosed:NO];
            [window setBackgroundColor:[NSColor blackColor]];
            NSString* title = slot > 1 ? [NSString stringWithFormat:@"FPP Video Output (slot %d)", slot]
                                       : @"FPP Video Output";
            [window setTitle:title];
            // macOS saves the frame under this name whenever the window moves
            // or resizes, and applies the saved frame here, so the window
            // always comes back where it was last left.
            [window setFrameAutosaveName:[NSString stringWithFormat:@"FPP Video Output %d", slot]];
            s_videoWindows[slot] = window;
        }
        s_videoWindowGeneration[slot]++; // cancels any pending hide
        [window orderFrontRegardless];
        view->store((__bridge void*)[window contentView]);
    },
                               2000);
    return ran ? view->load() : nullptr;
}

void MacOSHideVideoWindowSoon(int slot) {
    if (!s_active) {
        return;
    }
    dispatch_async(dispatch_get_main_queue(), ^{
        int generation = s_videoWindowGeneration[slot];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
            auto it = s_videoWindows.find(slot);
            if (it != s_videoWindows.end() && s_videoWindowGeneration[slot] == generation) {
                [it->second orderOut:nil];
            }
        });
    });
}
