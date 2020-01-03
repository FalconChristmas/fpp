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
#ifndef __FPP_PLAYLIST_COMMMANDS__
#define __FPP_PLAYLIST_COMMMANDS__

#include "Commands.h"

class StopPlaylistCommand : public Command {
public:
    StopPlaylistCommand() : Command("Stop Now") {}
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class StopGracefullyPlaylistCommand : public Command {
public:
    StopGracefullyPlaylistCommand() : Command("Stop Gracefully") {}
    
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
                        .setContentListUrl("api/playlists"));
        args.push_back(CommandArg("repeat", "bool", "Repeat", true));
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
    InsertPlaylistCommand() : Command("Insert Playlist After Current") {
        args.push_back(CommandArg("name", "string", "Playlist Name")
                        .setContentListUrl("api/playlists"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};

#endif
