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
#ifndef __FPP_MEDIA_COMMMANDS__
#define __FPP_MEDIA_COMMMANDS__

#include "Commands.h"

class SetVolumeCommand : public Command {
public:
    SetVolumeCommand() : Command("Volume Set") {
        args.push_back(CommandArg("volume", "int", "Volume").setRange(0, 100));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class AdjustVolumeCommand : public Command {
public:
    AdjustVolumeCommand() : Command("Volume Adjust") {
        args.push_back(CommandArg("volume", "int", "Volume").setRange(-100, 100));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class IncreaseVolumeCommand : public Command {
public:
    IncreaseVolumeCommand() : Command("Volume Increase") {
        args.push_back(CommandArg("volume", "int", "Volume").setRange(0, 100));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class DecreaseVolumeCommand : public Command {
public:
    DecreaseVolumeCommand() : Command("Volume Decrease") {
        args.push_back(CommandArg("volume", "int", "Volume").setRange(0, 100));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};


class URLCommand : public Command {
public:
    URLCommand() : Command("URL") {
        args.push_back(CommandArg("name", "string", "URL"));
        args.push_back(CommandArg("method", "string", "Method").setContentList({"GET", "POST"}));
        args.push_back(CommandArg("data", "string", "Post Data"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};



#endif
