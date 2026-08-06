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

#include "fpp-pch.h"

#include "fpp-json.h"
#include "fpphttp.h" // drogon/HTTP helpers used here; no longer pulled transitively (see fpphttp_types.h)

#include "../common.h"
#include "../log.h"
#include <unistd.h> // usleep -- needed directly for NOPCH builds

#include "ChannelTester.h"
#include "channeloutput/ChannelOutputSetup.h"
#include "commands/Commands.h"

// Test Patterns
#include "OutputTester.h"
#include "RGBChase.h"
#include "RGBCycle.h"
#include "RGBFill.h"
#include "SingleChase.h"

ChannelTester ChannelTester::INSTANCE;

/*
 *
 */
ChannelTester::ChannelTester() :
    m_testPattern(NULL) {
    LogExcess(VB_CHANNELOUT, "ChannelTester::ChannelTester()\n");
}

/*
 *
 */
ChannelTester::~ChannelTester() {
    LogExcess(VB_CHANNELOUT, "ChannelTester::~ChannelTester()\n");

    std::lock_guard<std::mutex> lock(m_testLock);

    if (m_testPattern) {
        delete m_testPattern;
        m_testPattern = NULL;
    }
}

class StopTestingCommand : public Command {
public:
    StopTestingCommand() :
        Command("Test Stop") {
    }
    virtual ~StopTestingCommand() {}

    virtual std::unique_ptr<Result> run(const std::vector<std::string>& args) {
        Json::Value config;
        config["enabled"] = 0;
        ChannelTester::INSTANCE.SetupTest(config);
        return std::make_unique<Result>("Stopped");
    }
};
class StartTestingCommand : public Command {
public:
    StartTestingCommand() :
        Command("Test Start") {
        args.push_back(CommandArg("UpdateInterval", "int", "Update Interval (ms)").setDefaultValue("1000").setRange(100, 10000));
        args.push_back(CommandArg("TestPattern", "subcommand", "Test Pattern").setContentListUrl("api/fppd/testing/tests/"));
    }
    virtual ~StartTestingCommand() {}

    std::string mapSubMode(const std::string& m) {
        if (m == "R-G-B-All") {
            return "RGBA";
        }
        if (m == "R-G-B-None") {
            return "RGBN";
        }
        if (m == "R-G-B-All-None") {
            return "RGBAN";
        }
        if (m == "R-G-B-W") {
            return "RGBW";
        }
        if (m == "R-G-B-W-All") {
            return "RGBWA";
        }
        if (m == "R-G-B-W-None") {
            return "RGBWN";
        }
        if (m == "R-G-B-W-All-None") {
            return "RGBWAN";
        }
        return "RGB";
    }
    std::string mapPattern(const std::string& m) {
        if (m == "R-G-B-All") {
            return "FF000000FF000000FFFFFFFF";
        }
        if (m == "R-G-B-None") {
            return "FF000000FF000000FF000000";
        }
        if (m == "R-G-B-All-None") {
            return "FF000000FF000000FFFFFFFF000000";
        }
        // RGBW patterns use 4-channel (8 hex digit) groups: R, G, B, W, All, None
        if (m == "R-G-B-W") {
            return "FF00000000FF00000000FF00000000FF";
        }
        if (m == "R-G-B-W-All") {
            return "FF00000000FF00000000FF00000000FFFFFFFFFF";
        }
        if (m == "R-G-B-W-None") {
            return "FF00000000FF00000000FF00000000FF00000000";
        }
        if (m == "R-G-B-W-All-None") {
            return "FF00000000FF00000000FF00000000FFFFFFFFFF00000000";
        }
        return "FF000000FF000000FF";
    }
    // Number of channels per pixel implied by the chase/cycle type (4 for RGBW patterns, 3 otherwise)
    int mapChannelsPerNode(const std::string& m) {
        return (m.find("W") != std::string::npos) ? 4 : 3;
    }

