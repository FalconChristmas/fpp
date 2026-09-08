/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the CC-BY-ND as described in the
 * included LICENSE.CC-BY-ND file.  This file may be modified for
 * personal use, but modified copies MAY NOT be redistributed in any form.
 */

#include <cstdint>
#include "fpp-json-fwd.h"
#include <string>
#include <vector>

class CapeInfo;
namespace Json
{
    class Value;
};

class CapeUtils {
public:
    static CapeUtils INSTANCE;

    enum class CapeStatus {
        NOT_PRESENT,
        CORRUPT,
        UNSIGNED,
        SIGNED_GENERIC,
        SIGNED
    };

    CapeStatus initCape(bool readOnly = true, bool forceDefaults = false);

    // Ask what an EEPROM's defaultSettings WOULD do under a given privacy
    // jurisdiction, without doing any of it.
    //
    // The setup wizard needs this because cape detection runs at boot, before
    // anyone has been asked where they are, so a cape's telemetry defaults are
    // held rather than applied. Once the user answers, the wizard needs to know
    // which of those are now permitted -- and it must find out without writing to
    // the settings file, because nothing in that page is persisted until the user
    // finishes it.
    //
    // Returns a JSON object of the settings a real run would apply. Anything the
    // user has already set is absent, because a cape never overrides that.
    // Writes nothing: no settings, no boot config, no file copies, no CSP.
    std::string dryRunSettings(const std::string& jurisdiction);

    const Json::Value& getCapeInfo();
    bool hasFile(const std::string& path);
    std::vector<uint8_t> getFile(const std::string& path);

    int getLicensedOutputs();
    std::string getKeyId();

    bool getStringConfig(const std::string& type, Json::Value& val);
    bool getPanelConfig(const std::string& type, Json::Value& val);
    bool getPWMConfig(const std::string& type, Json::Value& val);

private:
    CapeUtils();
    ~CapeUtils();
    CapeInfo* initCapeInfo(bool ro = true, bool forceDefaults = false);

    CapeInfo* capeInfo;
};
