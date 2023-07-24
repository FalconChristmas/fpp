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

#include "../common.h"
#include "../log.h"

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

    pthread_mutex_init(&m_testLock, NULL);
}

/*
 *
 */
ChannelTester::~ChannelTester() {
    LogExcess(VB_CHANNELOUT, "ChannelTester::~ChannelTester()\n");

    pthread_mutex_lock(&m_testLock);

    if (m_testPattern) {
        delete m_testPattern;
        m_testPattern = NULL;
    }

    pthread_mutex_unlock(&m_testLock);

    pthread_mutex_destroy(&m_testLock);
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
        return "FF000000FF000000FF";
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
        } else if (effect == "RGB Cycle") {
            config["mode"] = "RGBCycle";
            config["subMode"] = std::string("RGBCycle-") + mapSubMode(args[3]);
            config["colorPattern"] = mapPattern(args[3]);
        } else if (effect == "Custom Chase") {
            config["mode"] = "RGBChase";
            config["subMode"] = "RGBChase-RGBCustom";
            config["colorPattern"] = args[3];
        } else if (effect == "Custom Cycle") {
            config["mode"] = "RGBCycle";
            config["subMode"] = "RGBCycle-RGBCustom";
            config["colorPattern"] = args[3];
        } else if (effect == "RGB Single Color") {
            config["mode"] = "RGBFill";
            int v = std::stoi(args[3].substr(1), nullptr, 16);
            config["color3"] = v & 0xFF;
            config["color2"] = (v >> 8) & 0xFF;
            config["color1"] = (v >> 16) & 0xFF;
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
    CommandManager::INSTANCE.addCommand(new StartTestingCommand());
    CommandManager::INSTANCE.addCommand(new StopTestingCommand());
}
HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> ChannelTester::render_GET(const httpserver::http_request& req) {
    Json::Value result;
    int plen = req.get_path_pieces().size();
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
        std::string effect = req.get_path_pieces()[3];
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
            v["contents"].append("R-G-B-All");
            v["contents"].append("R-G-B-None");
            v["contents"].append("R-G-B-All-None");
            result["args"].append(v);
        } else if (effect == "RGB Cycle") {
            Json::Value v;
            v["description"] = "Cycle Type";
            v["name"] = "CycleType";
            v["optional"] = false;
            v["type"] = "string";
            v["contents"].append("R-G-B");
            v["contents"].append("R-G-B-All");
            v["contents"].append("R-G-B-None");
            v["contents"].append("R-G-B-All-None");
            result["args"].append(v);
        } else if (effect == "Custom Chase") {
            Json::Value v;
            v["description"] = "Pattern";
            v["name"] = "Pattern";
            v["optional"] = false;
            v["type"] = "string";
            v["default"] = "FF000000FF000000FF";
            result["args"].append(v);
        } else if (effect == "Custom Cycle") {
            Json::Value v;
            v["description"] = "Pattern";
            v["name"] = "Pattern";
            v["optional"] = false;
            v["type"] = "string";
            v["default"] = "FF000000FF000000FF";
            result["args"].append(v);
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
            v["max"] = 2;
            result["args"].append(v);
        } else {
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Test Pattern " + effect + " not found", 400, "text/plain"));
        }
    }
    std::string resultStr = SaveJsonToString(result);
    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
}
HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> ChannelTester::render_POST(const httpserver::http_request& req) {
    Json::Value result;
    std::string content = std::string{ req.get_content() };
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
    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
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
    pthread_mutex_lock(&m_testLock);
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
            pthread_mutex_unlock(&m_testLock);

            // Give the channel output loop time to clear test data
            usleep(150000);

            pthread_mutex_lock(&m_testLock);

            delete m_testPattern;
            m_testPattern = NULL;
        }
    }

    pthread_mutex_unlock(&m_testLock);

    m_configStr = SaveJsonToString(config);
    return result;
}

/*
 *
 */
void ChannelTester::OverlayTestData(char* channelData) {
    LogExcess(VB_CHANNELOUT, "ChannelTester::OverlayTestData()\n");

    pthread_mutex_lock(&m_testLock);

    if (!m_testPattern) {
        pthread_mutex_unlock(&m_testLock);
        return;
    }

    m_testPattern->OverlayTestData(channelData);

    pthread_mutex_unlock(&m_testLock);
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
