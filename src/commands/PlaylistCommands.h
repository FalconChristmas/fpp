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

class StopPlaylistCommand : public Command {
public:
    StopPlaylistCommand() : Command("Stop Now") {}
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class StopGracefullyPlaylistCommand : public Command {
public:
    StopGracefullyPlaylistCommand() : Command("Stop Gracefully") {
        args.push_back(CommandArg("loop", "bool", "After Loop", false));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class RestartPlaylistCommand : public Command {
public:
    RestartPlaylistCommand() : Command("Restart Playlist Item") {}
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class NextPlaylistCommand : public Command {
public:
    NextPlaylistCommand() : Command("Next Playlist Item") {}
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class PrevPlaylistCommand : public Command {
public:
    PrevPlaylistCommand() : Command("Prev Playlist Item") {}
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class StartPlaylistCommand : public Command {
public:
    StartPlaylistCommand() : Command("Start Playlist") {
        args.push_back(CommandArg("name", "string", "Playlist Name")
                        .setContentListUrl("api/playlists/playable"));
        args.push_back(CommandArg("repeat", "bool", "Repeat", true));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class TogglePlaylistCommand : public Command {
public:
    TogglePlaylistCommand() : Command("Toggle Playlist") {
        args.push_back(CommandArg("name", "string", "Playlist Name")
                        .setContentListUrl("api/playlists/playable"));
        args.push_back(CommandArg("repeat", "bool", "Repeat", true));
        args.push_back(CommandArg("stop", "string", "Stop Type")
                       .setContentList({"Gracefully", "After Loop", "Now"})
                       .setDefaultValue("Gracefully"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class StartPlaylistAtCommand : public Command {
public:
    StartPlaylistAtCommand() : Command("Start Playlist At Item") {
        args.push_back(CommandArg("name", "string", "Playlist Name")
                        .setContentListUrl("api/playlists"));
        args.push_back(CommandArg("item", "int", "Item Index").setRange(1, 100));
        args.push_back(CommandArg("repeat", "bool", "Repeat", true));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class StartPlaylistAtRandomCommand : public Command {
public:
    StartPlaylistAtRandomCommand() : Command("Start Playlist At Random Item") {
        args.push_back(CommandArg("name", "string", "Playlist Name")
                        .setContentListUrl("api/playlists"));
        args.push_back(CommandArg("repeat", "bool", "Repeat", true));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};


class InsertPlaylistCommand : public Command {
public:
    InsertPlaylistCommand() : Command("Insert Playlist After Current", "After the current item of the currently running playlist is complete, run the new playlist.  When complete, resumes the original playlist at the next position.") {
        args.push_back(CommandArg("name", "string", "Playlist Name")
                        .setContentListUrl("api/playlists/playable"));
        args.push_back(CommandArg("startItem", "int", "Start Item Index", true).setRange(-1, 100).setDefaultValue("-1"));
        args.push_back(CommandArg("endItem", "int", "End Item Index", true).setRange(-1, 100).setDefaultValue("-1"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};

class InsertPlaylistImmediate : public Command {
public:
    InsertPlaylistImmediate() : Command("Insert Playlist Immediate", "If possible, pauses the currently running playlist and then runs the new playlist.  When complete, resumes the original playlist where it was paused.") {
        args.push_back(CommandArg("name", "string", "Playlist Name")
                        .setContentListUrl("api/playlists/playable"));
        args.push_back(CommandArg("startItem", "int", "Start Item Index", true).setRange(0, 100).setDefaultValue("0"));
        args.push_back(CommandArg("endItem", "int", "End Item Index", true).setRange(0, 100).setDefaultValue("0"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class PlaylistPauseCommand : public Command {
public:
    PlaylistPauseCommand() : Command("Pause Playlist") {
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class PlaylistResumeCommand : public Command {
public:
    PlaylistResumeCommand() : Command("Resume Playlist") {
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
