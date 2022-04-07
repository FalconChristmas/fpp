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

class StopPlaylistCommand : public Command {
public:
    StopPlaylistCommand() :
        Command("Stop Now") {}

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class StopGracefullyPlaylistCommand : public Command {
public:
    StopGracefullyPlaylistCommand() :
        Command("Stop Gracefully") {
        args.push_back(CommandArg("loop", "bool", "After Loop", false));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class RestartPlaylistCommand : public Command {
public:
    RestartPlaylistCommand() :
        Command("Restart Playlist Item") {}

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class NextPlaylistCommand : public Command {
public:
    NextPlaylistCommand() :
        Command("Next Playlist Item") {}

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class PrevPlaylistCommand : public Command {
public:
    PrevPlaylistCommand() :
        Command("Prev Playlist Item") {}

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class StartPlaylistCommand : public Command {
public:
    StartPlaylistCommand() :
        Command("Start Playlist") {
        args.push_back(CommandArg("name", "string", "Playlist Name")
                           .setContentListUrl("api/playlists/playable"));
        args.push_back(CommandArg("repeat", "bool", "Repeat", true).setDefaultValue("false"));
        args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class TogglePlaylistCommand : public Command {
public:
    TogglePlaylistCommand() :
        Command("Toggle Playlist") {
        args.push_back(CommandArg("name", "string", "Playlist Name")
                           .setContentListUrl("api/playlists/playable"));
        args.push_back(CommandArg("repeat", "bool", "Repeat", true).setDefaultValue("false"));
        args.push_back(CommandArg("stop", "string", "Stop Type")
                           .setContentList({ "Gracefully", "After Loop", "Now" })
                           .setDefaultValue("Gracefully"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class StartPlaylistAtCommand : public Command {
public:
    StartPlaylistAtCommand() :
        Command("Start Playlist At Item") {
        args.push_back(CommandArg("name", "string", "Playlist Name")
                           .setContentListUrl("api/playlists"));
        args.push_back(CommandArg("item", "int", "Item Index").setRange(1, 100));
        args.push_back(CommandArg("repeat", "bool", "Repeat", true).setDefaultValue("false"));
        args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class StartPlaylistAtRandomCommand : public Command {
public:
    StartPlaylistAtRandomCommand() :
        Command("Start Playlist At Random Item") {
        args.push_back(CommandArg("name", "string", "Playlist Name")
                           .setContentListUrl("api/playlists"));
        args.push_back(CommandArg("repeat", "bool", "Repeat", true).setDefaultValue("false"));
        args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class InsertPlaylistCommand : public Command {
public:
    InsertPlaylistCommand() :
        Command("Insert Playlist After Current", "After the current item of the currently running playlist is complete, run the new playlist.  When complete, resumes the original playlist at the next position.") {
        args.push_back(CommandArg("name", "string", "Playlist Name")
                           .setContentListUrl("api/playlists/playable"));
        args.push_back(CommandArg("startItem", "int", "Start Item Index", true).setRange(-1, 100).setDefaultValue("-1"));
        args.push_back(CommandArg("endItem", "int", "End Item Index", true).setRange(-1, 100).setDefaultValue("-1"));
        args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class InsertPlaylistImmediate : public Command {
public:
    InsertPlaylistImmediate() :
        Command("Insert Playlist Immediate", "If possible, pauses the currently running playlist and then runs the new playlist.  When complete, resumes the original playlist where it was paused.") {
        args.push_back(CommandArg("name", "string", "Playlist Name")
                           .setContentListUrl("api/playlists/playable"));
        args.push_back(CommandArg("startItem", "int", "Start Item Index", true).setRange(0, 100).setDefaultValue("0"));
        args.push_back(CommandArg("endItem", "int", "End Item Index", true).setRange(0, 100).setDefaultValue("0"));
        args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class PlaylistPauseCommand : public Command {
public:
    PlaylistPauseCommand() :
        Command("Pause Playlist") {
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class PlaylistResumeCommand : public Command {
public:
    PlaylistResumeCommand() :
        Command("Resume Playlist") {
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
