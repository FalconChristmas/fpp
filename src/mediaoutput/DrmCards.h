#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2026 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <cstdint>
#include <string>

// One fd per DRM card for all of fppd, held as DRM master for as long as
// anything uses it.
//
// DRM master is exclusive per device.  The KMS framebuffer outputs used to
// take it and drop it around every flip on their own fd, while each media
// pipeline opened a second fd and kept master for the whole track.  On one
// card the two locked each other out: whichever lost got nothing on screen,
// a frozen virtual display during video, or a black video.
//
// Now the card's single fd is master throughout, the framebuffer outputs flip
// on it directly, and each kmssink gets a DRM lease: a separate fd that owns
// just its connector, a CRTC and planes.  The lessee is master of what it
// holds, so kmssink can modeset and flip, while page-flip events stay on
// separate fds -- the framebuffer outputs drain every event on theirs, which
// would swallow kmssink's.  Closing a lessee fd revokes the lease and frees
// whatever GEM handles and framebuffers it held, the cleanup backstop a
// per-track kmssink leak needs.
//
// Callers that touch DRM state hold drmMasterLock (mediaoutput.h).
namespace DrmCards {

// The card's shared fd ("/dev/dri/card1"), opened and made DRM master on the
// first acquire.  Refcounted: every acquire that returns >= 0 needs a matching
// Release().  -1 if the card can't be opened.
int Acquire(const std::string& cardPath);
void Release(const std::string& cardPath);

// Whether `fd` (one Acquire() returned) is DRM master, trying to become it if
// not -- another process may have held master when the card was opened.
bool EnsureMaster(int fd);

// KMS objects (CRTCs, planes, connectors) a framebuffer output drives on the
// card behind `fd`, so a lease never hands them to a kmssink.
void Claim(int fd, uint32_t objectId);
void Unclaim(int fd, uint32_t objectId);
// Claimed by a framebuffer output, or held by a lease.
bool InUse(int fd, uint32_t objectId);
// Held by a media pipeline's kmssink.  A framebuffer output on that connector
// stands aside until it is released.
bool HeldByMedia(int fd, uint32_t objectId);

struct Lease {
    int fd = -1;           // for kmssink's "fd" property
    int planeId = -1;      // for its "plane-id", or -1 to let it choose
    bool isLease = false;  // false: fd is the card's shared fd (no lease support)
    std::string cardPath;
    uint32_t lesseeId = 0;
    int connectorId = 0;
    bool retired = false;  // RetireLease() was called on it
    uint32_t objects[4] = {};
    int objectCount = 0;
};

// A lease on one connector: the CRTC driving it (or a free one that can),
// that CRTC's primary plane, and one overlay plane no one else uses.  Where
// the kernel refuses leases this falls back to the card's shared fd with the
// same plane picked, which is how kmssink ran before.  fd < 0 on failure.
// Every result with fd >= 0 needs a ReleaseLease(), only after the kmssink
// using it is gone.
Lease AcquireLease(const std::string& cardPath, int connectorId);
void ReleaseLease(Lease& lease);

// A lease still in use by a kmssink being torn down on a thread of its own,
// which releases it once the kmssink is gone.  Until then AcquireLease() for
// the same connector waits for that release (for a couple of seconds at
// most), rather than find the connector held and fall back to the shared fd.
void RetireLease(Lease& lease);

} // namespace DrmCards