    virtual std::unique_ptr<Result> run(const std::vector<std::string>& args) {
        // printf("Run test:\n");
        // for (auto& a : args) {
        //     printf("%s\n", a.c_str());
        // }
        Json::Value config;
        config["enabled"] = 1;
        config["cycleMS"] = std::stoi(args[0], nullptr, 10);
        std::string effect = args[1];
        config["colorPattern"] = "";
        if (effect == "RGB Chase") {
            config["mode"] = "RGBChase";
            config["subMode"] = std::string("RGBChase-") + mapSubMode(args[3]);
            config["colorPattern"] = mapPattern(args[3]);
            config["channelsPerNode"] = mapChannelsPerNode(args[3]);
        } else if (effect == "RGB Cycle") {
            config["mode"] = "RGBCycle";
            config["subMode"] = std::string("RGBCycle-") + mapSubMode(args[3]);
            config["colorPattern"] = mapPattern(args[3]);
            config["channelsPerNode"] = mapChannelsPerNode(args[3]);
        } else if (effect == "Custom Chase") {
            config["mode"] = "RGBChase";
            config["subMode"] = "RGBChase-RGBCustom";
            config["colorPattern"] = args[3];
            if (args.size() > 4) {
                config["channelsPerNode"] = std::stoi(args[4], nullptr, 10);
            }
        } else if (effect == "Custom Cycle") {
            config["mode"] = "RGBCycle";
            config["subMode"] = "RGBCycle-RGBCustom";
            config["colorPattern"] = args[3];
            if (args.size() > 4) {
                config["channelsPerNode"] = std::stoi(args[4], nullptr, 10);
            }
        } else if (effect == "RGB Single Color") {
            config["mode"] = "RGBFill";
            std::string hexStr = args[3].substr(1); // Remove # prefix
            // 8 hex digits (RGBW) can exceed INT_MAX, so parse as unsigned
            uint32_t v = (uint32_t)std::stoul(hexStr, nullptr, 16);

            if (hexStr.length() == 8) {
                // RGBW format (8 hex digits) - 4 channels per pixel
                config["channelsPerNode"] = 4;
                config["color4"] = (int)(v & 0xFF);         // W
                config["color3"] = (int)((v >> 8) & 0xFF);  // B
                config["color2"] = (int)((v >> 16) & 0xFF); // G
                config["color1"] = (int)((v >> 24) & 0xFF); // R
            } else {
                // RGB format (6 hex digits) - 3 channels per pixel
                config["channelsPerNode"] = 3;
                config["color3"] = (int)(v & 0xFF);         // B
                config["color2"] = (int)((v >> 8) & 0xFF);  // G
                config["color1"] = (int)((v >> 16) & 0xFF); // R
            }
        } else if (effect == "Single Channel Chase") {
            config["mode"] = "SingleChase";
            int v = std::stoi(args[3], nullptr, 10);
            config["chaseValue"] = v;
            config["chaseSize"] = std::stoi(args[4], nullptr, 10);
        } else if (effect == "Single Channel Fill") {
            config["mode"] = "RGBFill";
            int v = std::stoi(args[3], nullptr, 10);
            config["color1"] = v;
            config["color2"] = v;
            config["color3"] = v;
        } else if (effect == "Output Specific") {
            config["mode"] = "Outputs";
            config["outputs"] = args[2];
            int v = std::stoi(args[3], nullptr, 10);
            config["type"] = v;
            if (args.size() > 4) {
                config["config"] = LoadJsonFromString(args[4]);
            }
        }
        if (effect != "Output Specific") {
            if (args[2] == "" || args[2] == "*") {
                config["channelSet"] = "1-" + std::to_string(FPPD_MAX_CHANNELS);
            } else {
                config["channelSet"] = args[2];
            }
        }
        config["channelSetType"] = "channelRange";
        ChannelTester::INSTANCE.SetupTest(config);
        return std::make_unique<Result>("Started");
    }
};
void ChannelTester::StopTest() {
    Json::Value config;
    config["enabled"] = 0;
    ChannelTester::INSTANCE.SetupTest(config);
}

