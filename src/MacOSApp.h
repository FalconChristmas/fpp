#pragma once
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

// macOS only: lets fppd own on-screen windows (the video output window).
//
// AppKit requires an NSApplication whose events are pumped on the MAIN thread;
// GStreamer's macOS video sinks hang or never show a window without one. fppd's
// main thread already sits in one wait - EPollManager::waitForEvents() - so on
// macOS that wait pumps Cocoa instead of blocking in kevent(). Nothing else in
// fppd changes threads.
//
// Plain C++ declarations so ordinary .cpp files can call them; the
// implementation is Objective-C++ (MacOSApp.mm).

// Start Cocoa if this process is in a GUI login session (the LaunchAgent fppd
// runs as). Headless runs - ssh, no console user - are left exactly as before.
// Must be called on the main thread. Returns whether Cocoa is now active.
bool MacOSAppInit();
bool MacOSAppActive();

// Wait up to timeoutMs for `fd` to become readable while servicing Cocoa
// events and main-queue work. Returns 1 if fd became readable, 0 on timeout,
// and -1 without waiting when it does not apply (off the main thread, or Cocoa
// not active) - the caller then waits as it always has.
int MacOSWaitForFd(int fd, int timeoutMs);

// The view of the video window for a stream slot, shown and ready, for
// GstVideoOverlay::set_window_handle(). The window is created on first use and
// its frame is saved by macOS (NSUserDefaults, keyed per slot), so it reopens
// where it was last left. Call BEFORE the pipeline starts, never from a
// GStreamer bus handler: GStreamer posts the window-handle request while
// holding its GL display lock, and the main thread needs that same lock to
// draw. Returns nullptr when Cocoa is not active.
void* MacOSShowVideoWindow(int slot);

// Hide a slot's video window once playback ends. Deferred, and cancelled by a
// MacOSShowVideoWindow() for the same slot, so the window does not flicker
// between consecutive playlist items.
void MacOSHideVideoWindowSoon(int slot);
