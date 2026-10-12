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

// The ports are opened O_NONBLOCK, so a plain write() of a frame larger than
// the room left in the tty buffer sends part of it, and the next frame then
// follows a truncated one.  SerialWriteAll() waits (up to timeoutMs) for room
// until all of buf is written; it returns the bytes written, short only on an
// error or the timeout.  For output worker threads, which can afford to wait.
int SerialWriteAll(int fd, const void* data, int len, int timeoutMs);
// Waits (up to timeoutMs) for everything queued on the tty to be sent.
// Returns false on timeout.
bool SerialWaitDrained(int fd, int timeoutMs);
int SerialResetRTS(int fd);