void ChannelTester::RegisterCommands() {
    CommandManager::INSTANCE.addCategorizedCommand(new StartTestingCommand(), "Outputs", 1);
    CommandManager::INSTANCE.addCategorizedCommand(new StopTestingCommand(), "Outputs", 1);
}
// --------------------------------------------------------------------------
// OpenAPI docs for the /fppd/testing/* endpoints handled below.
// --------------------------------------------------------------------------

/**
 * Get the current test-mode configuration.
 *
 * @route GET /api/fppd/testing
 * @response 200 Object with the current test `config`.
 */

/**
 * List the available test pattern names.
 *
 * @route GET /api/fppd/testing/tests
 * @response 200 Array of test pattern names.
 * ```json
 * ["RGB Chase", "RGB Cycle", "Custom Chase", "Custom Cycle", "RGB Single Color", "Single Channel Chase", "Single Channel Fill", "Output Specific"]
 * ```
 */

/**
 * Get the argument definitions for a specific test pattern.
 *
 * @route GET /api/fppd/testing/tests/{pattern}
 * @response 200 Object with an `args` array describing the pattern's inputs.
 * @response 400 The named test pattern does not exist.
 */
HttpResponsePtr ChannelTester::render_GET(const HttpRequestPtr& req) {
    Json::Value result;
    auto parts = getPathPieces(req->path());
    int plen = parts.size();
    //  ex:  /fppd/testing/tests/RGB Chase
    if (plen == 2) {
        result["config"] = LoadJsonFromString(GetConfig().c_str());
        result["Status"] = "OK";
        result["respCode"] = 200;
        result["Message"] = "";
    } else if (plen == 3) {
        result.append("RGB Chase");
        result.append("RGB Cycle");
        result.append("Custom Chase");
        result.append("Custom Cycle");
        result.append("RGB Single Color");
        result.append("Single Channel Chase");
        result.append("Single Channel Fill");
        result.append("Output Specific");
    } else if (plen == 4) {
        std::string effect = parts[3];
        if (effect != "Output Specific") {
            Json::Value vcr;
            vcr["allowBlanks"] = false;
            vcr["contentListUrl"] = "api/models?simple=true";
            std::string rng = GetOutputRangesAsString(false, true);
            vcr["contents"].append(rng.c_str());
            vcr["description"] = "Channel Range/Model";
            vcr["name"] = "ChannelRange";
            vcr["optional"] = false;
            vcr["type"] = "datalist";
            result["args"].append(vcr);
        }
        if (effect == "RGB Chase") {
            Json::Value v;
            v["description"] = "Chase Type";
            v["name"] = "ChaseType";
            v["optional"] = false;
            v["type"] = "string";
            v["contents"].append("R-G-B");
            v["contents"].append("R-G-B-W");
            v["contents"].append("R-G-B-All");
            v["contents"].append("R-G-B-W-All");
            v["contents"].append("R-G-B-None");
            v["contents"].append("R-G-B-W-None");
            v["contents"].append("R-G-B-All-None");
            v["contents"].append("R-G-B-W-All-None");
            result["args"].append(v);
        } else if (effect == "RGB Cycle") {
            Json::Value v;
            v["description"] = "Cycle Type";
            v["name"] = "CycleType";
            v["optional"] = false;
            v["type"] = "string";
            v["contents"].append("R-G-B");
            v["contents"].append("R-G-B-W");
            v["contents"].append("R-G-B-All");
            v["contents"].append("R-G-B-W-All");
            v["contents"].append("R-G-B-None");
            v["contents"].append("R-G-B-W-None");
            v["contents"].append("R-G-B-All-None");
            v["contents"].append("R-G-B-W-All-None");
            result["args"].append(v);
        } else if (effect == "Custom Chase") {
            Json::Value v;
            v["description"] = "Pattern";
            v["name"] = "Pattern";
            v["optional"] = false;
            v["type"] = "string";
            v["default"] = "FF000000FF000000FF";
            result["args"].append(v);
            Json::Value cpn;
            cpn["description"] = "Channels Per Node";
            cpn["name"] = "ChannelsPerNode";
            cpn["optional"] = true;
            cpn["type"] = "string";
            cpn["contents"].append("3");
            cpn["contents"].append("4");
            cpn["default"] = "3";
            result["args"].append(cpn);
        } else if (effect == "Custom Cycle") {
            Json::Value v;
            v["description"] = "Pattern";
            v["name"] = "Pattern";
            v["optional"] = false;
            v["type"] = "string";
            v["default"] = "FF000000FF000000FF";
            result["args"].append(v);
            Json::Value cpn;
            cpn["description"] = "Channels Per Node";
            cpn["name"] = "ChannelsPerNode";
            cpn["optional"] = true;
            cpn["type"] = "string";
            cpn["contents"].append("3");
            cpn["contents"].append("4");
            cpn["default"] = "3";
            result["args"].append(cpn);
        } else if (effect == "RGB Single Color") {
            Json::Value v;
            v["description"] = "Color";
            v["name"] = "Color";
            v["optional"] = false;
            v["type"] = "color";
            v["default"] = "#FFFFFF";
            result["args"].append(v);
        } else if (effect == "Single Channel Chase") {
            Json::Value v;
            v["description"] = "Value";
            v["name"] = "Value";
            v["optional"] = false;
            v["type"] = "range";
            v["default"] = "255";
            v["min"] = 0;
            v["max"] = 255;
            result["args"].append(v);

            Json::Value v2;
            v2["description"] = "Chase Size";
            v2["name"] = "ChaseSize";
            v2["optional"] = false;
            v2["type"] = "int";
            v2["default"] = "2";
            v2["min"] = 2;
            v2["max"] = 10;
            result["args"].append(v2);
        } else if (effect == "Single Channel Fill") {
            Json::Value v;
            v["description"] = "Value";
            v["name"] = "Value";
            v["optional"] = false;
            v["type"] = "range";
            v["default"] = "255";
            v["min"] = 0;
            v["max"] = 255;
            result["args"].append(v);
        } else if (effect == "Output Specific") {
            Json::Value vcr;
            vcr["allowBlanks"] = false;
            vcr["contents"].append("--ALL--");
            for (auto& a : GetOutputTypes()) {
                vcr["contents"].append(a);
            }
            vcr["default"] = "--ALL--";
            vcr["description"] = "Output Types";
            vcr["name"] = "OutputTypes";
            vcr["optional"] = false;
            vcr["type"] = "string";
            result["args"].append(vcr);

            Json::Value v;
            v["description"] = "Test Type";
            v["name"] = "TestType";
            v["optional"] = false;
            v["type"] = "range";
            v["default"] = "1";
            v["min"] = 1;
            v["max"] = 99;
            result["args"].append(v);
        } else {
            return makeStringResponse("Test Pattern " + effect + " not found", 400, "text/plain");
        }
    }
    std::string resultStr = SaveJsonToString(result);
    return makeStringResponse(resultStr, 200, "application/json");
}
/**
 * Activate or deactivate test mode. POST the test configuration JSON (with an
 * `enabled` flag); an empty/disabled config turns test mode off.
 *
 * @route POST /api/fppd/testing
 * @response 200 Test mode activated or deactivated.
 */
