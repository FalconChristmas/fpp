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

#include <cstdint>

// A serial port that is not a kernel tty: a PRU sending on a GPIO pin, for a
// header pin with no UART transmit.  SerialAttachBackend() takes ownership and
// returns an fd that stands for it in the calls below, so an output written
// against them drives either kind of port.  SerialClose() releases it; it is
// deleted once no call is still using it.
class SerialPortBackend {
public:
    virtual ~SerialPortBackend() {}
    // queues up to len bytes, waiting up to timeoutMs for room; returns the
    // number queued
    virtual int write(const uint8_t* data, int len, int timeoutMs) = 0;
    // holds the line low for us microseconds, then marks before what follows
    virtual void sendBreak(int us) = 0;
    virtual bool waitDrained(int timeoutMs) = 0;
};
int SerialAttachBackend(SerialPortBackend* backend);

int SerialOpen(const char* device, int baud, const char* mode, bool output = true);
int SerialOpen(const char* device, int baud, const char* mode, bool output, bool configurePin, const char* desc);
int SerialClose(int fd);
// True when device (after resolving links) is the active kernel console.
bool SerialIsConsole(const char* device);
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
// write() for an output that must not wait: sends what fits now and returns
// the bytes taken, as a plain write() to a non-blocking tty does
int SerialWrite(int fd, const void* data, int len);
