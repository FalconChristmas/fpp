#pragma once
/*
*   The Falcon Player (FPP) is free software; you can redistribute it
*   and/or modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation; either version 2 of
*   the License, or (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include "Commands.h"

class SetVolumeCommand : public Command {
public:
    SetVolumeCommand() : Command("Volume Set", "Sets the volume to the specific value. (0 - 100)") {
        args.push_back(CommandArg("volume", "int", "Volume").setRange(0, 100).setDefaultValue("70")
                       .setGetAdjustableValueURL("api/fppd/volume?simple=true"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class AdjustVolumeCommand : public Command {
public:
    AdjustVolumeCommand() : Command("Volume Adjust", "Adjust volume either up or down by the given amount. (-100 - 100)") {
        args.push_back(CommandArg("volume", "int", "Volume").setRange(-100, 100).setDefaultValue("0"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class IncreaseVolumeCommand : public Command {
public:
    IncreaseVolumeCommand() : Command("Volume Increase", "Increases the volume by the given amount (0 - 100)") {
        args.push_back(CommandArg("volume", "int", "Volume").setRange(0, 100).setDefaultValue("0"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class DecreaseVolumeCommand : public Command {
public:
    DecreaseVolumeCommand() : Command("Volume Decrease", "Decreases the volume by the given amount (0 - 100)") {
        args.push_back(CommandArg("volume", "int", "Volume").setRange(0, 100).setDefaultValue("0"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};

class PlayMediaCommand : public Command {
public:
    PlayMediaCommand() : Command("Play Media", "Plays the media in the background") {
        args.push_back(CommandArg("media", "string", "Media").setContentListUrl("api/media"));
        args.push_back(CommandArg("loop", "int", "Loop Count").setRange(1, 100).setDefaultValue("1"));
        args.push_back(CommandArg("volume", "int", "Volume Adjust").setRange(-100, 100).setDefaultValue("0"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};


class URLCommand : public Command {
public:
    URLCommand() : Command("URL") {
        args.push_back(CommandArg("name", "string", "URL"));
        args.push_back(CommandArg("method", "string", "Method").setContentList({"GET", "POST"}).setDefaultValue("GET"));
        args.push_back(CommandArg("data", "string", "Post Data"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};

