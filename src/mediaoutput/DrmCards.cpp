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

#include "fpp-pch.h"

#include "DrmCards.h"

#include <map>
#include <mutex>
#include <set>

#include "../log.h"
#include "mediaoutput.h"

#if __has_include(<xf86drm.h>)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace DrmCards {

namespace {
struct Card {
    int fd = -1;
    int refCount = 0;
    bool master = false;
    std::set<uint32_t> claimed;  // driven by a framebuffer output
    // Held by a live lease or a lease-less holder, counted per holder: the
    // fallback in AcquireLease() hands a second holder the shared fd for a
    // connector someone already holds, and with a plain set the first
    // holder's release cleared the mark the second was still relying on (its
    // framebuffer output then drew over the video).
    std::map<uint32_t, int> leased;
};
// Guards the table and the per-card sets.  Never held across a blocking DRM
// call that waits on the display.
std::mutex s_lock;
std::map<std::string, Card> s_cards;

Card* CardForFdLocked(int fd) {
    for (auto& [path, card] : s_cards) {
        if (card.fd == fd) {
            return &card;
        }
    }
    return nullptr;
}

int PlaneType(int fd, uint32_t planeId) {
    int type = -1;
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, planeId, DRM_MODE_OBJECT_PLANE);
    if (!props) {
        return type;
    }
    for (uint32_t i = 0; i < props->count_props && type < 0; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(fd, props->props[i]);
        if (prop) {
            if (!strcmp(prop->name, "type")) {
                type = (int)props->prop_values[i];
            }
            drmModeFreeProperty(prop);
        }
    }
    drmModeFreeObjectProperties(props);
    return type;
}
} // namespace

