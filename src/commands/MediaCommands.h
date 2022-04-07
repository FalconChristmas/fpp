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

#include "Commands.h"
#include "mediaoutput/mediaoutput.h"
#include "mediaoutput/VLCOut.h"

class SetVolumeCommand : public Command {
public:
    SetVolumeCommand() :
        Command("Volume Set", "Sets the volume to the specific value. (0 - 100)") {
        args.push_back(CommandArg("volume", "int", "Volume").setRange(0, 100).setDefaultValue("70").setGetAdjustableValueURL("api/fppd/volume?simple=true"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class AdjustVolumeCommand : public Command {
public:
    AdjustVolumeCommand() :
        Command("Volume Adjust", "Adjust volume either up or down by the given amount. (-100 - 100)") {
        args.push_back(CommandArg("volume", "int", "Volume").setRange(-100, 100).setDefaultValue("0"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class IncreaseVolumeCommand : public Command {
public:
    IncreaseVolumeCommand() :
        Command("Volume Increase", "Increases the volume by the given amount (0 - 100)") {
        args.push_back(CommandArg("volume", "int", "Volume").setRange(0, 100).setDefaultValue("0"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class DecreaseVolumeCommand : public Command {
public:
    DecreaseVolumeCommand() :
        Command("Volume Decrease", "Decreases the volume by the given amount (0 - 100)") {
        args.push_back(CommandArg("volume", "int", "Volume").setRange(0, 100).setDefaultValue("0"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class URLCommand : public Command {
public:
    URLCommand() :
        Command("URL") {
        args.push_back(CommandArg("name", "string", "URL"));
        args.push_back(CommandArg("method", "string", "Method").setContentList({ "GET", "POST" }).setDefaultValue("GET"));
        args.push_back(CommandArg("data", "string", "Post Data"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};


#ifdef HAS_VLC
class PlayMediaCommand : public Command {
public:
    PlayMediaCommand() :
        Command("Play Media", "Plays the media in the background") {
        args.push_back(CommandArg("media", "string", "Media").setContentListUrl("api/media"));
        args.push_back(CommandArg("loop", "int", "Loop Count").setRange(1, 100).setDefaultValue("1"));
        args.push_back(CommandArg("volume", "int", "Volume Adjust").setRange(-100, 100).setDefaultValue("0"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StopMediaCommand : public Command {
public:
    StopMediaCommand() :
        Command("Stop Media", "Stops the media in the background started via a command") {
        args.push_back(CommandArg("media", "string", "Media").setContentListUrl("api/media"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StopAllMediaCommand : public Command {
public:
    StopAllMediaCommand() :
        Command("Stop All Media", "Stops all running media was was created via a Command") {
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
#endif
