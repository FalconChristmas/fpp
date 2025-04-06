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

#include "Sensors.h"
#include "../util/GPIOUtils.h"
#include <array>
#include <mutex>

class MultiSensorSource : public SensorSource {
public:
    MultiSensorSource(Json::Value& config);
    virtual ~MultiSensorSource();

    virtual void Init(std::map<int, std::function<bool(int)>>& callbacks) override;

    virtual void update(bool forceInstant = false) override;
    virtual void enable(int id) override;
    virtual int32_t getValue(int id) override;

    virtual void lockToGroup(int i) override;

    virtual bool isOK() const override { return !sources.empty(); }

private:
    std::vector<SensorSource*> sources;
    std::vector<int32_t> channels;
};
