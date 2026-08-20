#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2025 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

// Push side of the fppd status API: a WebSocket endpoint at /fppdws that
// pushes the /fppd/status payload to connected browsers whenever it changes,
// instead of every page polling api/system/status on a timer.  See
// StatusWebSocket.cpp for the wire format and rationale.
//
// StatusWebSocketInit() must be called once from APIServer::Init(), before
// drogon's event loop starts running.  The explicit call also keeps the
// translation unit (and thus the controller's self-registration) from being
// dropped by the linker.
void StatusWebSocketInit();

// Tear down the push side: unregister the warning listener and drop every
// tracked connection.  Must run while WarningHolder is still alive and BEFORE
// drogon is stopped and before fppd deletes the objects the status payload is
// built from (scheduler, sequence).  Idempotent; safe if Init was never called.
void StatusWebSocketShutdown();
