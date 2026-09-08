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

#pragma once

#include <string>

// A cape whose signature verifies but which is not attached to identifiable
// hardware -- no i2c EEPROM and no device serial -- is what a signed EEPROM
// image lifted off the board it was licensed to looks like.  Cape detection
// records that state in cape-info.json as `licenseCheck`; it cannot report it,
// because detection runs from fppinit before the network exists.
//
// This is the reporting half: once there is a network, tell the cape vendor
// directly, with what they need to match the cape against their order book.
//
// Lawful basis is legitimate interests -- GDPR Art 6(1)(f), whose Recital 47
// names fraud prevention as exactly such an interest -- and NOT consent.  That
// distinction is the whole reason this code exists: the mechanism it replaces
// deleted the user's FetchVendorLogos value so the page's logo <img> would carry
// the serial out, which is a consent-based setting being overridden to obtain
// the same disclosure.  A legitimate interest cannot be pursued that way, and a
// licence violation does not suspend anyone's data-protection rights.
//
// What Art 6(1)(f) does require is that the interest be balanced against the
// person's, which is why the shape below is what it is: the request is limited
// to what identifies the cape against the vendor's order book and carries
// nothing about the household or the network, it is disclosed in the privacy
// notice, it is logged locally so the device's own owner can see it happened,
// and it is rate-limited rather than continuous.  It is not gated on
// hideExternalURLs: that setting is kiosk mode, hiding links so somebody at the
// kiosk cannot navigate away, and it is not a statement about outbound traffic.
//
// Called from the idle tick.  Cheap and self-rate-limiting: it does nothing at
// all unless the marker is present, and at most one request per retry interval.
void CapeLicenseNotifyBackground();

// The two transports share one state file so that either one succeeding stops
// the other from repeating.  The browser half (www/cape-info.php, recorded
// through /api/cape/licenseCheck) writes the same file.
std::string CapeLicenseStateFile();
