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

    CapeStatus initCape(bool readOnly = true);

    const Json::Value& getCapeInfo();
    bool hasFile(const std::string& path);
    std::vector<uint8_t> getFile(const std::string& path);

    int getLicensedOutputs();
    std::string getKeyId();

    bool getStringConfig(const std::string& type, Json::Value& val);
    bool getPWMConfig(const std::string& type, Json::Value& val);

private:
    CapeUtils();
    ~CapeUtils();
    CapeInfo* initCapeInfo(bool ro = true);

    CapeInfo* capeInfo;
};