HttpResponsePtr ChannelTester::render_POST(const HttpRequestPtr& req) {
    Json::Value result;
    std::string content = std::string{ getRequestContent(req) };
    LogDebug(VB_COMMAND, "POST /fppd/testing from %s: \"%s\"\n", req->getPeerAddr().toIp().c_str(), TruncateForLog(content, 2000).c_str());
    if (ChannelTester::INSTANCE.SetupTest(content)) {
        result["Status"] = "OK";
        result["respCode"] = 200;
        result["Message"] = "Test Mode Activated";
    } else {
        result["Status"] = "OK";
        result["respCode"] = 200;
        result["Message"] = "Test Mode Deactivated";
    }
    std::string resultStr = SaveJsonToString(result);
    return makeStringResponse(resultStr, 200, "application/json");
}
/*
 *
 */
int ChannelTester::SetupTest(const std::string configStr) {
    LogDebug(VB_CHANNELOUT, "ChannelTester::SetupTest()\n");
    LogDebug(VB_CHANNELOUT, "     %s\n", configStr.c_str());

    Json::Value config;

    if (!LoadJsonFromString(configStr, config)) {
        LogErr(VB_CHANNELOUT,
               "Error parsing Test Pattern config string: '%s'\n",
               configStr.c_str());

        return 0;
    }
    return SetupTest(config);
}
int ChannelTester::SetupTest(const Json::Value& config) {
    // The config reaches here straight from the API and the command socket, so
    // it can be any shape at all.  The test patterns read it with jsoncpp
    // accessors, which throw on a type mismatch (a color sent as "#202020"
    // rather than an int, say), and the command dispatch above has no handler
    // for that, so a single malformed request used to abort fppd.
    try {
        return SetupTestInternal(config);
    } catch (const Json::Exception& e) {
        LogErr(VB_CHANNELOUT, "Ignoring malformed Test Pattern config: %s\n", e.what());
        // A pattern that threw part way through Init() must not be left
        // behind: Testing() only checks that the pointer is set, so it would
        // be handed to OverlayTestData with its channel data unallocated.
        // This mirrors the cleanup the Init() failure path below does.
        std::unique_lock<std::mutex> lock(m_testLock);
        if (m_testPattern) {
            delete m_testPattern;
            m_testPattern = nullptr;
        }
        return 0;
    }
}

