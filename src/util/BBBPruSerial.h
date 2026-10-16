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

#include <string>

class PinCapabilities;

// Opens a serial transmitter on a pin with no UART transmit, sent by a PRU
// toggling the pin's GPIO (AM62x only; see src/pru/PRUSerial.asm).  mode is
// SerialOpen()'s: data bits, parity, stop bits, e.g. "8N2".  Returns an fd for
// the Serial*() calls in channeloutput/serialutil.h, or -1 with the reason
// logged and raised as a warning.  The pin stays configured as an output until
// its owner releases it.
int BBBPruSerialOpen(const PinCapabilities& pin, int baud, const char* mode, const std::string& desc);
