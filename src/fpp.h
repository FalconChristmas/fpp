#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#ifdef PLATFORM_OSX
#define FPP_SOCKET_PATH "/var/tmp/fppd"
#define FPP_SERVER_SOCKET "/var/tmp/fppd/FPPD"
#define FPP_CLIENT_SOCKET "/var/tmp/fppd/FPP"
#else
#define FPP_SOCKET_PATH "/run/fppd"
#define FPP_SERVER_SOCKET "/run/fppd/FPPD"
#define FPP_CLIENT_SOCKET "/run/fppd/FPP"
#endif

#define FPP_SERVER_SOCKET_OLD "/tmp/FPPD"
#define FPP_CLIENT_SOCKET_OLD "/tmp/FPP"