int ChannelTester::SetupTestInternal(const Json::Value& config) {
    // unique_lock (not lock_guard) because this function unlocks and
    // re-locks mid-function below while waiting for the channel output
    // loop to clear test data.
    std::unique_lock<std::mutex> lock(m_testLock);
    int result = 0;

    if (config["enabled"].asInt()) {
        std::string patternName = config["mode"].asString();

        if (m_testPattern) {
            if (patternName != m_testPattern->Name()) {
                delete m_testPattern;
                m_testPattern = NULL;
            }
        }

        if (!m_testPattern) {
            if (patternName == "SingleChase")
                m_testPattern = new TestPatternSingleChase();
            else if (patternName == "RGBChase")
                m_testPattern = new TestPatternRGBChase();
            else if (patternName == "RGBFill")
                m_testPattern = new TestPatternRGBFill();
            else if (patternName == "RGBCycle")
                m_testPattern = new TestPatternRGBCycle();
            else if (patternName == "Outputs")
                m_testPattern = new OutputTester();
        }

        if (m_testPattern) {
            result = m_testPattern->Init(config);
            if (!result) {
                delete m_testPattern;
                m_testPattern = NULL;
            }
        }
    } else {
        if (m_testPattern) {
            m_testPattern->DisableTest();
            lock.unlock();

            // Give the channel output loop time to clear test data
            usleep(150000);

            lock.lock();

            delete m_testPattern;
            m_testPattern = NULL;
        }
    }

    // Matches the coverage of the original pthread_mutex_unlock() here:
    // m_configStr is intentionally set outside the lock.
    lock.unlock();

    m_configStr = SaveJsonToString(config);
    return result;
}

/*
 *
 */
void ChannelTester::OverlayTestData(char* channelData) {
    LogExcess(VB_CHANNELOUT, "ChannelTester::OverlayTestData()\n");

    // lock_guard releases the lock on every return path (including the
    // early return below), matching the coverage of the previous
    // pthread_mutex_lock()/unlock() pairs.
    std::lock_guard<std::mutex> lock(m_testLock);

    if (!m_testPattern) {
        return;
    }

    m_testPattern->OverlayTestData(channelData);
}

/*
 *
 */
int ChannelTester::Testing(void) {
    return m_testPattern ? 1 : 0;
}

/*
 *
 */
std::string ChannelTester::GetConfig(void) {
    if (!m_testPattern)
        return std::string("{ \"enabled\": 0 }");
    return m_configStr;
}
