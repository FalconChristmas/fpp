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

#include <functional>
#include <map>
#include <string>

#include "MultiSync.h"

class NetworkController {
public:
    NetworkController(const std::string& ipStr);
    ~NetworkController(){};

    // Works out what is answering at `ip` from the page it served, asking the
    // device follow-up questions where the HTML alone cannot tell (most vendors
    // need one; the Experience line needs up to three, in sequence).  Those all
    // go through CurlManager, so nothing here blocks.
    //
    // `callback` is invoked exactly once, with a heap-allocated
    // NetworkController the callback owns and must delete, or nullptr if
    // nothing claimed the device.  It may run inline, before this returns --
    // that is the normal path for the vendors the HTML alone identifies
    // (SanDevices, AlphaPix, HinksPix, DIYLEDExpress) and for a page no
    // detector recognised, neither of which needs a request.  So a caller
    // keeping an in-flight count must not hold a lock across the call.
    static void DetectControllerViaHTML(const std::string& ip, const std::string& html,
                                        std::function<void(NetworkController*)>&& callback);

    std::string ip;
    std::string hostname;
    std::string vendor;
    std::string vendorURL;
    systemType typeId;
    std::string typeStr;
    std::string ranges;
    std::string version;
    std::string uuid;
    // Identities this device reported for OTHER addresses.  Some vendors
    // describe their neighbours but not themselves, so an identity can only be
    // learned second-hand; the caller applies these to systems discovery has
    // already found.  Keyed by address.
    std::map<std::string, std::string> peerUUIDs;
    unsigned int majorVersion;
    unsigned int minorVersion;
    FPPMode systemMode;
    bool sendingMultiSync = false;

private:
    // One in-progress detection: the page, the controller being filled in, and
    // how far down the detector list we have got.  Heap-allocated for the life
    // of the walk because a detector that has to ask the device something
    // resumes in a curl callback long after its caller returned.  Defined in
    // NetworkController.cpp.
    class Detection;
    friend class Detection;

    // Each detector decides from the HTML whether the device is plausibly its
    // vendor, and where that is not conclusive asks the device.  Exactly one of
    // st->matched(), st->noMatch() or st->fetch() must be reached on every path
    // -- anything else strands the Detection and leaks it.
    void DetectFalconController(Detection* st);
    void DetectSanDevicesController(Detection* st);
    void DetectESPixelStickController(Detection* st);
    void DetectBaldrickController(Detection* st);
    void ParseBaldrickVersion(const std::string& fw);
    void DetectAlphaPixController(Detection* st);
    void DetectHinksPixController(Detection* st);
    void DetectDIYLEDExpressController(Detection* st);
    void DetectWLEDController(Detection* st);
    void DetectExperienceController(Detection* st);
    void DetectFPP(Detection* st);

    void DumpControllerInfo(void);
};
