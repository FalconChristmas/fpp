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

// Which mDNS implementation this build talks to.  MDNSManager holds the parts
// that are the same either way -- what counts as a new host, and what to do
// with one -- and each backend file implements MDNSManager::StartBackend() and
// ::StopBackend() for its own library.  Exactly one of them compiles anything.
//
// Avahi is tested first on purpose.  A Linux box may also carry
// <dns_sd.h> from avahi-compat-libdns_sd, and on such a box the native client
// is the one to use: the compat shim supports a subset and needs its own
// -ldns_sd.  macOS has Bonjour in the SDK and in libSystem, so it needs no
// package and no extra link flag.
#if __has_include(<avahi-client/client.h>)
#define HAVE_AVAHI 1
#define HAVE_BONJOUR 0
#elif __has_include(<dns_sd.h>)
#define HAVE_AVAHI 0
#define HAVE_BONJOUR 1
#else
#define HAVE_AVAHI 0
#define HAVE_BONJOUR 0
#endif

#include <string>

// The MAC of the first real interface, lowercase hex with no separators, as the
// _wled._tcp "mac=" TXT record wants it.  Empty when none could be read.  Lives
// in MDNSManager.cpp because both backends advertise the same record and the
// way to find it is per-platform, not per-library.
std::string MDNSPrimaryMacAddress();