int Acquire(const std::string& cardPath) {
    std::lock_guard<std::mutex> lock(s_lock);
    auto it = s_cards.find(cardPath);
    if (it != s_cards.end()) {
        it->second.refCount++;
        return it->second.fd;
    }
    int fd = open(cardPath.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    Card card;
    card.fd = fd;
    card.refCount = 1;
    // A new fd on a card with no master becomes master by itself; asking
    // covers the case where it didn't.
    card.master = ioctl(fd, DRM_IOCTL_SET_MASTER, 0) == 0;
    if (!card.master) {
        LogWarn(VB_MEDIAOUT, "DRM: %s opened without DRM master (%s); another process holds it\n",
                cardPath.c_str(), strerror(errno));
    }
    s_cards[cardPath] = card;
    LogDebug(VB_MEDIAOUT, "DRM: opened %s fd=%d master=%d\n", cardPath.c_str(), fd, card.master);
    return fd;
}

void Release(const std::string& cardPath) {
    std::lock_guard<std::mutex> lock(s_lock);
    auto it = s_cards.find(cardPath);
    if (it == s_cards.end()) {
        LogWarn(VB_MEDIAOUT, "DRM: release of %s, which was never acquired\n", cardPath.c_str());
        return;
    }
    if (--it->second.refCount > 0) {
        return;
    }
    // Closing it drops master, revokes any lease left over, and frees every
    // handle and framebuffer still open on it.
    close(it->second.fd);
    LogDebug(VB_MEDIAOUT, "DRM: closed %s\n", cardPath.c_str());
    s_cards.erase(it);
}

bool EnsureMaster(int fd) {
    std::lock_guard<std::mutex> lock(s_lock);
    Card* card = CardForFdLocked(fd);
    if (!card) {
        return ioctl(fd, DRM_IOCTL_SET_MASTER, 0) == 0;
    }
    if (!card->master) {
        card->master = ioctl(fd, DRM_IOCTL_SET_MASTER, 0) == 0;
    }
    return card->master;
}

void Claim(int fd, uint32_t objectId) {
    std::lock_guard<std::mutex> lock(s_lock);
    if (Card* card = CardForFdLocked(fd)) {
        card->claimed.insert(objectId);
    }
}

void Unclaim(int fd, uint32_t objectId) {
    std::lock_guard<std::mutex> lock(s_lock);
    if (Card* card = CardForFdLocked(fd)) {
        card->claimed.erase(objectId);
    }
}

bool InUse(int fd, uint32_t objectId) {
    std::lock_guard<std::mutex> lock(s_lock);
    Card* card = CardForFdLocked(fd);
    return card && (card->claimed.count(objectId) || card->leased.count(objectId));
}

bool HeldByMedia(int fd, uint32_t objectId) {
    std::lock_guard<std::mutex> lock(s_lock);
    Card* card = CardForFdLocked(fd);
    return card && card->leased.count(objectId);
}

Lease AcquireLease(const std::string& cardPath, int connectorId) {
    Lease lease;
    lease.cardPath = cardPath;
    if (connectorId <= 0) {
        return lease;
    }
    int fd = Acquire(cardPath);
    if (fd < 0) {
        LogErr(VB_MEDIAOUT, "DRM: cannot open %s: %s\n", cardPath.c_str(), strerror(errno));
        return lease;
    }
    // drmModeCreateLease, and keeping the framebuffer outputs off the card
    // while it is chosen.
    std::unique_lock<std::mutex> masterLock(drmMasterLock);
    EnsureMaster(fd);

    std::unique_lock<std::mutex> lock(s_lock);
    Card& card = s_cards[cardPath];
    auto leasable = [&card](uint32_t id) { return !card.leased.count(id); };
    auto unused = [&card](uint32_t id) { return !card.leased.count(id) && !card.claimed.count(id); };

    // The cached state: a real probe would read the EDID over DDC.
    drmModeConnectorPtr conn = drmModeGetConnectorCurrent(fd, connectorId);
    drmModeResPtr res = drmModeGetResources(fd);
    uint32_t crtcId = 0;
    int crtcIndex = -1;
    if (conn && res && leasable(connectorId)) {
        // The CRTC already driving the connector, even one a framebuffer
        // output on this connector uses (it stands aside while media plays).
        if (conn->encoder_id) {
            if (drmModeEncoderPtr enc = drmModeGetEncoder(fd, conn->encoder_id)) {
                if (enc->crtc_id && leasable(enc->crtc_id)) {
                    crtcId = enc->crtc_id;
                }
                drmModeFreeEncoder(enc);
            }
        }
        // Otherwise one that can drive it and nothing else is using.
        for (int e = 0; !crtcId && e < conn->count_encoders; e++) {
            drmModeEncoderPtr enc = drmModeGetEncoder(fd, conn->encoders[e]);
            if (!enc) {
                continue;
            }
            for (int i = 0; i < res->count_crtcs && !crtcId; i++) {
                if ((enc->possible_crtcs & (1u << i)) && unused(res->crtcs[i])) {
                    crtcId = res->crtcs[i];
                }
            }
            drmModeFreeEncoder(enc);
        }
        for (int i = 0; i < res->count_crtcs; i++) {
            if (res->crtcs[i] == crtcId) {
                crtcIndex = i;
            }
        }
    }

    uint32_t primary = 0, overlay = 0;
    if (crtcIndex >= 0) {
        if (drmModePlaneResPtr planes = drmModeGetPlaneResources(fd)) {
            for (uint32_t i = 0; i < planes->count_planes; i++) {
                uint32_t pid = planes->planes[i];
                drmModePlanePtr plane = drmModeGetPlane(fd, pid);
                if (!plane) {
                    continue;
                }
                if (plane->possible_crtcs & (1u << crtcIndex)) {
                    int type = PlaneType(fd, pid);
                    // The CRTC's own primary, which a modeset needs.
                    if (!primary && type == DRM_PLANE_TYPE_PRIMARY &&
                        plane->possible_crtcs == (1u << crtcIndex) && leasable(pid)) {
                        primary = pid;
                    } else if (!overlay && type == DRM_PLANE_TYPE_OVERLAY && unused(pid)) {
                        // An overlay, not the primary, for the picture:
                        // fbcon may be holding the primary.
                        overlay = pid;
                    }
                }
                drmModeFreePlane(plane);
            }
            drmModeFreePlaneResources(planes);
        }
    }
    if (res) {
        drmModeFreeResources(res);
    }
    if (conn) {
        drmModeFreeConnector(conn);
    }

    lease.planeId = overlay ? (int)overlay : -1;
    if (crtcId) {
        lease.objects[lease.objectCount++] = (uint32_t)connectorId;
        lease.objects[lease.objectCount++] = crtcId;
        if (primary) {
            lease.objects[lease.objectCount++] = primary;
        }
        if (overlay) {
            lease.objects[lease.objectCount++] = overlay;
        }
        uint32_t lessee = 0;
        int lfd = drmModeCreateLease(fd, lease.objects, lease.objectCount, O_CLOEXEC, &lessee);
        if (lfd >= 0) {
            lease.fd = lfd;
            lease.isLease = true;
            lease.lesseeId = lessee;
            for (int i = 0; i < lease.objectCount; i++) {
                card.leased[lease.objects[i]]++;
            }
            LogDebug(VB_MEDIAOUT, "DRM: lease %u on %s: connector %d crtc %u primary %u overlay %u\n",
                     lessee, cardPath.c_str(), connectorId, crtcId, primary, overlay);
            return lease;  // the card's ref is held until ReleaseLease()
        }
        LogWarn(VB_MEDIAOUT, "DRM: lease on %s connector %d refused (%s); kmssink will share the card's fd\n",
                cardPath.c_str(), connectorId, strerror(errno));
    } else {
        LogWarn(VB_MEDIAOUT, "DRM: no free CRTC for %s connector %d; kmssink will share the card's fd\n",
                cardPath.c_str(), connectorId);
    }
    // No lease: kmssink on the shared, master fd, as before leases.  Mark the
    // connector and the overlay it gets as held all the same, so a
    // framebuffer output there stands aside and no one else takes the plane.
    // Each holder counts once, so a release drops only its own hold and a
    // mark another holder of the connector still relies on stays.
    lease.objectCount = 0;
    lease.objects[lease.objectCount++] = (uint32_t)connectorId;
    card.leased[(uint32_t)connectorId]++;
    if (overlay) {
        lease.objects[lease.objectCount++] = overlay;
        card.leased[overlay]++;
    }
    lease.fd = fd;
    return lease;
}

void ReleaseLease(Lease& lease) {
    if (lease.fd < 0) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(s_lock);
        auto it = s_cards.find(lease.cardPath);
        if (it != s_cards.end()) {
            for (int i = 0; i < lease.objectCount; i++) {
                auto held = it->second.leased.find(lease.objects[i]);
                if (held != it->second.leased.end() && --held->second <= 0) {
                    it->second.leased.erase(held);
                }
            }
        }
    }
    if (lease.isLease) {
        // Revokes the lease and frees what kmssink left behind on it.
        close(lease.fd);
    }
    Release(lease.cardPath);
    lease = Lease();
}

} // namespace DrmCards

#else // no libdrm (macOS)

namespace DrmCards {
int Acquire(const std::string&) { return -1; }
void Release(const std::string&) {}
bool EnsureMaster(int) { return false; }
void Claim(int, uint32_t) {}
void Unclaim(int, uint32_t) {}
bool InUse(int, uint32_t) { return false; }
bool HeldByMedia(int, uint32_t) { return false; }
Lease AcquireLease(const std::string& cardPath, int) {
    Lease l;
    l.cardPath = cardPath;
    return l;
}
void ReleaseLease(Lease& lease) { lease = Lease(); }
} // namespace DrmCards

#endif
