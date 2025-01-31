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

#include <string>

#include "ThreadedChannelOutput.h"

class SerialChannelOutput {
public:
    SerialChannelOutput();
    virtual ~SerialChannelOutput();

    bool setupSerialPort(Json::Value& config, int baud, const char* mode);
    int closeSerialPort(void);
    void dumpSerialConfig(void);

    int getFD() const { return m_fd; }

protected:
    std::string m_deviceName;
    int m_fd;
};
