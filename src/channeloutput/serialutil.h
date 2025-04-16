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

int SerialOpen(const char* device, int baud, const char* mode, bool output = true);
int SerialOpen(const char* device, int baud, const char* mode, bool output, bool configurePin, const char* desc);
int SerialClose(int fd);
int SerialSendBreak(int fd, int duration);
int SerialResetRTS(int fd);
