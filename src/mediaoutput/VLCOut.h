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

#include "MediaOutputBase.h"

#if __has_include(<vlc/vlc.h>)
#define HAS_VLC

class VLCInternalData;

class VLCOutput : public MediaOutputBase {
public:
    VLCOutput(const std::string& mediaFilename, MediaOutputStatus* status, const std::string& videoOut, int loops = 1);
    virtual ~VLCOutput();

    virtual int Start(int msTime = 0) override;
    virtual int Stop(void) override;
    virtual int Process(void) override;
    virtual int Close(void) override;
    virtual int IsPlaying(void) override;

    virtual int AdjustSpeed(float master) override;

    void SetVolumeAdjustment(int volAdj);

    // events from VLC
    virtual void Starting() {}
    virtual void Playing() {}
    virtual void Stopping() {}
    virtual void Stopped() {}

private:
    VLCInternalData* data;
    bool m_allowSpeedAdjust;
};
#endif
