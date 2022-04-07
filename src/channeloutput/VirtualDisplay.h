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

#include "VirtualDisplayBase.h"
#include "overlays/PixelOverlayModel.h"

class VirtualDisplayOutput : public VirtualDisplayBaseOutput {
public:
    VirtualDisplayOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~VirtualDisplayOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int SendData(unsigned char* channelData) override;

private:
    std::string m_modelName;
    PixelOverlayModel* m_model = nullptr;
};
